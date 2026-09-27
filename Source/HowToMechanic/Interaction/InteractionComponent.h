#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/HTMTypes.h"
#include "InteractionComponent.generated.h"

class AGrabbableActor;
class AMechanicCharacter;
class ATool;
class IInteractable;
class UPhysicsHandleComponent;

/**
 * Único sistema de interacción del jugador (CLAUDE.md). Vive en el personaje.
 *
 * Flujo: el cliente dueño elige el objetivo (traza desde la cámara) y envía su INTENCIÓN por RPC
 * ("quiero coger la rueda"). El servidor valida distancia y reglas, ejecuta y replica el resultado.
 *
 * - Coger/soltar/lanzar: HeldObject + CarryMode replicados.
 * - Pulsaciones mantenidas (tornillos, gato, liberar atrapado...): el servidor lleva el tiempo;
 *   el dueño recibe HoldAlpha para la barra de progreso.
 * - Herramientas continuas (manguera, pintura, extintor...): ServerToolTrigger.
 */
UCLASS(ClassGroup = (HTM), meta = (BlueprintSpawnableComponent))
class HOWTOMECHANIC_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------------ Input (cliente dueño)
	void InputGrab();
	void InputThrow();
	void InputUse(bool bPressed);
	void InputAltUse(bool bPressed);
	void InputEnter();

	// ------------------------------------------------------------------ Consultas
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }
	AGrabbableActor* GetHeldObject() const { return HeldObject; }
	ECarryMode GetCarryMode() const { return CarryMode; }
	ATool* GetHeldTool() const;
	EToolType GetHeldToolType() const;
	/** Manos libres o solo algo ligero: se puede usar/pulsar cosas. */
	bool HasHandFree() const { return CarryMode == ECarryMode::None || CarryMode == ECarryMode::OneHand; }
	bool IsHoldingInteraction() const { return HoldTarget != nullptr; }
	float GetHoldAlpha() const { return HoldAlpha; }
	EInteractionVerb GetHoldVerb() const { return HoldVerb; }
	AActor* GetHoldTarget() const { return HoldTarget; }
	bool IsToolActive() const { return bToolActive; }

	/** Texto de ayuda para el HUD de un verbo sobre el objeto enfocado (vacío si no aplica). */
	FText GetPromptText(EInteractionVerb Verb) const;

	// ------------------------------------------------------------------ Servidor
	bool PickUp(AGrabbableActor* Object);
	void ReleaseHeld(const FVector& ExtraVelocity, bool bThrown);
	void CancelHold();
	bool IsWithinReach(const AActor* Target) const;

	static IInteractable* AsInteractable(AActor* Actor);
	static const IInteractable* AsInteractable(const AActor* Actor);

protected:
	UFUNCTION(Server, Reliable) void ServerGrab(AActor* Target);
	UFUNCTION(Server, Reliable) void ServerDrop(bool bThrow);
	UFUNCTION(Server, Reliable) void ServerInstant(AActor* Target, EInteractionVerb Verb);
	UFUNCTION(Server, Reliable) void ServerBeginHold(AActor* Target, EInteractionVerb Verb);
	UFUNCTION(Server, Reliable) void ServerEndHold();
	UFUNCTION(Server, Reliable) void ServerToolTrigger(bool bActive, bool bAlt);
	UFUNCTION(Server, Reliable) void ServerShove(AMechanicCharacter* Target);

	UFUNCTION() void OnRep_Held();

	void UpdateFocus();
	void TickServerHold(float DeltaTime);
	void TickHeavyCarry(float DeltaTime);
	bool ValidateTarget(AActor* Target, EInteractionVerb Verb) const;
	bool TryStartVerb(EInteractionVerb Verb);
	AMechanicCharacter* GetCharacter() const;

	UPROPERTY(ReplicatedUsing = OnRep_Held)
	TObjectPtr<AGrabbableActor> HeldObject;

	UPROPERTY(Replicated)
	ECarryMode CarryMode = ECarryMode::None;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> HoldTarget;

	UPROPERTY(Replicated)
	EInteractionVerb HoldVerb = EInteractionVerb::Use;

	UPROPERTY(Replicated)
	float HoldAlpha = 0.f;

	UPROPERTY(Replicated)
	bool bToolActive = false;

	float HoldElapsed = 0.f;
	float HoldDuration = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> Handle;

	TWeakObjectPtr<AActor> FocusedActor;
	TWeakObjectPtr<AActor> HighlightedActor;

	/** Botones mantenidos localmente (para reintentar al cambiar de objetivo). */
	bool bUseHeldLocal = false;
	bool bAltHeldLocal = false;
};
