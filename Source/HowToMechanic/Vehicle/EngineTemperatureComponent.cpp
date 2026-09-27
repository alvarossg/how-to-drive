#include "Vehicle/EngineTemperatureComponent.h"
#include "Vehicle/ModularCar.h"
#include "Parts/CarPart.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMTelemetrySubsystem.h"
#include "Core/HTMGameState.h"
#include "Core/HTMPalette.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HTMEngineTemp"

UEngineTemperatureComponent::UEngineTemperatureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.2f;
	SetIsReplicatedByDefault(true);
}

void UEngineTemperatureComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEngineTemperatureComponent, Temperature);
	DOREPLIFETIME(UEngineTemperatureComponent, bWarning);
	DOREPLIFETIME(UEngineTemperatureComponent, bSmoking);
}

void UEngineTemperatureComponent::BeginPlay()
{
	Super::BeginPlay();
	Temperature = UHTMTuningData::Get().AmbientTemp;
}

void UEngineTemperatureComponent::OnFireExtinguished()
{
	Temperature = UHTMTuningData::Get().WarnTemp - 25.f;
	bSmoking = false;
	bWarning = false;
	bSmokeReported = false;
	OnRep_Smoking();
}

void UEngineTemperatureComponent::OnRep_Smoking()
{
	UHTMVisualLibrary::StopFX(SmokeFX.Get());
	SmokeFX = nullptr;
	if (!bSmoking)
	{
		return;
	}
	AModularCar* Car = Cast<AModularCar>(GetOwner());
	FVector Where = FVector::ZeroVector;
	if (Car)
	{
		for (ACarPart* Part : Car->GetMountedParts())
		{
			if (Part->GetCategory() == EPartCategory::Engine)
			{
				Where = Car->GetActorTransform().InverseTransformPosition(Part->GetActorLocation()) + FVector(0.f, 0.f, 40.f);
			}
		}
		SmokeFX = UHTMVisualLibrary::SpawnFX(Car, EHTMFX::Smoke, Where, 1.2f, Car->GetChassis(), -1.f);
	}
}

void UEngineTemperatureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AModularCar* Car = Cast<AModularCar>(GetOwner());
	if (!Car || !Car->HasAuthority())
	{
		return;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	const FCarComputedStats& S = Car->GetStats();

	float Heat = 0.f;
	if (Car->IsEngineRunning())
	{
		const float Load = 0.25f + 0.75f * FMath::Abs(Car->GetInputThrottle());
		Heat = S.HeatGeneration * Load * T.GetHeatMult(Car);
	}
	const float Airflow = 1.f + Car->GetVelocity().Size() / 1500.f;
	const float Cooling = FMath::Max(0.02f, S.Cooling) * Airflow * T.CoolingScale * (Temperature - T.AmbientTemp) / 100.f;
	Temperature = FMath::Max(T.AmbientTemp, Temperature + (Heat - Cooling * 10.f) * DeltaTime);

	// Aviso sonoro antes del humo (telegrafiar, GDD §14).
	bWarning = Temperature >= T.WarnTemp;
	BeepTimer -= DeltaTime;
	if (bWarning && !bSmoking && BeepTimer <= 0.f)
	{
		BeepTimer = 3.f;
		if (AHTMGameState* GS = Car->GetWorld()->GetGameState<AHTMGameState>())
		{
			GS->MulticastToast(FText::Format(LOCTEXT("Beep", "{0}: ¡PIII! temperatura {1}º"), Car->GetCarName(), FText::AsNumber(FMath::RoundToInt(Temperature))), HTMPalette::SafetyOrange());
		}
	}

	const bool bShouldSmoke = Temperature >= T.SmokeTemp;
	if (bShouldSmoke != bSmoking)
	{
		bSmoking = bShouldSmoke;
		OnRep_Smoking();
		if (bSmoking && !bSmokeReported)
		{
			bSmokeReported = true;
			if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(Car))
			{
				Tel->Record(Car, ETelemetryEvent::EngineSmoke, FString(), Car->GetCarName().ToString(), Car->GetActorLocation(), Temperature);
				Tel->RecordSituation(Car, EHTMSituation::EngineSmoking, FString(), Car->GetActorLocation());
			}
		}
	}

	if (Temperature >= T.FireTemp)
	{
		Car->StartFire();
	}
}

#undef LOCTEXT_NAMESPACE
