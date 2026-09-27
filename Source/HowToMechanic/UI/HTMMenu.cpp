#include "UI/HTMMenu.h"
#include "Core/HTMGameInstance.h"
#include "Core/HTMPalette.h"
#include "Core/HTMSettings.h"
#include "Net/HTMSessionSubsystem.h"
#include "Progression/HTMSaveGame.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "HTMMenu"

namespace
{
	const FName BtnHostOnline(TEXT("HostOnline"));
	const FName BtnHostLAN(TEXT("HostLAN"));
	const FName BtnBrowseOnline(TEXT("BrowseOnline"));
	const FName BtnBrowseLAN(TEXT("BrowseLAN"));
	const FName BtnChaos(TEXT("Chaos"));
	const FName BtnPush(TEXT("Push"));
	const FName BtnNewGame(TEXT("NewGame"));
	const FName BtnQuit(TEXT("Quit"));
	const FName BtnBack(TEXT("Back"));
	const FName BtnRefresh(TEXT("Refresh"));
	const FString JoinPrefix(TEXT("Join_"));

	const FLinearColor Ink(0.035f, 0.03f, 0.045f, 1.f);

	UHTMGameInstance* GetHTMGameInstance(const UObject* Ctx)
	{
		return Ctx ? Cast<UHTMGameInstance>(UGameplayStatics::GetGameInstance(Ctx)) : nullptr;
	}

	UHTMSessionSubsystem* GetSessions(const UObject* Ctx)
	{
		UGameInstance* GI = Ctx ? UGameplayStatics::GetGameInstance(Ctx) : nullptr;
		return GI ? GI->GetSubsystem<UHTMSessionSubsystem>() : nullptr;
	}
}

// ============================================================================ GameMode / controlador

AHTMMenuGameMode::AHTMMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AHTMMenuPlayerController::StaticClass();
	HUDClass = AHTMMenuHUD::StaticClass();
}

AHTMMenuPlayerController::AHTMMenuPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AHTMMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
}

bool AHTMMenuPlayerController::InputKey(const FInputKeyParams& Params)
{
	const bool bHandled = Super::InputKey(Params);
	if (Params.Event != IE_Pressed)
	{
		return bHandled;
	}
	AHTMMenuHUD* Menu = Cast<AHTMMenuHUD>(GetHUD());
	if (!Menu)
	{
		return bHandled;
	}
	const FKey& Key = Params.Key;
	if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		Menu->MoveSelection(-1);
		return true;
	}
	if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		Menu->MoveSelection(1);
		return true;
	}
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		Menu->ActivateSelection();
		return true;
	}
	if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right)
	{
		Menu->Back();
		return true;
	}
	return bHandled;
}

// ============================================================================ HUD de menú

float AHTMMenuHUD::UIScale() const
{
	return Canvas ? FMath::Clamp(Canvas->ClipY / 1080.f, 0.6f, 2.f) : 1.f;
}

void AHTMMenuHUD::BuildButtons(TArray<FButton>& Out) const
{
	const UHTMGameInstance* GI = GetHTMGameInstance(this);
	const UHTMSessionSubsystem* Sessions = GetSessions(this);
	const bool bBusy = Sessions && Sessions->IsBusy();
	const bool bOnline = Sessions && Sessions->IsOnlinePlatformAvailable();

	if (Screen == EScreen::Main)
	{
		const bool bHasSave = UGameplayStatics::DoesSaveGameExist(UHTMSaveGame::SlotName(), 0);
		const bool bNew = GI && GI->bPendingNewGame;
		const FString HostWhat = bHasSave && !bNew ? LOCTEXT("Continue", "Continuar taller").ToString() : LOCTEXT("NewShop", "Abrir taller nuevo").ToString();
		Out.Add({ BtnHostOnline, HostWhat + (bOnline ? LOCTEXT("Online", " (online, invitar amigos)").ToString() : LOCTEXT("Local", " (local)").ToString()), !bBusy });
		Out.Add({ BtnHostLAN, HostWhat + LOCTEXT("LAN", " (LAN)").ToString(), !bBusy });
		Out.Add({ BtnBrowseOnline, LOCTEXT("Browse", "Buscar talleres de amigos").ToString(), !bBusy && bOnline });
		Out.Add({ BtnBrowseLAN, LOCTEXT("BrowseLAN", "Buscar talleres en LAN").ToString(), !bBusy });
		const bool bChaos = GI && GI->PendingRoomOptions.bChaoticPhysics;
		const bool bPush = !GI || GI->PendingRoomOptions.bFriendlyPush;
		Out.Add({ BtnChaos, FString::Printf(TEXT("%s: %s"), *LOCTEXT("ChaosOpt", "Fisica caotica").ToString(), bChaos ? TEXT("SI") : TEXT("NO")), true });
		Out.Add({ BtnPush, FString::Printf(TEXT("%s: %s"), *LOCTEXT("PushOpt", "Empujones entre amigos").ToString(), bPush ? TEXT("SI") : TEXT("NO")), true });
		if (bHasSave)
		{
			Out.Add({ BtnNewGame, FString::Printf(TEXT("%s: %s"), *LOCTEXT("NewGameOpt", "Empezar de cero (borra el taller guardado)").ToString(), bNew ? TEXT("SI") : TEXT("NO")), true });
		}
		Out.Add({ BtnQuit, LOCTEXT("Quit", "Salir").ToString(), true });
	}
	else
	{
		if (Sessions)
		{
			const TArray<FHTMSessionRow>& Rows = Sessions->GetResults();
			for (int32 i = 0; i < Rows.Num(); ++i)
			{
				const FHTMSessionRow& Row = Rows[i];
				Out.Add({ FName(*(JoinPrefix + FString::FromInt(i))),
					FString::Printf(TEXT("%s   %d/%d   %d ms%s"), *Row.HostName, Row.Players, Row.MaxPlayers, Row.PingMs,
						Row.bChaotic ? *LOCTEXT("RowChaos", "   [CAOS]").ToString() : TEXT("")),
					!bBusy && Row.Players < Row.MaxPlayers });
			}
		}
		Out.Add({ BtnRefresh, LOCTEXT("Refresh", "Buscar otra vez").ToString(), !bBusy });
		Out.Add({ BtnBack, LOCTEXT("Back", "Volver").ToString(), true });
	}
}

void AHTMMenuHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}
	const float S = UIScale();
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();

	// Fondo: pared menta y suelo gris cálido (paleta oficial), franja naranja de seguridad.
	DrawRect(HTMPalette::WallMint(), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY * 0.72f);
	DrawRect(HTMPalette::FloorWarmGrey(), 0.f, Canvas->ClipY * 0.72f, Canvas->ClipX, Canvas->ClipY * 0.28f);
	for (float X = 0.f; X < Canvas->ClipX; X += 80.f * S)
	{
		DrawRect(HTMPalette::SignalYellow(), X, Canvas->ClipY * 0.72f - 10.f * S, 40.f * S, 10.f * S);
		DrawRect(Ink, X + 40.f * S, Canvas->ClipY * 0.72f - 10.f * S, 40.f * S, 10.f * S);
	}

	// Título.
	const FString Title = TEXT("HOW TO MECHANIC");
	float TW = 0.f, TH = 0.f;
	GetTextSize(Title, TW, TH, Big, S * 3.f);
	const float TX = (Canvas->ClipX - TW) * 0.5f;
	const float TY = 60.f * S;
	DrawText(Title, Ink, TX + 5.f * S, TY + 5.f * S, Big, S * 3.f);
	DrawText(Title, HTMPalette::SafetyOrange(), TX, TY, Big, S * 3.f);
	const FString Sub = LOCTEXT("Subtitle", "Taller cutre cooperativo - 1 a 4 mecanicos").ToString();
	float SW = 0.f, SH = 0.f;
	GetTextSize(Sub, SW, SH, Mid, S * 1.2f);
	DrawText(Sub, Ink, (Canvas->ClipX - SW) * 0.5f, TY + TH + 8.f * S, Mid, S * 1.2f);

	// Botones.
	TArray<FButton> Buttons;
	BuildButtons(Buttons);
	LastButtonIds.Reset();
	for (const FButton& B : Buttons)
	{
		LastButtonIds.Add(B.Id);
	}
	Selected = Buttons.Num() > 0 ? FMath::Clamp(Selected, 0, Buttons.Num() - 1) : 0;

	const float BW = 620.f * S;
	const float BH = 46.f * S;
	const float Gap = 10.f * S;
	const float BX = (Canvas->ClipX - BW) * 0.5f;
	float BY = TY + TH + SH + 50.f * S;
	if (Screen == EScreen::Browse)
	{
		const FString Header = LOCTEXT("BrowseHeader", "Talleres abiertos").ToString();
		DrawText(Header, Ink, BX, BY, Big, S);
		BY += 44.f * S;
	}
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		const FButton& B = Buttons[i];
		const bool bSel = i == Selected;
		const FLinearColor Fill = !B.bEnabled ? FLinearColor(0.6f, 0.6f, 0.6f, 0.8f) : (bSel ? HTMPalette::SignalYellow() : HTMPalette::White());
		DrawRect(Ink, BX + 4.f * S, BY + 4.f * S, BW, BH);
		DrawRect(Fill, BX, BY, BW, BH);
		if (bSel)
		{
			DrawRect(HTMPalette::SafetyOrange(), BX, BY, 10.f * S, BH);
		}
		float LW = 0.f, LH = 0.f;
		GetTextSize(B.Label, LW, LH, Mid, S * 1.1f);
		DrawText(B.Label, B.bEnabled ? Ink : FLinearColor(0.3f, 0.3f, 0.3f), BX + 24.f * S, BY + (BH - LH) * 0.5f, Mid, S * 1.1f);
		if (B.bEnabled)
		{
			AddHitBox(FVector2D(BX, BY), FVector2D(BW, BH), B.Id, true);
		}
		BY += BH + Gap;
	}

	// Estado de la red y último error.
	FString StatusLine;
	FLinearColor StatusColor = Ink;
	if (const UHTMSessionSubsystem* Sessions = GetSessions(this))
	{
		StatusLine = Sessions->GetStatus().ToString();
		if (StatusLine.IsEmpty())
		{
			StatusLine = FText::Format(LOCTEXT("Platform", "Plataforma: {0}"), FText::FromName(Sessions->GetPlatformName())).ToString();
		}
	}
	if (const UHTMGameInstance* GI = GetHTMGameInstance(this))
	{
		if (!GI->LastErrorMessage.IsEmpty())
		{
			StatusLine = GI->LastErrorMessage.ToString();
			StatusColor = HTMPalette::ToolRed();
		}
	}
	float StW = 0.f, StH = 0.f;
	GetTextSize(StatusLine, StW, StH, Mid, S);
	DrawText(StatusLine, StatusColor, (Canvas->ClipX - StW) * 0.5f, Canvas->ClipY - 70.f * S, Mid, S);
	const FString Controls = LOCTEXT("Controls", "Raton o mando: arriba/abajo, A/Intro para aceptar, B/Esc para volver").ToString();
	GetTextSize(Controls, StW, StH, GEngine->GetSmallFont(), S);
	DrawText(Controls, Ink, (Canvas->ClipX - StW) * 0.5f, Canvas->ClipY - 36.f * S, GEngine->GetSmallFont(), S);
}

void AHTMMenuHUD::NotifyHitBoxBeginCursorOver(FName BoxName)
{
	const int32 Index = LastButtonIds.IndexOfByKey(BoxName);
	if (Index != INDEX_NONE)
	{
		Selected = Index;
	}
}

void AHTMMenuHUD::NotifyHitBoxClick(FName BoxName)
{
	Activate(BoxName);
}

void AHTMMenuHUD::MoveSelection(int32 Delta)
{
	if (LastButtonIds.Num() > 0)
	{
		Selected = (Selected + Delta + LastButtonIds.Num()) % LastButtonIds.Num();
	}
}

void AHTMMenuHUD::ActivateSelection()
{
	TArray<FButton> Buttons;
	BuildButtons(Buttons);
	if (Buttons.IsValidIndex(Selected) && Buttons[Selected].bEnabled)
	{
		Activate(Buttons[Selected].Id);
	}
}

void AHTMMenuHUD::Back()
{
	if (Screen == EScreen::Browse)
	{
		Screen = EScreen::Main;
		Selected = 0;
	}
}

void AHTMMenuHUD::Activate(FName Id)
{
	UHTMGameInstance* GI = GetHTMGameInstance(this);
	UHTMSessionSubsystem* Sessions = GetSessions(this);
	if (GI)
	{
		GI->LastErrorMessage = FText::GetEmpty();
	}

	if (Id == BtnHostOnline || Id == BtnHostLAN)
	{
		if (GI)
		{
			GI->bPendingLAN = Id == BtnHostLAN;
		}
		if (Sessions)
		{
			Sessions->HostSession(Id == BtnHostLAN);
		}
		else if (GI)
		{
			UGameplayStatics::OpenLevel(this, FName(*UHTMSettings::Get()->WorkshopMap), true, GI->BuildHostOptions());
		}
	}
	else if (Id == BtnBrowseOnline || Id == BtnBrowseLAN)
	{
		if (GI)
		{
			GI->bPendingLAN = Id == BtnBrowseLAN;
		}
		Screen = EScreen::Browse;
		Selected = 0;
		if (Sessions)
		{
			Sessions->FindSessions(Id == BtnBrowseLAN);
		}
	}
	else if (Id == BtnRefresh)
	{
		if (Sessions)
		{
			Sessions->FindSessions(GI && GI->bPendingLAN);
		}
	}
	else if (Id == BtnBack)
	{
		Back();
	}
	else if (Id == BtnChaos && GI)
	{
		GI->PendingRoomOptions.bChaoticPhysics = !GI->PendingRoomOptions.bChaoticPhysics;
	}
	else if (Id == BtnPush && GI)
	{
		GI->PendingRoomOptions.bFriendlyPush = !GI->PendingRoomOptions.bFriendlyPush;
	}
	else if (Id == BtnNewGame && GI)
	{
		GI->bPendingNewGame = !GI->bPendingNewGame;
	}
	else if (Id == BtnQuit)
	{
		UKismetSystemLibrary::QuitGame(this, PlayerOwner, EQuitPreference::Quit, false);
	}
	else if (Id.ToString().StartsWith(JoinPrefix))
	{
		const int32 Index = FCString::Atoi(*Id.ToString().RightChop(JoinPrefix.Len()));
		if (Sessions)
		{
			Sessions->JoinSession(Index);
		}
	}
}

#undef LOCTEXT_NAMESPACE
