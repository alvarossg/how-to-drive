#include "Core/HTMPlayerController.h"
#include "Core/HTMGameMode.h"
#include "Core/HTMGameState.h"
#include "Core/HTMPalette.h"
#include "Core/HTMLog.h"
#include "Character/MechanicCharacter.h"
#include "Customers/JobDirectorComponent.h"
#include "Net/HTMSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "HTMPlayerController"

void AHTMPlayerController::ClientToast_Implementation(const FText& Text, FLinearColor Color)
{
	if (AHTMGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHTMGameState>() : nullptr)
	{
		GS->OnToast.Broadcast(Text, Color);
	}
}

void AHTMPlayerController::HTMHelp()
{
	const TCHAR* Lines[] = {
		TEXT("HTMStartDay / HTMEndDay           - abrir o cerrar la jornada"),
		TEXT("HTMSpawnCustomer [JobId]          - llega un cliente ya"),
		TEXT("HTMSpawnCar [ModelId]             - coche delante de ti"),
		TEXT("HTMSpawnPart PartId               - pieza delante de ti"),
		TEXT("HTMGiveMoney N                    - dinero para probar"),
		TEXT("HTMUpgrade                        - ampliar el taller (fuera de jornada)"),
		TEXT("HTMChaos 0/1 / HTMFriendlyPush 0/1 - opciones de sala"),
		TEXT("HTMSave / HTMResetSave            - guardado del anfitrion"),
		TEXT("HTMInvite                         - invitar amigos (Steam)"),
		TEXT("htm.Outline 0/1                   - contorno A/B"),
		TEXT("htm.PhysicsBudget.Stats 1         - cuerpos simulando"),
	};
	for (const TCHAR* Line : Lines)
	{
		ClientMessage(Line);
		UE_LOG(LogHTM, Display, TEXT("%s"), Line);
	}
}

void AHTMPlayerController::HTMStartDay() { ServerDebugCommand(TEXT("StartDay"), FString()); }
void AHTMPlayerController::HTMEndDay() { ServerDebugCommand(TEXT("EndDay"), FString()); }
void AHTMPlayerController::HTMSpawnCustomer(FName JobId) { ServerDebugCommand(TEXT("SpawnCustomer"), JobId.ToString()); }
void AHTMPlayerController::HTMSpawnCar(FName ModelId) { ServerDebugCommand(TEXT("SpawnCar"), ModelId.ToString()); }
void AHTMPlayerController::HTMSpawnPart(FName PartId) { ServerDebugCommand(TEXT("SpawnPart"), PartId.ToString()); }
void AHTMPlayerController::HTMGiveMoney(int32 Amount) { ServerDebugCommand(TEXT("GiveMoney"), FString::FromInt(Amount)); }
void AHTMPlayerController::HTMUpgrade() { ServerDebugCommand(TEXT("Upgrade"), FString()); }
void AHTMPlayerController::HTMChaos(int32 bEnabled) { ServerDebugCommand(TEXT("Chaos"), FString::FromInt(bEnabled)); }
void AHTMPlayerController::HTMFriendlyPush(int32 bEnabled) { ServerDebugCommand(TEXT("FriendlyPush"), FString::FromInt(bEnabled)); }
void AHTMPlayerController::HTMSave() { ServerDebugCommand(TEXT("Save"), FString()); }
void AHTMPlayerController::HTMResetSave() { ServerDebugCommand(TEXT("ResetSave"), FString()); }

void AHTMPlayerController::HTMInvite()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UHTMSessionSubsystem* Sessions = GI->GetSubsystem<UHTMSessionSubsystem>())
		{
			Sessions->ShowInviteUI();
		}
	}
}

void AHTMPlayerController::ServerDebugCommand_Implementation(const FString& Command, const FString& Arg)
{
	// Solo el anfitrión (controlador local en el servidor) puede tocar el estado con la consola.
	if (!IsLocalController())
	{
		UE_LOG(LogHTM, Warning, TEXT("Comando %s rechazado: solo el anfitrión puede usar la consola de depuración"), *Command);
		return;
	}
	AHTMGameMode* GM = GetWorld()->GetAuthGameMode<AHTMGameMode>();
	AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	AMechanicCharacter* Me = Cast<AMechanicCharacter>(GetPawn());
	if (!GM || !GS)
	{
		return;
	}
	UE_LOG(LogHTM, Display, TEXT("Debug: %s %s"), *Command, *Arg);

	if (Command == TEXT("StartDay"))
	{
		if (GS->DayPhase != EDayPhase::Open)
		{
			if (GS->DayPhase == EDayPhase::Summary)
			{
				GM->PrepareNextDay();
			}
			GM->StartDay();
		}
	}
	else if (Command == TEXT("EndDay"))
	{
		if (GS->DayPhase == EDayPhase::Open)
		{
			GM->EndDay();
		}
	}
	else if (Command == TEXT("SpawnCustomer"))
	{
		if (UJobDirectorComponent* Director = GM->GetJobDirector())
		{
			Director->SpawnCustomer(Arg.IsEmpty() || Arg == TEXT("None") ? NAME_None : FName(*Arg));
		}
	}
	else if (Command == TEXT("SpawnCar"))
	{
		GM->DebugSpawnCar(Arg.IsEmpty() || Arg == TEXT("None") ? NAME_None : FName(*Arg), Me);
	}
	else if (Command == TEXT("SpawnPart"))
	{
		GM->DebugSpawnPart(FName(*Arg), Me);
	}
	else if (Command == TEXT("GiveMoney"))
	{
		GS->AddMoney(FCString::Atoi(*Arg), LOCTEXT("Cheat", "Consola"));
	}
	else if (Command == TEXT("Upgrade"))
	{
		GM->UpgradeWorkshop(Me);
	}
	else if (Command == TEXT("Chaos"))
	{
		GS->RoomOptions.bChaoticPhysics = FCString::Atoi(*Arg) != 0;
		GS->MulticastToast(GS->RoomOptions.bChaoticPhysics ? LOCTEXT("ChaosOn", "Fisica CAOTICA activada") : LOCTEXT("ChaosOff", "Fisica normal"),
			HTMPalette::SignalYellow());
	}
	else if (Command == TEXT("FriendlyPush"))
	{
		GS->RoomOptions.bFriendlyPush = FCString::Atoi(*Arg) != 0;
		GS->MulticastToast(GS->RoomOptions.bFriendlyPush ? LOCTEXT("PushOn", "Empujones entre amigos: SI") : LOCTEXT("PushOff", "Empujones entre amigos: NO"),
			HTMPalette::SignalYellow());
	}
	else if (Command == TEXT("Save"))
	{
		GM->SaveWorkshop();
		ClientToast(LOCTEXT("Saved", "Taller guardado"), HTMPalette::CarLime());
	}
	else if (Command == TEXT("ResetSave"))
	{
		GM->ResetSave();
		ClientToast(LOCTEXT("Reset", "Guardado borrado (se nota al reabrir la partida)"), HTMPalette::ToolRed());
	}
}

#undef LOCTEXT_NAMESPACE
