#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterPatch.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * Charco (o barrizal): moja el suelo y hace resbalar (GDD §8), moja/ensucia lo que pasa por encima
 * (estropea la pintura fresca: "coche recién pintado en un charco", GDD §13).
 * - Temporal: lo crea la manguera y se evapora.
 * - Permanente: el charco grande y el camino de tierra de la zona de pruebas.
 * Replicado; los solapes con personajes se procesan en todas las máquinas (predicción del movimiento).
 */
UCLASS()
class HOWTOMECHANIC_API AWaterPatch : public AActor
{
	GENERATED_BODY()

public:
	AWaterPatch();

	/** Servidor: crea un charco nuevo o hace crecer uno cercano. */
	static AWaterPatch* SpawnOrGrow(const UObject* WorldContext, const FVector& Location);

	/** Servidor: configura un charco permanente de tamaño fijo. */
	void InitPermanent(const FVector2D& SizeCm, bool bInMud);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsMud() const { return bMud; }

protected:
	UFUNCTION() void OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION() void OnEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	UFUNCTION() void OnRep_Size();

	void ApplySize();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Area;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(ReplicatedUsing = OnRep_Size) FVector2D Size = FVector2D(120.f, 120.f);
	UPROPERTY(ReplicatedUsing = OnRep_Size) bool bMud = false;
	UPROPERTY(Replicated) bool bPermanent = false;

	float TimeLeft = 60.f;
	float WetTimer = 0.f;
	TSet<TWeakObjectPtr<AActor>> WetCharacters;
};
