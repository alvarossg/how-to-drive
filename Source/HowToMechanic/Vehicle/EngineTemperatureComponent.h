#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EngineTemperatureComponent.generated.h"

/**
 * Temperatura del motor según piezas y uso (GDD §9/§13): aviso (pitido) → humo → pequeño fuego.
 * Las consecuencias graves se telegrafían antes (GDD §14). Servidor autoritativo; replicado.
 */
UCLASS(ClassGroup = (HTM), meta = (BlueprintSpawnableComponent))
class HOWTOMECHANIC_API UEngineTemperatureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEngineTemperatureComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	float GetTemperature() const { return Temperature; }
	bool IsSmoking() const { return bSmoking; }
	bool IsWarning() const { return bWarning; }

	/** Tras apagar el fuego, el motor queda caliente pero por debajo del aviso. */
	void OnFireExtinguished();

protected:
	UFUNCTION() void OnRep_Smoking();

	UPROPERTY(Replicated) float Temperature = 20.f;
	UPROPERTY(Replicated) bool bWarning = false;
	UPROPERTY(ReplicatedUsing = OnRep_Smoking) bool bSmoking = false;

	TWeakObjectPtr<USceneComponent> SmokeFX;
	float BeepTimer = 0.f;
	bool bSmokeReported = false;
};
