#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/GrabbableActor.h"
#include "Interaction/Interactable.h"
#include "LiftingDevices.generated.h"

class AModularCar;
class UStaticMeshComponent;
class UBoxComponent;
class UTextRenderComponent;

/**
 * Gato (GDD §7.3): levanta UNA esquina. Si el coche se empuja o se arranca, vuelca y el coche cae.
 * Se coloca bajo el coche (objeto medio) y se bombea manteniendo "usar"; "uso secundario" lo baja.
 * El gato empuja con una fuerza tipo muelle en el punto de contacto: el coche sigue siendo físico.
 */
UCLASS()
class HOWTOMECHANIC_API AJack : public AGrabbableActor
{
	GENERATED_BODY()

public:
	AJack();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool ShouldSimulatePhysics() const override;
	virtual void OnPickedUp(AMechanicCharacter* Who) override;

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual float GetHoldDuration(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual bool IsHoldRepeatable(const AMechanicCharacter* Who, EInteractionVerb Verb) const override { return Verb != EInteractionVerb::Grab; }
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;

	bool IsDeployed() const { return bDeployed; }

protected:
	AModularCar* FindCarAbove(FVector& OutContact) const;
	void Tip(const FString& Reason);

	UFUNCTION() void OnRep_Lift();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Pad;

	UPROPERTY(ReplicatedUsing = OnRep_Lift) bool bDeployed = false;
	UPROPERTY(ReplicatedUsing = OnRep_Lift) float LiftHeight = 0.f;
	UPROPERTY(Replicated) TObjectPtr<AModularCar> SupportedCar;

	FVector ContactLocal = FVector::ZeroVector;
	FVector DeployedAt = FVector::ZeroVector;
};

/**
 * Elevador hidráulico (mejora nivel 2). Levanta el coche entero. CUALQUIERA puede bajarlo desde el
 * panel, aunque haya alguien debajo (GDD §7.3): así nacen los "atrapados".
 */
UCLASS()
class HOWTOMECHANIC_API AHydraulicLift : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AHydraulicLift();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual float GetHoldDuration(const AMechanicCharacter* Who, EInteractionVerb Verb) const override { return 0.1f; }
	virtual bool IsHoldRepeatable(const AMechanicCharacter* Who, EInteractionVerb Verb) const override { return true; }
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;
	virtual FVector GetInteractionLocation() const override;

protected:
	UFUNCTION() void OnRep_Height();
	void UpdateArms(float Height);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ArmLeft;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ArmRight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PostLeft;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PostRight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Panel;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> PanelLabel;

	UPROPERTY(ReplicatedUsing = OnRep_Height) float TargetHeight = 0.f;
	float CurrentHeight = 0.f;
};

/**
 * Grúa de motor (mejora nivel 3): hace posible mover motores EN SOLITARIO (GDD §11).
 * Carrito con ruedas que se arrastra; "usar" engancha la pieza pesada más cercana y la sube,
 * "uso secundario" la baja/suelta (y se encaja si está sobre su hueco).
 */
UCLASS()
class HOWTOMECHANIC_API AEngineCrane : public AGrabbableActor
{
	GENERATED_BODY()

public:
	AEngineCrane();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual EHTMWeightClass GetWeightClass() const override { return EHTMWeightClass::Heavy; }
	virtual float GetCarrySpeedMultiplier(int32 NumCarriers) const override { return 0.85f; }

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;

protected:
	class ACarPart* FindHookablePart() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Boom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Hook;

	UPROPERTY(Replicated) TObjectPtr<class ACarPart> HookedPart;
	UPROPERTY(Replicated) float HookHeight = 60.f;
};
