#include "Core/HTMVisualLibrary.h"
#include "Core/HTMSettings.h"
#include "Core/HTMPalette.h"
#include "Core/HTMPlaceholderFX.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

namespace
{
	UMaterialInterface* GetBaseMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		if (UMaterialInterface* M = Cached.Get())
		{
			return M;
		}
		UMaterialInterface* Material = nullptr;
		const UHTMSettings* Settings = UHTMSettings::Get();
		if (!Settings->MasterMaterial.IsNull())
		{
			Material = Settings->MasterMaterial.LoadSynchronous();
		}
		if (!Material)
		{
			Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		Cached = Material;
		return Material;
	}
}

UStaticMesh* UHTMVisualLibrary::GetShapeMesh(FName Shape)
{
	static TMap<FName, TWeakObjectPtr<UStaticMesh>> Cache;
	if (Shape.IsNone())
	{
		Shape = TEXT("Cube");
	}
	if (TWeakObjectPtr<UStaticMesh>* Found = Cache.Find(Shape))
	{
		if (UStaticMesh* Mesh = Found->Get())
		{
			return Mesh;
		}
	}
	const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), *Shape.ToString(), *Shape.ToString());
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	Cache.Add(Shape, Mesh);
	return Mesh;
}

UMaterialInstanceDynamic* UHTMVisualLibrary::ApplyPlaceholderMaterial(UPrimitiveComponent* Component, const FLinearColor& Color)
{
	if (!Component)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(GetBaseMaterial(), Component);
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);      // BasicShapeMaterial
		MID->SetVectorParameterValue(TEXT("BaseColor"), Color);  // M_Master
		MID->SetScalarParameterValue(TEXT("Roughness"), 0.8f);
		for (int32 i = 0; i < FMath::Max(1, Component->GetNumMaterials()); ++i)
		{
			Component->SetMaterial(i, MID);
		}
	}
	return MID;
}

void UHTMVisualLibrary::ApplySurface(UMaterialInstanceDynamic* MID, const FPartSurfaceState& S)
{
	if (!MID)
	{
		return;
	}
	MID->SetVectorParameterValue(TEXT("BaseColor"), S.BaseColor);
	MID->SetVectorParameterValue(TEXT("PaintColor"), S.PaintColor);
	MID->SetScalarParameterValue(TEXT("PaintAmount"), S.PaintAmount);
	MID->SetScalarParameterValue(TEXT("Dirt"), S.Dirt);
	MID->SetScalarParameterValue(TEXT("Rust"), S.Rust);
	MID->SetScalarParameterValue(TEXT("Wetness"), S.Wetness);
	MID->SetScalarParameterValue(TEXT("FreshPaint"), S.FreshPaint);
	MID->SetScalarParameterValue(TEXT("Damage"), S.bPaintRuined ? FMath::Max(S.Damage, 0.5f) : S.Damage);

	// Respaldo para BasicShapeMaterial: color compuesto en CPU.
	FLinearColor Composite = S.GetVisibleColor();
	Composite = FLinearColor::LerpUsingHSV(Composite, HTMPalette::Rust(), FMath::Clamp(S.Rust, 0.f, 1.f) * 0.8f);
	Composite = FMath::Lerp(Composite, HTMPalette::Dirt() * 0.6f, FMath::Clamp(S.Dirt, 0.f, 1.f) * 0.6f);
	Composite *= (1.f - 0.25f * FMath::Clamp(S.Wetness, 0.f, 1.f));
	if (S.bPaintRuined)
	{
		Composite = FMath::Lerp(Composite, HTMPalette::Dirt(), 0.35f);
	}
	Composite.A = 1.f;
	MID->SetVectorParameterValue(TEXT("Color"), Composite);
}

void UHTMVisualLibrary::SetHighlighted(UPrimitiveComponent* Component, UMaterialInstanceDynamic* MID, bool bHighlighted)
{
	if (Component)
	{
		Component->SetRenderCustomDepth(bHighlighted);
		Component->SetCustomDepthStencilValue(bHighlighted ? 1 : 0);
	}
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("Highlight"), bHighlighted ? 1.f : 0.f);
	}
}

UStaticMeshComponent* UHTMVisualLibrary::AddShape(AActor* Owner, USceneComponent* Parent, FName Shape, const FVector& Location,
	const FVector& SizeCm, const FRotator& Rotation, const FLinearColor& Color, bool bCollision, FName Name)
{
	if (!Owner)
	{
		return nullptr;
	}
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner, Name.IsNone() ? NAME_None : MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), Name));
	Comp->SetStaticMesh(GetShapeMesh(Shape));
	Comp->SetMobility(EComponentMobility::Movable);
	if (Parent)
	{
		Comp->SetupAttachment(Parent);
	}
	Comp->SetRelativeLocation(Location);
	Comp->SetRelativeRotation(Rotation);
	Comp->SetRelativeScale3D(SizeCm / 100.f);
	if (bCollision)
	{
		Comp->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Comp->SetCanEverAffectNavigation(bCollision);
	Comp->RegisterComponent();
	Owner->AddInstanceComponent(Comp);
	ApplyPlaceholderMaterial(Comp, Color);
	return Comp;
}

USceneComponent* UHTMVisualLibrary::SpawnFX(const UObject* WorldContext, EHTMFX Type, const FVector& Location, float Scale,
	USceneComponent* AttachTo, float Duration, FLinearColor Tint)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	const FName ShortKey(*StaticEnum<EHTMFX>()->GetNameStringByValue(static_cast<int64>(Type)));
	if (const TSoftObjectPtr<UNiagaraSystem>* SystemPtr = UHTMSettings::Get()->Effects.Find(ShortKey))
	{
		if (UNiagaraSystem* System = SystemPtr->LoadSynchronous())
		{
			UNiagaraComponent* NC = AttachTo
				? UNiagaraFunctionLibrary::SpawnSystemAttached(System, AttachTo, NAME_None, Location, FRotator::ZeroRotator, FVector(Scale), EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None)
				: UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location, FRotator::ZeroRotator, FVector(Scale), true);
			if (NC)
			{
				NC->SetVariableLinearColor(TEXT("User.Tint"), Tint);
			}
			return NC;
		}
	}

	// Placeholder de primitivas.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector WorldLocation = AttachTo ? AttachTo->GetComponentTransform().TransformPosition(Location) : Location;
	AHTMPlaceholderFX* FX = World->SpawnActor<AHTMPlaceholderFX>(WorldLocation, FRotator::ZeroRotator, Params);
	if (!FX)
	{
		return nullptr;
	}
	FX->Init(Type, Scale, Duration, Tint);
	if (AttachTo)
	{
		FX->AttachToComponent(AttachTo, FAttachmentTransformRules::KeepWorldTransform);
	}
	return FX->GetRootComponent();
}

void UHTMVisualLibrary::StopFX(USceneComponent* FX)
{
	if (!FX)
	{
		return;
	}
	if (UNiagaraComponent* NC = Cast<UNiagaraComponent>(FX))
	{
		NC->Deactivate();
		NC->SetAutoDestroy(true);
		return;
	}
	if (AActor* Owner = FX->GetOwner())
	{
		Owner->Destroy();
	}
}
