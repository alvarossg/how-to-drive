#include "Tools/LiftingDevices.h"
#include "Character/MechanicCharacter.h"
#include "Parts/CarPart.h"
#include "Vehicle/ModularCar.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Core/HTMGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#define LOCTEXT_NAMESPACE "HTMLifting"

namespace
{
	/** Posición relativa (en espacio local escalado) para un desplazamiento en cm desde la raíz escalada. */
	FVector ScaledOffset(const USceneComponent* ScaledRoot, const FVector& OffsetCm)
	{
		const FVector S = ScaledRoot->GetRelativeScale3D();
		return FVector(OffsetCm.X / FMath::Max(S.X, 0.01f), OffsetCm.Y / FMath::Max(S.Y, 0.01f), OffsetCm.Z / FMath::Max(S.Z, 0.01f));
	}

	constexpr float JackPadRest = 9.f;
	constexpr float JackPumpHold = 0.25f;
}

// ============================================================================ Gato

AJack::AJack()
{
	PrimaryActorTick.bCanEverTick = true;
	Spec.Shape = TEXT("Cube");
	Spec.SizeCm = FVector(55.f, 30.f, 12.f);
	Spec.Color = HTMPalette::ToolRed();
	Spec.MassKg = 14.f;
	Spec.DisplayName = LOCTEXT("Jack", "Gato");
	Spec.bForceWeightClass = true;
	Spec.WeightClass = EHTMWeightClass::Medium;

	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	Pad->SetupAttachment(Mesh);
	Pad->SetUsingAbsoluteScale(true);
	Pad->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Pad->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void AJack::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJack, bDeployed);
	DOREPLIFETIME(AJack, LiftHeight);
	DOREPLIFETIME(AJack, SupportedCar);
}

void AJack::BeginPlay()
{
	Super::BeginPlay();
	Pad->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cylinder")));
	Pad->SetWorldScale3D(FVector(0.16f, 0.16f, 0.08f));
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Pad, HTMPalette::Chrome());
	OnRep_Lift();
}

void AJack::OnRep_Lift()
{
	Pad->SetRelativeLocation(ScaledOffset(Mesh, FVector(0.f, 0.f, JackPadRest + LiftHeight)));
	RefreshPhysicsState();
}

bool AJack::ShouldSimulatePhysics() const
{
	return !bDeployed && Super::ShouldSimulatePhysics();
}

AModularCar* AJack::FindCarAbove(FVector& OutContact) const
{
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 5.f);
	const FVector End = Start + FVector(0.f, 0.f, 80.f);
	FCollisionObjectQueryParams Objects(ECC_Vehicle);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMJack), false, this);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(8.f), Params))
	{
		OutContact = Hit.ImpactPoint;
		return Cast<AModularCar>(Hit.GetActor());
	}
	return nullptr;
}

bool AJack::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	if (Verb == EInteractionVerb::Grab)
	{
		return Super::CanInteract(Who, Verb);
	}
	if (IsCarried() || !Who || !Who->GetInteraction()->HasHandFree())
	{
		return false;
	}
	if (Verb == EInteractionVerb::Use)
	{
		FVector Contact;
		return bDeployed ? LiftHeight < UHTMTuningData::Get().JackMaxHeight : FindCarAbove(Contact) != nullptr;
	}
	return Verb == EInteractionVerb::AltUse && bDeployed;
}

FText AJack::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	switch (Verb)
	{
	case EInteractionVerb::Use:    return LOCTEXT("Pump", "Bombear el gato (mantener)");
	case EInteractionVerb::AltUse: return LOCTEXT("Lower", "Bajar el gato (mantener)");
	default:                       return bDeployed ? LOCTEXT("Yank", "Quitar el gato (¡el coche caerá!)") : Super::GetInteractionText(Who, Verb);
	}
}

float AJack::GetHoldDuration(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return Verb == EInteractionVerb::Grab ? 0.f : JackPumpHold;
}

void AJack::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	if (Verb == EInteractionVerb::Grab)
	{
		Super::Interact(Who, Verb);
		return;
	}
	if (Verb == EInteractionVerb::Use)
	{
		if (!bDeployed)
		{
			FVector Contact;
			AModularCar* Car = FindCarAbove(Contact);
			if (!Car)
			{
				return;
			}
			bDeployed = true;
			SupportedCar = Car;
			ContactLocal = Car->GetActorTransform().InverseTransformPosition(Contact);
			DeployedAt = Contact;
			SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
		}
		LiftHeight = FMath::Min(T.JackMaxHeight, LiftHeight + T.JackLiftSpeed * JackPumpHold);
	}
	else if (Verb == EInteractionVerb::AltUse && bDeployed)
	{
		LiftHeight = FMath::Max(0.f, LiftHeight - T.JackLiftSpeed * JackPumpHold * 1.5f);
		if (LiftHeight <= 0.f)
		{
			bDeployed = false;
			SupportedCar = nullptr;
		}
	}
	OnRep_Lift();
}

void AJack::OnPickedUp(AMechanicCharacter* Who)
{
	if (bDeployed)
	{
		Tip(TEXT("Alguien ha quitado el gato"));
	}
	Super::OnPickedUp(Who);
}

void AJack::Tip(const FString& Reason)
{
	bDeployed = false;
	LiftHeight = 0.f;
	SupportedCar = nullptr;
	OnRep_Lift();
	if (Mesh->IsSimulatingPhysics())
	{
		Mesh->AddImpulse(FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.6f).GetSafeNormal() * 250.f, NAME_None, true);
		Mesh->AddAngularImpulseInDegrees(FMath::VRand() * 500.f, NAME_None, true);
	}
	if (AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>())
	{
		GS->MulticastToast(FText::Format(LOCTEXT("JackTipped", "¡El gato ha volcado! ({0})"), FText::FromString(Reason)), HTMPalette::SafetyOrange());
	}
}

void AJack::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bDeployed || !SupportedCar)
	{
		return;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	UPrimitiveComponent* Body = SupportedCar->GetChassis();
	if (!Body || !Body->IsSimulatingPhysics())
	{
		return;
	}
	const FVector Contact = SupportedCar->GetActorTransform().TransformPosition(ContactLocal);
	const FVector PointVel = Body->GetPhysicsLinearVelocityAtPoint(Contact);

	// Vuelco: el coche se ha desplazado o se mueve de lado (empujón, motor en marcha...).
	const float Drift = FVector::Dist2D(Contact, DeployedAt);
	if (Drift > T.JackTipOffset || PointVel.Size2D() > T.JackTipLateralSpeed || (SupportedCar->IsEngineRunning() && PointVel.Size2D() > 10.f))
	{
		Tip(Drift > T.JackTipOffset ? TEXT("el coche se ha movido") : TEXT("empujón"));
		return;
	}

	// Muelle vertical hacia la altura objetivo en el punto de contacto.
	const float TargetZ = GetActorLocation().Z + JackPadRest + 3.f + LiftHeight;
	const float Error = TargetZ - Contact.Z;
	if (Error > -10.f)
	{
		const float Accel = FMath::Clamp(Error * 60.f - PointVel.Z * 10.f, 0.f, 980.f * 1.4f);
		Body->AddForceAtLocation(FVector(0.f, 0.f, Body->GetMass() * Accel * 0.5f), Contact);
	}
}

// ============================================================================ Elevador hidráulico

AHydraulicLift::AHydraulicLift()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto MakeBox = [this](const TCHAR* Name, bool bCollision)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetMobility(EComponentMobility::Movable);
		if (bCollision)
		{
			C->SetCollisionProfileName(TEXT("BlockAllDynamic"));
			C->SetCollisionResponseToChannel(ECC_Interaction, ECR_Block);
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		return C;
	};
	ArmLeft = MakeBox(TEXT("ArmLeft"), true);
	ArmRight = MakeBox(TEXT("ArmRight"), true);
	PostLeft = MakeBox(TEXT("PostLeft"), true);
	PostRight = MakeBox(TEXT("PostRight"), true);
	Panel = MakeBox(TEXT("Panel"), true);

	PanelLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PanelLabel"));
	PanelLabel->SetupAttachment(Panel);
	PanelLabel->SetUsingAbsoluteScale(true);
	PanelLabel->SetHorizontalAlignment(EHTA_Center);
	PanelLabel->SetWorldSize(18.f);
	PanelLabel->SetText(LOCTEXT("LiftPanel", "▲ ELEVADOR ▼"));
}

void AHydraulicLift::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHydraulicLift, TargetHeight);
}

void AHydraulicLift::BeginPlay()
{
	Super::BeginPlay();
	auto Setup = [](UStaticMeshComponent* C, const FVector& Loc, const FVector& Size, const FLinearColor& Color)
	{
		C->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cube")));
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Size / 100.f);
		UHTMVisualLibrary::ApplyPlaceholderMaterial(C, Color);
	};
	// Elevadores azules (referencia 03_taller).
	Setup(ArmLeft, FVector(0.f, -55.f, 3.f), FVector(380.f, 30.f, 6.f), HTMPalette::ToolBlue());
	Setup(ArmRight, FVector(0.f, 55.f, 3.f), FVector(380.f, 30.f, 6.f), HTMPalette::ToolBlue());
	Setup(PostLeft, FVector(0.f, -170.f, 110.f), FVector(30.f, 30.f, 220.f), HTMPalette::ToolBlue());
	Setup(PostRight, FVector(0.f, 170.f, 110.f), FVector(30.f, 30.f, 220.f), HTMPalette::ToolBlue());
	Setup(Panel, FVector(120.f, 200.f, 110.f), FVector(20.f, 35.f, 45.f), HTMPalette::SignalYellow());
	PanelLabel->SetRelativeLocation(FVector(60.f, 0.f, 70.f));
	PanelLabel->SetWorldRotation(FRotator(0.f, 0.f, 0.f));
	PanelLabel->SetTextRenderColor(FColor::Black);
}

bool AHydraulicLift::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	const float Max = UHTMTuningData::Get().LiftMaxHeight;
	if (!Who || !Who->GetInteraction()->HasHandFree())
	{
		return false;
	}
	return (Verb == EInteractionVerb::Use && TargetHeight < Max) || (Verb == EInteractionVerb::AltUse && TargetHeight > 0.f);
}

FText AHydraulicLift::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return Verb == EInteractionVerb::Use ? LOCTEXT("LiftUp", "Subir elevador") : LOCTEXT("LiftDown", "Bajar elevador (¿hay alguien debajo?)");
}

void AHydraulicLift::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	const float Step = T.LiftSpeed * 0.1f;
	TargetHeight = FMath::Clamp(TargetHeight + (Verb == EInteractionVerb::Use ? Step : -Step), 0.f, T.LiftMaxHeight);
}

FVector AHydraulicLift::GetInteractionLocation() const
{
	return Panel->GetComponentLocation() + FVector(0.f, 0.f, 40.f);
}

void AHydraulicLift::OnRep_Height()
{
}

void AHydraulicLift::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Speed = UHTMTuningData::Get().LiftSpeed;
	if (!FMath::IsNearlyEqual(CurrentHeight, TargetHeight, 0.1f))
	{
		CurrentHeight = FMath::FInterpConstantTo(CurrentHeight, TargetHeight, DeltaSeconds, Speed);
		UpdateArms(CurrentHeight);
	}
}

void AHydraulicLift::UpdateArms(float Height)
{
	// Brazos cinemáticos: al moverse arrastran/levantan el coche físico que tengan encima.
	ArmLeft->SetRelativeLocation(FVector(0.f, -55.f, 3.f + Height));
	ArmRight->SetRelativeLocation(FVector(0.f, 55.f, 3.f + Height));
}

// ============================================================================ Grúa de motor

AEngineCrane::AEngineCrane()
{
	PrimaryActorTick.bCanEverTick = true;
	Spec.Shape = TEXT("Cube");
	Spec.SizeCm = FVector(170.f, 90.f, 18.f);
	Spec.Color = HTMPalette::SignalYellow();
	Spec.MassKg = 110.f;
	Spec.DisplayName = LOCTEXT("Crane", "Grúa de motor");

	Boom = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Boom"));
	Boom->SetupAttachment(Mesh);
	Boom->SetUsingAbsoluteScale(true);
	Boom->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Hook = CreateDefaultSubobject<USceneComponent>(TEXT("Hook"));
	Hook->SetupAttachment(Mesh);
	Hook->SetUsingAbsoluteScale(true);
}

void AEngineCrane::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEngineCrane, HookedPart);
	DOREPLIFETIME(AEngineCrane, HookHeight);
}

void AEngineCrane::BeginPlay()
{
	Super::BeginPlay();
	Boom->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cube")));
	Boom->SetWorldScale3D(FVector(1.4f, 0.12f, 0.12f));
	Boom->SetRelativeLocation(ScaledOffset(Mesh, FVector(40.f, 0.f, 150.f)));
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Boom, HTMPalette::SignalYellow());

	// Ruedas del carrito: poco rozamiento para poder moverlo en solitario.
	UPhysicalMaterial* Slippery = NewObject<UPhysicalMaterial>(this);
	Slippery->Friction = 0.05f;
	Slippery->FrictionCombineMode = EFrictionCombineMode::Min;
	Mesh->SetPhysMaterialOverride(Slippery);
}

ACarPart* AEngineCrane::FindHookablePart() const
{
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMCrane), false, this);
	GetWorld()->OverlapMultiByChannel(Overlaps, Hook->GetComponentLocation() - FVector(0.f, 0.f, 40.f), FQuat::Identity, ECC_Interaction,
		FCollisionShape::MakeSphere(110.f), Params);
	for (const FOverlapResult& O : Overlaps)
	{
		ACarPart* Part = Cast<ACarPart>(O.GetActor());
		if (Part && Part->GetWeightClass() == EHTMWeightClass::Heavy && !Part->IsCarried() && (!Part->IsMounted() || Part->GetBoltsTight() == 0))
		{
			return Part;
		}
	}
	return nullptr;
}

bool AEngineCrane::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	if (Verb == EInteractionVerb::Grab)
	{
		return Super::CanInteract(Who, Verb);
	}
	if (!Who || !Who->GetInteraction()->HasHandFree())
	{
		return false;
	}
	if (Verb == EInteractionVerb::Use)
	{
		return HookedPart ? HookHeight < 140.f : FindHookablePart() != nullptr;
	}
	return Verb == EInteractionVerb::AltUse && HookedPart != nullptr;
}

FText AEngineCrane::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	switch (Verb)
	{
	case EInteractionVerb::Use:    return HookedPart ? LOCTEXT("CraneUp", "Subir gancho") : LOCTEXT("CraneHook", "Enganchar pieza pesada");
	case EInteractionVerb::AltUse: return HookHeight <= 45.f ? LOCTEXT("CraneRelease", "Soltar pieza") : LOCTEXT("CraneDown", "Bajar gancho");
	default:                       return LOCTEXT("CraneDrag", "Mover la grúa");
	}
}

void AEngineCrane::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	if (Verb == EInteractionVerb::Grab)
	{
		Super::Interact(Who, Verb);
		return;
	}
	if (Verb == EInteractionVerb::Use)
	{
		if (!HookedPart)
		{
			ACarPart* Part = FindHookablePart();
			if (!Part)
			{
				return;
			}
			if (Part->IsMounted())
			{
				Part->DetachFromCar(false);
			}
			HookedPart = Part;
			Part->AttachToComponent(Hook, FAttachmentTransformRules::KeepWorldTransform);
			Part->RefreshPhysicsState();
		}
		HookHeight = FMath::Min(140.f, HookHeight + 25.f);
	}
	else if (Verb == EInteractionVerb::AltUse && HookedPart)
	{
		if (HookHeight > 45.f)
		{
			HookHeight = FMath::Max(40.f, HookHeight - 25.f);
		}
		else
		{
			ACarPart* Part = HookedPart;
			HookedPart = nullptr;
			Part->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Part->RefreshPhysicsState();
			Part->TrySnapToNearbySlot(Who);
		}
	}
}

void AEngineCrane::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Hook->SetRelativeLocation(ScaledOffset(Mesh, FVector(105.f, 0.f, HookHeight)));
}

#undef LOCTEXT_NAMESPACE
