#include "Core/HTMGameInstance.h"
#include "Core/HTMSettings.h"
#include "Core/HTMLog.h"
#include "Net/HTMSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "HTMGameInstance"

void UHTMGameInstance::Init()
{
	Super::Init();
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UHTMGameInstance::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UHTMGameInstance::HandleTravelFailure);
	}
}

void UHTMGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	Super::Shutdown();
}

FString UHTMGameInstance::BuildHostOptions() const
{
	FString Options = TEXT("listen");
	Options += FString::Printf(TEXT("?Chaotic=%d"), PendingRoomOptions.bChaoticPhysics ? 1 : 0);
	Options += FString::Printf(TEXT("?FriendlyPush=%d"), PendingRoomOptions.bFriendlyPush ? 1 : 0);
	if (bPendingNewGame)
	{
		Options += TEXT("?NewGame=1");
	}
	if (bPendingLAN)
	{
		Options += TEXT("?bIsLanMatch=1");
	}
	return Options;
}

void UHTMGameInstance::ReturnToMainMenuWithError(const FText& Error)
{
	LastErrorMessage = Error;
	if (UHTMSessionSubsystem* Sessions = GetSubsystem<UHTMSessionSubsystem>())
	{
		Sessions->LeaveSession();
	}
	UGameplayStatics::OpenLevel(this, FName(*UHTMSettings::Get()->MainMenuMap));
}

void UHTMGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogHTM, Warning, TEXT("Fallo de red (%s): %s"), ENetworkFailure::ToString(FailureType), *ErrorString);
	LastErrorMessage = FailureType == ENetworkFailure::ConnectionLost
		? LOCTEXT("Lost", "Se ha perdido la conexion con el anfitrion.")
		: FText::Format(LOCTEXT("NetFail", "Error de red: {0}"), FText::FromString(ErrorString));
	// El motor ya vuelve al mapa por defecto (L_MainMenu); aquí solo cerramos la sesión online.
	if (UHTMSessionSubsystem* Sessions = GetSubsystem<UHTMSessionSubsystem>())
	{
		Sessions->LeaveSession();
	}
}

void UHTMGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogHTM, Warning, TEXT("Fallo de viaje (%s): %s"), ETravelFailure::ToString(FailureType), *ErrorString);
	LastErrorMessage = FText::Format(LOCTEXT("TravelFail", "No se pudo entrar en la partida: {0}"), FText::FromString(ErrorString));
	if (UHTMSessionSubsystem* Sessions = GetSubsystem<UHTMSessionSubsystem>())
	{
		Sessions->LeaveSession();
	}
}

#undef LOCTEXT_NAMESPACE
