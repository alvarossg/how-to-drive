#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/HTMTypes.h"
#include "Interactable.generated.h"

class AMechanicCharacter;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Único contrato de interacción del juego (CLAUDE.md). Coger, soltar, lanzar, usar herramienta,
 * atornillar, arrancar, subir al coche... todo pasa por aquí.
 *
 * - CanInteract / GetInteractionText / GetHoldDuration: consultas puras, se llaman en cliente
 *   (para el resalte y el texto) y en servidor (para validar). Solo deben leer estado replicado.
 * - Interact: SOLO servidor. Cambia el estado.
 */
class HOWTOMECHANIC_API IInteractable
{
	GENERATED_BODY()

public:
	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const { return false; }
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const { return FText::GetEmpty(); }

	/** 0 = instantáneo. >0 = hay que mantener el botón ese tiempo. */
	virtual float GetHoldDuration(const AMechanicCharacter* Who, EInteractionVerb Verb) const { return 0.f; }

	/** Si true, al completar una pulsación mantenida se vuelve a empezar (un tornillo tras otro). */
	virtual bool IsHoldRepeatable(const AMechanicCharacter* Who, EInteractionVerb Verb) const { return false; }

	/** Servidor: ejecuta la interacción (al pulsar o al completar el tiempo de mantener). */
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) {}

	/** Servidor: se llama cada tick mientras se mantiene (antes de completar). */
	virtual void InteractHoldTick(AMechanicCharacter* Who, EInteractionVerb Verb, float Alpha, float DeltaSeconds) {}

	/** Local: resalte visual (contorno suave). */
	virtual void SetHighlighted(bool bHighlighted) {}

	/** Punto donde se dibuja el icono/texto. */
	virtual FVector GetInteractionLocation() const;
};
