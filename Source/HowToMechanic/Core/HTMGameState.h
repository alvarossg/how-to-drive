#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/HTMTypes.h"
#include "Core/HTMTelemetrySubsystem.h"
#include "Customers/JobTypes.h"
#include "HTMGameState.generated.h"

/** Resumen de fin de día (GDD §4, bucle de jornada). */
USTRUCT(BlueprintType)
struct FHTMDaySummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Day = 0;
	UPROPERTY(BlueprintReadOnly) int32 MoneyEarned = 0;
	UPROPERTY(BlueprintReadOnly) int32 MoneySpent = 0;
	UPROPERTY(BlueprintReadOnly) float ReputationDelta = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 JobsFull = 0;
	UPROPERTY(BlueprintReadOnly) int32 JobsPartial = 0;
	UPROPERTY(BlueprintReadOnly) int32 JobsAngry = 0;
	UPROPERTY(BlueprintReadOnly) int32 LostItemsRecovered = 0;
	UPROPERTY(BlueprintReadOnly) int32 LostItemsFee = 0;
	UPROPERTY(BlueprintReadOnly) int32 DistinctSituations = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FHTMDisaster> Disasters;
};

/** Oferta del desguace: información incompleta a propósito (GDD §10.2). */
USTRUCT(BlueprintType)
struct FJunkyardOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 OfferId = 0;
	UPROPERTY(BlueprintReadOnly) FName ModelId;
	UPROPERTY(BlueprintReadOnly) int32 Price = 0;
	/** Pista vaga del vendedor ("arranca... a veces"). */
	UPROPERTY(BlueprintReadOnly) FText Hint;
	UPROPERTY(BlueprintReadOnly) bool bSold = false;
	/** Semilla del estado real (solo la usa el servidor al generar el coche). */
	UPROPERTY() int32 Seed = 0;
};

DECLARE_MULTICAST_DELEGATE(FHTMSimpleEvent);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHTMToastEvent, const FText& /*Text*/, const FLinearColor& /*Color*/);

/**
 * Estado compartido de la partida. Todo cambia SOLO en el servidor y se replica.
 * Dinero y reputación viven aquí (y los guarda el anfitrión).
 */
UCLASS()
class HOWTOMECHANIC_API AHTMGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AHTMGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------------ Economía (servidor)
	/** Suma (o resta) dinero. Solo servidor. */
	void AddMoney(int32 Delta, const FText& Reason);
	/** Intenta gastar. Devuelve false (y avisa) si no hay dinero. Solo servidor. */
	bool TrySpend(int32 Amount, const FText& Reason);
	void AddReputation(float Delta);

	/** Aviso visible para todos (bocadillo/toast). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastToast(const FText& Text, FLinearColor Color);

	// ------------------------------------------------------------------ Estado replicado
	UPROPERTY(ReplicatedUsing = OnRep_Money, BlueprintReadOnly) int32 Money = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) float Reputation = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_WorkshopLevel, BlueprintReadOnly) int32 WorkshopLevel = 1;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 DayNumber = 0;
	UPROPERTY(ReplicatedUsing = OnRep_DayPhase, BlueprintReadOnly) EDayPhase DayPhase = EDayPhase::Closed;
	UPROPERTY(Replicated, BlueprintReadOnly) float DayEndServerTime = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Jobs, BlueprintReadOnly) TArray<FActiveJob> ActiveJobs;
	UPROPERTY(ReplicatedUsing = OnRep_LastEvaluation, BlueprintReadOnly) FJobEvaluation LastEvaluation;
	UPROPERTY(ReplicatedUsing = OnRep_DaySummary, BlueprintReadOnly) FHTMDaySummary LastDaySummary;
	UPROPERTY(Replicated, BlueprintReadOnly) FHTMRoomOptions RoomOptions;
	UPROPERTY(ReplicatedUsing = OnRep_WallColor, BlueprintReadOnly) int32 WallColorIndex = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Jobs, BlueprintReadOnly) TArray<FJunkyardOffer> JunkyardOffers;
	/** Cosméticos comprados (del taller, para todos). */
	UPROPERTY(Replicated, BlueprintReadOnly) TArray<FName> UnlockedCosmetics;
	/** Comprador de hoy en el punto de venta. */
	UPROPERTY(ReplicatedUsing = OnRep_Jobs, BlueprintReadOnly) FName TodaysBuyer;

	/** Contadores del día (servidor). */
	int32 DayMoneyEarned = 0;
	int32 DayMoneySpent = 0;
	float DayReputationDelta = 0.f;

	// ------------------------------------------------------------------ Eventos locales
	FHTMSimpleEvent OnMoneyChanged;
	FHTMSimpleEvent OnWorkshopLevelChanged;
	FHTMSimpleEvent OnJobsChanged;
	FHTMSimpleEvent OnEvaluation;
	FHTMSimpleEvent OnDayPhaseChanged;
	FHTMSimpleEvent OnDaySummary;
	FHTMSimpleEvent OnWallColorChanged;
	FHTMToastEvent OnToast;

	/** Llamar en el servidor tras modificar arrays replicados para disparar también los eventos locales. */
	void NotifyJobsChanged() { OnRep_Jobs(); }
	void NotifyWorkshopLevelChanged() { OnRep_WorkshopLevel(); }
	void NotifyWallColorChanged() { OnRep_WallColor(); }
	void NotifyEvaluation() { OnRep_LastEvaluation(); }
	void NotifyDaySummary() { OnRep_DaySummary(); }
	void NotifyDayPhase() { OnRep_DayPhase(); }

	float GetDayTimeRemaining() const;
	const FActiveJob* FindJobForCar(const class AModularCar* Car) const;
	FActiveJob* FindJobByUid(int32 Uid);
	int32 GetNumPlayers() const { return PlayerArray.Num(); }

protected:
	UFUNCTION() void OnRep_Money();
	UFUNCTION() void OnRep_WorkshopLevel();
	UFUNCTION() void OnRep_DayPhase();
	UFUNCTION() void OnRep_Jobs();
	UFUNCTION() void OnRep_LastEvaluation();
	UFUNCTION() void OnRep_DaySummary();
	UFUNCTION() void OnRep_WallColor();
};
