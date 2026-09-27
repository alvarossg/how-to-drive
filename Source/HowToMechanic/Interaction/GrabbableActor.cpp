#include "Interaction/GrabbableActor.h"
#include "Interaction/InteractionComponent.h"
#include "Character/MechanicCharacter.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMGameState.h"
#include "Core/HTMPhysicsBudgetSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HTMGrabbable"

AGrabbableActor::AGrabbableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);
	NetUpdateFrequency = 30.f;
	MinNetUpdateFrequency = 5.f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetCollisionResponseToChannel(ECC_Interaction, ECR_Block);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->BodyInstance.bUseCCD = false;
	Mesh->SetLinearDamping(0.1f);
	Mesh->SetAngularDamping(0.3f);
}

void AGrabbableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGrabbableActor, Spec);
	DOREPLIFETIME(AGrabbableActor, Carriers);
	DOREPLIFETIME(AGrabbableActor, bFrozenByBudget);
}

void AGrabbableActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisuals();
	RefreshPhysicsState();
	if (HasAuthority())
	{
		Mesh->OnComponentHit.AddDynamic(this, &AGrabbableActor::OnMeshHit);
		if (UHTMPhysicsBudgetSubsystem* Budget = GetWorld()->GetSubsystem<UHTMPhysicsBudgetSubsystem>())
		{
			Budget->Register(this);
		}
	}
}

void AGrabbableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		// Soltar de las manos a quien lo lleve.
		for (AMechanicCharacter* Carrier : TArray<TObjectPtr<AMechanicCharacter>>(Carriers))
		{
			if (Carrier && Carrier->GetInteraction())
			{
				Carrier->GetInteraction()->ReleaseHeld(FVector::ZeroVector, false);
			}
		}
		if (UWorld* World = GetWorld())
		{
			if (UHTMPhysicsBudgetSubsystem* Budget = World->GetSubsystem<UHTMPhysicsBudgetSubsystem>())
			{
				Budget->Unregister(this);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AGrabbableActor::SetPropSpec(const FGrabbablePropSpec& InSpec)
{
	Spec = InSpec;
	ApplyVisuals();
}

void AGrabbableActor::OnRep_Spec()
{
	ApplyVisuals();
}

void AGrabbableActor::ApplyVisuals()
{
	Mesh->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(Spec.Shape));
	Mesh->SetWorldScale3D(Spec.SizeCm / 100.f);
	Mesh->SetMassOverrideInKg(NAME_None, Spec.MassKg, true);
	MID = UHTMVisualLibrary::ApplyPlaceholderMaterial(Mesh, Spec.Color);
}

EHTMWeightClass AGrabbableActor::GetWeightClass() const
{
	if (Spec.bForceWeightClass)
	{
		return Spec.WeightClass;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	const float Mass = GetMassKg();
	return Mass >= T.HeavyMassKg ? EHTMWeightClass::Heavy : (Mass >= T.MediumMassKg ? EHTMWeightClass::Medium : EHTMWeightClass::Light);
}

float AGrabbableActor::GetMassKg() const
{
	return Spec.MassKg;
}

FText AGrabbableActor::GetDisplayName() const
{
	return Spec.DisplayName.IsEmpty() ? LOCTEXT("Object", "Objeto") : Spec.DisplayName;
}

UPrimitiveComponent* AGrabbableActor::GetPhysicsBody() const
{
	return Mesh;
}

bool AGrabbableActor::IsCarriedBy(const AMechanicCharacter* Who) const
{
	return Who && Carriers.Contains(Who);
}

bool AGrabbableActor::CanBeGrabbedBy(const AMechanicCharacter* Who) const
{
	if (!Who || IsCarriedBy(Who))
	{
		return false;
	}
	if (GetWeightClass() == EHTMWeightClass::Heavy)
	{
		return Carriers.Num() < 2;
	}
	// Ligero/medio: se le puede quitar de las manos a otro (fricción cooperativa).
	return true;
}

void AGrabbableActor::OnPickedUp(AMechanicCharacter* Who)
{
	check(HasAuthority());
	Carriers.AddUnique(Who);
	SetFrozenByBudget(false);
	RefreshPhysicsState();
	if (GetWeightClass() != EHTMWeightClass::Heavy)
	{
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	}
}

void AGrabbableActor::OnDropped(AMechanicCharacter* Who, const FVector& Velocity, bool bThrown)
{
	check(HasAuthority());
	Carriers.Remove(Who);
	if (!IsCarried())
	{
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
		Mesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
	}
	RefreshPhysicsState();
	if (Mesh->IsSimulatingPhysics())
	{
		Mesh->SetPhysicsLinearVelocity(Velocity);
		if (bThrown)
		{
			Mesh->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 360.f);
		}
	}
	if (bThrown && Who)
	{
		LastThrower = Who->GetController();
		LastThrowTime = GetWorld()->GetTimeSeconds();
	}
}

AController* AGrabbableActor::GetRecentThrower() const
{
	const float Window = UHTMTuningData::Get().ThrowAttributionTime;
	return (GetWorld()->GetTimeSeconds() - LastThrowTime) <= Window ? LastThrower.Get() : nullptr;
}

void AGrabbableActor::SetFrozenByBudget(bool bFrozen)
{
	if (bFrozenByBudget != bFrozen)
	{
		bFrozenByBudget = bFrozen;
		RefreshPhysicsState();
	}
}

bool AGrabbableActor::ShouldSimulatePhysics() const
{
	return !IsHeldInHands() && !bFrozenByBudget && !GetAttachParentActor();
}

void AGrabbableActor::RefreshPhysicsState()
{
	const bool bSimulate = ShouldSimulatePhysics();
	if (Mesh->IsSimulatingPhysics() != bSimulate)
	{
		Mesh->SetSimulatePhysics(bSimulate);
		if (!bSimulate && !HasAuthority())
		{
			// Mientras simulaba, la AttachmentReplication recibida no tenía efecto: aplicarla ahora.
			AActor::OnRep_AttachmentReplication();
		}
	}
}

void AGrabbableActor::OnRep_AttachmentReplication()
{
	Super::OnRep_AttachmentReplication();
	RefreshPhysicsState();
}

void AGrabbableActor::OnRep_CarryState()
{
	if (!IsCarried() && Cast<AMechanicCharacter>(GetAttachParentActor()))
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	RefreshPhysicsState();
}

bool AGrabbableActor::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return Verb == EInteractionVerb::Grab && CanBeGrabbedBy(Who);
}

FText AGrabbableActor::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	if (Verb != EInteractionVerb::Grab)
	{
		return FText::GetEmpty();
	}
	switch (GetWeightClass())
	{
	case EHTMWeightClass::Heavy:
		return FText::Format(Carriers.Num() == 1 ? LOCTEXT("GrabHeavyHelp", "Ayudar a cargar {0}") : LOCTEXT("GrabHeavy", "Arrastrar {0} (¡pesa!)"), GetDisplayName());
	case EHTMWeightClass::Medium:
		return FText::Format(IsCarried() ? LOCTEXT("StealMedium", "Quitarle {0}") : LOCTEXT("GrabMedium", "Coger {0} (dos manos)"), GetDisplayName());
	default:
		return FText::Format(IsCarried() ? LOCTEXT("StealLight", "Quitarle {0}") : LOCTEXT("GrabLight", "Coger {0}"), GetDisplayName());
	}
}

void AGrabbableActor::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	if (Verb == EInteractionVerb::Grab && Who && Who->GetInteraction())
	{
		Who->GetInteraction()->PickUp(this);
	}
}

void AGrabbableActor::SetHighlighted(bool bHighlighted)
{
	UHTMVisualLibrary::SetHighlighted(Mesh, MID, bHighlighted);
}

void AGrabbableActor::OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !OtherActor || OtherActor == this)
	{
		return;
	}
	const FVector MyVel = Mesh->GetPhysicsLinearVelocity();
	const FVector OtherVel = OtherComp ? OtherComp->GetComponentVelocity() : FVector::ZeroVector;
	const float RelSpeed = (MyVel - OtherVel).Size();

	OnServerImpact(RelSpeed, OtherActor, Hit);

	const float Now = GetWorld()->GetTimeSeconds();
	if (RelSpeed > 350.f && Now - LastImpactFXTime > 0.25f)
	{
		LastImpactFXTime = Now;
		MulticastImpactFX(Hit.ImpactPoint, FMath::Clamp(RelSpeed / 1500.f, 0.2f, 1.f));
	}

	AMechanicCharacter* Victim = Cast<AMechanicCharacter>(OtherActor);
	if (!Victim || IsCarriedBy(Victim) || RelSpeed < 150.f)
	{
		return;
	}
	// Enfriamiento por víctima para no contar el mismo golpe varias veces.
	if (float* Last = LastCharacterImpactTime.Find(Victim))
	{
		if (Now - *Last < UHTMTuningData::Get().ImpactCooldown) { return; }
	}
	LastCharacterImpactTime.Add(Victim, Now);

	// Fuego amigo: los objetos lanzados por compañeros no tumban si la sala lo desactiva.
	AController* Thrower = GetRecentThrower();
	if (Thrower && Thrower != Victim->GetController())
	{
		const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
		if (GS && !GS->RoomOptions.bFriendlyPush)
		{
			return;
		}
	}

	const UHTMTuningData& T = UHTMTuningData::Get();
	const float ImpactScore = (RelSpeed / 100.f) * FMath::Min(GetMassKg(), T.ImpactMassCapKg);
	Victim->ReceiveImpact(ImpactScore, MyVel.GetSafeNormal(), this);
}

void AGrabbableActor::MulticastImpactFX_Implementation(FVector_NetQuantize Location, float Strength)
{
	UHTMVisualLibrary::SpawnFX(this, EHTMFX::DustPoof, Location, 0.4f + Strength * 0.6f, nullptr, 0.5f);
}

#undef LOCTEXT_NAMESPACE
