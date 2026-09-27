#include "Character/MechanicExpressionComponent.h"
#include "Character/MechanicCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

UMechanicExpressionComponent::UMechanicExpressionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
}

void UMechanicExpressionComponent::SetupPlaceholder(UStaticMeshComponent* InEyeL, UStaticMeshComponent* InEyeR,
	UStaticMeshComponent* InBrowL, UStaticMeshComponent* InBrowR, UStaticMeshComponent* InMouth)
{
	EyeL = InEyeL; EyeR = InEyeR; BrowL = InBrowL; BrowR = InBrowR; Mouth = InMouth;
	if (EyeL) { EyeLRest = EyeL->GetRelativeTransform(); }
	if (EyeR) { EyeRRest = EyeR->GetRelativeTransform(); }
	if (BrowL) { BrowLRest = BrowL->GetRelativeTransform(); }
	if (BrowR) { BrowRRest = BrowR->GetRelativeTransform(); }
	if (Mouth) { MouthRest = Mouth->GetRelativeTransform(); }
}

void UMechanicExpressionComponent::PlayOverride(EFaceExpression Expression, float Duration)
{
	OverrideExpression = Expression;
	OverrideTime = Duration;
}

EFaceExpression UMechanicExpressionComponent::ComputeExpression() const
{
	if (OverrideTime > 0.f)
	{
		return OverrideExpression;
	}
	const AMechanicCharacter* Char = Cast<AMechanicCharacter>(GetOwner());
	if (!Char)
	{
		return EFaceExpression::Happy;
	}
	switch (Char->GetMechanicState())
	{
	case EMechanicState::KnockedOut: return EFaceExpression::KO;
	case EMechanicState::Trapped:    return EFaceExpression::Scared;
	case EMechanicState::Stumbling:  return EFaceExpression::Surprised;
	default: break;
	}
	const UInteractionComponent* Interaction = Char->GetInteraction();
	if (Interaction && (Interaction->GetCarryMode() == ECarryMode::Heavy || Interaction->GetCarryMode() == ECarryMode::TwoHands || Interaction->IsHoldingInteraction()))
	{
		return EFaceExpression::Effort;
	}
	return EFaceExpression::Happy;
}

void UMechanicExpressionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	OverrideTime = FMath::Max(0.f, OverrideTime - DeltaTime);
	BlinkTimer -= DeltaTime;

	const EFaceExpression NewExpression = ComputeExpression();
	Apply(NewExpression);
	Current = NewExpression;
}

void UMechanicExpressionComponent::Apply(EFaceExpression Expression)
{
	// Texturas (arte final).
	if (FaceMID && ExpressionSet)
	{
		if (const FFaceExpressionTextures* Tex = ExpressionSet->Expressions.Find(Expression))
		{
			if (Tex->Eyes)  { FaceMID->SetTextureParameterValue(TEXT("EyesTex"), Tex->Eyes); }
			if (Tex->Brows) { FaceMID->SetTextureParameterValue(TEXT("BrowsTex"), Tex->Brows); }
			if (Tex->Mouth) { FaceMID->SetTextureParameterValue(TEXT("MouthTex"), Tex->Mouth); }
		}
	}

	if (!EyeL || !EyeR || !BrowL || !BrowR || !Mouth)
	{
		return;
	}

	// Placeholder: escala de ojos, ángulo/altura de cejas, forma de boca.
	float EyeScaleZ = 1.f, EyeScaleXY = 1.f, BrowAngle = 0.f, BrowLift = 0.f, MouthW = 1.f, MouthH = 1.f;
	switch (Expression)
	{
	case EFaceExpression::Happy:     BrowLift = 1.f; MouthW = 1.2f; break;
	case EFaceExpression::Effort:    EyeScaleZ = 0.45f; BrowAngle = 18.f; BrowLift = -1.5f; MouthW = 1.4f; MouthH = 0.5f; break;
	case EFaceExpression::Scared:    EyeScaleXY = 1.25f; EyeScaleZ = 1.3f; BrowAngle = -20.f; BrowLift = 3.f; MouthW = 0.6f; MouthH = 2.f; break;
	case EFaceExpression::KO:        EyeScaleZ = 0.12f; EyeScaleXY = 1.1f; BrowAngle = -10.f; MouthW = 0.8f; MouthH = 1.6f; break;
	case EFaceExpression::Angry:     EyeScaleZ = 0.7f; BrowAngle = 28.f; BrowLift = -2.f; MouthW = 1.1f; MouthH = 0.4f; break;
	case EFaceExpression::Surprised: EyeScaleXY = 1.3f; EyeScaleZ = 1.4f; BrowLift = 4.f; MouthW = 0.7f; MouthH = 2.4f; break;
	}
	if (BlinkTimer < 0.f)
	{
		EyeScaleZ *= 0.1f;
		if (BlinkTimer < -0.12f)
		{
			BlinkTimer = FMath::FRandRange(2.5f, 5.f);
		}
	}

	auto ScaleEye = [&](UStaticMeshComponent* Eye, const FTransform& Rest)
	{
		FTransform T = Rest;
		const FVector S = Rest.GetScale3D();
		T.SetScale3D(FVector(S.X, S.Y * EyeScaleXY, S.Z * EyeScaleZ));
		Eye->SetRelativeTransform(T);
	};
	ScaleEye(EyeL, EyeLRest);
	ScaleEye(EyeR, EyeRRest);

	auto PoseBrow = [&](UStaticMeshComponent* Brow, const FTransform& Rest, float Sign)
	{
		FTransform T = Rest;
		T.AddToTranslation(FVector(0.f, 0.f, BrowLift));
		T.SetRotation((FRotator(Rest.GetRotation()) + FRotator(0.f, 0.f, BrowAngle * Sign)).Quaternion());
		Brow->SetRelativeTransform(T);
	};
	PoseBrow(BrowL, BrowLRest, 1.f);
	PoseBrow(BrowR, BrowRRest, -1.f);

	FTransform M = MouthRest;
	const FVector MS = MouthRest.GetScale3D();
	M.SetScale3D(FVector(MS.X, MS.Y * MouthW, MS.Z * MouthH));
	Mouth->SetRelativeTransform(M);
}
