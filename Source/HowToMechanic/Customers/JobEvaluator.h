#pragma once

#include "CoreMinimal.h"
#include "Customers/JobTypes.h"

class AModularCar;

/**
 * Evaluación automática de encargos (GDD §10.1): cada petición es una condición MEDIBLE.
 * Añade siempre "coche completo" y "sin piezas flojas" (volver de la prueba sin puerta se paga),
 * y el plazo si lo hay. Pago completo / parcial / cliente enfadado.
 */
struct HOWTOMECHANIC_API FJobEvaluator
{
	static FJobEvaluation Evaluate(const FJobDefinitionRow& Definition, const FActiveJob& Job, const AModularCar* Car, float ServerNow);

	/** Texto de la checklist para un requisito (usa Label si viene en los datos). */
	static FText DescribeRequirement(const FJobRequirement& Requirement);

	/** Todas las líneas que se verán en la pizarra (explícitas + implícitas). */
	static TArray<FText> BuildLabels(const FJobDefinitionRow& Definition);

	static bool CheckRequirement(const FJobRequirement& Requirement, const AModularCar* Car, float Tolerance);
};
