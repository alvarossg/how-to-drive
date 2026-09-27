#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/HTMTypes.h"
#include "JobTypes.generated.h"

class AModularCar;

/** Condición medible de un encargo (GDD §10.1). */
UENUM(BlueprintType)
enum class EJobRequirementType : uint8
{
	FixNoisyFaults,             // "Hace un ruido horrible"
	FixAllFaults,
	TopSpeedAtLeast,            // km/h medidos en la trampa de velocidad
	HasPartId,                  // Param = PartId (p. ej. Horn_Ship)
	HasPartWithTag,             // Param = etiqueta (Sport, Eco...)
	HasCategory,                // Param = nombre de EPartCategory
	VividPaint,
	DistinctPaintColorsAtLeast,
	FuelUseAtMost,
	TotalMassAtMost,
	WheelRadiusAtLeast,
	AirTimeAtLeast,             // segundos en el aire (rampa)
	AllRequiredSlotsFilled,
	NoLooseParts,
	NoBrokenParts,
	MinAverageCondition
};

USTRUCT(BlueprintType)
struct FJobRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) EJobRequirementType Type = EJobRequirementType::AllRequiredSlotsFilled;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Value = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Param;
	/** Texto de la checklist visible ("≥ 140 km/h en la recta"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Label;
};

/** Plantilla de encargo (DT_Jobs). */
USTRUCT(BlueprintType)
struct FJobDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText CustomerName;
	/** Lo que dice el cliente en su bocadillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText RequestText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Budget = 300;
	/** 0 = sin plazo. Se escala con el número de jugadores. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TimeLimitSeconds = 0.f;
	/** Tolerancia en umbrales numéricos (0.1 = 10 %). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Tolerance = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FJobRequirement> Requirements;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAbsurd = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinReputation = 0.f;
	/** Modelos posibles (vacío = cualquiera). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> CarModels;
	/** Avería que se garantiza en el coche entrante (para "Hace un ruido horrible"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EHiddenFault ForcedFault = EHiddenFault::None;
	/** Pieza de serie que se sustituye por otra al generar el coche (p. ej. rueda pinchada). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ForcedSlot;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ForcedPartId;
	/** Color del cliente (personalidad visual). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor CustomerColor = FLinearColor(1.f, 0.54f, 0.24f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Weight = 1.f;
};

UENUM(BlueprintType)
enum class EJobOutcome : uint8
{
	Pending,
	FullPay,
	PartialPay,
	Angry
};

/** Encargo activo (replicado en GameState para la pizarra). */
USTRUCT(BlueprintType)
struct FActiveJob
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 JobUid = 0;
	UPROPERTY(BlueprintReadOnly) FName JobId;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AModularCar> Car = nullptr;
	UPROPERTY(BlueprintReadOnly) FText CustomerName;
	UPROPERTY(BlueprintReadOnly) FText RequestText;
	UPROPERTY(BlueprintReadOnly) TArray<FText> RequirementLabels;
	UPROPERTY(BlueprintReadOnly) int32 Budget = 0;
	UPROPERTY(BlueprintReadOnly) float StartServerTime = 0.f;
	/** 0 = sin plazo. */
	UPROPERTY(BlueprintReadOnly) float DeadlineServerTime = 0.f;
	UPROPERTY(BlueprintReadOnly) EJobOutcome Outcome = EJobOutcome::Pending;
};

/** Resultado de evaluar una entrega (checklist visible). */
USTRUCT(BlueprintType)
struct FJobEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 JobUid = 0;
	UPROPERTY(BlueprintReadOnly) FText CustomerName;
	UPROPERTY(BlueprintReadOnly) TArray<FText> Labels;
	UPROPERTY(BlueprintReadOnly) TArray<bool> Passed;
	UPROPERTY(BlueprintReadOnly) int32 Payment = 0;
	UPROPERTY(BlueprintReadOnly) float ReputationDelta = 0.f;
	UPROPERTY(BlueprintReadOnly) EJobOutcome Outcome = EJobOutcome::Pending;
	UPROPERTY(BlueprintReadOnly) float ServerTime = 0.f;
};
