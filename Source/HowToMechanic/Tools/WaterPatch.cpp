#include "Tools/WaterPatch.h"
#include "Character/MechanicCharacter.h"
#include "Parts/SurfaceTreatable.h"
#include "Parts/CarPart.h"
#include "Vehicle/ModularCar.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AWaterPatch::AWaterPatch()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	SetRootComponent(Area);
	Area->SetBoxExtent(FVector(60.f, 60.f, 20.f));
	Area->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Area->SetCollisionResponseToAllChannels(ECR_Ignore);
	Area->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Area->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	Area->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
	Area->SetGenerateOverlapEvents(true);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Area);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
}

void AWaterPatch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWaterPatch, Size);
	DOREPLIFETIME(AWaterPatch, bMud);
	DOREPLIFETIME(AWaterPatch, bPermanent);
}

AWaterPatch* AWaterPatch::SpawnOrGrow(const UObject* WorldContext, const FVector& Location)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	int32 Count = 0;
	for (TActorIterator<AWaterPatch> It(World); It; ++It)
	{
		if (It->bPermanent)
		{
			continue;
		}
		++Count;
		if (FVector::Dist2D(It->GetActorLocation(), Location) < FMath::Max(It->Size.X, It->Size.Y) * 0.6f)
		{
			It->Size = FVector2D(FMath::Min(It->Size.X + 20.f, 400.f), FMath::Min(It->Size.Y + 20.f, 400.f));
			It->TimeLeft = T.WaterPatchLifetime;
			It->ApplySize();
			return *It;
		}
	}
	if (Count >= T.MaxWaterPatches)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWaterPatch* Patch = World->SpawnActor<AWaterPatch>(Location + FVector(0.f, 0.f, 2.f), FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Params);
	if (Patch)
	{
		Patch->TimeLeft = T.WaterPatchLifetime;
	}
	return Patch;
}

void AWaterPatch::InitPermanent(const FVector2D& SizeCm, bool bInMud)
{
	bPermanent = true;
	bMud = bInMud;
	Size = SizeCm;
	ApplySize();
}

void AWaterPatch::BeginPlay()
{
	Super::BeginPlay();
	Visual->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cylinder")));
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Visual, bMud ? HTMPalette::Dirt() : HTMPalette::Water());
	ApplySize();
	Area->OnComponentBeginOverlap.AddDynamic(this, &AWaterPatch::OnBeginOverlap);
	Area->OnComponentEndOverlap.AddDynamic(this, &AWaterPatch::OnEndOverlap);
}

void AWaterPatch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<AActor>& Weak : WetCharacters)
	{
		if (AMechanicCharacter* Char = Cast<AMechanicCharacter>(Weak.Get()))
		{
			Char->AddWetOverlap(-1);
		}
	}
	WetCharacters.Reset();
	Super::EndPlay(EndPlayReason);
}

void AWaterPatch::OnRep_Size()
{
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Visual, bMud ? HTMPalette::Dirt() : HTMPalette::Water());
	ApplySize();
}

void AWaterPatch::ApplySize()
{
	Area->SetBoxExtent(FVector(Size.X * 0.5f, Size.Y * 0.5f, 25.f));
	Visual->SetRelativeScale3D(FVector(Size.X / 100.f, Size.Y / 100.f, 0.015f));
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -23.f));
}

void AWaterPatch::OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AMechanicCharacter* Char = Cast<AMechanicCharacter>(OtherActor))
	{
		// El barro no hace resbalar; el agua sí.
		if (!bMud && !WetCharacters.Contains(Char))
		{
			WetCharacters.Add(Char);
			Char->AddWetOverlap(1);
		}
		return;
	}
	if (HasAuthority())
	{
		if (ISurfaceTreatable* Surface = Cast<ISurfaceTreatable>(OtherActor))
		{
			Surface->TreatSurface(bMud ? ESurfaceTreatment::Dirt : ESurfaceTreatment::Water, 1.f, FLinearColor::White, nullptr);
		}
	}
}

void AWaterPatch::OnEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AMechanicCharacter* Char = Cast<AMechanicCharacter>(OtherActor))
	{
		if (WetCharacters.Remove(Char) > 0)
		{
			Char->AddWetOverlap(-1);
		}
	}
}

void AWaterPatch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}

	// Coches y piezas que siguen dentro: mojar/ensuciar poco a poco (salpicaduras al pasar).
	WetTimer -= DeltaSeconds;
	if (WetTimer <= 0.f)
	{
		WetTimer = 0.5f;
		TArray<AActor*> Overlapping;
		Area->GetOverlappingActors(Overlapping);
		for (AActor* Actor : Overlapping)
		{
			if (AModularCar* Car = Cast<AModularCar>(Actor))
			{
				Car->SplashAllSurfaces(bMud ? ESurfaceTreatment::Dirt : ESurfaceTreatment::Water, 0.5f);
			}
			else if (ISurfaceTreatable* Surface = Cast<ISurfaceTreatable>(Actor))
			{
				Surface->TreatSurface(bMud ? ESurfaceTreatment::Dirt : ESurfaceTreatment::Water, 0.5f, FLinearColor::White, nullptr);
			}
		}
	}

	if (!bPermanent)
	{
		TimeLeft -= DeltaSeconds;
		if (TimeLeft <= 0.f)
		{
			Destroy();
		}
	}
}
