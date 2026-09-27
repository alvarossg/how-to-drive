#include "UI/MechanicHUD.h"
#include "Core/HTMGameState.h"
#include "Core/HTMPalette.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMTypes.h"
#include "Character/MechanicCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Parts/CarPart.h"
#include "Vehicle/ModularCar.h"
#include "Vehicle/ModularVehicleMovement.h"
#include "Vehicle/EngineTemperatureComponent.h"
#include "Customers/JobTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "MechanicHUD"

namespace
{
	constexpr float ToastSeconds = 4.f;
	constexpr float EvaluationSeconds = 9.f;
	constexpr float BubbleSeconds = 2.5f;
	constexpr float MarkerRange = 1500.f;
	constexpr float LooseMarkerRange = 900.f;
	constexpr int32 MaxToasts = 5;

	const FLinearColor PanelFill(0.02f, 0.02f, 0.03f, 0.72f);
	const FLinearColor Ink(0.035f, 0.03f, 0.045f, 1.f);

	FString FormatClock(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}

	FLinearColor WithAlpha(FLinearColor C, float A)
	{
		C.A = A;
		return C;
	}
}

// ============================================================================ Utilidades

float AMechanicHUD::UIScale() const
{
	return Canvas ? FMath::Clamp(Canvas->ClipY / 1080.f, 0.6f, 2.f) : 1.f;
}

bool AMechanicHUD::WorldToScreen(const FVector& World, FVector2D& OutScreen) const
{
	if (!Canvas || !PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return false;
	}
	const FVector CamLoc = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FVector CamFwd = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
	if (FVector::DotProduct(World - CamLoc, CamFwd) <= 10.f)
	{
		return false; // detrás de la cámara
	}
	const FVector P = Canvas->Project(World);
	OutScreen = FVector2D(P.X, P.Y);
	return P.X >= -50.f && P.Y >= -50.f && P.X <= Canvas->ClipX + 50.f && P.Y <= Canvas->ClipY + 50.f;
}

void AMechanicHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border)
{
	DrawRect(Fill, X, Y, W, H);
	const float T = FMath::Max(1.f, 2.f * UIScale());
	DrawRect(Border, X, Y, W, T);
	DrawRect(Border, X, Y + H - T, W, T);
	DrawRect(Border, X, Y, T, H);
	DrawRect(Border, X + W - T, Y, T, H);
}

FVector2D AMechanicHUD::DrawLabel(const FString& Str, float X, float Y, const FLinearColor& InColor, UFont* Font, float Scale, float Align)
{
	if (Str.IsEmpty() || !Font)
	{
		return FVector2D::ZeroVector;
	}
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, Font, Scale);
	const float DrawX = X - W * Align;
	const float Shadow = FMath::Max(1.f, 2.f * UIScale());
	DrawText(Str, WithAlpha(Ink, InColor.A * 0.85f), DrawX + Shadow, Y + Shadow, Font, Scale);
	DrawText(Str, InColor, DrawX, Y, Font, Scale);
	return FVector2D(W, H);
}

void AMechanicHUD::DrawRing(const FVector2D& Center, float Radius, const FLinearColor& InColor, float Thickness, int32 Segments)
{
	FVector2D Prev = Center + FVector2D(Radius, 0.f);
	for (int32 i = 1; i <= Segments; ++i)
	{
		const float A = 2.f * PI * i / Segments;
		const FVector2D Next = Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius;
		DrawLine(Prev.X, Prev.Y, Next.X, Next.Y, InColor, Thickness);
		Prev = Next;
	}
}

void AMechanicHUD::DrawBar(float X, float Y, float W, float H, float Alpha, const FLinearColor& Fill)
{
	DrawRect(WithAlpha(Ink, 0.8f), X, Y, W, H);
	const float Pad = FMath::Max(1.f, 2.f * UIScale());
	DrawRect(Fill, X + Pad, Y + Pad, FMath::Max(0.f, (W - 2.f * Pad) * FMath::Clamp(Alpha, 0.f, 1.f)), H - 2.f * Pad);
}

// ============================================================================ Eventos

void AMechanicHUD::BindGameState()
{
	AHTMGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHTMGameState>() : nullptr;
	if (!GS || BoundGameState.Get() == GS)
	{
		return;
	}
	GS->OnToast.AddUObject(this, &AMechanicHUD::HandleToast);
	GS->OnEvaluation.AddUObject(this, &AMechanicHUD::HandleEvaluation);
	BoundGameState = GS;
	bBoundToGameState = true;
}

void AMechanicHUD::HandleToast(const FText& Text, const FLinearColor& InColor)
{
	FToast T;
	T.Text = Text;
	T.Color = InColor;
	T.StartTime = GetWorld()->GetRealTimeSeconds();
	Toasts.Add(T);
	while (Toasts.Num() > MaxToasts)
	{
		Toasts.RemoveAt(0);
	}
}

void AMechanicHUD::HandleEvaluation()
{
	EvaluationShownAt = GetWorld()->GetRealTimeSeconds();
}

// ============================================================================ Dibujo principal

void AMechanicHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}
	BindGameState();
	const AHTMGameState* GS = BoundGameState.Get();
	const AMechanicCharacter* Me = PlayerOwner ? Cast<AMechanicCharacter>(PlayerOwner->GetPawn()) : nullptr;

	if (Me)
	{
		DrawWorldMarkers(Me);
		DrawSnapGhost(Me);
		DrawBubbles(Me);
	}
	if (GS)
	{
		DrawTopBar(GS);
		DrawJobList(GS);
	}
	if (Me)
	{
		if (Me->GetMechanicState() == EMechanicState::Driving)
		{
			DrawDrivingPanel(Me);
		}
		else
		{
			DrawCrosshairAndPrompts(Me);
		}
		DrawStateOverlay(Me);
	}
	DrawToasts();
	if (GS)
	{
		if (GS->DayPhase == EDayPhase::Summary)
		{
			DrawDaySummary(GS);
		}
		else
		{
			DrawEvaluationPanel(GS);
		}
	}
}

void AMechanicHUD::DrawTopBar(const AHTMGameState* GS)
{
	const float S = UIScale();
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = 14.f * S;

	// Centro: día y reloj de jornada.
	FString DayStr;
	FLinearColor DayColor = HTMPalette::White();
	switch (GS->DayPhase)
	{
	case EDayPhase::Closed:
		DayStr = FText::Format(LOCTEXT("Closed", "Dia {0} - CERRADO (usa la caja para abrir)"), FText::AsNumber(GS->DayNumber + 1)).ToString();
		DayColor = HTMPalette::SignalYellow();
		break;
	case EDayPhase::Open:
		DayStr = FString::Printf(TEXT("%s %d   %s"), *LOCTEXT("Day", "Dia").ToString(), GS->DayNumber, *FormatClock(GS->GetDayTimeRemaining()));
		DayColor = GS->GetDayTimeRemaining() < 60.f ? HTMPalette::ToolRed() : HTMPalette::White();
		break;
	case EDayPhase::Summary:
		DayStr = FText::Format(LOCTEXT("SummaryTitle", "Fin del dia {0}"), FText::AsNumber(GS->DayNumber)).ToString();
		DayColor = HTMPalette::SignalYellow();
		break;
	}
	float W = 0.f, H = 0.f;
	GetTextSize(DayStr, W, H, Big, S);
	DrawPanel(CX - W * 0.5f - 16.f * S, Y - 6.f * S, W + 32.f * S, H + 12.f * S + (GS->DayPhase == EDayPhase::Open ? 8.f * S : 0.f), PanelFill, WithAlpha(HTMPalette::SafetyOrange(), 0.9f));
	DrawLabel(DayStr, CX, Y, DayColor, Big, S, 0.5f);
	if (GS->DayPhase == EDayPhase::Open)
	{
		const float Length = FMath::Max(1.f, UHTMTuningData::Get().DayLengthSeconds);
		DrawBar(CX - W * 0.5f, Y + H + 1.f * S, W, 7.f * S, GS->GetDayTimeRemaining() / Length, HTMPalette::SignalYellow());
	}

	// Derecha: dinero y reputación.
	const FString MoneyStr = FString::Printf(TEXT("%d EUR"), GS->Money);
	const FString RepStr = FText::Format(LOCTEXT("Rep", "Reputacion {0}"), FText::AsNumber(FMath::RoundToInt(GS->Reputation))).ToString();
	const float RX = Canvas->ClipX - 20.f * S;
	GetTextSize(MoneyStr, W, H, Big, S);
	DrawPanel(RX - FMath::Max(W, 170.f * S) - 16.f * S, Y - 6.f * S, FMath::Max(W, 170.f * S) + 24.f * S, H * 2.f + 14.f * S, PanelFill, WithAlpha(HTMPalette::CarLime(), 0.9f));
	DrawLabel(MoneyStr, RX, Y, GS->Money < 0 ? HTMPalette::ToolRed() : HTMPalette::CarLime(), Big, S, 1.f);
	DrawLabel(RepStr, RX, Y + H + 2.f * S, HTMPalette::SignalYellow(), Mid, S, 1.f);
	if (GS->RoomOptions.bChaoticPhysics)
	{
		DrawLabel(LOCTEXT("ChaosTag", "FISICA CAOTICA").ToString(), RX, Y + H * 2.f + 12.f * S, HTMPalette::SafetyOrange(), Mid, S, 1.f);
	}
}

void AMechanicHUD::DrawJobList(const AHTMGameState* GS)
{
	const float S = UIScale();
	UFont* Mid = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	float Y = 90.f * S;
	const float X = 18.f * S;
	const float PanelW = 330.f * S;
	const float Now = GS->GetServerWorldTimeSeconds();
	for (const FActiveJob& Job : GS->ActiveJobs)
	{
		if (Job.Outcome != EJobOutcome::Pending)
		{
			continue;
		}
		const int32 Lines = Job.RequirementLabels.Num();
		const float PanelH = (44.f + 17.f * Lines) * S;
		DrawPanel(X, Y, PanelW, PanelH, PanelFill, WithAlpha(HTMPalette::CarSky(), 0.9f));
		DrawLabel(FString::Printf(TEXT("%s  (%d EUR)"), *Job.CustomerName.ToString(), Job.Budget), X + 10.f * S, Y + 5.f * S, HTMPalette::White(), Mid, S);
		float LY = Y + 24.f * S;
		for (const FText& Label : Job.RequirementLabels)
		{
			DrawLabel(TEXT("- ") + Label.ToString(), X + 14.f * S, LY, HTMPalette::CarCream(), Small, S);
			LY += 17.f * S;
		}
		if (Job.DeadlineServerTime > 0.f)
		{
			const float Total = FMath::Max(1.f, Job.DeadlineServerTime - Job.StartServerTime);
			const float Left = FMath::Max(0.f, Job.DeadlineServerTime - Now);
			const float Alpha = Left / Total;
			DrawBar(X + 10.f * S, Y + PanelH - 12.f * S, PanelW - 20.f * S, 7.f * S, Alpha,
				Alpha < 0.25f ? HTMPalette::ToolRed() : (Alpha < 0.5f ? HTMPalette::SignalYellow() : HTMPalette::CarLime()));
		}
		Y += PanelH + 8.f * S;
		if (Y > Canvas->ClipY * 0.6f)
		{
			break;
		}
	}
}

void AMechanicHUD::DrawCrosshairAndPrompts(const AMechanicCharacter* Me)
{
	const UInteractionComponent* Interaction = Me->GetInteraction();
	if (!Interaction || !Me->CanAct())
	{
		return;
	}
	const float S = UIScale();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const bool bFocus = Interaction->GetFocusedActor() != nullptr;
	DrawRing(FVector2D(CX, CY), (bFocus ? 7.f : 3.f) * S, bFocus ? HTMPalette::SignalYellow() : WithAlpha(HTMPalette::White(), 0.7f), 2.f * S, 12);

	// Avisos por verbo: "[E / X] Coger rueda".
	UFont* Mid = GEngine->GetMediumFont();
	float Y = CY + 28.f * S;
	static const EInteractionVerb Verbs[] = { EInteractionVerb::Grab, EInteractionVerb::Use, EInteractionVerb::AltUse, EInteractionVerb::Enter };
	for (EInteractionVerb Verb : Verbs)
	{
		const FText Prompt = Interaction->GetPromptText(Verb);
		if (Prompt.IsEmpty())
		{
			continue;
		}
		const FString Line = FString::Printf(TEXT("%s %s"), *HTM::VerbKeyHint(Verb).ToString(), *Prompt.ToString());
		const FVector2D Size = DrawLabel(Line, CX, Y, HTMPalette::White(), Mid, S, 0.5f);
		Y += Size.Y + 3.f * S;
	}

	// Barra de "mantener" (apretar, bombear, liberar...). HoldAlpha llega replicado al dueño.
	const float Hold = Interaction->GetHoldAlpha();
	if (Hold > 0.f)
	{
		const float BW = 160.f * S;
		DrawBar(CX - BW * 0.5f, CY - 34.f * S, BW, 12.f * S, Hold, HTMPalette::SafetyOrange());
	}
}

void AMechanicHUD::DrawWorldMarkers(const AMechanicCharacter* Me)
{
	const float S = UIScale();
	UFont* Small = GEngine->GetSmallFont();
	const FVector Eye = PlayerOwner && PlayerOwner->PlayerCameraManager ? PlayerOwner->PlayerCameraManager->GetCameraLocation() : Me->GetActorLocation();
	const float Now = GetWorld()->GetTimeSeconds();

	// Piezas: avería revelada (círculo rojo "!") y flojas (tornillo amarillo parpadeando).
	for (TActorIterator<ACarPart> It(GetWorld()); It; ++It)
	{
		const ACarPart* Part = *It;
		const float Dist = FVector::Dist(Part->GetActorLocation(), Eye);
		if (Dist > MarkerRange)
		{
			continue;
		}
		FVector2D P;
		if (!WorldToScreen(Part->GetActorLocation() + FVector(0.f, 0.f, 25.f), P))
		{
			continue;
		}
		if (Part->IsFaultRevealed() && Part->GetHiddenFault() != EHiddenFault::None)
		{
			const float R = 11.f * S;
			DrawRect(WithAlpha(HTMPalette::ToolRed(), 0.9f), P.X - R, P.Y - R, 2.f * R, 2.f * R);
			DrawLabel(TEXT("!"), P.X, P.Y - R + 1.f * S, HTMPalette::White(), GEngine->GetMediumFont(), S, 0.5f);
			DrawLabel(HTM::FaultName(Part->GetHiddenFault()).ToString(), P.X, P.Y + R + 2.f * S, HTMPalette::ToolRed(), Small, S, 0.5f);
		}
		else if (Part->IsBroken() && Dist < LooseMarkerRange)
		{
			DrawLabel(TEXT("X"), P.X, P.Y - 8.f * S, HTMPalette::ToolRed(), GEngine->GetMediumFont(), S, 0.5f);
		}
		if (Part->IsLoose() && Dist < LooseMarkerRange)
		{
			const float Blink = 0.55f + 0.45f * FMath::Sin(Now * 8.f);
			DrawRing(P + FVector2D(18.f * S, 0.f), 7.f * S, WithAlpha(HTMPalette::SignalYellow(), Blink), 3.f * S, 8);
			DrawLabel(FString::Printf(TEXT("%d/%d"), Part->GetBoltsTight(), Part->GetBoltCount()), P.X + 30.f * S, P.Y - 7.f * S,
				WithAlpha(HTMPalette::SignalYellow(), Blink), Small, S);
		}
	}

	// Coches: nombre, humo/temperatura y "volcado".
	for (TActorIterator<AModularCar> It(GetWorld()); It; ++It)
	{
		const AModularCar* Car = *It;
		if (Car == Me->GetCurrentCar() || FVector::Dist(Car->GetActorLocation(), Eye) > MarkerRange * 1.5f)
		{
			continue;
		}
		FVector2D P;
		if (!WorldToScreen(Car->GetActorLocation() + FVector(0.f, 0.f, 190.f), P))
		{
			continue;
		}
		FLinearColor NameColor = HTMPalette::White();
		FString Extra;
		if (const UEngineTemperatureComponent* Temp = Car->GetTemperature())
		{
			if (Temp->IsSmoking()) { NameColor = HTMPalette::ToolRed(); Extra = LOCTEXT("Hot", "  [MOTOR ARDIENDO]").ToString(); }
			else if (Temp->IsWarning()) { NameColor = HTMPalette::SafetyOrange(); Extra = LOCTEXT("Warm", "  [CALIENTE]").ToString(); }
		}
		if (Car->IsFlipped())
		{
			Extra += LOCTEXT("Flipped", "  [VOLCADO: empujad entre varios]").ToString();
		}
		DrawLabel(Car->GetCarName().ToString() + Extra, P.X, P.Y, WithAlpha(NameColor, 0.9f), Small, S, 0.5f);
	}

	// Nombres de compañeros.
	for (TActorIterator<AMechanicCharacter> It(GetWorld()); It; ++It)
	{
		const AMechanicCharacter* Other = *It;
		if (Other == Me)
		{
			continue;
		}
		FVector2D P;
		if (WorldToScreen(Other->GetActorLocation() + FVector(0.f, 0.f, 95.f), P))
		{
			DrawLabel(Other->GetPlayerNameSafe(), P.X, P.Y, WithAlpha(HTMPalette::CarSky(), 0.85f), Small, S, 0.5f);
		}
	}
}

void AMechanicHUD::DrawSnapGhost(const AMechanicCharacter* Me)
{
	const UInteractionComponent* Interaction = Me->GetInteraction();
	const ACarPart* Held = Interaction ? Cast<ACarPart>(Interaction->GetHeldObject()) : nullptr;
	if (!Held)
	{
		return;
	}
	const float S = UIScale();
	const float SnapRadius = UHTMTuningData::Get().SnapRadius;
	const AModularCar* BestCar = nullptr;
	FName BestSlot;
	bool bBestReversed = false;
	float BestDist = SnapRadius;
	for (TActorIterator<AModularCar> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), Held->GetActorLocation()) > 600.f)
		{
			continue;
		}
		FName Slot;
		bool bReversed = false;
		float Dist = 0.f;
		if (It->FindSnapSlot(Held, Slot, bReversed, Dist) && Dist < BestDist)
		{
			BestCar = *It;
			BestSlot = Slot;
			bBestReversed = bReversed;
			BestDist = Dist;
		}
	}
	FVector2D P;
	if (!BestCar || !WorldToScreen(BestCar->GetSlotWorldLocation(BestSlot), P))
	{
		return;
	}
	const float Pulse = 0.6f + 0.4f * FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f);
	const FLinearColor C = bBestReversed ? HTMPalette::SafetyOrange() : HTMPalette::CarLime();
	DrawRing(P, 22.f * S, WithAlpha(C, Pulse), 3.f * S, 24);
	DrawRing(P, 12.f * S, WithAlpha(C, Pulse * 0.6f), 2.f * S, 16);
	DrawLabel(bBestReversed ? LOCTEXT("Reversed", "Suelta: encaja... AL REVES").ToString() : LOCTEXT("Snap", "Suelta para encajar").ToString(),
		P.X, P.Y + 26.f * S, C, GEngine->GetSmallFont(), S, 0.5f);
}

void AMechanicHUD::DrawBubbles(const AMechanicCharacter* Me)
{
	const float S = UIScale();
	UFont* Mid = GEngine->GetMediumFont();
	const float Now = GetWorld()->GetTimeSeconds();
	for (TActorIterator<AMechanicCharacter> It(GetWorld()); It; ++It)
	{
		const AMechanicCharacter* Char = *It;
		const float Age = Now - Char->GetLastShoutTime();
		if (Char->GetLastShoutTime() <= 0.f || Age > BubbleSeconds || Char->GetBubbleText().IsEmpty())
		{
			continue;
		}
		FVector2D P;
		if (!WorldToScreen(Char->GetActorLocation() + FVector(0.f, 0.f, 130.f), P))
		{
			continue;
		}
		const FString Str = Char->GetBubbleText().ToString();
		const float Pop = FMath::Min(1.f, Age * 8.f); // aparece de golpe, estilo cómic
		const float Scale = S * (1.1f + 0.25f * (1.f - Pop));
		float W = 0.f, H = 0.f;
		GetTextSize(Str, W, H, Mid, Scale);
		const float Fade = Age > BubbleSeconds - 0.4f ? (BubbleSeconds - Age) / 0.4f : 1.f;
		DrawPanel(P.X - W * 0.5f - 10.f * S, P.Y - H - 8.f * S, W + 20.f * S, H + 10.f * S, WithAlpha(HTMPalette::White(), 0.95f * Fade), WithAlpha(Ink, Fade));
		DrawRect(WithAlpha(HTMPalette::White(), 0.95f * Fade), P.X - 5.f * S, P.Y + 2.f * S, 10.f * S, 8.f * S); // rabito
		DrawText(Str, WithAlpha(Ink, Fade), P.X - W * 0.5f, P.Y - H - 3.f * S, Mid, Scale);
	}
}

void AMechanicHUD::DrawStateOverlay(const AMechanicCharacter* Me)
{
	const float S = UIScale();
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.68f;
	switch (Me->GetMechanicState())
	{
	case EMechanicState::KnockedOut:
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.25f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		DrawLabel(LOCTEXT("KO", "K.O.").ToString(), CX, Y, HTMPalette::SignalYellow(), Big, S * 2.f, 0.5f);
		DrawLabel(LOCTEXT("KOHint", "Te levantas en un momento...").ToString(), CX, Y + 48.f * S, HTMPalette::White(), Mid, S, 0.5f);
		break;
	case EMechanicState::Trapped:
		DrawLabel(LOCTEXT("Trapped", "ATRAPADO").ToString(), CX, Y, HTMPalette::ToolRed(), Big, S * 1.6f, 0.5f);
		DrawLabel(LOCTEXT("TrappedHint", "Grita [T / LB] para que un companero te saque").ToString(), CX, Y + 40.f * S, HTMPalette::White(), Mid, S, 0.5f);
		break;
	case EMechanicState::Stumbling:
		DrawLabel(TEXT("!!"), CX, Y, HTMPalette::SignalYellow(), Big, S * 1.4f, 0.5f);
		break;
	default:
		break;
	}
	if (Me->IsWet())
	{
		DrawLabel(LOCTEXT("Wet", "Suelo mojado: resbala").ToString(), CX, Canvas->ClipY - 60.f * S, HTMPalette::Water(), Mid, S, 0.5f);
	}
}

void AMechanicHUD::DrawDrivingPanel(const AMechanicCharacter* Me)
{
	const AModularCar* Car = Me->GetCurrentCar();
	if (!Car)
	{
		return;
	}
	const float S = UIScale();
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	const float PW = 280.f * S;
	const float PH = 128.f * S;
	const float X = Canvas->ClipX - PW - 24.f * S;
	const float Y = Canvas->ClipY - PH - 24.f * S;
	DrawPanel(X, Y, PW, PH, PanelFill, WithAlpha(HTMPalette::SafetyOrange(), 0.9f));

	const float Kmh = Car->GetVehicleMovement() ? FMath::Abs(Car->GetVehicleMovement()->GetSpeedKmh()) : 0.f;
	DrawLabel(FString::Printf(TEXT("%d km/h"), FMath::RoundToInt(Kmh)), X + 14.f * S, Y + 8.f * S, HTMPalette::White(), Big, S * 1.3f);

	if (const UEngineTemperatureComponent* Temp = Car->GetTemperature())
	{
		const UHTMTuningData& T = UHTMTuningData::Get();
		const float Alpha = (Temp->GetTemperature() - T.AmbientTemp) / FMath::Max(1.f, T.FireTemp - T.AmbientTemp);
		const FLinearColor C = Temp->IsSmoking() ? HTMPalette::ToolRed() : (Temp->IsWarning() ? HTMPalette::SafetyOrange() : HTMPalette::CarSky());
		DrawLabel(LOCTEXT("TempLabel", "Temp").ToString(), X + 14.f * S, Y + 58.f * S, C, Mid, S);
		DrawBar(X + 70.f * S, Y + 62.f * S, PW - 86.f * S, 10.f * S, Alpha, C);
	}
	FString Status;
	FLinearColor StatusColor = HTMPalette::CarLime();
	if (!Car->IsEngineRunning())
	{
		Status = LOCTEXT("EngineOff", "Motor apagado: acelera para dar al contacto").ToString();
		StatusColor = HTMPalette::SignalYellow();
	}
	else if (Car->IsHandbrakeOn())
	{
		Status = LOCTEXT("Handbrake", "FRENO DE MANO puesto: pulsa freno de mano para quitarlo").ToString();
		StatusColor = HTMPalette::ToolRed();
	}
	else if (Car->IsFlipped())
	{
		Status = LOCTEXT("FlippedDrive", "Volcado: sal y dale la vuelta").ToString();
		StatusColor = HTMPalette::ToolRed();
	}
	DrawLabel(Status, X + 14.f * S, Y + 90.f * S, StatusColor, GEngine->GetSmallFont(), S);
	if (Car->GetBestTopSpeedKmh() > 0.f)
	{
		DrawLabel(FString::Printf(TEXT("max %d"), FMath::RoundToInt(Car->GetBestTopSpeedKmh())), X + PW - 14.f * S, Y + 14.f * S,
			WithAlpha(HTMPalette::White(), 0.7f), GEngine->GetSmallFont(), S, 1.f);
	}
}

void AMechanicHUD::DrawToasts()
{
	const float S = UIScale();
	UFont* Mid = GEngine->GetMediumFont();
	const float Now = GetWorld()->GetRealTimeSeconds();
	Toasts.RemoveAll([Now](const FToast& T) { return Now - T.StartTime > ToastSeconds; });
	float Y = Canvas->ClipY * 0.22f;
	for (const FToast& T : Toasts)
	{
		const float Age = Now - T.StartTime;
		const float Fade = Age > ToastSeconds - 0.6f ? (ToastSeconds - Age) / 0.6f : 1.f;
		const FString Str = T.Text.ToString();
		float W = 0.f, H = 0.f;
		GetTextSize(Str, W, H, Mid, S * 1.1f);
		const float CX = Canvas->ClipX * 0.5f;
		DrawPanel(CX - W * 0.5f - 14.f * S, Y - 4.f * S, W + 28.f * S, H + 8.f * S, WithAlpha(PanelFill, PanelFill.A * Fade), WithAlpha(T.Color, Fade));
		DrawLabel(Str, CX, Y, WithAlpha(T.Color, Fade), Mid, S * 1.1f, 0.5f);
		Y += H + 14.f * S;
	}
}

void AMechanicHUD::DrawEvaluationPanel(const AHTMGameState* GS)
{
	const FJobEvaluation& Eval = GS->LastEvaluation;
	if (Eval.JobUid == 0 || GetWorld()->GetRealTimeSeconds() - EvaluationShownAt > EvaluationSeconds)
	{
		return;
	}
	const float S = UIScale();
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	const int32 Lines = Eval.Labels.Num();
	const float PW = 420.f * S;
	const float PH = (96.f + 24.f * Lines) * S;
	const float X = Canvas->ClipX - PW - 24.f * S;
	const float Y = Canvas->ClipY * 0.3f;

	FLinearColor Border = HTMPalette::CarLime();
	FText Verdict = LOCTEXT("Full", "PAGO COMPLETO");
	if (Eval.Outcome == EJobOutcome::PartialPay) { Border = HTMPalette::SignalYellow(); Verdict = LOCTEXT("Partial", "PAGO PARCIAL"); }
	if (Eval.Outcome == EJobOutcome::Angry) { Border = HTMPalette::ToolRed(); Verdict = LOCTEXT("Angry", "CLIENTE ENFADADO"); }
	DrawPanel(X, Y, PW, PH, PanelFill, Border);
	DrawLabel(Eval.CustomerName.ToString(), X + 14.f * S, Y + 8.f * S, HTMPalette::White(), Mid, S);
	DrawLabel(Verdict.ToString(), X + 14.f * S, Y + 30.f * S, Border, Big, S);

	float LY = Y + 66.f * S;
	for (int32 i = 0; i < Lines; ++i)
	{
		const bool bOk = Eval.Passed.IsValidIndex(i) && Eval.Passed[i];
		const FLinearColor C = bOk ? HTMPalette::CarLime() : HTMPalette::ToolRed();
		DrawRect(C, X + 16.f * S, LY + 3.f * S, 14.f * S, 14.f * S);
		DrawLabel(bOk ? TEXT("OK") : TEXT("X"), X + 38.f * S, LY, C, Mid, S);
		DrawLabel(Eval.Labels[i].ToString(), X + 76.f * S, LY, HTMPalette::White(), Mid, S);
		LY += 24.f * S;
	}
	const FString Pay = FString::Printf(TEXT("%+d EUR   %+.0f rep"), Eval.Payment, Eval.ReputationDelta);
	DrawLabel(Pay, X + PW - 14.f * S, Y + PH - 26.f * S, Border, Mid, S, 1.f);
}

void AMechanicHUD::DrawDaySummary(const AHTMGameState* GS)
{
	const FHTMDaySummary& Sum = GS->LastDaySummary;
	const float S = UIScale();
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	const float PW = 620.f * S;
	const float PH = (330.f + 26.f * Sum.Disasters.Num()) * S;
	const float X = (Canvas->ClipX - PW) * 0.5f;
	const float Y = FMath::Max(80.f * S, (Canvas->ClipY - PH) * 0.5f);
	DrawPanel(X, Y, PW, PH, FLinearColor(0.02f, 0.02f, 0.03f, 0.86f), HTMPalette::SafetyOrange());

	float LY = Y + 14.f * S;
	DrawLabel(FText::Format(LOCTEXT("SumTitle", "RESUMEN DEL DIA {0}"), FText::AsNumber(Sum.Day)).ToString(), X + PW * 0.5f, LY, HTMPalette::SignalYellow(), Big, S * 1.2f, 0.5f);
	LY += 52.f * S;

	auto Row = [&](const FString& Label, const FString& Value, const FLinearColor& C)
	{
		DrawLabel(Label, X + 24.f * S, LY, HTMPalette::White(), Mid, S);
		DrawLabel(Value, X + PW - 24.f * S, LY, C, Mid, S, 1.f);
		LY += 26.f * S;
	};
	const int32 NetMoney = Sum.MoneyEarned - Sum.MoneySpent;
	Row(LOCTEXT("Earned", "Ingresos").ToString(), FString::Printf(TEXT("+%d EUR"), Sum.MoneyEarned), HTMPalette::CarLime());
	Row(LOCTEXT("Spent", "Gastos").ToString(), FString::Printf(TEXT("-%d EUR"), Sum.MoneySpent), HTMPalette::ToolRed());
	Row(LOCTEXT("Net", "Balance").ToString(), FString::Printf(TEXT("%+d EUR"), NetMoney), NetMoney >= 0 ? HTMPalette::CarLime() : HTMPalette::ToolRed());
	Row(LOCTEXT("RepDelta", "Reputacion").ToString(), FString::Printf(TEXT("%+.0f"), Sum.ReputationDelta), HTMPalette::SignalYellow());
	Row(LOCTEXT("Jobs", "Encargos (completo / parcial / enfadados)").ToString(),
		FString::Printf(TEXT("%d / %d / %d"), Sum.JobsFull, Sum.JobsPartial, Sum.JobsAngry), HTMPalette::White());
	if (Sum.LostItemsRecovered > 0)
	{
		Row(LOCTEXT("Lost", "Objetos perdidos recogidos por la grua").ToString(),
			FString::Printf(TEXT("%d (-%d EUR)"), Sum.LostItemsRecovered, Sum.LostItemsFee), HTMPalette::SafetyOrange());
	}
	Row(LOCTEXT("Situations", "Situaciones de caos distintas").ToString(), FString::FromInt(Sum.DistinctSituations), HTMPalette::CarSky());

	if (Sum.Disasters.Num() > 0)
	{
		LY += 8.f * S;
		DrawLabel(LOCTEXT("Disasters", "Desastres del dia").ToString(), X + 24.f * S, LY, HTMPalette::SafetyOrange(), Mid, S * 1.1f);
		LY += 30.f * S;
		int32 Rank = 1;
		for (const FHTMDisaster& D : Sum.Disasters)
		{
			const FString Line = D.Who.IsEmpty()
				? FString::Printf(TEXT("%d. %s"), Rank, *D.Description.ToString())
				: FString::Printf(TEXT("%d. %s (%s)"), Rank, *D.Description.ToString(), *D.Who);
			DrawLabel(Line, X + 32.f * S, LY, HTMPalette::White(), Mid, S);
			LY += 26.f * S;
			++Rank;
		}
	}
	DrawLabel(LOCTEXT("Continue", "Usad la caja registradora para preparar el dia siguiente").ToString(),
		X + PW * 0.5f, Y + PH - 30.f * S, HTMPalette::CarCream(), Mid, S, 0.5f);
}

#undef LOCTEXT_NAMESPACE
