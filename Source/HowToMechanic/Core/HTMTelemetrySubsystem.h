#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/HTMTypes.h"
#include "HTMTelemetrySubsystem.generated.h"

/** Un evento de telemetría (fase 5). */
USTRUCT()
struct FHTMTelemetryRecord
{
	GENERATED_BODY()

	double Time = 0.0;
	int32 Day = 0;
	ETelemetryEvent Event = ETelemetryEvent::Situation;
	EHTMSituation Situation = EHTMSituation::MAX;
	FString Who;
	FString Detail;
	FVector Location = FVector::ZeroVector;
	float Value = 0.f;
};

/** Desastre memorable para el resumen de fin de día. */
USTRUCT(BlueprintType)
struct FHTMDisaster
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FText Description;
	UPROPERTY(BlueprintReadOnly) FString Who;
	UPROPERTY(BlueprintReadOnly) int32 Score = 0;
};

/**
 * Telemetría local simple (fase 5): eventos a CSV en Saved/Telemetry/ y recuento de las
 * situaciones de GDD §13 para comprobar la aceptación del playtest (≥ 5 sin forzarlas).
 * Solo registra en el servidor (es quien conoce la verdad).
 */
UCLASS()
class HOWTOMECHANIC_API UHTMTelemetrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UHTMTelemetrySubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Registra un evento. Ignorado en clientes. */
	void Record(const UObject* WorldContext, ETelemetryEvent Event, const FString& Who = FString(), const FString& Detail = FString(),
		const FVector& Location = FVector::ZeroVector, float Value = 0.f);

	/** Registra una situación de la tabla §13 (con enfriamiento para no contar la misma dos veces). */
	void RecordSituation(const UObject* WorldContext, EHTMSituation Situation, const FString& Who = FString(), const FVector& Location = FVector::ZeroVector);

	void BeginDay(int32 DayNumber);
	/** Cierra el día: vuelca CSV y devuelve los peores desastres del día. */
	TArray<FHTMDisaster> EndDay(int32 MaxDisasters = 3);

	int32 GetDaySituationCount(EHTMSituation Situation) const { return DaySituationCounts[(int32)Situation]; }
	int32 GetDistinctSituationsToday() const;
	int32 GetDistinctSituationsSession() const;

	/** Escribe los CSV ahora. */
	void Flush();

private:
	static int32 DisasterScore(const FHTMTelemetryRecord& R);
	static FText DescribeDisaster(const FHTMTelemetryRecord& R);

	TArray<FHTMTelemetryRecord> Pending;
	TArray<FHTMTelemetryRecord> DayRecords;
	int32 DaySituationCounts[(int32)EHTMSituation::MAX] = {};
	int32 SessionSituationCounts[(int32)EHTMSituation::MAX] = {};
	double LastSituationTime[(int32)EHTMSituation::MAX] = {};
	int32 CurrentDay = 0;
	FString SessionStamp;
	FString EventsFile;
	FString SummaryFile;
};
