#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HTMGameMode.generated.h"

class UJobDirectorComponent;
class AWorkshopBlockout;
class AModularCar;
class AMechanicCharacter;
class UHTMSaveGame;
struct FWorkshopLayout;

/**
 * Reglas de la partida (solo servidor). Dueño de la jornada (GDD §4), la compraventa (§10.2),
 * la grúa y los objetos perdidos (§9), la ampliación del taller (§11) y el guardado del anfitrión (§3).
 *
 * Opciones de URL al abrir el mapa: ?listen ?Chaotic=1 ?FriendlyPush=0 ?NewGame=1
 */
UCLASS()
class HOWTOMECHANIC_API AHTMGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHTMGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	UJobDirectorComponent* GetJobDirector() const { return JobDirector; }

	// ------------------------------------------------------------------ Jornada
	void StartDay();
	void EndDay();
	void PrepareNextDay();

	// ------------------------------------------------------------------ Economía
	bool BuyJunkyardOffer(int32 OfferIndex, AMechanicCharacter* Who);
	bool SellCar(AModularCar* Car, AMechanicCharacter* Who);
	void CallTow(AMechanicCharacter* Who);
	bool UpgradeWorkshop(AMechanicCharacter* Who);

	// ------------------------------------------------------------------ Guardado
	void SaveWorkshop();
	void ResetSave();

	// ------------------------------------------------------------------ Depuración (consola del anfitrión)
	void DebugSpawnCar(FName ModelId, AMechanicCharacter* Near);
	void DebugSpawnPart(FName PartId, AMechanicCharacter* Near);

protected:
	void EnsureBlockout();
	void SpawnLayoutActors(int32 Level, int32 PreviousLevel);
	void SpawnTestZone();
	void SpawnStarterContent();
	void RestockShelves();
	void GenerateJunkyardOffers();
	int32 CollectLostItems();
	void LoadWorkshop();
	void RestoreSavedObjects(const UHTMSaveGame* Save);

	template <typename T>
	T* SpawnLayout(const FTransform& Where);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UJobDirectorComponent> JobDirector;

	UPROPERTY(Transient) TObjectPtr<AWorkshopBlockout> Blockout;
	/** Actores del taller que se recolocan al ampliarlo (estanterías, carteles, elevadores...). */
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> LayoutActors;
	UPROPERTY(Transient) TObjectPtr<UHTMSaveGame> LoadedSave;

	bool bNewGameRequested = false;
	bool bChaoticOption = false;
	bool bFriendlyPushOption = true;
	FRandomStream Rng;
};
