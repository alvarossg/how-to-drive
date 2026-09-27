#include "Vehicle/CarFireActor.h"
#include "Vehicle/ModularCar.h"
#include "Character/MechanicCharacter.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMVisualLibrary.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ACarFireActor::ACarFireActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ACarFireActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACarFireActor, Car);
	DOREPLIFETIME(ACarFireActor, Intensity);
}

void ACarFireActor::Init(AModularCar* InCar)
{
	Car = InCar;
}

void ACarFireActor::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Intensity();
}

void ACarFireActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UHTMVisualLibrary::StopFX(FireFX.Get());
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		UHTMVisualLibrary::SpawnFX(this, EHTMFX::Smoke, GetActorLocation(), 1.2f, nullptr, 2.f);
	}
	Super::EndPlay(EndPlayReason);
}

void ACarFireActor::OnRep_Intensity()
{
	// Recrear el efecto solo cuando cambia de forma visible (formas grandes, pocas partículas).
	if (FMath::Abs(Intensity - VisualIntensity) < 0.2f && FireFX.IsValid())
	{
		return;
	}
	VisualIntensity = Intensity;
	UHTMVisualLibrary::StopFX(FireFX.Get());
	FireFX = UHTMVisualLibrary::SpawnFX(this, EHTMFX::Fire, FVector::ZeroVector, 0.6f + Intensity, GetRootComponent(), -1.f);
}

void ACarFireActor::Extinguish(float Amount)
{
	if (!HasAuthority())
	{
		return;
	}
	Intensity -= Amount;
	if (Intensity <= 0.f)
	{
		if (Car)
		{
			Car->OnFireExtinguished();
		}
		Destroy();
		return;
	}
	OnRep_Intensity();
}

void ACarFireActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	Intensity = FMath::Min(1.f, Intensity + T.FireGrowPerSec * DeltaSeconds);

	DamageTimer -= DeltaSeconds;
	if (DamageTimer <= 0.f)
	{
		DamageTimer = 1.f;
		OnRep_Intensity();
		if (Car)
		{
			Car->DamagePartsNear(GetActorLocation(), 120.f, T.FireDamagePerSec * Intensity);
		}
		// Susto a quien esté demasiado cerca.
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams Objects(ECC_Pawn);
		GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, Objects, FCollisionShape::MakeSphere(110.f));
		for (const FOverlapResult& O : Overlaps)
		{
			if (AMechanicCharacter* Char = Cast<AMechanicCharacter>(O.GetActor()))
			{
				Char->Stumble((Char->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 250.f);
			}
		}
	}
}
