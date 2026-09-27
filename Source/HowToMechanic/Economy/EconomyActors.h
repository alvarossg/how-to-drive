#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "Core/HTMTypes.h"
#include "EconomyActors.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class AModularCar;

/** Base de los objetos diegéticos del taller: un cuerpo sólido + un cartel de texto. */
UCLASS(Abstract)
class HOWTOMECHANIC_API AHTMDiegeticActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AHTMDiegeticActor();

	virtual void BeginPlay() override;
	virtual void SetHighlighted(bool bHighlighted) override;
	virtual FVector GetInteractionLocation() const override;

protected:
	/** Forma, tamaño y color del cuerpo placeholder. */
	virtual void GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const;
	void SetLabel(const FText& Text);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> BodyMID;
};

/**
 * Hueco de la estantería de repuestos: muestra la pieza con su etiqueta de precio colgando
 * (ART_DIRECTION §12). Coger = comprar: la pieza aparece en tus manos.
 */
UCLASS()
class HOWTOMECHANIC_API AShelfSlot : public AHTMDiegeticActor
{
	GENERATED_BODY()

public:
	AShelfSlot();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** Servidor. */
	void SetStock(FName InPartId);
	FName GetStock() const { return PartId; }

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;

protected:
	virtual void GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const override;
	UFUNCTION() void OnRep_Stock();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Display;
	UPROPERTY(ReplicatedUsing = OnRep_Stock) FName PartId;
};

/** Caja registradora: dinero diegético + abrir el taller / pasar al día siguiente. */
UCLASS()
class HOWTOMECHANIC_API ACashRegister : public AHTMDiegeticActor
{
	GENERATED_BODY()

public:
	ACashRegister();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;

protected:
	virtual void GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const override;
	void RefreshDisplay();
	float Timer = 0.f;
};

/** Cartel de oferta del desguace (información incompleta a propósito, GDD §10.2). */
UCLASS()
class HOWTOMECHANIC_API AJunkyardOfferSign : public AHTMDiegeticActor
{
	GENERATED_BODY()

public:
	AJunkyardOfferSign();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	void SetOfferIndex(int32 Index) { OfferIndex = Index; }

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;

protected:
	virtual void GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const override;
	const struct FJunkyardOffer* GetOffer() const;

	UPROPERTY(Replicated) int32 OfferIndex = 0;
	float Timer = 0.f;
};

/** Punto de venta: el comprador de hoy paga según estado, piezas y sus gustos (GDD §10.2). */
UCLASS()
class HOWTOMECHANIC_API ASellPoint : public AHTMDiegeticActor
{
	GENERATED_BODY()

public:
	ASellPoint();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;

	/** Precio que pagaría el comprador de hoy por este coche. */
	static int32 QuotePrice(const AModularCar* Car, FName BuyerId, const UObject* WorldContext);

protected:
	virtual void GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const override;
	AModularCar* FindCarInZone() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Zone;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FloorMark;
	float Timer = 0.f;
};

/** Contenedor de chatarra: las piezas que caen dentro se venden (recuperas algo, GDD §14). */
UCLASS()
class HOWTOMECHANIC_API AScrapBin : public AHTMDiegeticActor
{
	GENERATED_BODY()

public:
	AScrapBin();

	virtual void BeginPlay() override;

protected:
	virtual void GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const override;
	UFUNCTION() void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Mouth;
};

/** Teléfono de la grúa: trae de vuelta los coches perdidos/volcados, por dinero (GDD §9). */
UCLASS()
class HOWTOMECHANIC_API ATowPhone : public AHTMDiegeticActor
{
	GENERATED_BODY()

public:
	ATowPhone();

	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;

protected:
	virtual void GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const override;
};
