#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JobBoard.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Pizarra de encargos en la pared (UI diegética, ART_DIRECTION §12). Lee el estado replicado del
 * GameState en cada máquina: encargos pendientes, checklist, presupuesto y plazo; y la última
 * evaluación de entrega con sus ✔/✘.
 */
UCLASS()
class HOWTOMECHANIC_API AJobBoard : public AActor
{
	GENERATED_BODY()

public:
	AJobBoard();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	void Refresh();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Board;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Frame;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Title;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Jobs;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> LastResult;

	float RefreshTimer = 0.f;
};
