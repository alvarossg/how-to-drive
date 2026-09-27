#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestZoneActors.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Trampa de velocidad en la recta (GDD §10.1: "Velocidad máxima ≥ X, medible en la zona de pruebas").
 * Registra en cada coche que la cruza su velocidad y muestra el último valor en un cartel.
 */
UCLASS()
class HOWTOMECHANIC_API ASpeedTrap : public AActor
{
	GENERATED_BODY()

public:
	ASpeedTrap();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

protected:
	UFUNCTION() void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION() void OnRep_LastSpeed();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Trigger;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Gantry;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Display;

	UPROPERTY(ReplicatedUsing = OnRep_LastSpeed) float LastSpeedKmh = 0.f;
};

/**
 * Farola derribable (GDD §9). Estática hasta que un coche o algo pesado la golpea fuerte:
 * entonces pasa a ser física y cae. Replicada.
 */
UCLASS()
class HOWTOMECHANIC_API AKnockableLamp : public AActor
{
	GENERATED_BODY()

public:
	AKnockableLamp();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

protected:
	UFUNCTION() void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	UFUNCTION() void OnRep_Knocked();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Pole;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> LampHead;
	UPROPERTY(ReplicatedUsing = OnRep_Knocked) bool bKnocked = false;
};
