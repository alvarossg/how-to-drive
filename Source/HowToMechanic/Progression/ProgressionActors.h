#pragma once

#include "CoreMinimal.h"
#include "Economy/EconomyActors.h"
#include "Core/HTMTypes.h"
#include "ProgressionActors.generated.h"

class UBoxComponent;

/** Panel de ampliación del taller (GDD §11): muestra coste y reputación necesaria. */
UCLASS()
class HOWTOMECHANIC_API AWorkshopUpgradeTerminal : public AHTMDiegeticActor
{
	GENERATED_BODY()

public:
	AWorkshopUpgradeTerminal();
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;

protected:
	virtual void GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const override;
	float Timer = 0.f;
};

/**
 * Armario de cosméticos (GDD §12): gorros, ropa, guantes y color de herramientas.
 * Usar = ponerse el siguiente desbloqueado. Uso secundario = comprar el siguiente bloqueado.
 * Algunos solo se desbloquean con logros absurdos.
 */
UCLASS()
class HOWTOMECHANIC_API AWardrobe : public AHTMDiegeticActor
{
	GENERATED_BODY()

public:
	AWardrobe();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	void SetSlot(ECosmeticSlot InSlot);

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;

	static bool IsUnlocked(const UObject* WorldContext, FName CosmeticId, const class AHTMPlayerState* PS);

protected:
	virtual void GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const override;
	UFUNCTION() void OnRep_Slot();
	FName FindNext(const AMechanicCharacter* Who, bool bUnlocked) const;

	UPROPERTY(ReplicatedUsing = OnRep_Slot) ECosmeticSlot Slot = ECosmeticSlot::Hat;
};

/** Panel de color de paredes (personalización del taller, GDD §11). */
UCLASS()
class HOWTOMECHANIC_API AWallColorPanel : public AHTMDiegeticActor
{
	GENERATED_BODY()

public:
	AWallColorPanel();

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;

protected:
	virtual void GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const override;
};

/**
 * Cabina de pintura (mejora nivel 3): lo que está dentro seca más rápido y a salvo de la suciedad.
 */
UCLASS()
class HOWTOMECHANIC_API APaintBooth : public AActor
{
	GENERATED_BODY()

public:
	APaintBooth();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Volume;
	float Timer = 0.f;
};
