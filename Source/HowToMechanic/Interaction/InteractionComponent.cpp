#include "Interaction/InteractionComponent.h"
#include "Interaction/Interactable.h"
#include "Interaction/GrabbableActor.h"
#include "Character/MechanicCharacter.h"
#include "Tools/Tool.h"
#include "Parts/CarPart.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMGameState.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

#define LOCTEXT_NAMESPACE "HTMInteraction"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		const UHTMTuningData& T = UHTMTuningData::Get();
		Handle = NewObject<UPhysicsHandleComponent>(GetOwner(), TEXT("HeavyCarryHandle"));
		Handle->bSoftLinearConstraint = true;
		Handle->bSoftAngularConstraint = true;
		Handle->bInterpolateTarget = true;
		Handle->SetLinearStiffness(T.HeavyHandleStiffness);
		Handle->SetLinearDamping(T.HeavyHandleDamping);
		Handle->SetAngularStiffness(T.HeavyHandleStiffness * 0.2f);
		Handle->SetAngularDamping(T.HeavyHandleDamping);
		Handle->SetInterpolationSpeed(8.f);
		Handle->RegisterComponent();
	}
}

void UInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UInteractionComponent, HeldObject);
	DOREPLIFETIME(UInteractionComponent, CarryMode);
	DOREPLIFETIME(UInteractionComponent, HoldTarget);
	DOREPLIFETIME(UInteractionComponent, HoldVerb);
	DOREPLIFETIME_CONDITION(UInteractionComponent, HoldAlpha, COND_OwnerOnly);
	DOREPLIFETIME(UInteractionComponent, bToolActive);
}

AMechanicCharacter* UInteractionComponent::GetCharacter() const
{
	return Cast<AMechanicCharacter>(GetOwner());
}

ATool* UInteractionComponent::GetHeldTool() const
{
	return Cast<ATool>(HeldObject);
}

EToolType UInteractionComponent::GetHeldToolType() const
{
	const ATool* Tool = GetHeldTool();
	return Tool ? Tool->GetToolType() : EToolType::None;
}

IInteractable* UInteractionComponent::AsInteractable(AActor* Actor)
{
	return Actor ? Cast<IInteractable>(Actor) : nullptr;
}

const IInteractable* UInteractionComponent::AsInteractable(const AActor* Actor)
{
	return Actor ? Cast<const IInteractable>(Actor) : nullptr;
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AMechanicCharacter* Char = GetCharacter();
	if (!Char)
	{
		return;
	}
	if (Char->IsLocallyControlled())
	{
		UpdateFocus();
	}
	if (GetOwner()->HasAuthority())
	{
		TickServerHold(DeltaTime);
		TickHeavyCarry(DeltaTime);
	}
}

// ============================================================================ Foco (local)

void UInteractionComponent::UpdateFocus()
{
	AMechanicCharacter* Char = GetCharacter();
	APlayerController* PC = Char ? Cast<APlayerController>(Char->GetController()) : nullptr;
	AActor* NewFocus = nullptr;

	if (PC && PC->PlayerCameraManager && Char->CanAct())
	{
		const UHTMTuningData& T = UHTMTuningData::Get();
		const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
		const FVector CamDir = PC->PlayerCameraManager->GetCameraRotation().Vector();
		const float CamToChar = FVector::Dist(CamLoc, Char->GetActorLocation());
		const FVector End = CamLoc + CamDir * (CamToChar + T.MaxInteractDistance);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMInteractFocus), false, Char);
		if (HeldObject)
		{
			Params.AddIgnoredActor(HeldObject);
		}
		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByChannel(Hits, CamLoc, End, FQuat::Identity, ECC_Interaction,
			FCollisionShape::MakeSphere(T.InteractTraceRadius), Params);

		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || HitActor == Char)
			{
				continue;
			}
			// No aceptar cosas detrás del personaje (la cámara está detrás).
			if (FVector::DotProduct(Hit.ImpactPoint - Char->GetActorLocation(), CamDir) < -40.f)
			{
				continue;
			}
			if (!IsWithinReach(HitActor))
			{
				continue;
			}
			const IInteractable* Interactable = AsInteractable(HitActor);
			if (Interactable)
			{
				const bool bAny = Interactable->CanInteract(Char, EInteractionVerb::Grab)
					|| Interactable->CanInteract(Char, EInteractionVerb::Use)
					|| Interactable->CanInteract(Char, EInteractionVerb::AltUse)
					|| Interactable->CanInteract(Char, EInteractionVerb::Enter);
				if (bAny)
				{
					NewFocus = HitActor;
					break;
				}
			}
			if (Hit.bBlockingHit && !Hit.Component.IsValid())
			{
				break;
			}
		}
	}

	FocusedActor = NewFocus;
	if (HighlightedActor.Get() != NewFocus)
	{
		if (IInteractable* Old = AsInteractable(HighlightedActor.Get()))
		{
			Old->SetHighlighted(false);
		}
		if (IInteractable* New = AsInteractable(NewFocus))
		{
			New->SetHighlighted(true);
		}
		HighlightedActor = NewFocus;
	}
}

FText UInteractionComponent::GetPromptText(EInteractionVerb Verb) const
{
	const AMechanicCharacter* Char = GetCharacter();
	const IInteractable* Interactable = AsInteractable(FocusedActor.Get());
	if (Interactable && Char && Interactable->CanInteract(Char, Verb))
	{
		return Interactable->GetInteractionText(Char, Verb);
	}
	return FText::GetEmpty();
}

bool UInteractionComponent::IsWithinReach(const AActor* Target) const
{
	const AMechanicCharacter* Char = GetCharacter();
	if (!Char || !Target)
	{
		return false;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	const float Slack = GetOwner()->HasAuthority() && !Char->IsLocallyControlled() ? T.ServerDistanceSlack : 0.f;
	const FBox Box = Target->GetComponentsBoundingBox(true);
	if (!Box.IsValid)
	{
		return FVector::Dist(Target->GetActorLocation(), Char->GetActorLocation()) <= T.MaxInteractDistance + Slack;
	}
	const float DistSq = Box.ComputeSquaredDistanceToPoint(Char->GetActorLocation());
	return DistSq <= FMath::Square(T.MaxInteractDistance + Slack);
}

// ============================================================================ Input (local)

void UInteractionComponent::InputGrab()
{
	AMechanicCharacter* Char = GetCharacter();
	if (!Char || !Char->CanAct())
	{
		return;
	}
	if (HeldObject)
	{
		ServerDrop(false);
		return;
	}
	if (AActor* Target = FocusedActor.Get())
	{
		const IInteractable* Interactable = AsInteractable(Target);
		if (Interactable && Interactable->CanInteract(Char, EInteractionVerb::Grab))
		{
			ServerGrab(Target);
			return;
		}
	}
	// Sin objetivo: empujón a un compañero delante (si la sala lo permite; lo valida el servidor).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMShove), false, Char);
	FHitResult Hit;
	const FVector Start = Char->GetActorLocation();
	const FVector End = Start + Char->GetActorForwardVector() * 120.f;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(30.f), Params))
	{
		if (AMechanicCharacter* Other = Cast<AMechanicCharacter>(Hit.GetActor()))
		{
			ServerShove(Other);
		}
	}
}

void UInteractionComponent::InputThrow()
{
	const AMechanicCharacter* Char = GetCharacter();
	if (Char && Char->CanAct() && HeldObject && CarryMode != ECarryMode::Heavy)
	{
		ServerDrop(true);
	}
}

bool UInteractionComponent::TryStartVerb(EInteractionVerb Verb)
{
	AMechanicCharacter* Char = GetCharacter();
	AActor* Target = FocusedActor.Get();
	const IInteractable* Interactable = AsInteractable(Target);
	if (!Char || !Interactable || !Interactable->CanInteract(Char, Verb))
	{
		return false;
	}
	if (Interactable->GetHoldDuration(Char, Verb) > 0.f)
	{
		ServerBeginHold(Target, Verb);
	}
	else
	{
		ServerInstant(Target, Verb);
	}
	return true;
}

void UInteractionComponent::InputUse(bool bPressed)
{
	AMechanicCharacter* Char = GetCharacter();
	bUseHeldLocal = bPressed;
	if (!Char)
	{
		return;
	}
	if (!bPressed)
	{
		ServerEndHold();
		if (bToolActive)
		{
			ServerToolTrigger(false, false);
		}
		return;
	}
	if (!Char->CanAct())
	{
		return;
	}
	// Prioridad: interacción con el objetivo (atornillar, pulsar...). Si no, herramienta continua.
	if (TryStartVerb(EInteractionVerb::Use))
	{
		return;
	}
	if (const ATool* Tool = GetHeldTool())
	{
		if (Tool->IsContinuous())
		{
			ServerToolTrigger(true, false);
		}
	}
}

void UInteractionComponent::InputAltUse(bool bPressed)
{
	AMechanicCharacter* Char = GetCharacter();
	bAltHeldLocal = bPressed;
	if (!Char)
	{
		return;
	}
	if (!bPressed)
	{
		ServerEndHold();
		if (bToolActive)
		{
			ServerToolTrigger(false, true);
		}
		return;
	}
	if (!Char->CanAct())
	{
		return;
	}
	if (TryStartVerb(EInteractionVerb::AltUse))
	{
		return;
	}
	if (const ATool* Tool = GetHeldTool())
	{
		if (Tool->IsContinuous() || Tool->HandlesAltUseItself())
		{
			ServerToolTrigger(true, true);
		}
	}
}

void UInteractionComponent::InputEnter()
{
	const AMechanicCharacter* Char = GetCharacter();
	if (Char && Char->CanAct())
	{
		TryStartVerb(EInteractionVerb::Enter);
	}
}

// ============================================================================ RPC (servidor)

bool UInteractionComponent::ValidateTarget(AActor* Target, EInteractionVerb Verb) const
{
	const AMechanicCharacter* Char = GetCharacter();
	const IInteractable* Interactable = AsInteractable(Target);
	return Char && Char->CanAct() && Interactable && IsWithinReach(Target) && Interactable->CanInteract(Char, Verb);
}

void UInteractionComponent::ServerGrab_Implementation(AActor* Target)
{
	if (HeldObject || !ValidateTarget(Target, EInteractionVerb::Grab))
	{
		return;
	}
	AsInteractable(Target)->Interact(GetCharacter(), EInteractionVerb::Grab);
}

void UInteractionComponent::ServerDrop_Implementation(bool bThrow)
{
	AMechanicCharacter* Char = GetCharacter();
	if (!HeldObject || !Char)
	{
		return;
	}
	FVector Extra = FVector::ZeroVector;
	if (bThrow && CarryMode != ECarryMode::Heavy)
	{
		const UHTMTuningData& T = UHTMTuningData::Get();
		const float Speed = (CarryMode == ECarryMode::OneHand ? T.ThrowSpeedLight : T.ThrowSpeedMedium) * T.GetThrowMult(this);
		FVector Dir = Char->GetBaseAimRotation().Vector();
		Dir.Z = FMath::Max(Dir.Z, 0.f) + 0.25f;
		Extra = Dir.GetSafeNormal() * Speed;
	}
	ReleaseHeld(Extra, bThrow);
}

void UInteractionComponent::ServerInstant_Implementation(AActor* Target, EInteractionVerb Verb)
{
	if (!ValidateTarget(Target, Verb))
	{
		return;
	}
	IInteractable* Interactable = AsInteractable(Target);
	if (Interactable->GetHoldDuration(GetCharacter(), Verb) > 0.f)
	{
		return; // debe mantenerse, no se acepta como instantáneo
	}
	Interactable->Interact(GetCharacter(), Verb);
}

void UInteractionComponent::ServerBeginHold_Implementation(AActor* Target, EInteractionVerb Verb)
{
	if (!ValidateTarget(Target, Verb))
	{
		return;
	}
	HoldTarget = Target;
	HoldVerb = Verb;
	HoldElapsed = 0.f;
	HoldDuration = AsInteractable(Target)->GetHoldDuration(GetCharacter(), Verb);
	HoldAlpha = 0.f;
}

void UInteractionComponent::ServerEndHold_Implementation()
{
	CancelHold();
}

void UInteractionComponent::CancelHold()
{
	HoldTarget = nullptr;
	HoldElapsed = 0.f;
	HoldAlpha = 0.f;
}

void UInteractionComponent::TickServerHold(float DeltaTime)
{
	if (!HoldTarget)
	{
		return;
	}
	AMechanicCharacter* Char = GetCharacter();
	if (!ValidateTarget(HoldTarget, HoldVerb))
	{
		CancelHold();
		return;
	}
	IInteractable* Interactable = AsInteractable(HoldTarget);
	HoldElapsed += DeltaTime;
	HoldAlpha = HoldDuration > 0.f ? FMath::Clamp(HoldElapsed / HoldDuration, 0.f, 1.f) : 1.f;
	Interactable->InteractHoldTick(Char, HoldVerb, HoldAlpha, DeltaTime);

	if (HoldElapsed >= HoldDuration)
	{
		AActor* Target = HoldTarget;
		const EInteractionVerb Verb = HoldVerb;
		const bool bRepeat = Interactable->IsHoldRepeatable(Char, Verb);
		Interactable->Interact(Char, Verb);

		if (bRepeat && IsValid(Target) && ValidateTarget(Target, Verb))
		{
			HoldElapsed = 0.f;
			HoldAlpha = 0.f;
			HoldDuration = Interactable->GetHoldDuration(Char, Verb);
		}
		else
		{
			CancelHold();
		}
	}
}

void UInteractionComponent::ServerToolTrigger_Implementation(bool bActive, bool bAlt)
{
	ATool* Tool = GetHeldTool();
	AMechanicCharacter* Char = GetCharacter();
	if (!Tool || !Char)
	{
		bToolActive = false;
		return;
	}
	if (bActive && !Char->CanAct())
	{
		return;
	}
	if (bActive && bAlt && Tool->HandlesAltUseItself() && !Tool->IsContinuous())
	{
		Tool->AltUse(Char);
		return;
	}
	bToolActive = bActive;
	Tool->SetUsing(Char, bActive, bAlt);
}

void UInteractionComponent::ServerShove_Implementation(AMechanicCharacter* Target)
{
	AMechanicCharacter* Char = GetCharacter();
	if (!Char || !Target || Target == Char || !Char->CanAct() || HeldObject)
	{
		return;
	}
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	if (GS && !GS->RoomOptions.bFriendlyPush)
	{
		return;
	}
	if (FVector::Dist(Target->GetActorLocation(), Char->GetActorLocation()) > 200.f)
	{
		return;
	}
	const FVector Dir = (Target->GetActorLocation() - Char->GetActorLocation()).GetSafeNormal2D();
	Target->Stumble(Dir * UHTMTuningData::Get().ShoveImpulse);
	Char->PlayEmote(1);
}

// ============================================================================ Transporte (servidor)

bool UInteractionComponent::PickUp(AGrabbableActor* Object)
{
	AMechanicCharacter* Char = GetCharacter();
	if (!GetOwner()->HasAuthority() || !Char || !Object || HeldObject || !Object->CanBeGrabbedBy(Char))
	{
		return false;
	}

	const EHTMWeightClass Weight = Object->GetWeightClass();

	// Ligero/medio en manos de otro: se lo quitamos.
	if (Weight != EHTMWeightClass::Heavy && Object->IsCarried())
	{
		for (AMechanicCharacter* Other : TArray<TObjectPtr<AMechanicCharacter>>(Object->GetCarriers()))
		{
			if (Other && Other->GetInteraction())
			{
				Other->GetInteraction()->ReleaseHeld(FVector::ZeroVector, false);
			}
		}
	}

	HeldObject = Object;
	CarryMode = Weight == EHTMWeightClass::Heavy ? ECarryMode::Heavy : (Weight == EHTMWeightClass::Medium ? ECarryMode::TwoHands : ECarryMode::OneHand);
	Object->SetOwner(Char);
	Object->OnPickedUp(Char);

	if (CarryMode == ECarryMode::Heavy)
	{
		UPrimitiveComponent* Body = Object->GetPhysicsBody();
		FVector GripPoint = Body->GetComponentLocation();
		Body->GetClosestPointOnCollision(Char->GetActorLocation(), GripPoint);
		Handle->GrabComponentAtLocation(Body, NAME_None, GripPoint);
	}
	else
	{
		USceneComponent* CarryPoint = CarryMode == ECarryMode::OneHand ? Char->GetCarryPointOneHand() : Char->GetCarryPointTwoHands();
		Object->AttachToComponent(CarryPoint, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false));
		Object->SetActorRelativeRotation(Object->GetCarryRotationOffset());
	}

	CancelHold();
	Char->OnCarryChanged();
	return true;
}

void UInteractionComponent::ReleaseHeld(const FVector& ExtraVelocity, bool bThrown)
{
	AMechanicCharacter* Char = GetCharacter();
	AGrabbableActor* Object = HeldObject;
	if (!GetOwner()->HasAuthority() || !Object || !Char)
	{
		return;
	}

	if (bToolActive)
	{
		if (ATool* Tool = GetHeldTool())
		{
			Tool->SetUsing(Char, false, false);
		}
		bToolActive = false;
	}

	if (CarryMode == ECarryMode::Heavy)
	{
		Handle->ReleaseComponent();
	}
	else
	{
		Object->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		// Colocar delante para no dejarlo dentro del propio personaje.
		const FVector Ahead = Char->GetActorLocation() + Char->GetActorForwardVector() * 70.f + FVector(0.f, 0.f, 10.f);
		Object->SetActorLocation(Ahead, false, nullptr, ETeleportType::TeleportPhysics);
	}

	HeldObject = nullptr;
	CarryMode = ECarryMode::None;
	Object->SetOwner(nullptr);
	Object->OnDropped(Char, Char->GetVelocity() + ExtraVelocity, bThrown);

	if (!bThrown)
	{
		if (ACarPart* Part = Cast<ACarPart>(Object))
		{
			Part->TrySnapToNearbySlot(Char);
		}
	}

	Char->OnCarryChanged();
}

void UInteractionComponent::TickHeavyCarry(float DeltaTime)
{
	if (CarryMode != ECarryMode::Heavy || !HeldObject)
	{
		return;
	}
	AMechanicCharacter* Char = GetCharacter();
	const UHTMTuningData& T = UHTMTuningData::Get();

	// Un solo portador: "arrastrar y rezar" (objetivo bajo). Dos: se levanta.
	const bool bTeam = HeldObject->GetNumCarriers() >= 2;
	const float Height = bTeam ? T.HeavyTeamHoldHeight : T.HeavySoloHoldHeight;
	const FVector Target = Char->GetActorLocation() + Char->GetActorForwardVector() * T.HeavyHoldForward + FVector(0.f, 0.f, Height);
	Handle->SetTargetLocation(Target);

	const float Dist = FVector::Dist(HeldObject->GetActorLocation(), Char->GetActorLocation());
	const float Extent = HeldObject->GetComponentsBoundingBox().GetExtent().Size();
	if (Dist > T.HandleBreakDistance + Extent || !Char->CanAct())
	{
		ReleaseHeld(FVector::ZeroVector, false);
	}
}

void UInteractionComponent::OnRep_Held()
{
	if (AMechanicCharacter* Char = GetCharacter())
	{
		Char->OnCarryChanged();
	}
}

#undef LOCTEXT_NAMESPACE
