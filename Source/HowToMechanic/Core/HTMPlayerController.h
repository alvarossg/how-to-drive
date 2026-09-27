#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HTMPlayerController.generated.h"

/**
 * Controlador del jugador. Avisos privados (ClientToast) y comandos de consola para probar y
 * ajustar (solo el ANFITRIÓN puede ejecutar los que cambian el estado).
 *
 * Consola (`):  HTMHelp · HTMStartDay · HTMEndDay · HTMSpawnCustomer [JobId] · HTMSpawnCar [ModelId]
 *               HTMSpawnPart PartId · HTMGiveMoney N · HTMUpgrade · HTMChaos 0/1 · HTMFriendlyPush 0/1
 *               HTMSave · HTMResetSave · HTMInvite · htm.Outline 0/1 · htm.PhysicsBudget.Stats 1
 */
UCLASS()
class HOWTOMECHANIC_API AHTMPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(Client, Unreliable)
	void ClientToast(const FText& Text, FLinearColor Color);

	UFUNCTION(Exec) void HTMHelp();
	UFUNCTION(Exec) void HTMStartDay();
	UFUNCTION(Exec) void HTMEndDay();
	UFUNCTION(Exec) void HTMSpawnCustomer(FName JobId);
	UFUNCTION(Exec) void HTMSpawnCar(FName ModelId);
	UFUNCTION(Exec) void HTMSpawnPart(FName PartId);
	UFUNCTION(Exec) void HTMGiveMoney(int32 Amount);
	UFUNCTION(Exec) void HTMUpgrade();
	UFUNCTION(Exec) void HTMChaos(int32 bEnabled);
	UFUNCTION(Exec) void HTMFriendlyPush(int32 bEnabled);
	UFUNCTION(Exec) void HTMSave();
	UFUNCTION(Exec) void HTMResetSave();
	UFUNCTION(Exec) void HTMInvite();

protected:
	UFUNCTION(Server, Reliable)
	void ServerDebugCommand(const FString& Command, const FString& Arg);
};
