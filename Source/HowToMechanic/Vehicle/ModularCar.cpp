#include "Vehicle/ModularCar.h"
#include "Vehicle/ModularVehicleMovement.h"
#include "Vehicle/EngineTemperatureComponent.h"
#include "Vehicle/CarFireActor.h"
#include "Parts/CarPart.h"
#include "Parts/SurfaceLogic.h"
#include "Character/MechanicCharacter.h"
#include "Character/HTMInputConfig.h"
#include "Interaction/InteractionComponent.h"
#include "Tools/Tool.h"
#include "Core/HTMDataSubsystem.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Core/HTMGameState.h"
#include "Core/HTMPlayerState.h"
#include "Core/HTMTelemetrySubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HTMCar"

AModularCar::AModularCar()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	NetUpdateFrequency = 40.f;
	MinNetUpdateFrequency = 10.f;
	AutoPossessAI = EAutoPossessAI::Disabled;

	Chassis = CreateDefaultSubobject<UBoxComponent>(TEXT("Chassis"));
	SetRootComponent(Chassis);
	Chassis->SetBoxExtent(FVector(160.f, 80.f, 28.f));
	Chassis->SetCollisionProfileName(TEXT("Vehicle"));
	Chassis->SetCollisionResponseToChannel(ECC_Interaction, ECR_Block);
	Chassis->SetSimulatePhysics(true);
	Chassis->SetNotifyRigidBodyCollision(true);
	Chassis->SetLinearDamping(0.05f);
	Chassis->SetAngularDamping(0.6f);
	Chassis->BodyInstance.bUseCCD = true;

	CabinCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CabinCollision"));
	CabinCollision->SetupAttachment(Chassis);
	CabinCollision->SetCollisionProfileName(TEXT("Vehicle"));
	CabinCollision->SetCollisionResponseToChannel(ECC_Interaction, ECR_Block);

	BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
	BodyVisual->SetupAttachment(Chassis);
	BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CabinVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinVisual"));
	CabinVisual->SetupAttachment(Chassis);
	CabinVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DriverSeat = CreateDefaultSubobject<USceneComponent>(TEXT("DriverSeat"));
	DriverSeat->SetupAttachment(Chassis);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Chassis);
	CameraBoom->TargetArmLength = 700.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 180.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 6.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	PlateText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PlateText"));
	PlateText->SetupAttachment(Chassis);
	PlateText->SetHorizontalAlignment(EHTA_Center);
	PlateText->SetVerticalAlignment(EVRTA_TextCenter);
	PlateText->SetWorldSize(14.f);
	PlateText->SetTextRenderColor(FColor::Black);

	VehicleMovement = CreateDefaultSubobject<UModularVehicleMovement>(TEXT("VehicleMovement"));
	Temperature = CreateDefaultSubobject<UEngineTemperatureComponent>(TEXT("EngineTemperature"));
}

void AModularCar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AModularCar, ModelId);
	DOREPLIFETIME(AModularCar, Slots);
	DOREPLIFETIME(AModularCar, ChassisSurface);
	DOREPLIFETIME(AModularCar, bEngineRunning);
	DOREPLIFETIME(AModularCar, bHandbrake);
	DOREPLIFETIME(AModularCar, bFlipped);
	DOREPLIFETIME(AModularCar, Driver);
	DOREPLIFETIME(AModularCar, Fire);
	DOREPLIFETIME(AModularCar, CarName);
	DOREPLIFETIME(AModularCar, Plate);
	DOREPLIFETIME(AModularCar, Traits);
	DOREPLIFETIME(AModularCar, JobUid);
	DOREPLIFETIME(AModularCar, bOwnedByWorkshop);
	DOREPLIFETIME(AModularCar, BestTopSpeedKmh);
	DOREPLIFETIME(AModularCar, MaxAirTime);
}

void AModularCar::BeginPlay()
{
	Super::BeginPlay();
	BodyVisual->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cube")));
	CabinVisual->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cube")));
	BodyMID = UHTMVisualLibrary::ApplyPlaceholderMaterial(BodyVisual, ChassisSurface.BaseColor);
	CabinMID = UHTMVisualLibrary::ApplyPlaceholderMaterial(CabinVisual, ChassisSurface.BaseColor);
	BuildFromModel();
	OnRep_ChassisSurface();
	OnRep_Identity();
	if (HasAuthority())
	{
		Chassis->OnComponentHit.AddDynamic(this, &AModularCar::OnChassisHit);
	}
}

// ============================================================================ Modelo

const FCarModelRow* AModularCar::GetModel() const
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	return Data ? Data->FindCarModel(ModelId) : nullptr;
}

void AModularCar::InitModel(FName InModelId, const FLinearColor& BodyColor)
{
	check(HasAuthority());
	ModelId = InModelId;
	ChassisSurface = FPartSurfaceState();
	ChassisSurface.BaseColor = BodyColor;
	Slots.Reset();
	if (const FCarModelRow* Model = GetModel())
	{
		for (const FCarSlotDefinition& Def : Model->Slots)
		{
			FCarSlotState State;
			State.SlotName = Def.SlotName;
			Slots.Add(State);
		}
	}
	BuildFromModel();
	OnRep_ChassisSurface();
	RecalculateStats();
}

void AModularCar::SetIdentity(const FText& InName, const FString& InPlate, const TArray<ECarTrait>& InTraits)
{
	CarName = InName;
	Plate = InPlate;
	Traits = InTraits;
	OnRep_Identity();
	RecalculateStats();
}

void AModularCar::RestoreRecords(const FPartSurfaceState& InChassis, float InTopSpeedKmh, float InAirTime)
{
	ChassisSurface = InChassis;
	BestTopSpeedKmh = InTopSpeedKmh;
	MaxAirTime = InAirTime;
	OnRep_ChassisSurface();
}

void AModularCar::OnRep_Model()
{
	BuildFromModel();
	RecalculateStats();
}

void AModularCar::BuildFromModel()
{
	const FCarModelRow* Model = GetModel();
	if (!Model)
	{
		return;
	}
	const FVector E = Model->ChassisExtent;
	Chassis->SetBoxExtent(E);
	CabinCollision->SetBoxExtent(Model->CabinExtent);
	CabinCollision->SetRelativeLocation(Model->CabinOffset);

	if (!Model->BodyMesh.IsNull())
	{
		if (UStaticMesh* Final = Model->BodyMesh.LoadSynchronous())
		{
			BodyVisual->SetStaticMesh(Final);
			BodyVisual->SetRelativeScale3D(FVector::OneVector);
			CabinVisual->SetVisibility(false);
		}
	}
	else
	{
		// Rechoncho y "de juguete": caja del chasis + cabina alta (ART_DIRECTION §8).
		BodyVisual->SetRelativeScale3D(E * 2.f / 100.f);
		CabinVisual->SetRelativeLocation(Model->CabinOffset);
		CabinVisual->SetRelativeScale3D(Model->CabinExtent * 2.f / 100.f);
	}
	DriverSeat->SetRelativeLocation(Model->SeatOffset);
	PlateText->SetRelativeLocation(FVector(-E.X - 1.f, 0.f, 0.f));
	PlateText->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	CameraBoom->TargetArmLength = 450.f + E.X * 1.5f;
}

void AModularCar::OnRep_Identity()
{
	PlateText->SetText(FText::Format(LOCTEXT("PlateFmt", "{0}\n{1}"), CarName, FText::FromString(Plate)));
}

void AModularCar::OnRep_ChassisSurface()
{
	UHTMVisualLibrary::ApplySurface(BodyMID, ChassisSurface);
	UHTMVisualLibrary::ApplySurface(CabinMID, ChassisSurface);
}

void AModularCar::OnRep_Slots()
{
	RecalculateStats();
}

void AModularCar::OnRep_Engine()
{
	const FCarModelRow* Model = GetModel();
	if (bEngineRunning && Model)
	{
		UHTMVisualLibrary::SpawnFX(this, EHTMFX::Smoke, FVector(-Model->ChassisExtent.X - 20.f, 30.f, -10.f), 0.5f, Chassis, 1.f);
	}
}

// ============================================================================ Slots

int32 AModularCar::FindSlotIndex(FName SlotName) const
{
	return Slots.IndexOfByPredicate([SlotName](const FCarSlotState& S) { return S.SlotName == SlotName; });
}

const FCarSlotState* AModularCar::GetSlotState(FName SlotName) const
{
	const int32 Index = FindSlotIndex(SlotName);
	return Slots.IsValidIndex(Index) ? &Slots[Index] : nullptr;
}

const FCarSlotDefinition* AModularCar::GetSlotDefinition(FName SlotName) const
{
	const FCarModelRow* Model = GetModel();
	return Model ? Model->Slots.FindByPredicate([SlotName](const FCarSlotDefinition& D) { return D.SlotName == SlotName; }) : nullptr;
}

ACarPart* AModularCar::GetPartInSlot(FName SlotName) const
{
	const FCarSlotState* State = GetSlotState(SlotName);
	return State ? State->Part.Get() : nullptr;
}

TArray<ACarPart*> AModularCar::GetMountedParts() const
{
	TArray<ACarPart*> Result;
	for (const FCarSlotState& S : Slots)
	{
		if (S.Part)
		{
			Result.Add(S.Part);
		}
	}
	return Result;
}

bool AModularCar::IsSlotAccessible(FName SlotName) const
{
	const FCarSlotDefinition* Def = GetSlotDefinition(SlotName);
	return !Def || Def->BlockedBySlot.IsNone() || GetPartInSlot(Def->BlockedBySlot) == nullptr;
}

FVector AModularCar::GetSlotWorldLocation(FName SlotName) const
{
	const FCarSlotDefinition* Def = GetSlotDefinition(SlotName);
	return Def ? GetActorTransform().TransformPosition(Def->Location) : GetActorLocation();
}

bool AModularCar::FindSnapSlot(const ACarPart* Part, FName& OutSlot, bool& bOutReversed, float& OutDistance) const
{
	const FCarModelRow* Model = GetModel();
	if (!Model || !Part)
	{
		return false;
	}
	const EPartCategory Category = Part->GetCategory();
	const float SnapRadius = UHTMTuningData::Get().SnapRadius;
	float Best = SnapRadius;
	bool bFound = false;
	const FTransform CarT = GetActorTransform();

	for (const FCarSlotDefinition& Def : Model->Slots)
	{
		if (Def.Category != Category || GetPartInSlot(Def.SlotName) || !IsSlotAccessible(Def.SlotName))
		{
			continue;
		}
		const FVector SlotWorld = CarT.TransformPosition(Def.Location);
		const float Dist = FVector::Dist(SlotWorld, Part->GetActorLocation());
		if (Dist < Best)
		{
			Best = Dist;
			OutSlot = Def.SlotName;
			// Orientación libre: si la pieza llega boca abajo respecto al hueco, se monta al revés.
			const FVector SlotUp = CarT.TransformVectorNoScale(Def.Rotation.Quaternion().GetUpVector());
			const bool bSymmetric = Category == EPartCategory::Wheel || Category == EPartCategory::Suspension;
			bOutReversed = !bSymmetric && FVector::DotProduct(SlotUp, Part->GetActorUpVector()) < 0.f;
			bFound = true;
		}
	}
	OutDistance = Best;
	return bFound;
}

bool AModularCar::MountPart(ACarPart* Part, FName SlotName, bool bReversed, AMechanicCharacter* Who, bool bFullyTightened)
{
	if (!HasAuthority() || !Part || Part->IsMounted())
	{
		return false;
	}
	const int32 Index = FindSlotIndex(SlotName);
	const FCarSlotDefinition* Def = GetSlotDefinition(SlotName);
	if (!Slots.IsValidIndex(Index) || !Def || Slots[Index].Part || Def->Category != Part->GetCategory())
	{
		return false;
	}

	FTransform Rel(Def->Rotation, Def->Location);
	if (Def->Category == EPartCategory::Wheel)
	{
		// Ruedas más grandes/pequeñas que la de referencia: el centro baja/sube para apoyar en el suelo.
		const float Nominal = UHTMTuningData::Get().DefaultWheelRadius;
		Rel.AddToTranslation(FVector(0.f, 0.f, -(Part->GetStats().WheelRadius - Nominal)));
	}
	if (bReversed)
	{
		Rel.SetRotation(Rel.GetRotation() * FQuat(FVector::ForwardVector, UE_PI));
	}

	Part->OnMountedToCar(this, SlotName, Rel);
	FCarSlotState& State = Slots[Index];
	State.Part = Part;
	State.BoltsTotal = (uint8)Part->GetBoltCount();
	State.BoltsTight = bFullyTightened ? State.BoltsTotal : 0;
	State.bReversed = bReversed;
	RecalculateStats();

	if (Who)
	{
		MulticastSparks(Part->GetActorLocation(), 0.3f);
		UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this);
		if (Tel && bReversed)
		{
			Tel->RecordSituation(this, EHTMSituation::PartMountedReversed, Who->GetPlayerNameSafe(), Part->GetActorLocation());
		}
		// Dos jugadores colocando a la vez se empujan (GDD §13).
		TArray<FOverlapResult> Overlaps;
		GetWorld()->OverlapMultiByObjectType(Overlaps, Part->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(180.f));
		int32 Nearby = 0;
		for (const FOverlapResult& O : Overlaps) { Nearby += Cast<AMechanicCharacter>(O.GetActor()) ? 1 : 0; }
		if (Tel && Nearby >= 2)
		{
			Tel->RecordSituation(this, EHTMSituation::SimultaneousSnap, Who->GetPlayerNameSafe(), Part->GetActorLocation());
		}
	}
	return true;
}

void AModularCar::DetachPart(ACarPart* Part, bool bInheritVelocity, bool bCascade)
{
	if (!HasAuthority() || !Part)
	{
		return;
	}
	const int32 Index = Slots.IndexOfByPredicate([Part](const FCarSlotState& S) { return S.Part == Part; });
	if (!Slots.IsValidIndex(Index))
	{
		return;
	}
	const FName SlotName = Slots[Index].SlotName;
	Slots[Index].Part = nullptr;
	Slots[Index].BoltsTight = 0;
	Slots[Index].bReversed = false;

	const FVector Velocity = bInheritVelocity && Chassis->IsSimulatingPhysics() ? Chassis->GetPhysicsLinearVelocityAtPoint(Part->GetActorLocation()) : FVector::ZeroVector;
	Part->OnDetachedFromCar(Velocity);

	const float Speed = GetVelocity().Size();
	if (bInheritVelocity && Speed > 200.f)
	{
		const bool bWheel = Part->GetCategory() == EPartCategory::Wheel;
		const FString Who = Driver ? Driver->GetPlayerNameSafe() : FString();
		if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
		{
			Tel->Record(this, ETelemetryEvent::PartDetached, Who, FString::Printf(TEXT("%s (%s)"), *Part->GetDisplayName().ToString(), bWheel ? TEXT("Wheel") : TEXT("Part")), Part->GetActorLocation(), Speed);
			Tel->RecordSituation(this, bWheel ? EHTMSituation::WheelRollingAway : EHTMSituation::ReturnedWithoutPart, Who, Part->GetActorLocation());
		}
		if (bWheel && Driver)
		{
			if (AHTMPlayerState* PS = Driver->GetHTMPlayerState())
			{
				PS->AddAchievement(EHTMAchievement::WheelsLost);
			}
		}
		MulticastHorn(FText::Format(LOCTEXT("PartFell", "¡CLONK! Se ha soltado: {0}"), Part->GetDisplayName()));
	}

	// "Quitar X suelta Y" (GDD §13).
	if (bCascade)
	{
		bool bCascaded = false;
		if (const FCarModelRow* Model = GetModel())
		{
			for (const FCarSlotDefinition& Def : Model->Slots)
			{
				if (Def.ParentSlot == SlotName)
				{
					if (ACarPart* Child = GetPartInSlot(Def.SlotName))
					{
						DetachPart(Child, bInheritVelocity, true);
						bCascaded = true;
					}
				}
			}
		}
		if (bCascaded)
		{
			if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
			{
				Tel->RecordSituation(this, EHTMSituation::DependencyCascade, FString(), Part->GetActorLocation());
			}
		}
	}
	RecalculateStats();
}

void AModularCar::TightenBolt(FName SlotName, AMechanicCharacter* Who)
{
	const int32 Index = FindSlotIndex(SlotName);
	if (!HasAuthority() || !Slots.IsValidIndex(Index) || !Slots[Index].Part)
	{
		return;
	}
	FCarSlotState& State = Slots[Index];
	State.BoltsTight = FMath::Min<uint8>(State.BoltsTotal, State.BoltsTight + 1);
	if (State.BoltsTight == State.BoltsTotal)
	{
		MulticastSparks(State.Part->GetActorLocation(), 0.2f);
	}
}

void AModularCar::LoosenBolt(FName SlotName, AMechanicCharacter* Who)
{
	const int32 Index = FindSlotIndex(SlotName);
	if (!HasAuthority() || !Slots.IsValidIndex(Index) || !Slots[Index].Part)
	{
		return;
	}
	FCarSlotState& State = Slots[Index];
	if (State.BoltsTight > 0)
	{
		State.BoltsTight--;
	}
	if (State.BoltsTight == 0)
	{
		// Al quitar el último tornillo la pieza cae como objeto físico (GDD §7.2).
		DetachPart(State.Part, false, true);
	}
}

void AModularCar::SetBolts(FName SlotName, int32 Bolts)
{
	const int32 Index = FindSlotIndex(SlotName);
	if (HasAuthority() && Slots.IsValidIndex(Index))
	{
		Slots[Index].BoltsTight = (uint8)FMath::Clamp(Bolts, 0, (int32)Slots[Index].BoltsTotal);
	}
}

void AModularCar::OnPartBroken(ACarPart* Part)
{
	if (!Part)
	{
		return;
	}
	// Puertas y paragolpes rotos pueden quedar colgando y caer.
	const EPartCategory Cat = Part->GetCategory();
	if ((Cat == EPartCategory::Door || Cat == EPartCategory::Bumper || Cat == EPartCategory::Hood) && FMath::FRand() < 0.5f)
	{
		SetBolts(Part->GetSlotName(), 0);
	}
	if (Cat == EPartCategory::Wheel)
	{
		MulticastHorn(LOCTEXT("Flat", "¡PSSSHHH! Pinchazo"));
	}
}

// ============================================================================ Estadísticas (GDD §7.6)

void AModularCar::RecalculateStats()
{
	const FCarModelRow* Model = GetModel();
	Stats = FCarComputedStats();
	if (!Model)
	{
		return;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();

	const FVector ChassisCoM(0.f, 0.f, -Model->ChassisExtent.Z * 0.3f);
	float Mass = Model->ChassisMassKg;
	FVector Moment = ChassisCoM * Mass;
	float ExhaustMult = 0.9f;
	float HeatMult = 1.3f; // sin escape: más calor
	bool bSuspension[4] = { false, false, false, false };

	for (const FCarSlotState& S : Slots)
	{
		const ACarPart* Part = S.Part;
		const FCarSlotDefinition* Def = GetSlotDefinition(S.SlotName);
		if (!Part || !Def)
		{
			continue;
		}
		const float PartMass = Part->GetMassKg();
		Mass += PartMass;
		Moment += Def->Location * PartMass;

		const FPartStats& PS = Part->GetStats();
		const float Cond = Part->GetCondition() / 100.f;
		const EHiddenFault Fault = Part->GetHiddenFault();
		const int32 W = Def->WheelIndex;

		switch (Part->GetCategory())
		{
		case EPartCategory::Engine:
			if (!Part->IsBroken())
			{
				Stats.bEngineWorks = true;
				Stats.EngineTorqueNm = PS.TorqueNm * FMath::Lerp(0.4f, 1.f, Cond) * (Fault == EHiddenFault::EngineKnock ? 0.75f : 1.f);
				Stats.EngineDirection = S.bReversed ? -1.f : 1.f;
				Stats.MaxSpeedKmh = PS.MaxSpeedKmh;
				Stats.FuelUse = PS.FuelUse;
				Stats.HeatGeneration = PS.HeatGeneration * (Fault == EHiddenFault::WaterPumpFailing ? 2.f : Fault == EHiddenFault::EngineKnock ? 1.3f : 1.f);
				Stats.Cooling = PS.Cooling * FMath::Lerp(0.6f, 1.f, Cond);
			}
			break;
		case EPartCategory::Exhaust:
			ExhaustMult = PS.TorqueMult > 0.f ? PS.TorqueMult : 1.f;
			HeatMult = (S.bReversed ? 1.6f : 1.f) * (Fault == EHiddenFault::ExhaustLeak ? 1.2f : 1.f);
			break;
		case EPartCategory::Wheel:
			if (W >= 0 && W < 4)
			{
				Stats.bWheelPresent[W] = true;
				Stats.WheelRadius[W] = PS.WheelRadius * (Part->IsBroken() ? 0.75f : 1.f);
				Stats.Grip[W] = PS.Grip * (Part->IsBroken() ? 0.4f : 1.f) * (HasTrait(ECarTrait::WobblyWheels) ? 0.85f : 1.f);
			}
			break;
		case EPartCategory::Suspension:
			if (W >= 0 && W < 4)
			{
				bSuspension[W] = true;
				Stats.RestLength[W] = PS.RestLength;
				Stats.Stiffness[W] = PS.Stiffness * (Part->IsBroken() ? 0.3f : 1.f);
				Stats.Damping[W] = PS.Damping;
				Stats.BrakePower[W] = PS.BrakePower * (Fault == EHiddenFault::BrakesWorn ? 0.15f : 1.f) * (Part->IsBroken() ? 0.5f : 1.f);
			}
			break;
		case EPartCategory::Spoiler:
			// Alerón al revés: levanta el coche en vez de pegarlo (GDD §7.2).
			Stats.Downforce += PS.Downforce * (S.bReversed ? -1.5f : 1.f);
			Stats.Drag += PS.Drag;
			break;
		case EPartCategory::Horn:
			Stats.bHasHorn = true;
			if (const FPartDefinitionRow* Row = Part->GetDefinition())
			{
				Stats.HornSound = Row->SoundTag;
			}
			break;
		default:
			Stats.Drag += PS.Drag;
			break;
		}
	}

	Stats.EngineTorqueNm *= ExhaustMult;
	Stats.HeatGeneration *= HeatMult;
	Stats.TotalMassKg = Mass;
	Stats.CenterOfMassLocal = Moment / FMath::Max(1.f, Mass);

	for (const FCarSlotDefinition& Def : Model->Slots)
	{
		const int32 W = Def.WheelIndex;
		if (Def.Category != EPartCategory::Wheel || W < 0 || W >= 4)
		{
			continue;
		}
		if (!bSuspension[W])
		{
			// Sin suspensión: el buje va a tope. Duro, bajo y con frenos malos.
			Stats.RestLength[W] = 4.f;
			Stats.Stiffness[W] = 2.5f;
			Stats.Damping[W] = 0.5f;
			Stats.BrakePower[W] = 0.4f;
		}
		Stats.WheelAnchorLocal[W] = Def.Location + FVector(0.f, 0.f, Stats.RestLength[W] * 0.5f + (Stats.WheelRadius[W] - T.DefaultWheelRadius));
		Stats.bSteers[W] = W <= 1;
		Stats.bDriven[W] = Model->DriveType == 2 || (Model->DriveType == 0 ? W >= 2 : W <= 1);
	}

	Stats.SteerBias = HasTrait(ECarTrait::PullsLeft) ? -0.08f : HasTrait(ECarTrait::PullsRight) ? 0.08f : 0.f;
	// Ruedas desiguales: tira hacia el lado de la pequeña.
	Stats.SteerBias += ((Stats.WheelRadius[1] + Stats.WheelRadius[3]) - (Stats.WheelRadius[0] + Stats.WheelRadius[2])) * 0.004f;

	Chassis->SetMassOverrideInKg(NAME_None, Mass, true);
	Chassis->SetCenterOfMass(Stats.CenterOfMassLocal);
}

// ============================================================================ Consultas físicas

float AModularCar::GetForwardSpeed() const
{
	return FVector::DotProduct(GetVelocity(), GetActorForwardVector());
}

float AModularCar::GetVibration() const
{
	float V = GetVelocity().Size() / 3000.f;
	if (bEngineRunning)
	{
		V += 1.f;
		const ACarPart* Engine = nullptr;
		for (const FCarSlotState& S : Slots)
		{
			if (S.Part && S.Part->GetCategory() == EPartCategory::Engine) { Engine = S.Part; break; }
		}
		if (Engine && Engine->GetHiddenFault() == EHiddenFault::EngineKnock)
		{
			V += 0.5f;
		}
	}
	for (int32 i = 0; i < 4; ++i)
	{
		V += Stats.bWheelPresent[i] ? 0.f : 0.25f;
	}
	return V;
}

// ============================================================================ Tick

void AModularCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled() && Driver)
	{
		SendDriveInputIfChanged(DeltaSeconds);
	}

	if (!HasAuthority())
	{
		return;
	}

	LooseTimer += DeltaSeconds;
	if (LooseTimer >= 0.25f)
	{
		ServerTickLooseParts(LooseTimer);
		ServerTickRunningEngine(LooseTimer);
		LooseTimer = 0.f;
	}
	FaultTimer += DeltaSeconds;
	if (FaultTimer >= 0.5f)
	{
		ServerTickFaults(FaultTimer);
		// Superficie de la carrocería (secado).
		if (HTMSurface::TickDrying(ChassisSurface, FaultTimer))
		{
			OnRep_ChassisSurface();
		}
		FaultTimer = 0.f;
	}
	ServerTickFlip(DeltaSeconds);
	ServerTickTraits(DeltaSeconds);

	// Coche que se escapa solo (GDD §13: freno de mano quitado + cuesta / mal gato).
	RunawayCooldown -= DeltaSeconds;
	SparksCooldown -= DeltaSeconds;
	if (!Driver && GetVelocity().Size() > UHTMTuningData::Get().RunawaySpeed && RunawayCooldown <= 0.f)
	{
		RunawayCooldown = 15.f;
		if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
		{
			Tel->RecordSituation(this, EHTMSituation::CarRanAway, FString(), GetActorLocation());
		}
		MulticastHorn(LOCTEXT("Runaway", "¡EL COCHE SE ESCAPA!"));
	}
}

void AModularCar::ServerTickLooseParts(float DeltaSeconds)
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	const float Speed = GetVelocity().Size();
	if (Speed < 50.f && !bEngineRunning)
	{
		return;
	}
	const float Vibration = GetVibration();
	const float ChaosMult = T.GetDetachMult(this);

	for (const FCarSlotState& S : TArray<FCarSlotState>(Slots))
	{
		ACarPart* Part = S.Part;
		if (!Part || S.BoltsTotal == 0)
		{
			continue;
		}
		// Rodamiento gastado: con velocidad, se van aflojando tornillos solos.
		if (S.BoltsTight == S.BoltsTotal)
		{
			if (Part->GetHiddenFault() == EHiddenFault::WornBearing && Speed > 1200.f && FMath::FRand() < 0.03f * DeltaSeconds)
			{
				SetBolts(S.SlotName, S.BoltsTight - 1);
				Part->RevealFault(nullptr);
			}
			continue;
		}
		const float Looseness = 1.f - (float)S.BoltsTight / (float)S.BoltsTotal;
		float Chance = (T.LooseDetachChancePerSec + T.LooseDetachSpeedFactor * Speed / 1000.f + T.LooseDetachVibrationFactor * Vibration) * Looseness;
		Chance *= (S.BoltsTight == 0 ? T.ZeroBoltsDetachMult : 1.f) * ChaosMult;
		if (FMath::FRand() < Chance * DeltaSeconds)
		{
			DetachPart(Part, true, true);
		}
	}
}

void AModularCar::ServerTickFaults(float DeltaSeconds)
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	const float Speed = GetVelocity().Size();
	for (const FCarSlotState& S : Slots)
	{
		ACarPart* Part = S.Part;
		if (!Part || Part->GetHiddenFault() == EHiddenFault::None)
		{
			continue;
		}
		// Las averías ocultas aparecen en la prueba de conducción (GDD §7.4 y §9).
		switch (Part->GetHiddenFault())
		{
		case EHiddenFault::EngineKnock:
			if (bEngineRunning && FMath::Abs(InputThrottle) > 0.5f && !Part->IsFaultRevealed())
			{
				Part->RevealFault(nullptr);
				MulticastHorn(LOCTEXT("Knock", "¡CLONC CLONC CLONC! (motor)"));
			}
			break;
		case EHiddenFault::ExhaustLeak:
			if (bEngineRunning && Speed > 600.f && !Part->IsFaultRevealed())
			{
				Part->RevealFault(nullptr);
				MulticastHorn(LOCTEXT("ExhaustNoise", "¡BRRRAAAP! (escape)"));
			}
			break;
		case EHiddenFault::WornBearing:
			if (Speed > 1000.f && !Part->IsFaultRevealed())
			{
				Part->RevealFault(nullptr);
				MulticastHorn(LOCTEXT("Bearing", "¡ÑIIIIC! (rueda)"));
			}
			break;
		case EHiddenFault::BrakesWorn:
			if (InputBrake > 0.5f && Speed > 800.f && !Part->IsFaultRevealed())
			{
				Part->RevealFault(nullptr);
				MulticastHorn(LOCTEXT("Brakes", "¡Los frenos no frenan!"));
			}
			break;
		case EHiddenFault::SlowPuncture:
			if (Speed > 200.f)
			{
				Part->ApplyDamage(T.SlowPunctureRate * 100.f * DeltaSeconds * (Speed / 1000.f));
				if (Part->GetCondition() < 30.f)
				{
					Part->RevealFault(nullptr);
				}
			}
			break;
		case EHiddenFault::WaterPumpFailing:
			if (Temperature->GetTemperature() > T.WarnTemp)
			{
				Part->RevealFault(nullptr);
			}
			break;
		default:
			break;
		}
	}
}

void AModularCar::ServerTickFlip(float DeltaSeconds)
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	const float UpDot = FVector::DotProduct(GetActorUpVector(), FVector::UpVector);
	if (UpDot < T.FlippedUpDot)
	{
		FlippedTimer += DeltaSeconds;
		if (FlippedTimer > T.FlippedTime && !bFlipped)
		{
			bFlipped = true;
			if (!bFlipReported)
			{
				bFlipReported = true;
				const FString Who = Driver ? Driver->GetPlayerNameSafe() : FString();
				if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
				{
					Tel->Record(this, ETelemetryEvent::CarFlipped, Who, CarName.ToString(), GetActorLocation());
				}
				if (Driver)
				{
					if (AHTMPlayerState* PS = Driver->GetHTMPlayerState())
					{
						PS->AddAchievement(EHTMAchievement::CarsFlipped);
					}
				}
			}
			if (Driver)
			{
				EjectDriver(FVector(0.f, 0.f, 400.f));
			}
			SetEngineRunning(false, nullptr);
		}
	}
	else if (UpDot > 0.8f)
	{
		FlippedTimer = 0.f;
		bFlipped = false;
		bFlipReported = false;
	}
}

void AModularCar::ServerTickRunningEngine(float DeltaSeconds)
{
	if (!bEngineRunning)
	{
		return;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	const FTransform CarT = GetActorTransform();
	const bool bWheelsTurning = FMath::Abs(GetForwardSpeed()) > 40.f || FMath::Abs(InputThrottle) > 0.05f;

	TArray<FVector> Pushers;
	if (bWheelsTurning)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			if (Stats.bWheelPresent[i] && Stats.bDriven[i])
			{
				Pushers.Add(CarT.TransformPosition(Stats.WheelAnchorLocal[i]));
			}
		}
	}
	// Sin capó, el ventilador del motor empuja a quien meta la cabeza.
	if (const FCarSlotState* Hood = Slots.FindByPredicate([this](const FCarSlotState& S) { const FCarSlotDefinition* D = GetSlotDefinition(S.SlotName); return D && D->Category == EPartCategory::Hood; }))
	{
		if (!Hood->Part)
		{
			if (const FCarSlotState* Eng = Slots.FindByPredicate([this](const FCarSlotState& S) { return S.Part && S.Part->GetCategory() == EPartCategory::Engine; }))
			{
				Pushers.Add(Eng->Part->GetActorLocation());
			}
		}
	}

	for (const FVector& Point : Pushers)
	{
		TArray<FOverlapResult> Overlaps;
		GetWorld()->OverlapMultiByObjectType(Overlaps, Point, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(90.f));
		for (const FOverlapResult& O : Overlaps)
		{
			AMechanicCharacter* Char = Cast<AMechanicCharacter>(O.GetActor());
			if (Char && Char != Driver && Char->CanAct() && !Char->IsProne())
			{
				const FVector Away = (Char->GetActorLocation() - Point).GetSafeNormal2D();
				Char->LaunchCharacter(Away * T.RunningEngineShove + FVector(0.f, 0.f, 80.f), true, false);
			}
		}
	}
}

void AModularCar::ServerTickTraits(float DeltaSeconds)
{
	TraitTimer -= DeltaSeconds;
	if (TraitTimer > 0.f)
	{
		return;
	}
	TraitTimer = FMath::FRandRange(10.f, 22.f);
	if (HasTrait(ECarTrait::HornSticks))
	{
		MulticastHorn(LOCTEXT("HornStuck", "¡MOOOOC! (el claxon se ha atascado)"));
	}
	else if (HasTrait(ECarTrait::RadioStuckOn) && bEngineRunning)
	{
		MulticastHorn(LOCTEXT("Radio", "♪ ♫ la radio no se apaga ♫ ♪"));
	}
	else if (HasTrait(ECarTrait::SmellsWeird))
	{
		MulticastHorn(LOCTEXT("Smell", "¿Qué es ese olor?"));
	}
}

// ============================================================================ Estado

void AModularCar::SetEngineRunning(bool bRunning, AMechanicCharacter* Who)
{
	if (!HasAuthority() || bRunning == bEngineRunning)
	{
		return;
	}
	if (bRunning && !Stats.bEngineWorks)
	{
		MulticastHorn(LOCTEXT("NoStart", "*cof cof* ...no arranca"));
		return;
	}
	bEngineRunning = bRunning;
	OnRep_Engine();

	if (bRunning && Who)
	{
		LastStarter = Who;
		// ¿Alguien trabajando en el coche? (GDD §13)
		TArray<FOverlapResult> Overlaps;
		const FBox Box = GetComponentsBoundingBox(false);
		GetWorld()->OverlapMultiByObjectType(Overlaps, Box.GetCenter(), GetActorQuat(), FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeSphere(Box.GetExtent().Size() + 60.f));
		for (const FOverlapResult& O : Overlaps)
		{
			const AMechanicCharacter* Other = Cast<AMechanicCharacter>(O.GetActor());
			if (Other && Other != Who && (Other->IsProne() || Other->GetInteraction()->IsHoldingInteraction()))
			{
				if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
				{
					Tel->RecordSituation(this, EHTMSituation::EngineStartedWhileWorking, Who->GetPlayerNameSafe(), GetActorLocation());
				}
				break;
			}
		}
	}
}

void AModularCar::SetHandbrake(bool bOn)
{
	if (HasAuthority())
	{
		bHandbrake = bOn;
	}
}

void AModularCar::RecordTopSpeed(float Kmh)
{
	if (HasAuthority() && Kmh > BestTopSpeedKmh)
	{
		BestTopSpeedKmh = Kmh;
	}
}

void AModularCar::DamagePartsNear(const FVector& WorldLocation, float Radius, float Amount)
{
	if (!HasAuthority() || Amount <= 0.f)
	{
		return;
	}
	for (ACarPart* Part : GetMountedParts())
	{
		const float Dist = FVector::Dist(Part->GetActorLocation(), WorldLocation);
		if (Dist < Radius)
		{
			Part->ApplyDamage(Amount * (1.f - Dist / Radius));
		}
	}
	ChassisSurface.Damage = FMath::Min(1.f, ChassisSurface.Damage + Amount * 0.004f);
	OnRep_ChassisSurface();
}

int32 AModularCar::RevealAllFaults(AMechanicCharacter* Who)
{
	int32 Count = 0;
	for (ACarPart* Part : GetMountedParts())
	{
		if (Part->GetHiddenFault() != EHiddenFault::None)
		{
			Part->RevealFault(Who);
			++Count;
		}
	}
	return Count;
}

void AModularCar::StartFire()
{
	if (!HasAuthority() || Fire)
	{
		return;
	}
	FVector Where = GetActorLocation();
	for (const FCarSlotState& S : Slots)
	{
		if (S.Part && S.Part->GetCategory() == EPartCategory::Engine)
		{
			Where = S.Part->GetActorLocation() + FVector(0.f, 0.f, 30.f);
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Fire = GetWorld()->SpawnActor<ACarFireActor>(Where, FRotator::ZeroRotator, Params);
	if (Fire)
	{
		Fire->Init(this);
		Fire->AttachToComponent(Chassis, FAttachmentTransformRules::KeepWorldTransform);
		AMechanicCharacter* Culprit = Driver ? Driver.Get() : LastStarter.Get();
		if (Culprit)
		{
			if (AHTMPlayerState* PS = Culprit->GetHTMPlayerState())
			{
				PS->AddAchievement(EHTMAchievement::FiresStarted);
			}
		}
		if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
		{
			Tel->Record(this, ETelemetryEvent::EngineFire, Culprit ? Culprit->GetPlayerNameSafe() : FString(), CarName.ToString(), Where);
		}
		MulticastHorn(LOCTEXT("Fire", "¡FUEGO! ¡El extintor!"));
	}
}

void AModularCar::OnFireExtinguished()
{
	Fire = nullptr;
	Temperature->OnFireExtinguished();
}

void AModularCar::SplashAllSurfaces(ESurfaceTreatment Treatment, float Amount)
{
	TreatSurface(Treatment, Amount, FLinearColor::White, nullptr);
	for (ACarPart* Part : GetMountedParts())
	{
		Part->TreatSurface(Treatment, Amount, FLinearColor::White, nullptr);
	}
}

void AModularCar::RegisterPusher(AMechanicCharacter* Pusher)
{
	const float Now = GetWorld()->GetTimeSeconds();
	RecentPushers.Add(Pusher, Now);
	int32 Active = 0;
	for (auto It = RecentPushers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || Now - It.Value() > 1.f)
		{
			It.RemoveCurrent();
			continue;
		}
		++Active;
	}
	if (Active >= 2)
	{
		if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
		{
			Tel->RecordSituation(this, EHTMSituation::GroupPush, Pusher->GetPlayerNameSafe(), GetActorLocation());
		}
	}
}

void AModularCar::ResetTo(const FTransform& Where)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Driver)
	{
		ExitDriver();
	}
	SetEngineRunning(false, nullptr);
	bHandbrake = true;
	SetActorTransform(Where, false, nullptr, ETeleportType::ResetPhysics);
	Chassis->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Chassis->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	bFlipped = false;
	FlippedTimer = 0.f;
}

void AModularCar::EjectDriver(const FVector& Impulse)
{
	if (!HasAuthority() || !Driver)
	{
		return;
	}
	AMechanicCharacter* D = Driver;
	AController* C = GetController();
	Driver = nullptr;
	InputThrottle = InputBrake = InputSteer = 0.f;
	if (C)
	{
		C->Possess(D);
	}
	D->EjectFromCar(Impulse);
}

// ============================================================================ Conductor

void AModularCar::EnterAsDriver(AMechanicCharacter* Who)
{
	if (!HasAuthority() || Driver || !Who)
	{
		return;
	}
	AController* C = Who->GetController();
	Driver = Who;
	bHandbrake = false; // el conductor suelta el freno de mano... y puede olvidarse al bajar
	Who->EnterCar(this, DriverSeat);
	if (C)
	{
		C->Possess(this);
	}
}

void AModularCar::ExitDriver()
{
	if (!HasAuthority() || !Driver)
	{
		return;
	}
	AMechanicCharacter* D = Driver;
	AController* C = GetController();
	Driver = nullptr;
	InputThrottle = InputBrake = InputSteer = 0.f;
	bInputHandbrake = false;
	D->ExitCar(FindExitLocation());
	if (C)
	{
		C->Possess(D);
	}
	if (bEngineRunning && !bHandbrake)
	{
		MulticastHorn(LOCTEXT("NoHandbrake", "Motor en marcha y sin freno de mano..."));
	}
}

FVector AModularCar::FindExitLocation() const
{
	const FCarModelRow* Model = GetModel();
	const FVector Offsets[3] = {
		Model ? Model->ExitOffset : FVector(0.f, -200.f, 60.f),
		Model ? FVector(Model->ExitOffset.X, -Model->ExitOffset.Y, Model->ExitOffset.Z) : FVector(0.f, 200.f, 60.f),
		FVector(0.f, 0.f, 250.f)
	};
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMExit), false, this);
	for (const FVector& Offset : Offsets)
	{
		FVector Candidate = GetActorTransform().TransformPosition(Offset);
		Candidate.Z = FMath::Max(Candidate.Z, GetActorLocation().Z + 40.f);
		if (!GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.f, 60.f), Params))
		{
			return Candidate;
		}
	}
	return GetActorLocation() + FVector(0.f, 0.f, 250.f);
}

// ============================================================================ Input del conductor

void AModularCar::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(UHTMInputConfig::Get()->DrivingContext, 0);
		}
	}
}

void AModularCar::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	const UHTMInputConfig* Cfg = UHTMInputConfig::Get();
	Input->BindAction(Cfg->Throttle, ETriggerEvent::Triggered, this, &AModularCar::InputThrottleAxis);
	Input->BindAction(Cfg->Throttle, ETriggerEvent::Completed, this, &AModularCar::InputThrottleReleased);
	Input->BindAction(Cfg->Brake, ETriggerEvent::Triggered, this, &AModularCar::InputBrakeAxis);
	Input->BindAction(Cfg->Brake, ETriggerEvent::Completed, this, &AModularCar::InputBrakeReleased);
	Input->BindAction(Cfg->Steer, ETriggerEvent::Triggered, this, &AModularCar::InputSteerAxis);
	Input->BindAction(Cfg->Steer, ETriggerEvent::Completed, this, &AModularCar::InputSteerReleased);
	Input->BindAction(Cfg->Handbrake, ETriggerEvent::Started, this, &AModularCar::InputHandbrakeStart);
	Input->BindAction(Cfg->Handbrake, ETriggerEvent::Completed, this, &AModularCar::InputHandbrakeStop);
	Input->BindAction(Cfg->Horn, ETriggerEvent::Started, this, &AModularCar::InputHorn);
	Input->BindAction(Cfg->ExitCar, ETriggerEvent::Started, this, &AModularCar::InputExit);
	Input->BindAction(Cfg->CarLook, ETriggerEvent::Triggered, this, &AModularCar::InputLook);
}

void AModularCar::InputThrottleAxis(const FInputActionValue& Value) { InputThrottle = FMath::Clamp(Value.Get<float>(), 0.f, 1.f); }
void AModularCar::InputBrakeAxis(const FInputActionValue& Value)    { InputBrake = FMath::Clamp(Value.Get<float>(), 0.f, 1.f); }
void AModularCar::InputSteerAxis(const FInputActionValue& Value)    { InputSteer = FMath::Clamp(Value.Get<float>(), -1.f, 1.f); }
void AModularCar::InputThrottleReleased() { InputThrottle = 0.f; }
void AModularCar::InputBrakeReleased()    { InputBrake = 0.f; }
void AModularCar::InputSteerReleased()    { InputSteer = 0.f; }
void AModularCar::InputHandbrakeStart()   { bInputHandbrake = true; }
void AModularCar::InputHandbrakeStop()    { bInputHandbrake = false; }
void AModularCar::InputHorn()             { ServerHorn(); }
void AModularCar::InputExit()             { ServerExit(); }

void AModularCar::InputLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void AModularCar::SendDriveInputIfChanged(float DeltaSeconds)
{
	const int8 T = (int8)FMath::RoundToInt(InputThrottle * 100.f);
	const int8 B = (int8)FMath::RoundToInt(InputBrake * 100.f);
	const int8 S = (int8)FMath::RoundToInt(InputSteer * 100.f);
	InputSendTimer += DeltaSeconds;
	if (T != LastSentThrottle || B != LastSentBrake || S != LastSentSteer || bInputHandbrake != bLastSentHandbrake || InputSendTimer > 0.2f)
	{
		InputSendTimer = 0.f;
		LastSentThrottle = T; LastSentBrake = B; LastSentSteer = S; bLastSentHandbrake = bInputHandbrake;
		if (!HasAuthority())
		{
			ServerSetDriveInput(T, B, S, bInputHandbrake);
		}
	}
}

void AModularCar::ServerSetDriveInput_Implementation(int8 Throttle, int8 Brake, int8 Steer, bool bHandbrakeHeld)
{
	if (!Driver)
	{
		return;
	}
	InputThrottle = FMath::Clamp(Throttle / 100.f, 0.f, 1.f);
	InputBrake = FMath::Clamp(Brake / 100.f, 0.f, 1.f);
	InputSteer = FMath::Clamp(Steer / 100.f, -1.f, 1.f);
	bInputHandbrake = bHandbrakeHeld;
}

void AModularCar::ServerExit_Implementation()
{
	ExitDriver();
}

void AModularCar::ServerHorn_Implementation()
{
	if (!Stats.bHasHorn)
	{
		MulticastHorn(LOCTEXT("NoHorn", "*clic* (no hay bocina)"));
		return;
	}
	const FName Sound = Stats.HornSound;
	FText Text = LOCTEXT("HornDefault", "¡MEEEC!");
	if (Sound == TEXT("Ship")) { Text = LOCTEXT("HornShip", "¡BUUUUUUUUP! (barco)"); }
	else if (Sound == TEXT("Clown")) { Text = LOCTEXT("HornClown", "¡HONK HONK!"); }
	else if (Sound == TEXT("Cow")) { Text = LOCTEXT("HornCow", "¡MUUUUU!"); }
	MulticastHorn(Text);
}

void AModularCar::MulticastHorn_Implementation(const FText& Sound)
{
	if (AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>())
	{
		GS->OnToast.Broadcast(FText::Format(LOCTEXT("CarBubble", "{0}: {1}"), CarName, Sound), HTMPalette::SignalYellow());
	}
}

void AModularCar::MulticastSparks_Implementation(FVector_NetQuantize Location, float Scale)
{
	UHTMVisualLibrary::SpawnFX(this, EHTMFX::Sparks, Location, Scale, nullptr, 0.5f);
}

// ============================================================================ Golpes

void AModularCar::OnChassisHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !OtherActor || OtherActor == this)
	{
		return;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();

	// Atropellos: nadie muere, pero se vuela.
	if (AMechanicCharacter* Char = Cast<AMechanicCharacter>(OtherActor))
	{
		if (Char == Driver)
		{
			return;
		}
		const FVector MyVel = Chassis->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
		const float RelSpeed = (MyVel - Char->GetVelocity()).Size();
		if (RelSpeed > 250.f)
		{
			Char->ReceiveImpact(RelSpeed / 100.f * T.ImpactMassCapKg, MyVel.GetSafeNormal(), this);
		}
		return;
	}

	const float Mass = FMath::Max(1.f, Chassis->GetMass());
	const float DeltaV = NormalImpulse.Size() / Mass;

	// Suspensión muy baja rozando: chispas (GDD §7.6).
	if (Hit.ImpactNormal.Z > 0.6f && GetVelocity().Size() > 400.f && SparksCooldown <= 0.f)
	{
		SparksCooldown = 0.3f;
		MulticastSparks(Hit.ImpactPoint, 0.5f);
	}

	if (DeltaV < T.CarDamageSpeedThreshold)
	{
		return;
	}
	const float Damage = (DeltaV - T.CarDamageSpeedThreshold) * T.PartDamagePerSpeed;
	DamagePartsNear(Hit.ImpactPoint, T.DamageRadius, Damage);
	MulticastSparks(Hit.ImpactPoint, FMath::Clamp(DeltaV / 1500.f, 0.4f, 1.5f));

	// Las piezas flojas cerca del golpe tienen muchas papeletas de caer.
	for (const FCarSlotState& S : TArray<FCarSlotState>(Slots))
	{
		if (S.Part && S.BoltsTight < S.BoltsTotal && FVector::Dist(S.Part->GetActorLocation(), Hit.ImpactPoint) < T.DamageRadius * 1.5f
			&& FMath::FRand() < T.LooseImpactDetachChance * T.GetDetachMult(this))
		{
			DetachPart(S.Part, true, true);
		}
	}

	if (const AModularCar* OtherCar = Cast<AModularCar>(OtherActor))
	{
		if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
		{
			Tel->Record(this, ETelemetryEvent::CarCrash, Driver ? Driver->GetPlayerNameSafe() : FString(),
				FString::Printf(TEXT("%s vs %s"), *CarName.ToString(), *OtherCar->GetCarName().ToString()), Hit.ImpactPoint, DeltaV);
			Tel->RecordSituation(this, EHTMSituation::TeammateCarBroken, Driver ? Driver->GetPlayerNameSafe() : FString(), Hit.ImpactPoint);
		}
	}

	if (Driver && DeltaV > T.EjectDeltaV)
	{
		EjectDriver(GetVelocity().GetSafeNormal() * 600.f + FVector(0.f, 0.f, 400.f));
	}
}

// ============================================================================ Evaluación

bool AModularCar::HasFault(bool bOnlyNoisy) const
{
	for (const FCarSlotState& S : Slots)
	{
		if (S.Part && S.Part->GetHiddenFault() != EHiddenFault::None && (!bOnlyNoisy || HTM::IsNoisyFault(S.Part->GetHiddenFault())))
		{
			return true;
		}
	}
	return false;
}

bool AModularCar::HasPartId(FName PartId) const
{
	return Slots.ContainsByPredicate([PartId](const FCarSlotState& S) { return S.Part && S.Part->GetPartId() == PartId; });
}

bool AModularCar::HasPartWithTag(FName Tag) const
{
	return Slots.ContainsByPredicate([Tag](const FCarSlotState& S) { return S.Part && S.Part->HasTag(Tag); });
}

bool AModularCar::HasCategory(EPartCategory Category) const
{
	return Slots.ContainsByPredicate([Category](const FCarSlotState& S) { return S.Part && S.Part->GetCategory() == Category; });
}

bool AModularCar::HasVividPaint() const
{
	return ChassisSurface.HasVividPaint();
}

int32 AModularCar::CountDistinctPaintColors() const
{
	TArray<FLinearColor> Colors;
	auto AddColor = [&Colors](const FPartSurfaceState& S)
	{
		if (S.PaintAmount >= 0.8f && !S.bPaintRuined && !Colors.ContainsByPredicate([&S](const FLinearColor& C) { return C.Equals(S.PaintColor, 0.08f); }))
		{
			Colors.Add(S.PaintColor);
		}
	};
	AddColor(ChassisSurface);
	for (const FCarSlotState& S : Slots)
	{
		if (S.Part)
		{
			AddColor(S.Part->GetSurface());
		}
	}
	return Colors.Num();
}

float AModularCar::GetFuelUse() const
{
	// Consumo según motor y peso total (referencia 1000 kg).
	return Stats.FuelUse * (Stats.TotalMassKg / 1000.f);
}

float AModularCar::GetMinWheelRadius() const
{
	float Min = TNumericLimits<float>::Max();
	for (int32 i = 0; i < 4; ++i)
	{
		Min = FMath::Min(Min, Stats.bWheelPresent[i] ? Stats.WheelRadius[i] : 0.f);
	}
	return Min;
}

bool AModularCar::AllRequiredSlotsFilled() const
{
	const FCarModelRow* Model = GetModel();
	if (!Model)
	{
		return false;
	}
	for (const FCarSlotDefinition& Def : Model->Slots)
	{
		if (Def.bRequired && !GetPartInSlot(Def.SlotName))
		{
			return false;
		}
	}
	return true;
}

bool AModularCar::HasLooseParts() const
{
	return Slots.ContainsByPredicate([](const FCarSlotState& S) { return S.Part && S.BoltsTight < S.BoltsTotal; });
}

bool AModularCar::HasBrokenParts() const
{
	return Slots.ContainsByPredicate([](const FCarSlotState& S) { return S.Part && S.Part->IsBroken(); });
}

float AModularCar::GetAverageCondition() const
{
	float Sum = 0.f;
	int32 N = 0;
	for (const FCarSlotState& S : Slots)
	{
		if (S.Part)
		{
			Sum += S.Part->GetCondition();
			++N;
		}
	}
	return N > 0 ? Sum / N : 0.f;
}

int32 AModularCar::EstimateValue() const
{
	const FCarModelRow* Model = GetModel();
	if (!Model)
	{
		return 0;
	}
	int32 Required = 0, Filled = 0;
	for (const FCarSlotDefinition& Def : Model->Slots)
	{
		if (Def.bRequired)
		{
			++Required;
			Filled += GetPartInSlot(Def.SlotName) ? 1 : 0;
		}
	}
	const float Completeness = Required > 0 ? (float)Filled / Required : 1.f;
	const float Condition = GetAverageCondition() / 100.f;
	float Value = Model->BaseValue * (0.25f + 0.75f * Condition) * Completeness;
	Value *= HasLooseParts() ? 0.85f : 1.f;
	Value *= HasFault(false) ? 0.8f : 1.f;
	return FMath::RoundToInt(Value);
}

// ============================================================================ IInteractable

bool AModularCar::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	if (!Who || Who->GetCurrentCar() == this)
	{
		return false;
	}
	const UInteractionComponent* Interaction = Who->GetInteraction();
	switch (Verb)
	{
	case EInteractionVerb::Enter:
		return !Driver && !bFlipped && (Interaction->GetCarryMode() == ECarryMode::None || Interaction->GetCarryMode() == ECarryMode::OneHand);
	case EInteractionVerb::Use:
		// Con las manos vacías: arrancar/parar o enderezar. Con herramienta, la herramienta manda.
		return Interaction->GetHeldObject() == nullptr;
	case EInteractionVerb::AltUse:
		return Interaction->GetHeldObject() == nullptr && !bFlipped;
	default:
		return false;
	}
}

FText AModularCar::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	switch (Verb)
	{
	case EInteractionVerb::Enter:
		return HasTrait(ECarTrait::DriverDoorJammed) ? LOCTEXT("EnterJammed", "Subir (la puerta se atasca... forcejea)") : FText::Format(LOCTEXT("Enter", "Conducir {0}"), CarName);
	case EInteractionVerb::Use:
		return bFlipped ? LOCTEXT("Flip", "Empujar para darle la vuelta (mejor entre varios)")
			: bEngineRunning ? LOCTEXT("StopEngine", "Parar motor") : LOCTEXT("StartEngine", "Arrancar motor");
	case EInteractionVerb::AltUse:
		return bHandbrake ? LOCTEXT("HandbrakeOff", "Quitar freno de mano") : LOCTEXT("HandbrakeOn", "Poner freno de mano");
	default:
		return FText::GetEmpty();
	}
}

float AModularCar::GetHoldDuration(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	if (Verb == EInteractionVerb::Use && bFlipped)
	{
		return 0.3f;
	}
	if (Verb == EInteractionVerb::Enter && HasTrait(ECarTrait::DriverDoorJammed))
	{
		return 2.5f;
	}
	return 0.f;
}

bool AModularCar::IsHoldRepeatable(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return Verb == EInteractionVerb::Use && bFlipped;
}

void AModularCar::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	switch (Verb)
	{
	case EInteractionVerb::Enter:
		EnterAsDriver(Who);
		break;
	case EInteractionVerb::Use:
		if (bFlipped)
		{
			// Enderezar: cada empujón suma giro; varios jugadores lo hacen antes (GDD §9).
			const UHTMTuningData& T = UHTMTuningData::Get();
			FVector Axis = FVector::CrossProduct(GetActorUpVector(), FVector::UpVector);
			if (Axis.SizeSquared() < 0.01f)
			{
				Axis = GetActorForwardVector() * (FVector::DotProduct(Who->GetActorLocation() - GetActorLocation(), GetActorRightVector()) > 0.f ? -1.f : 1.f);
			}
			Chassis->AddAngularImpulseInRadians(Axis.GetSafeNormal() * T.FlipImpulsePerPush, NAME_None, true);
			Chassis->AddImpulse(FVector(0.f, 0.f, 120.f), NAME_None, true);
			RecentFlippers.Add(Who, GetWorld()->GetTimeSeconds());
			RegisterPusher(Who);
		}
		else
		{
			SetEngineRunning(!bEngineRunning, Who);
		}
		break;
	case EInteractionVerb::AltUse:
		SetHandbrake(!bHandbrake);
		break;
	default:
		break;
	}
}

void AModularCar::SetHighlighted(bool bHighlighted)
{
	UHTMVisualLibrary::SetHighlighted(BodyVisual, BodyMID, bHighlighted);
	UHTMVisualLibrary::SetHighlighted(CabinVisual, CabinMID, bHighlighted);
}

void AModularCar::TreatSurface(ESurfaceTreatment Treatment, float Amount, const FLinearColor& Color, AMechanicCharacter* By)
{
	if (!HasAuthority())
	{
		return;
	}
	if (HTMSurface::Apply(ChassisSurface, Treatment, Amount, Color, true))
	{
		if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
		{
			Tel->Record(this, ETelemetryEvent::PaintRuined, By ? By->GetPlayerNameSafe() : FString(), CarName.ToString(), GetActorLocation());
			Tel->RecordSituation(this, EHTMSituation::FreshPaintRuined, By ? By->GetPlayerNameSafe() : FString(), GetActorLocation());
		}
	}
	OnRep_ChassisSurface();
}

#undef LOCTEXT_NAMESPACE
