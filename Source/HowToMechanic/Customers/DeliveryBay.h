#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "DeliveryBay.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class AModularCar;

/**
 * Zona de entrega frente al taller. Se deja el coche del cliente dentro y se pulsa el cartel:
 * se evalúa automáticamente con la checklist visible (GDD §10.1).
 */
UCLASS()
class HOWTOMECHANIC_API ADeliveryBay : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADeliveryBay();

	virtual void BeginPlay() override;

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;
	virtual FVector GetInteractionLocation() const override;

protected:
	AModularCar* FindDeliverableCar() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Zone;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Sign;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FloorMark;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
};
