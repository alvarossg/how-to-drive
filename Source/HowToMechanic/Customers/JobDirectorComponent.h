#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Customers/JobTypes.h"
#include "JobDirectorComponent.generated.h"

class AModularCar;
class AMechanicCharacter;
class ACustomerNPC;

/**
 * Director de clientes (vive en el GameMode: solo servidor). Durante la jornada hace llegar clientes
 * con su coche y un encargo medible, vigila plazos, evalúa entregas y paga (GDD §10).
 * Los encargos escalan con el número de jugadores (GDD §3).
 */
UCLASS(ClassGroup = (HTM))
class HOWTOMECHANIC_API UJobDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJobDirectorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void StartDay(int32 DayNumber);
	/** Los clientes sin atender se van enfadados (con media penalización). */
	void EndDay();

	/** Entrega: evalúa, paga y el cliente se va con el coche. */
	bool DeliverCar(AModularCar* Car, AMechanicCharacter* Who);

	int32 GetPendingArrivals() const { return ArrivalQueue.Num(); }

	/** Resultados del día (resumen de fin de jornada). */
	int32 DayFull = 0;
	int32 DayPartial = 0;
	int32 DayAngry = 0;

	/** Para depurar: llega un cliente ya (consola: HTM.SpawnCustomer [JobId]). */
	void SpawnCustomer(FName JobId);

private:
	FName PickJob(FRandomStream& Rng, float Reputation, bool bWantAbsurd) const;
	void ResolveJob(int32 JobUid, const FJobEvaluation& Eval, AMechanicCharacter* Who);
	void DismissCustomer(int32 JobUid, float Delay);

	TArray<FName> ArrivalQueue;
	float NextArrivalTime = 0.f;
	int32 NextUid = 1;
	FRandomStream Rng;

	UPROPERTY() TMap<int32, TObjectPtr<ACustomerNPC>> Customers;
};
