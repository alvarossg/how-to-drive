#include "Net/HTMSessionSubsystem.h"
#include "Core/HTMGameInstance.h"
#include "Core/HTMSettings.h"
#include "Core/HTMLog.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "HTMSessions"

namespace
{
	const FName KeyGame(TEXT("HTM_GAME"));
	const FName KeyChaotic(TEXT("HTM_CHAOTIC"));
	constexpr int32 MaxPlayers = 4;
}

void UHTMSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		InviteHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
			FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UHTMSessionSubsystem::HandleInviteAccepted));
	}
}

void UHTMSessionSubsystem::Deinitialize()
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}
	Super::Deinitialize();
}

IOnlineSessionPtr UHTMSessionSubsystem::GetSessionInterface() const
{
	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	return OSS ? OSS->GetSessionInterface() : nullptr;
}

bool UHTMSessionSubsystem::IsOnlinePlatformAvailable() const
{
	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	return OSS && OSS->GetSubsystemName() != NULL_SUBSYSTEM;
}

FName UHTMSessionSubsystem::GetPlatformName() const
{
	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	return OSS ? OSS->GetSubsystemName() : NAME_None;
}

void UHTMSessionSubsystem::SetStatus(const FText& InStatus, bool bInBusy)
{
	Status = InStatus;
	bBusy = bInBusy;
	UE_LOG(LogHTM, Log, TEXT("Sesiones: %s"), *Status.ToString());
	OnStatusChanged.Broadcast();
}

// ============================================================================ Anfitrión

void UHTMSessionSubsystem::HostSession(bool bLAN)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions)
	{
		// Sin Online Subsystem: partida local/LAN directa.
		SetStatus(LOCTEXT("NoOSS", "Sin plataforma online: abriendo partida local"), false);
		OpenWorkshopAsHost();
		return;
	}
	bPendingHostLAN = bLAN || !IsOnlinePlatformAvailable();

	// Si quedó una sesión vieja (volver al menú sin cerrar), se destruye antes.
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UHTMSessionSubsystem::HandleDestroyBeforeHost));
		SetStatus(LOCTEXT("Cleaning", "Cerrando la sesion anterior..."), true);
		Sessions->DestroySession(NAME_GameSession);
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = bPendingHostLAN;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = !bPendingHostLAN;
	Settings.bAllowJoinViaPresence = !bPendingHostLAN;
	Settings.bUseLobbiesIfAvailable = !bPendingHostLAN;
	Settings.Set(KeyGame, FString(TEXT("HowToMechanic")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	const UHTMGameInstance* GI = Cast<UHTMGameInstance>(GetGameInstance());
	const bool bChaotic = GI && GI->PendingRoomOptions.bChaoticPhysics;
	Settings.Set(KeyChaotic, bChaotic ? 1 : 0, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(SETTING_MAPNAME, UHTMSettings::Get()->WorkshopMap, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UHTMSessionSubsystem::HandleCreateComplete));
	SetStatus(LOCTEXT("Creating", "Creando sala..."), true);

	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	const bool bStarted = LocalPlayer && LocalPlayer->GetPreferredUniqueNetId().IsValid()
		? Sessions->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Settings)
		: Sessions->CreateSession(0, NAME_GameSession, Settings);
	if (!bStarted)
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		SetStatus(LOCTEXT("CreateFailNow", "No se pudo crear la sala online: abriendo partida local"), false);
		OpenWorkshopAsHost();
	}
}

void UHTMSessionSubsystem::HandleDestroyBeforeHost(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	bBusy = false;
	HostSession(bPendingHostLAN);
}

void UHTMSessionSubsystem::HandleCreateComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}
	SetStatus(bWasSuccessful ? LOCTEXT("Created", "Sala creada. Abriendo el taller...")
	                         : LOCTEXT("CreateFail", "La sala online fallo: abriendo partida local"), false);
	// Aunque falle la sesión online se abre el taller: se puede jugar en local y reintentar invitar.
	OpenWorkshopAsHost();
}

void UHTMSessionSubsystem::OpenWorkshopAsHost()
{
	UHTMGameInstance* GI = Cast<UHTMGameInstance>(GetGameInstance());
	const FString Options = GI ? GI->BuildHostOptions() : FString(TEXT("listen"));
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*UHTMSettings::Get()->WorkshopMap), true, Options);
}

// ============================================================================ Buscar / unirse

void UHTMSessionSubsystem::FindSessions(bool bLAN)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || bBusy)
	{
		return;
	}
	Rows.Reset();
	Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = 100;
	Search->bIsLanQuery = bLAN || !IsOnlinePlatformAvailable();
	if (!Search->bIsLanQuery)
	{
		Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}

	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UHTMSessionSubsystem::HandleFindComplete));
	SetStatus(LOCTEXT("Searching", "Buscando talleres..."), true);

	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	const bool bStarted = LocalPlayer && LocalPlayer->GetPreferredUniqueNetId().IsValid()
		? Sessions->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), Search.ToSharedRef())
		: Sessions->FindSessions(0, Search.ToSharedRef());
	if (!bStarted)
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		SetStatus(LOCTEXT("SearchFailNow", "No se pudo buscar partidas"), false);
		OnSearchFinished.Broadcast();
	}
}

void UHTMSessionSubsystem::HandleFindComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}
	Rows.Reset();
	if (Search.IsValid())
	{
		for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
		{
			FString Game;
			Result.Session.SessionSettings.Get(KeyGame, Game);
			if (Game != TEXT("HowToMechanic"))
			{
				// Con AppId 480 (Spacewar) salen salas de otros juegos: se filtran.
				continue;
			}
			FHTMSessionRow Row;
			Row.HostName = Result.Session.OwningUserName;
			Row.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Row.Players = Row.MaxPlayers - Result.Session.NumOpenPublicConnections;
			Row.PingMs = Result.PingInMs;
			int32 Chaotic = 0;
			Result.Session.SessionSettings.Get(KeyChaotic, Chaotic);
			Row.bChaotic = Chaotic != 0;
			Rows.Add(Row);
		}
	}
	SetStatus(Rows.Num() > 0 ? FText::Format(LOCTEXT("Found", "{0} taller(es) encontrado(s)"), FText::AsNumber(Rows.Num()))
	                         : LOCTEXT("NoneFound", "No hay talleres abiertos. Crea uno o invita a tus amigos."), false);
	OnSearchFinished.Broadcast();
}

void UHTMSessionSubsystem::JoinSession(int32 ResultIndex)
{
	if (!Search.IsValid() || bBusy)
	{
		return;
	}
	// Rows está filtrado: buscar el resultado real equivalente.
	int32 Visible = -1;
	for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
	{
		FString Game;
		Result.Session.SessionSettings.Get(KeyGame, Game);
		if (Game == TEXT("HowToMechanic") && ++Visible == ResultIndex)
		{
			JoinResult(Result);
			return;
		}
	}
}

void UHTMSessionSubsystem::JoinResult(const FOnlineSessionSearchResult& Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || !Result.IsValid())
	{
		return;
	}
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Sessions->DestroySession(NAME_GameSession);
	}
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UHTMSessionSubsystem::HandleJoinComplete));
	SetStatus(LOCTEXT("Joining", "Entrando al taller..."), true);

	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	const bool bStarted = LocalPlayer && LocalPlayer->GetPreferredUniqueNetId().IsValid()
		? Sessions->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Result)
		: Sessions->JoinSession(0, NAME_GameSession, Result);
	if (!bStarted)
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		SetStatus(LOCTEXT("JoinFailNow", "No se pudo unir a la partida"), false);
	}
}

void UHTMSessionSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions)
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}
	FString Connect;
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions || !Sessions->GetResolvedConnectString(SessionName, Connect))
	{
		SetStatus(Result == EOnJoinSessionCompleteResult::SessionIsFull ? LOCTEXT("Full", "El taller esta lleno (4/4)")
		                                                               : LOCTEXT("JoinFail", "No se pudo unir a la partida"), false);
		return;
	}
	SetStatus(LOCTEXT("Travelling", "Conectando..."), false);
	if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
	{
		PC->ClientTravel(Connect, TRAVEL_Absolute);
	}
}

void UHTMSessionSubsystem::HandleInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		SetStatus(LOCTEXT("InviteFail", "La invitacion ya no es valida"), false);
		return;
	}
	bBusy = false;
	JoinResult(InviteResult);
}

// ============================================================================ Salir / invitar

void UHTMSessionSubsystem::LeaveSession()
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		if (Sessions->GetNamedSession(NAME_GameSession))
		{
			Sessions->DestroySession(NAME_GameSession);
		}
	}
	bBusy = false;
}

bool UHTMSessionSubsystem::ShowInviteUI()
{
	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	IOnlineExternalUIPtr UI = OSS ? OSS->GetExternalUIInterface() : nullptr;
	if (!UI.IsValid())
	{
		UE_LOG(LogHTM, Warning, TEXT("Invitar: la plataforma %s no tiene overlay de invitaciones"), *GetPlatformName().ToString());
		return false;
	}
	return UI->ShowInviteUI(0, NAME_GameSession);
}

#undef LOCTEXT_NAMESPACE
