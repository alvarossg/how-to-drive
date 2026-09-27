#include "Character/ActiveRagdollComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "GameFramework/Actor.h"

UActiveRagdollComponent::UActiveRagdollComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UActiveRagdollComponent::Setup(USkeletalMeshComponent* InMesh, USceneComponent* InPlaceholderRoot)
{
	Mesh = InMesh;
	PlaceholderRoot = InPlaceholderRoot;
	if (PlaceholderRoot)
	{
		PlaceholderRestTransform = PlaceholderRoot->GetRelativeTransform();
	}

	bSkeletalPhysics = Mesh && Mesh->GetSkeletalMeshAsset() && Mesh->GetPhysicsAsset();
	if (!bSkeletalPhysics)
	{
		return;
	}

	MeshRestRelativeTransform = Mesh->GetRelativeTransform();
	PhysicalAnimation = NewObject<UPhysicalAnimationComponent>(GetOwner(), TEXT("PhysicalAnimation"));
	PhysicalAnimation->RegisterComponent();
	PhysicalAnimation->SetSkeletalMeshComponent(Mesh);

	FPhysicalAnimationData Data;
	Data.bIsLocalSimulation = true;
	Data.OrientationStrength = OrientationStrength;
	Data.AngularVelocityStrength = AngularVelocityStrength;
	Data.PositionStrength = 0.f;
	Data.VelocityStrength = 0.f;
	PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(UpperBodyBone, Data, true);

	Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetAllBodiesBelowSimulatePhysics(UpperBodyBone, true, true);
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(UpperBodyBone, BaseBlendWeight, false, true);
}

void UActiveRagdollComponent::Stagger(const FVector& WorldImpulse, float Duration)
{
	StaggerDir = WorldImpulse.GetSafeNormal2D();
	StaggerDuration = FMath::Max(0.1f, Duration);
	StaggerTime = StaggerDuration;

	if (bSkeletalPhysics && PhysicalAnimation && !bRagdoll)
	{
		PhysicalAnimation->SetStrengthMultiplyer(0.15f);
		Mesh->SetAllBodiesBelowPhysicsBlendWeight(UpperBodyBone, 0.9f, false, true);
		Mesh->AddImpulse(WorldImpulse * 0.5f, UpperBodyBone, true);
	}
	else
	{
		// Placeholder: patada al muelle de inclinación.
		const FVector Local = GetOwner()->GetActorTransform().InverseTransformVectorNoScale(StaggerDir);
		LeanVelocity += FVector2D(Local.X, Local.Y) * 260.f;
	}
}

void UActiveRagdollComponent::StartRagdoll(const FVector& WorldImpulse)
{
	bRagdoll = true;
	RecoverAlpha = 0.f;
	if (bSkeletalPhysics)
	{
		PhysicalAnimation->SetStrengthMultiplyer(0.f);
		Mesh->SetAllBodiesSimulatePhysics(true);
		Mesh->SetAllBodiesPhysicsBlendWeight(1.f);
		Mesh->AddImpulse(WorldImpulse, PelvisBone, true);
	}
	else
	{
		StaggerDir = WorldImpulse.GetSafeNormal2D();
		if (StaggerDir.IsNearlyZero())
		{
			StaggerDir = GetOwner()->GetActorForwardVector();
		}
	}
}

void UActiveRagdollComponent::StopRagdoll()
{
	if (!bRagdoll)
	{
		return;
	}
	bRagdoll = false;
	RecoverAlpha = 0.f;
	if (bSkeletalPhysics)
	{
		Mesh->SetAllBodiesSimulatePhysics(false);
		Mesh->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		Mesh->SetRelativeTransform(MeshRestRelativeTransform);
		Mesh->SetAllBodiesBelowSimulatePhysics(UpperBodyBone, true, true);
		Mesh->SetAllBodiesBelowPhysicsBlendWeight(UpperBodyBone, BaseBlendWeight, false, true);
		PhysicalAnimation->SetStrengthMultiplyer(1.f);
	}
}

void UActiveRagdollComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Time += DeltaTime;
	StaggerTime = FMath::Max(0.f, StaggerTime - DeltaTime);
	RecoverAlpha = FMath::Min(1.f, RecoverAlpha + DeltaTime * 2.f);

	if (bSkeletalPhysics)
	{
		TickSkeletal(DeltaTime);
	}
	else
	{
		TickPlaceholder(DeltaTime);
	}
}

void UActiveRagdollComponent::TickSkeletal(float DeltaTime)
{
	if (bRagdoll || !PhysicalAnimation)
	{
		return;
	}
	// Recuperar fuerza tras un tambaleo; la inestabilidad (carga/mojado) deja el tronco más blando.
	const float StaggerAlpha = StaggerDuration > 0.f ? StaggerTime / StaggerDuration : 0.f;
	const float Strength = FMath::Lerp(1.f, 0.35f, Instability) * (1.f - 0.85f * StaggerAlpha);
	PhysicalAnimation->SetStrengthMultiplyer(Strength);
	const float Blend = FMath::Clamp(BaseBlendWeight + 0.4f * Instability + 0.5f * StaggerAlpha, 0.f, 1.f);
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(UpperBodyBone, Blend, false, true);
}

void UActiveRagdollComponent::TickPlaceholder(float DeltaTime)
{
	if (!PlaceholderRoot)
	{
		return;
	}
	const AActor* Owner = GetOwner();
	const FVector Velocity = Owner->GetVelocity();
	const FVector Accel = DeltaTime > 0.f ? (Velocity - LastVelocity) / DeltaTime : FVector::ZeroVector;
	LastVelocity = Velocity;

	// Inclinación hacia la aceleración + bamboleo por inestabilidad.
	const FVector LocalAccel = Owner->GetActorTransform().InverseTransformVectorNoScale(Accel);
	FVector2D Target(LocalAccel.X, LocalAccel.Y);
	Target *= 0.012f;
	Target += FVector2D(FMath::Sin(Time * 2.3f), FMath::Sin(Time * 1.7f)) * (6.f * Instability);
	Target.X = FMath::Clamp(Target.X, -25.f, 25.f);
	Target.Y = FMath::Clamp(Target.Y, -25.f, 25.f);

	const float Stiffness = FMath::Lerp(90.f, 35.f, Instability);
	const float Damping = 9.f;
	const FVector2D Force = (Target - Lean) * Stiffness - LeanVelocity * Damping;
	LeanVelocity += Force * DeltaTime;
	Lean += LeanVelocity * DeltaTime;

	FallAlpha = FMath::FInterpTo(FallAlpha, bRagdoll ? 1.f : 0.f, DeltaTime, bRagdoll ? 9.f : 4.f);
	LieAlpha = FMath::FInterpTo(LieAlpha, bLyingDown ? 1.f : 0.f, DeltaTime, 8.f);

	// Caída de muñeco: rotar hacia la dirección del golpe y bajar al suelo, con un pequeño rebote.
	FRotator Rot(Lean.X * -1.f, 0.f, Lean.Y);
	FVector Offset = FVector::ZeroVector;
	if (FallAlpha > 0.001f)
	{
		const FVector LocalFall = Owner->GetActorTransform().InverseTransformVectorNoScale(StaggerDir);
		const float Bounce = FMath::Abs(FMath::Sin(FallAlpha * UE_PI * 1.5f)) * (1.f - FallAlpha) * 10.f;
		Rot.Pitch += -85.f * FallAlpha * FMath::Sign(LocalFall.X == 0.f ? 1.f : LocalFall.X);
		Rot.Roll += 20.f * FallAlpha * LocalFall.Y;
		Offset.Z -= 35.f * FallAlpha - Bounce;
	}
	if (LieAlpha > 0.001f)
	{
		Rot.Pitch += 90.f * LieAlpha;
	}
	if (bSitting)
	{
		Offset.Z -= 20.f;
	}

	FTransform T = PlaceholderRestTransform;
	T.SetRotation((FRotator(T.GetRotation()) + Rot).Quaternion());
	T.AddToTranslation(Offset);
	PlaceholderRoot->SetRelativeTransform(T);
}
