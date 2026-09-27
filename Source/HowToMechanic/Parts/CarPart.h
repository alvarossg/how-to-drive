#pragma once

#include "CoreMinimal.h"
#include "Interaction/GrabbableActor.h"
#include "Parts/PartTypes.h"
#include "Parts/SurfaceTreatable.h"
#include "CarPart.generated.h"

class AModularCar;
class AMechanicCharacter;

/**
 * Pieza de coche: actor físico independiente que se acopla a un slot del chasis (CLAUDE.md, GDD §7).
 * Desmontada es un objeto físico más del mundo (se tropieza con ella, rueda, se lanza...).
 *
 * Estado replicado: PartId, condición, superficie (suciedad/óxido/pintura), avería oculta y a qué
 * coche/slot está montada. Los tornillos viven en el slot del coche (FCarSlotState), fuente única.
 *
 * Interacción sobre una pieza MONTADA (servidor valida):
 *   Usar (mantener, herramienta correcta) → apretar un tornillo tras otro.
 *   Uso secundario (mantener)             → aflojar; al soltar el último, la pieza cae.
 *   Coger con 0 tornillos                 → arrancarla del hueco.
 * Montar: soltarla cerca de su hueco (imán suave). Orientación libre: puede quedar al revés.
 */
UCLASS()
class HOWTOMECHANIC_API ACarPart : public AGrabbableActor, public ISurfaceTreatable
{
	GENERATED_BODY()

public:
	ACarPart();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------ Datos
	/** Servidor, justo tras spawnear. */
	void InitPart(FName InPartId, float InCondition, const FPartSurfaceState& InSurface, EHiddenFault InFault = EHiddenFault::None);

	const FPartDefinitionRow* GetDefinition() const;
	FName GetPartId() const { return PartId; }
	EPartCategory GetCategory() const;
	const FPartStats& GetStats() const;
	float GetCondition() const { return Condition; }
	EPartState GetState() const;
	bool IsBroken() const { return Condition <= 0.f; }
	bool HasTag(FName Tag) const;
	int32 GetPrice() const;
	EToolType GetRequiredTool() const;
	const FPartSurfaceState& GetSurface() const { return Surface; }
	EHiddenFault GetHiddenFault() const { return HiddenFault; }
	bool IsFaultRevealed() const { return bFaultRevealed; }

	virtual FText GetDisplayName() const override;
	virtual EHTMWeightClass GetWeightClass() const override;

	// ------------------------------------------------------------------ Montaje
	bool IsMounted() const { return OwningCar != nullptr; }
	AModularCar* GetOwningCar() const { return OwningCar; }
	FName GetSlotName() const { return SlotName; }
	int32 GetBoltsTight() const;
	int32 GetBoltCount() const;
	bool IsLoose() const { return IsMounted() && GetBoltsTight() < GetBoltCount(); }
	bool IsReversed() const;

	/** Servidor: lo llama AModularCar al montar/desmontar. */
	void OnMountedToCar(AModularCar* Car, FName InSlotName, const FTransform& RelativeTransform);
	void OnDetachedFromCar(const FVector& InheritVelocity);

	/** Servidor: busca un hueco compatible cerca y se encaja (imán suave). */
	bool TrySnapToNearbySlot(AMechanicCharacter* Who);
	/** Servidor: se suelta del coche (cae). */
	void DetachFromCar(bool bInheritVelocity);

	// ------------------------------------------------------------------ Estado (servidor)
	void ApplyDamage(float Amount);
	void SetCondition(float NewCondition);
	void RevealFault(AMechanicCharacter* By);
	void SetHiddenFault(EHiddenFault Fault) { HiddenFault = Fault; }
	void SetSurface(const FPartSurfaceState& InSurface) { Surface = InSurface; OnRep_Surface(); }

	virtual bool ShouldSimulatePhysics() const override;

	// ------------------------------------------------------------------ IInteractable
	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual float GetHoldDuration(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual bool IsHoldRepeatable(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;

	// ------------------------------------------------------------------ ISurfaceTreatable
	virtual void TreatSurface(ESurfaceTreatment Treatment, float Amount, const FLinearColor& Color, AMechanicCharacter* By) override;
	virtual bool IsPaintable() const override;

protected:
	virtual void ApplyVisuals() override;
	virtual void OnServerImpact(float SpeedCmS, AActor* OtherActor, const FHitResult& Hit) override;

	UFUNCTION() void OnRep_PartId();
	UFUNCTION() void OnRep_Surface();
	UFUNCTION() void OnRep_Mount();

	bool HasCorrectTool(const AMechanicCharacter* Who) const;
	bool IsSlotAccessible() const;
	void ServerTickSurface(float DeltaSeconds);

	UPROPERTY(ReplicatedUsing = OnRep_PartId) FName PartId;
	UPROPERTY(Replicated) float Condition = 100.f;
	UPROPERTY(ReplicatedUsing = OnRep_Surface) FPartSurfaceState Surface;
	UPROPERTY(Replicated) EHiddenFault HiddenFault = EHiddenFault::None;
	UPROPERTY(Replicated) bool bFaultRevealed = false;
	UPROPERTY(ReplicatedUsing = OnRep_Mount) TObjectPtr<AModularCar> OwningCar;
	UPROPERTY(ReplicatedUsing = OnRep_Mount) FName SlotName;

	FTransform MountedRelative = FTransform::Identity;
	float SurfaceTimer = 0.f;
	float RattleTime = 0.f;
	bool bPaintRuinReported = false;
};
