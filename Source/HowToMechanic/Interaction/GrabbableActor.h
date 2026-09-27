#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "Core/HTMTypes.h"
#include "GrabbableActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AMechanicCharacter;

/** Especificación de un objeto genérico placeholder (caja, cono, neumático, bidón...). */
USTRUCT(BlueprintType)
struct FGrabbablePropSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Shape = TEXT("Cube");
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector SizeCm = FVector(50.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FRotator MeshRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor Color = FLinearColor(0.7f, 0.5f, 0.3f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MassKg = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	/** Si no se fuerza, la categoría sale de la masa (UHTMTuningData::MediumMassKg/HeavyMassKg). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bForceWeightClass = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EHTMWeightClass WeightClass = EHTMWeightClass::Light;
};

/**
 * Cualquier objeto físico que se puede coger (GDD §6). Todo lo que se suelta queda en el mundo y es
 * un obstáculo físico. Base de piezas (ACarPart) y herramientas (ATool).
 *
 * Replicación:
 * - El servidor simula; los clientes reciben ReplicatedMovement (bRepPhysics).
 * - Ligero/medio en las manos: el servidor apaga la física y lo ADJUNTA al personaje
 *   (AttachmentReplication). Los clientes apagan su física en OnRep_CarryState.
 * - Pesado: sigue simulando en el servidor; cada portador tira de él con un UPhysicsHandle.
 */
UCLASS()
class HOWTOMECHANIC_API AGrabbableActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AGrabbableActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void OnRep_AttachmentReplication() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ------------------------------------------------------------------ Datos
	/** Aplica una especificación placeholder. Llamar en el servidor antes de FinishSpawning o justo después de spawnear. */
	void SetPropSpec(const FGrabbablePropSpec& InSpec);
	const FGrabbablePropSpec& GetPropSpec() const { return Spec; }

	virtual EHTMWeightClass GetWeightClass() const;
	virtual float GetMassKg() const;
	virtual FText GetDisplayName() const;
	UPrimitiveComponent* GetPhysicsBody() const;
	UStaticMeshComponent* GetMesh() const { return Mesh; }

	// ------------------------------------------------------------------ Transporte
	bool IsCarried() const { return Carriers.Num() > 0; }
	bool IsCarriedBy(const AMechanicCharacter* Who) const;
	/** Ligero o medio en las manos de alguien (adjunto, sin física). */
	bool IsHeldInHands() const { return IsCarried() && GetWeightClass() != EHTMWeightClass::Heavy; }
	int32 GetNumCarriers() const { return Carriers.Num(); }
	const TArray<TObjectPtr<AMechanicCharacter>>& GetCarriers() const { return Carriers; }

	virtual bool CanBeGrabbedBy(const AMechanicCharacter* Who) const;

	/** Multiplicador de velocidad de quien lo lleva (< 0 = el de tuning según categoría y portadores). */
	virtual float GetCarrySpeedMultiplier(int32 NumCarriers) const { return -1.f; }

	/** Rotación relativa al punto de agarre cuando va en las manos. */
	virtual FRotator GetCarryRotationOffset() const { return FRotator::ZeroRotator; }

	/** Servidor. */
	virtual void OnPickedUp(AMechanicCharacter* Who);
	/** Servidor. bThrown = lanzado a propósito (para atribuir impactos). */
	virtual void OnDropped(AMechanicCharacter* Who, const FVector& Velocity, bool bThrown);

	/** ¿Se puede tropezar con esto si está en el suelo? */
	virtual bool IsTripHazard() const { return !IsCarried() && bTripHazard; }

	/** Quién lo lanzó (para fuego amigo). Servidor. */
	AController* GetRecentThrower() const;

	// ------------------------------------------------------------------ Presupuesto de física
	bool IsFrozenByBudget() const { return bFrozenByBudget; }
	void SetFrozenByBudget(bool bFrozen);

	/** Decide si el cuerpo debe simular y aplica el estado (servidor y clientes). */
	virtual bool ShouldSimulatePhysics() const;
	void RefreshPhysicsState();

	// ------------------------------------------------------------------ IInteractable
	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;
	virtual void SetHighlighted(bool bHighlighted) override;

	/** Material dinámico principal (placeholder). */
	UMaterialInstanceDynamic* GetMID() const { return MID; }

protected:
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Aplica malla/escala/color a partir del estado replicado. Local. */
	virtual void ApplyVisuals();

	/** Reacción física a golpes contra algo (servidor). Las piezas lo usan para dañarse. */
	virtual void OnServerImpact(float SpeedCmS, AActor* OtherActor, const FHitResult& Hit) {}

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpactFX(FVector_NetQuantize Location, float Strength);

	UFUNCTION()
	void OnRep_Spec();

	UFUNCTION()
	void OnRep_CarryState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grabbable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Spec, Category = "Grabbable")
	FGrabbablePropSpec Spec;

	UPROPERTY(ReplicatedUsing = OnRep_CarryState)
	TArray<TObjectPtr<AMechanicCharacter>> Carriers;

	UPROPERTY(ReplicatedUsing = OnRep_CarryState)
	bool bFrozenByBudget = false;

	UPROPERTY(EditAnywhere, Category = "Grabbable")
	bool bTripHazard = true;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	TWeakObjectPtr<AController> LastThrower;
	float LastThrowTime = -100.f;
	float LastImpactFXTime = -100.f;
	TMap<TWeakObjectPtr<AActor>, float> LastCharacterImpactTime;
};
