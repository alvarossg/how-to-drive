#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/HTMTypes.h"
#include "HTMGameInstance.generated.h"

class UNetDriver;

/**
 * Vive toda la sesión del ejecutable (sobrevive a los viajes de mapa). Guarda lo que el menú decide
 * antes de abrir el taller y el último error de red, para mostrarlo al volver al menú.
 */
UCLASS()
class HOWTOMECHANIC_API UHTMGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	/** Opciones de sala elegidas en el menú (el anfitrión las pasa por URL al abrir el taller). */
	UPROPERTY(BlueprintReadWrite) FHTMRoomOptions PendingRoomOptions;
	UPROPERTY(BlueprintReadWrite) bool bPendingNewGame = false;
	UPROPERTY(BlueprintReadWrite) bool bPendingLAN = false;

	/** Último fallo de red/viaje. El menú lo enseña y lo limpia. */
	UPROPERTY(BlueprintReadOnly) FText LastErrorMessage;

	/** URL de opciones para abrir L_Workshop como anfitrión. */
	FString BuildHostOptions() const;

	/** Vuelve al menú principal (sale de la sesión online). */
	void ReturnToMainMenuWithError(const FText& Error);

private:
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
