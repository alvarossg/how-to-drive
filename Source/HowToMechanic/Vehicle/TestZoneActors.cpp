#include "Vehicle/TestZoneActors.h"
#include "Vehicle/ModularCar.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HTMTestZone"

// ============================================================================ Trampa de velocidad

ASpeedTrap::ASpeedTrap()
{
	bReplicates = true;
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(40.f, 500.f, 200.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);

	Gantry = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gantry"));
	Gantry->SetupAttachment(Trigger);
	Gantry->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Display = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Display"));
	Display->SetupAttachment(Trigger);
	Display->SetHorizontalAlignment(EHTA_Center);
	Display->SetWorldSize(70.f);
	Display->SetTextRenderColor(FColor(255, 210, 63));
}

void ASpeedTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASpeedTrap, LastSpeedKmh);
}

void ASpeedTrap::BeginPlay()
{
	Super::BeginPlay();
	Gantry->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cube")));
	Gantry->SetRelativeLocation(FVector(0.f, 0.f, 450.f));
	Gantry->SetRelativeScale3D(FVector(0.4f, 10.4f, 0.6f));
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Gantry, HTMPalette::SafetyOrange());
	Display->SetRelativeLocation(FVector(-30.f, 0.f, 520.f));
	Display->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	OnRep_LastSpeed();
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ASpeedTrap::OnOverlap);
	}
}

void ASpeedTrap::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AModularCar* Car = Cast<AModularCar>(OtherActor))
	{
		LastSpeedKmh = Car->GetVelocity().Size() * 0.036f;
		Car->OnSpeedTrap(LastSpeedKmh);
		OnRep_LastSpeed();
	}
}

void ASpeedTrap::OnRep_LastSpeed()
{
	Display->SetText(FText::Format(LOCTEXT("Speed", "{0} km/h"), FText::AsNumber(FMath::RoundToInt(LastSpeedKmh))));
}

// ============================================================================ Farola

AKnockableLamp::AKnockableLamp()
{
	bReplicates = true;
	SetReplicatingMovement(true);

	Pole = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pole"));
	SetRootComponent(Pole);
	Pole->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Pole->SetMobility(EComponentMobility::Movable);
	Pole->SetNotifyRigidBodyCollision(true);

	LampHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LampHead"));
	LampHead->SetupAttachment(Pole);
	LampHead->SetUsingAbsoluteScale(true);
	LampHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AKnockableLamp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnockableLamp, bKnocked);
}

void AKnockableLamp::BeginPlay()
{
	Super::BeginPlay();
	Pole->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cylinder")));
	Pole->SetWorldScale3D(FVector(0.16f, 0.16f, 4.5f));
	Pole->SetMassOverrideInKg(NAME_None, 60.f, true);
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Pole, HTMPalette::Asphalt());
	LampHead->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Sphere")));
	LampHead->SetWorldScale3D(FVector(0.5f));
	LampHead->SetRelativeLocation(FVector(0.f, 0.f, 50.f)); // tope del cilindro (escala del padre)
	UHTMVisualLibrary::ApplyPlaceholderMaterial(LampHead, HTMPalette::SignalYellow());
	if (HasAuthority())
	{
		Pole->OnComponentHit.AddDynamic(this, &AKnockableLamp::OnHit);
	}
}

void AKnockableLamp::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bKnocked || !OtherComp || !OtherComp->IsSimulatingPhysics())
	{
		return;
	}
	const float Speed = OtherComp->GetPhysicsLinearVelocity().Size();
	if (Speed * OtherComp->GetMass() > 300.f * 400.f)
	{
		bKnocked = true;
		OnRep_Knocked();
		Pole->AddImpulseAtLocation(OtherComp->GetPhysicsLinearVelocity().GetSafeNormal() * 60.f * 400.f, Hit.ImpactPoint + FVector(0.f, 0.f, 150.f));
	}
}

void AKnockableLamp::OnRep_Knocked()
{
	if (bKnocked)
	{
		Pole->SetSimulatePhysics(true);
		UHTMVisualLibrary::SpawnFX(this, EHTMFX::Sparks, GetActorLocation() + FVector(0.f, 0.f, 60.f), 1.f, nullptr, 0.6f);
	}
}

#undef LOCTEXT_NAMESPACE
