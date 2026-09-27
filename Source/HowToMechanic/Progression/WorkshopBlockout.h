#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HTMTypes.h"
#include "WorkshopBlockout.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UTextRenderComponent;

/** Posiciones de todo lo que el GameMode coloca para un nivel de taller. */
struct FWorkshopLayout
{
	int32 Level = 1;
	FBox WorkshopBounds;
	TArray<FTransform> PlayerStarts;
	TArray<FTransform> ShelfSlots;
	TArray<TPair<EToolType, FTransform>> Tools;
	TArray<FTransform> Jacks;
	TArray<FTransform> JackStands;
	TArray<FTransform> Lifts;
	TArray<FTransform> Cranes;
	TArray<FTransform> PaintBooths;
	TArray<FTransform> Wardrobes;
	FTransform CashRegister;
	FTransform JobBoard;
	FTransform TowPhone;
	FTransform ScrapBin;
	FTransform UpgradeTerminal;
	FTransform WallPanel;
	FTransform DeliveryBay;
	FTransform SellPoint;
	TArray<FTransform> JunkyardSigns;
	FVector LostAndFound = FVector::ZeroVector;
	/** Coches de prueba iniciales dentro del taller (nivel 1). */
	TArray<FTransform> StarterCars;
};

/**
 * Blockout del mundo (fase 1–4 en gris, fase 6 con paleta): taller por niveles, explanada, calle,
 * zona de pruebas compacta (recta, cuesta, rampa, tierra, charco, curva, conos, muro de neumáticos,
 * farolas) y desguace. Todo a menos de 30 s del taller (GDD §9, referencia 25_plano_zona_pruebas).
 *
 * La GEOMETRÍA estática se construye igual en cada máquina a partir del nivel replicado del taller.
 * Los actores con estado (estanterías, coches, farolas...) los crea el GameMode con GetLayout().
 * Coordenadas: X hacia la calle, la puerta del taller está fija en X = DoorX; el taller crece hacia -X y ±Y.
 */
UCLASS()
class HOWTOMECHANIC_API AWorkshopBlockout : public AActor
{
	GENERATED_BODY()

public:
	AWorkshopBlockout();

	static AWorkshopBlockout* Get(const UObject* WorldContext);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Reconstruye la geometría del taller para un nivel (local). */
	void BuildWorkshop(int32 Level);

	FWorkshopLayout GetLayout(int32 Level) const;
	FBox GetWorkshopBounds(int32 Level) const;
	/** Zona jugable total (para recoger objetos perdidos). */
	FBox GetPlayableBounds() const;
	TArray<FTransform> GetCustomerSpots() const;
	TArray<FTransform> GetTowSpots() const;
	FTransform GetJunkyardDropSpot() const;
	/** Elementos de la zona de pruebas con estado (farolas, trampa, charcos, conos, neumáticos). */
	void GetTestZoneSpawns(TArray<FTransform>& OutLamps, FTransform& OutSpeedTrap, TArray<TPair<FTransform, FVector2D>>& OutPuddles,
		TArray<TPair<FTransform, FVector2D>>& OutMud, TArray<FTransform>& OutCones, TArray<FTransform>& OutTires, TArray<FTransform>& OutClutter) const;

	/** Prueba A/B del contorno (ART_DIRECTION §10). Consola: htm.Outline 0/1 */
	void SetOutlineEnabled(bool bEnabled);

	static constexpr float DoorX = 700.f;

protected:
	void BuildStaticWorld();
	void BuildLighting();
	void ClearWorkshop();
	void RecolorWalls();
	FVector2D GetFloorSize(int32 Level) const;
	int32 GetBays(int32 Level) const;

	UStaticMeshComponent* Box(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollision = true,
		const FRotator& Rotation = FRotator::ZeroRotator, bool bWorkshopPart = false, FName Shape = TEXT("Cube"));

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(Transient) TArray<TObjectPtr<UActorComponent>> WorkshopComponents;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> WallComponents;

	int32 BuiltLevel = 0;
	int32 LastOutlineValue = -1;
	bool bStaticWorldBuilt = false;
	FDelegateHandle LevelChangedHandle;
	FDelegateHandle WallColorHandle;
};
