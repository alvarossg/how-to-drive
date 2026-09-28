#include "Parts/CarPart.h"
#include "Parts/SurfaceLogic.h"
#include "Vehicle/ModularCar.h"
#include "Character/MechanicCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Tools/Tool.h"
#include "Core/HTMDataSubsystem.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPlayerController.h"
#include "Core/HTMTelemetrySubsystem.h"
#include "Core/HTMPalette.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HTMPart"

namespace
{
	/** Qué haría un verbo sobre una pieza montada. */
	enum class EPartVerbMode : uint8 { None, Tighten, Loosen, WrongTool, Blocked, PullOff, ForwardToCar };

	const FPartDefinitionRow& EmptyRow()
	{
		static const FPartDefinitionRow Row;
		return Row;
	}
}

static EPartVerbMode ResolveVerb(const ACarPart* Part, const AMechanicCharacter* Who, EInteractionVerb Verb, bool bCorrectTool, bool bAccessible)
{
	if (!Part->IsMounted() || !Who)
	{
		return EPartVerbMode::None;
	}
	const UInteractionComponent* Interaction = Who->GetInteraction();
	const bool bHandFree = Interaction->HasHandFree();
	const EToolType Held = Interaction->GetHeldToolType();
	const bool bHoldingBoltTool = HTM::IsBoltTool(Held);

	switch (Verb)
	{
	case EInteractionVerb::Grab:
		return (Part->GetBoltsTight() == 0 && Interaction->GetHeldObject() == nullptr) ? (bAccessible ? EPartVerbMode::PullOff : EPartVerbMode::Blocked) : EPartVerbMode::None;

	case EInteractionVerb::Use:
		if (!bHandFree) { return EPartVerbMode::None; }
		if (bHoldingBoltTool && Part->GetBoltsTight() < Part->GetBoltCount())
		{
			if (!bAccessible) { return EPartVerbMode::Blocked; }
			return bCorrectTool ? EPartVerbMode::Tighten : EPartVerbMode::WrongTool;
		}
		return EPartVerbMode::ForwardToCar;

	case EInteractionVerb::AltUse:
		if (!bHandFree) { return EPartVerbMode::None; }
		if (bHoldingBoltTool && Part->GetBoltsTight() > 0)
		{
			if (!bAccessible) { return EPartVerbMode::Blocked; }
			return bCorrectTool ? EPartVerbMode::Loosen : EPartVerbMode::WrongTool;
		}
		return Held == EToolType::None ? EPartVerbMode::ForwardToCar : EPartVerbMode::None;

	case EInteractionVerb::Enter:
		return EPartVerbMode::ForwardToCar;
	}
	return EPartVerbMode::None;
}

ACarPart::ACarPart()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
}

void ACarPart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACarPart, PartId);
	DOREPLIFETIME(ACarPart, Condition);
	DOREPLIFETIME(ACarPart, Surface);
	DOREPLIFETIME(ACarPart, HiddenFault);
	DOREPLIFETIME(ACarPart, bFaultRevealed);
	DOREPLIFETIME(ACarPart, OwningCar);
	DOREPLIFETIME(ACarPart, SlotName);
}

void ACarPart::BeginPlay()
{
	Super::BeginPlay();
}

void ACarPart::InitPart(FName InPartId, float InCondition, const FPartSurfaceState& InSurface, EHiddenFault InFault)
{
	PartId = InPartId;
	Condition = FMath::Clamp(InCondition, 0.f, 100.f);
	Surface = InSurface;
	HiddenFault = InFault;

	if (const FPartDefinitionRow* Row = GetDefinition())
	{
		FGrabbablePropSpec NewSpec;
		NewSpec.Shape = Row->PlaceholderShape;
		NewSpec.SizeCm = Row->PlaceholderSize;
		NewSpec.Color = Surface.BaseColor;
		NewSpec.MassKg = Row->MassKg;
		NewSpec.DisplayName = Row->DisplayName;
		Spec = NewSpec;
	}
	Surface.Damage = 1.f - Condition / 100.f;
	ApplyVisuals();
}

const FPartDefinitionRow* ACarPart::GetDefinition() const
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	return Data ? Data->FindPart(PartId) : nullptr;
}

EPartCategory ACarPart::GetCategory() const
{
	const FPartDefinitionRow* Row = GetDefinition();
	return Row ? Row->Category : EPartCategory::Accessory;
}

const FPartStats& ACarPart::GetStats() const
{
	const FPartDefinitionRow* Row = GetDefinition();
	return Row ? Row->Stats : EmptyRow().Stats;
}

bool ACarPart::HasTag(FName Tag) const
{
	const FPartDefinitionRow* Row = GetDefinition();
	return Row && Row->Tags.Contains(Tag);
}

int32 ACarPart::GetPrice() const
{
	const FPartDefinitionRow* Row = GetDefinition();
	return Row ? Row->Price : 0;
}

EToolType ACarPart::GetRequiredTool() const
{
	const FPartDefinitionRow* Row = GetDefinition();
	return Row ? Row->RequiredTool : EToolType::Wrench;
}

FText ACarPart::GetDisplayName() const
{
	const FPartDefinitionRow* Row = GetDefinition();
	return Row ? Row->DisplayName : Super::GetDisplayName();
}

EHTMWeightClass ACarPart::GetWeightClass() const
{
	return Super::GetWeightClass();
}

EPartState ACarPart::GetState() const
{
	if (Condition <= 0.f) { return EPartState::Broken; }
	if (Surface.Rust > 0.5f) { return EPartState::Rusty; }
	if (Surface.Dirt > 0.5f) { return EPartState::Dirty; }
	if (Condition < 50.f) { return EPartState::Damaged; }
	return EPartState::Good;
}

bool ACarPart::IsPaintable() const
{
	const FPartDefinitionRow* Row = GetDefinition();
	return Row && Row->bPaintable;
}

// ============================================================================ Visual

void ACarPart::OnRep_PartId()
{
	ApplyVisuals();
}

void ACarPart::OnRep_Surface()
{
	UHTMVisualLibrary::ApplySurface(MID, Surface);
}

void ACarPart::ApplyVisuals()
{
	const FPartDefinitionRow* Row = GetDefinition();
	if (Row && !Row->Mesh.IsNull())
	{
		// Arte final: SM_ con su propio pivote y colisión UCX_.
		if (UStaticMesh* Final = Row->Mesh.LoadSynchronous())
		{
			Mesh->SetStaticMesh(Final);
			Mesh->SetWorldScale3D(FVector::OneVector);
			Mesh->SetMassOverrideInKg(NAME_None, Row->MassKg, true);
			MID = UHTMVisualLibrary::ApplyPlaceholderMaterial(Mesh, Surface.BaseColor);
			UHTMVisualLibrary::ApplySurface(MID, Surface);
			return;
		}
	}
	Super::ApplyVisuals();
	UHTMVisualLibrary::ApplySurface(MID, Surface);
}

// ============================================================================ Montaje

int32 ACarPart::GetBoltsTight() const
{
	if (const FCarSlotState* State = OwningCar ? OwningCar->GetSlotState(SlotName) : nullptr)
	{
		return State->BoltsTight;
	}
	return 0;
}

int32 ACarPart::GetBoltCount() const
{
	const FPartDefinitionRow* Row = GetDefinition();
	return Row ? FMath::Max(1, Row->BoltCount) : 1;
}

bool ACarPart::IsReversed() const
{
	const FCarSlotState* State = OwningCar ? OwningCar->GetSlotState(SlotName) : nullptr;
	return State && State->bReversed;
}

bool ACarPart::ShouldSimulatePhysics() const
{
	return !IsMounted() && Super::ShouldSimulatePhysics();
}

void ACarPart::OnMountedToCar(AModularCar* Car, FName InSlotName, const FTransform& RelativeTransform)
{
	check(HasAuthority());
	OwningCar = Car;
	SlotName = InSlotName;
	MountedRelative = RelativeTransform;

	const FVector WorldScale = Mesh->GetComponentScale();
	RefreshPhysicsState();
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Interaction, ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	AttachToComponent(Car->GetChassis(), FAttachmentTransformRules::KeepWorldTransform);
	FTransform Rel = RelativeTransform;
	Rel.SetScale3D(WorldScale);
	SetActorRelativeTransform(Rel);
	OnRep_Mount();
}

void ACarPart::OnDetachedFromCar(const FVector& InheritVelocity)
{
	check(HasAuthority());
	OwningCar = nullptr;
	SlotName = NAME_None;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetCollisionResponseToChannel(ECC_Interaction, ECR_Block);
	RefreshPhysicsState();
	if (Mesh->IsSimulatingPhysics())
	{
		Mesh->SetPhysicsLinearVelocity(InheritVelocity);
		if (GetCategory() == EPartCategory::Wheel && !InheritVelocity.IsNearlyZero())
		{
			// La rueda sale rodando (GDD §13): giro sobre su eje (Z local).
			const float Radius = FMath::Max(10.f, GetStats().WheelRadius);
			Mesh->SetPhysicsAngularVelocityInRadians(GetActorUpVector() * (InheritVelocity.Size() / Radius));
		}
	}
	OnRep_Mount();
}

void ACarPart::OnRep_Mount()
{
	if (IsMounted())
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Interaction, ECR_Block);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	else
	{
		Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Mesh->SetCollisionResponseToChannel(ECC_Interaction, ECR_Block);
		if (Cast<AModularCar>(GetAttachParentActor()))
		{
			DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
	RefreshPhysicsState();
}

bool ACarPart::TrySnapToNearbySlot(AMechanicCharacter* Who)
{
	if (!HasAuthority() || IsMounted())
	{
		return false;
	}
	const float SnapRadius = UHTMTuningData::Get().SnapRadius;
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects(ECC_Vehicle);
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, Objects, FCollisionShape::MakeSphere(450.f));

	AModularCar* BestCar = nullptr;
	FName BestSlot;
	bool bBestReversed = false;
	float BestDist = SnapRadius;
	for (const FOverlapResult& O : Overlaps)
	{
		AModularCar* Car = Cast<AModularCar>(O.GetActor());
		if (!Car)
		{
			continue;
		}
		FName Slot;
		bool bReversed = false;
		float Dist = 0.f;
		if (Car->FindSnapSlot(this, Slot, bReversed, Dist) && Dist < BestDist)
		{
			BestCar = Car;
			BestSlot = Slot;
			bBestReversed = bReversed;
			BestDist = Dist;
		}
	}
	if (!BestCar)
	{
		return false;
	}
	// Si lo sujetaba alguien más (pesado entre dos), se suelta al encajar.
	for (AMechanicCharacter* Carrier : TArray<TObjectPtr<AMechanicCharacter>>(Carriers))
	{
		if (Carrier && Carrier->GetInteraction())
		{
			Carrier->GetInteraction()->ReleaseHeld(FVector::ZeroVector, false);
		}
	}
	return BestCar->MountPart(this, BestSlot, bBestReversed, Who, false);
}

void ACarPart::DetachFromCar(bool bInheritVelocity)
{
	if (HasAuthority() && OwningCar)
	{
		OwningCar->DetachPart(this, bInheritVelocity, true);
	}
}

// ============================================================================ Estado

void ACarPart::ApplyDamage(float Amount)
{
	if (!HasAuthority() || Amount <= 0.f)
	{
		return;
	}
	SetCondition(Condition - Amount);
}

void ACarPart::SetCondition(float NewCondition)
{
	const bool bWasBroken = IsBroken();
	Condition = FMath::Clamp(NewCondition, 0.f, 100.f);
	Surface.Damage = 1.f - Condition / 100.f;
	OnRep_Surface();
	if (!bWasBroken && IsBroken() && OwningCar)
	{
		OwningCar->OnPartBroken(this);
	}
	if (OwningCar)
	{
		OwningCar->RecalculateStats();
	}
}

void ACarPart::RevealFault(AMechanicCharacter* By)
{
	if (HasAuthority() && HiddenFault != EHiddenFault::None && !bFaultRevealed)
	{
		bFaultRevealed = true;
	}
}

void ACarPart::OnServerImpact(float SpeedCmS, AActor* OtherActor, const FHitResult& Hit)
{
	// Las piezas sueltas se estropean si caen o se estampan fuerte.
	if (!IsMounted() && SpeedCmS > 900.f)
	{
		ApplyDamage((SpeedCmS - 900.f) * 0.01f);
	}
}

// ============================================================================ Superficie (GDD §8)

void ACarPart::TreatSurface(ESurfaceTreatment Treatment, float Amount, const FLinearColor& Color, AMechanicCharacter* By)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Treatment == ESurfaceTreatment::Paint)
	{
		bPaintRuinReported = false;
	}
	const bool bRuined = HTMSurface::Apply(Surface, Treatment, Amount, Color, IsPaintable());
	if (bRuined && !bPaintRuinReported)
	{
		bPaintRuinReported = true;
		if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
		{
			Tel->Record(this, ETelemetryEvent::PaintRuined, By ? By->GetPlayerNameSafe() : FString(), GetDisplayName().ToString(), GetActorLocation());
			Tel->RecordSituation(this, EHTMSituation::FreshPaintRuined, By ? By->GetPlayerNameSafe() : FString(), GetActorLocation());
		}
	}
	OnRep_Surface();
}

void ACarPart::ServerTickSurface(float DeltaSeconds)
{
	if (HTMSurface::TickDrying(Surface, DeltaSeconds))
	{
		OnRep_Surface();
	}
}

void ACarPart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		SurfaceTimer += DeltaSeconds;
		if (SurfaceTimer >= 0.5f)
		{
			ServerTickSurface(SurfaceTimer);
			SurfaceTimer = 0.f;
		}
	}

	// Aviso legible de pieza floja: polvillo al traquetear con el coche en marcha (GDD §14).
	if (IsLoose() && OwningCar && (OwningCar->IsEngineRunning() || OwningCar->GetVelocity().Size() > 150.f))
	{
		RattleTime += DeltaSeconds;
		if (RattleTime > 1.2f)
		{
			RattleTime = 0.f;
			UHTMVisualLibrary::SpawnFX(this, EHTMFX::DustPoof, GetActorLocation(), 0.25f, nullptr, 0.35f);
		}
	}
}

// ============================================================================ IInteractable

bool ACarPart::HasCorrectTool(const AMechanicCharacter* Who) const
{
	return Who && Who->GetInteraction()->GetHeldToolType() == GetRequiredTool();
}

bool ACarPart::IsSlotAccessible() const
{
	return OwningCar && OwningCar->IsSlotAccessible(SlotName);
}

bool ACarPart::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	if (!IsMounted())
	{
		return Super::CanInteract(Who, Verb);
	}
	const EPartVerbMode Mode = ResolveVerb(this, Who, Verb, HasCorrectTool(Who), IsSlotAccessible());
	switch (Mode)
	{
	case EPartVerbMode::None:         return false;
	case EPartVerbMode::ForwardToCar: return OwningCar->CanInteract(Who, Verb);
	default:                          return true;
	}
}

FText ACarPart::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	if (!IsMounted())
	{
		return Super::GetInteractionText(Who, Verb);
	}
	const FText Name = GetDisplayName();
	switch (ResolveVerb(this, Who, Verb, HasCorrectTool(Who), IsSlotAccessible()))
	{
	case EPartVerbMode::Tighten:
		return FText::Format(LOCTEXT("Tighten", "Apretar {0} ({1}/{2})"), Name, GetBoltsTight(), GetBoltCount());
	case EPartVerbMode::Loosen:
		return FText::Format(LOCTEXT("Loosen", "Aflojar {0} ({1}/{2})"), Name, GetBoltsTight(), GetBoltCount());
	case EPartVerbMode::WrongTool:
		return FText::Format(LOCTEXT("WrongTool", "{0}: necesitas {1}"), Name, HTM::ToolName(GetRequiredTool()));
	case EPartVerbMode::Blocked:
	{
		const FCarSlotDefinition* Def = OwningCar->GetSlotDefinition(SlotName);
		return FText::Format(LOCTEXT("Blocked", "Quita antes: {0}"), FText::FromName(Def ? Def->BlockedBySlot : NAME_None));
	}
	case EPartVerbMode::PullOff:
		return FText::Format(LOCTEXT("PullOff", "Arrancar {0} (sin tornillos)"), Name);
	case EPartVerbMode::ForwardToCar:
		return OwningCar->GetInteractionText(Who, Verb);
	default:
		return FText::GetEmpty();
	}
}

float ACarPart::GetHoldDuration(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	if (!IsMounted())
	{
		return 0.f;
	}
	switch (ResolveVerb(this, Who, Verb, HasCorrectTool(Who), IsSlotAccessible()))
	{
	case EPartVerbMode::Tighten:
	case EPartVerbMode::Loosen:
		return UHTMTuningData::Get().BoltTime * (GetWeightClass() == EHTMWeightClass::Heavy ? 1.5f : 1.f);
	case EPartVerbMode::ForwardToCar:
		return OwningCar->GetHoldDuration(Who, Verb);
	default:
		return 0.f;
	}
}

bool ACarPart::IsHoldRepeatable(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	if (!IsMounted())
	{
		return false;
	}
	switch (ResolveVerb(this, Who, Verb, HasCorrectTool(Who), IsSlotAccessible()))
	{
	case EPartVerbMode::Tighten:
	case EPartVerbMode::Loosen:
		return true;
	case EPartVerbMode::ForwardToCar:
		return OwningCar->IsHoldRepeatable(Who, Verb);
	default:
		return false;
	}
}

void ACarPart::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	if (!IsMounted())
	{
		Super::Interact(Who, Verb);
		return;
	}
	AModularCar* Car = OwningCar;
	switch (ResolveVerb(this, Who, Verb, HasCorrectTool(Who), IsSlotAccessible()))
	{
	case EPartVerbMode::Tighten:
		Car->TightenBolt(SlotName, Who);
		break;
	case EPartVerbMode::Loosen:
		Car->LoosenBolt(SlotName, Who);
		break;
	case EPartVerbMode::PullOff:
		Car->DetachPart(this, false, true);
		Who->GetInteraction()->PickUp(this);
		break;
	case EPartVerbMode::WrongTool:
	case EPartVerbMode::Blocked:
		if (AHTMPlayerController* PC = Cast<AHTMPlayerController>(Who->GetController()))
		{
			PC->ClientToast(GetInteractionText(Who, Verb), HTMPalette::SafetyOrange());
		}
		break;
	case EPartVerbMode::ForwardToCar:
		Car->Interact(Who, Verb);
		break;
	default:
		break;
	}
}

#undef LOCTEXT_NAMESPACE
