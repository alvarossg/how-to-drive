#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "HTMSessionSubsystem.generated.h"

/** Fila simplificada de la lista de partidas para el menú. */
USTRUCT(BlueprintType)
struct FHTMSessionRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString HostName;
	UPROPERTY(BlueprintReadOnly) int32 Players = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaxPlayers = 4;
	UPROPERTY(BlueprintReadOnly) int32 PingMs = 0;
	UPROPERTY(BlueprintReadOnly) bool bChaotic = false;
};

DECLARE_MULTICAST_DELEGATE(FHTMSessionsEvent);

/**
 * Sesiones online (GDD §3: 1-4 jugadores, invitación de amigos). Envuelve el Online Subsystem activo:
 * Steam si está disponible (DefaultPlatformService=Steam) o Null (LAN/local) como respaldo.
 *
 * El anfitrión crea la sesión y abre L_Workshop con ?listen; los demás la buscan o aceptan una
 * invitación de Steam y viajan con ClientTravel. Todo el juego después es replicación nativa.
 */
UCLASS()
class HOWTOMECHANIC_API UHTMSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Crea la sesión y, al terminar, abre el taller como listen server. */
	void HostSession(bool bLAN);
	/** Busca partidas (resultado en GetResults / OnSearchFinished). */
	void FindSessions(bool bLAN);
	void JoinSession(int32 ResultIndex);
	/** Sale de la sesión online (sin viajar). */
	void LeaveSession();
	/** Overlay de invitaciones de la plataforma (Steam). */
	bool ShowInviteUI();

	bool IsBusy() const { return bBusy; }
	bool IsOnlinePlatformAvailable() const;
	FName GetPlatformName() const;
	const TArray<FHTMSessionRow>& GetResults() const { return Rows; }
	const FText& GetStatus() const { return Status; }

	FHTMSessionsEvent OnSearchFinished;
	FHTMSessionsEvent OnStatusChanged;

private:
	IOnlineSessionPtr GetSessionInterface() const;
	void SetStatus(const FText& InStatus, bool bInBusy);
	void OpenWorkshopAsHost();
	void JoinResult(const FOnlineSessionSearchResult& Result);

	void HandleCreateComplete(FName SessionName, bool bWasSuccessful);
	void HandleDestroyBeforeHost(FName SessionName, bool bWasSuccessful);
	void HandleFindComplete(bool bWasSuccessful);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FHTMSessionRow> Rows;
	FText Status;
	bool bBusy = false;
	bool bPendingHostLAN = false;

	FDelegateHandle CreateHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle InviteHandle;
};
