#include "Character/MechanicAnimInstance.h"
#include "Character/MechanicCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void UMechanicAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CachedCharacter = Cast<AMechanicCharacter>(TryGetPawnOwner());
}

void UMechanicAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	AMechanicCharacter* Character = CachedCharacter.Get();
	if (!Character)
	{
		CachedCharacter = Cast<AMechanicCharacter>(TryGetPawnOwner());
		Character = CachedCharacter.Get();
		if (!Character)
		{
			return;
		}
	}

	const FVector Velocity = Character->GetVelocity();
	GroundSpeed = Velocity.Size2D();
	MoveDirection = GroundSpeed > 5.f ? FRotator::NormalizeAxis(Velocity.Rotation().Yaw - Character->GetActorRotation().Yaw) : 0.f;
	bIsFalling = Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsFalling();
	bIsSprinting = Character->IsSprinting();
	Stance = Character->GetStance();

	MechanicState = Character->GetMechanicState();
	bIsDriving = MechanicState == EMechanicState::Driving;
	bIsKnockedOut = MechanicState == EMechanicState::KnockedOut;
	bIsTrapped = MechanicState == EMechanicState::Trapped;
	bIsWet = Character->IsWet();

	const UInteractionComponent* Interaction = Character->GetInteraction();
	CarryMode = Interaction ? Interaction->GetCarryMode() : ECarryMode::None;
	bIsWorking = Interaction && (Interaction->IsToolActive() || Interaction->GetHoldTarget() != nullptr);
	const float ArmsTarget = (CarryMode == ECarryMode::TwoHands || CarryMode == ECarryMode::Heavy) ? 1.f : 0.f;
	CarryArmsAlpha = FMath::FInterpTo(CarryArmsAlpha, ArmsTarget, DeltaSeconds, 8.f);

	float WobbleTarget = 1.f - Character->GetBalance();
	if (MechanicState == EMechanicState::Stumbling)
	{
		WobbleTarget += 0.6f;
	}
	Wobble = FMath::FInterpTo(Wobble, FMath::Clamp(WobbleTarget, 0.f, 1.f), DeltaSeconds, 6.f);
	const FVector Impact = Character->GetLastImpactDirection();
	const float Side = FVector::DotProduct(Impact.GetSafeNormal2D(), Character->GetActorRightVector());
	LeanSide = FMath::FInterpTo(LeanSide, MechanicState == EMechanicState::Stumbling ? Side : 0.f, DeltaSeconds, 5.f);
}
