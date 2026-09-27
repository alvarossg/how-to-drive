#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CarFireActor.generated.h"

class AModularCar;
class USceneComponent;

/**
 * Pequeño fuego estilizado en el motor (GDD §9). Sale del SISTEMA de temperatura, no de un guion.
 * Crece si nadie lo apaga, daña las piezas cercanas y se apaga con el extintor.
 */
UCLASS()
class HOWTOMECHANIC_API ACarFireActor : public AActor
{
	GENERATED_BODY()

public:
	ACarFireActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Servidor. */
	void Init(AModularCar* InCar);
	/** Servidor: resta intensidad; a 0 se apaga. */
	void Extinguish(float Amount);

	float GetIntensity() const { return Intensity; }

protected:
	UFUNCTION() void OnRep_Intensity();

	UPROPERTY(Replicated) TObjectPtr<AModularCar> Car;
	UPROPERTY(ReplicatedUsing = OnRep_Intensity) float Intensity = 0.4f;

	TWeakObjectPtr<USceneComponent> FireFX;
	float DamageTimer = 0.f;
	float VisualIntensity = -1.f;
};
