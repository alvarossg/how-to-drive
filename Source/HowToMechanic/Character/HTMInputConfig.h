#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HTMInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Acciones y mapeos de Enhanced Input construidos en C++ (teclado/ratón + mando desde el principio).
 * Evita depender de IA_/IMC_ binarios. Si UHTMSettings define un IMC de sustitución, se usa ese
 * para los mapeos, pero las acciones siguen siendo estas.
 *
 * A pie:   WASD/stick izq. mover · ratón/stick der. mirar · Espacio/A saltar · Shift/L3 correr
 *          C/B agacharse · X/Cruceta abajo tumbarse · E/X coger-soltar · Q/RB lanzar
 *          Clic izq./RT usar · Clic der./LT uso secundario · F/Y subir al coche · T/LB "¡EH!" · G/Cruceta arriba emote
 * Coche:   W/RT acelerar · S/LT frenar-marcha atrás · A-D/stick dirección · Espacio/A freno de mano
 *          H/RB bocina · F/Y bajar · ratón/stick der. cámara
 */
UCLASS()
class HOWTOMECHANIC_API UHTMInputConfig : public UObject
{
	GENERATED_BODY()

public:
	static UHTMInputConfig* Get();

	// A pie
	UPROPERTY() TObjectPtr<UInputAction> Move;
	UPROPERTY() TObjectPtr<UInputAction> Look;
	UPROPERTY() TObjectPtr<UInputAction> Jump;
	UPROPERTY() TObjectPtr<UInputAction> Sprint;
	UPROPERTY() TObjectPtr<UInputAction> Crouch;
	UPROPERTY() TObjectPtr<UInputAction> Prone;
	UPROPERTY() TObjectPtr<UInputAction> Grab;
	UPROPERTY() TObjectPtr<UInputAction> Throw;
	UPROPERTY() TObjectPtr<UInputAction> Use;
	UPROPERTY() TObjectPtr<UInputAction> AltUse;
	UPROPERTY() TObjectPtr<UInputAction> Enter;
	UPROPERTY() TObjectPtr<UInputAction> Shout;
	UPROPERTY() TObjectPtr<UInputAction> Emote;

	// Coche
	UPROPERTY() TObjectPtr<UInputAction> Throttle;
	UPROPERTY() TObjectPtr<UInputAction> Brake;
	UPROPERTY() TObjectPtr<UInputAction> Steer;
	UPROPERTY() TObjectPtr<UInputAction> Handbrake;
	UPROPERTY() TObjectPtr<UInputAction> Horn;
	UPROPERTY() TObjectPtr<UInputAction> ExitCar;
	UPROPERTY() TObjectPtr<UInputAction> CarLook;

	UPROPERTY() TObjectPtr<UInputMappingContext> OnFootContext;
	UPROPERTY() TObjectPtr<UInputMappingContext> DrivingContext;

private:
	void Build();
};
