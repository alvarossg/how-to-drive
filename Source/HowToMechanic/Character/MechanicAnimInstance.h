#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Core/HTMTypes.h"
#include "MechanicAnimInstance.generated.h"

class AMechanicCharacter;

/**
 * Lógica de animación en C++ (CLAUDE.md): el futuro ABP_Mechanic solo LEE estas variables en su grafo
 * (locomoción, posturas, brazos de carga, tambaleo). No hay lógica en Blueprint.
 *
 * Todo se deriva de estado ya replicado (postura, estado, modo de carga, sprint, coche), así que funciona
 * igual en el dueño, en el servidor y en los demás clientes.
 */
UCLASS()
class HOWTOMECHANIC_API UMechanicAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// --- Locomoción ---
	UPROPERTY(BlueprintReadOnly, Category = "HTM") float GroundSpeed = 0.f;
	/** -180..180: dirección del movimiento respecto a donde mira. */
	UPROPERTY(BlueprintReadOnly, Category = "HTM") float MoveDirection = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "HTM") bool bIsFalling = false;
	UPROPERTY(BlueprintReadOnly, Category = "HTM") bool bIsSprinting = false;
	UPROPERTY(BlueprintReadOnly, Category = "HTM") EMechanicStance Stance = EMechanicStance::Standing;

	// --- Estado ---
	UPROPERTY(BlueprintReadOnly, Category = "HTM") EMechanicState MechanicState = EMechanicState::Normal;
	UPROPERTY(BlueprintReadOnly, Category = "HTM") bool bIsDriving = false;
	UPROPERTY(BlueprintReadOnly, Category = "HTM") bool bIsKnockedOut = false;
	UPROPERTY(BlueprintReadOnly, Category = "HTM") bool bIsTrapped = false;
	UPROPERTY(BlueprintReadOnly, Category = "HTM") bool bIsWet = false;

	// --- Brazos ---
	UPROPERTY(BlueprintReadOnly, Category = "HTM") ECarryMode CarryMode = ECarryMode::None;
	/** 0 = brazos libres, 1 = brazos estirados cargando a dos manos. Suavizado. */
	UPROPERTY(BlueprintReadOnly, Category = "HTM") float CarryArmsAlpha = 0.f;
	/** Usando una herramienta o manteniendo una acción (apretar, bombear...). */
	UPROPERTY(BlueprintReadOnly, Category = "HTM") bool bIsWorking = false;

	// --- Equilibrio / tambaleo (para aditivos de "a punto de caerse") ---
	/** 0 = estable, 1 = muy inestable (carga pesada, suelo mojado, tropiezo). Suavizado. */
	UPROPERTY(BlueprintReadOnly, Category = "HTM") float Wobble = 0.f;
	/** Inclinación lateral hacia la que se tambalea (-1 izquierda, 1 derecha). */
	UPROPERTY(BlueprintReadOnly, Category = "HTM") float LeanSide = 0.f;

private:
	TWeakObjectPtr<AMechanicCharacter> CachedCharacter;
};
