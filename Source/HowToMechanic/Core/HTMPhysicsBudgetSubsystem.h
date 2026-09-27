#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HTMPhysicsBudgetSubsystem.generated.h"

class AGrabbableActor;

/**
 * Presupuesto de física (CLAUDE.md): limita los cuerpos simulando a la vez.
 * - Los objetos en reposo duermen (Chaos lo hace solo; aquí forzamos si nos pasamos).
 * - Los pequeños y lejanos de todo jugador dejan de simular hasta que alguien se acerca.
 * Solo actúa en el servidor. Consola: htm.PhysicsBudget.Stats 1
 */
UCLASS()
class HOWTOMECHANIC_API UHTMPhysicsBudgetSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	void Register(AGrabbableActor* Actor);
	void Unregister(AGrabbableActor* Actor);

	int32 GetLastActiveCount() const { return LastActiveCount; }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Evaluate();

	TArray<TWeakObjectPtr<AGrabbableActor>> Tracked;
	float Accumulator = 0.f;
	int32 LastActiveCount = 0;
};
