#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MechanicHUD.generated.h"

class AHTMGameState;
class AMechanicCharacter;
class UFont;

/**
 * HUD de juego dibujado con Canvas (sin assets): placeholder funcional hasta que existan los WBP_.
 * Todo lo importante se entiende con iconos y colores (principio 4); el texto acompaña.
 *
 *  - Centro: punto de mira, avisos de interacción por verbo y barra de "mantener".
 *  - Arriba: día, reloj de jornada, dinero, reputación. Izquierda: encargos activos con su plazo.
 *  - Mundo: bocadillos (gritos), nombres de compañeros, averías reveladas y piezas flojas,
 *    y fantasma del hueco donde encajará la pieza que llevas (avisa si quedará AL REVÉS).
 *  - Estados: KO, atrapado, conduciendo (velocímetro, temperatura, freno de mano).
 *  - Paneles: checklist de la última entrega y resumen del día.
 */
UCLASS()
class HOWTOMECHANIC_API AMechanicHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	struct FToast
	{
		FText Text;
		FLinearColor Color;
		float StartTime = 0.f;
	};

	void BindGameState();
	void HandleToast(const FText& Text, const FLinearColor& InColor);
	void HandleEvaluation();

	// Bloques de dibujo.
	void DrawTopBar(const AHTMGameState* GS);
	void DrawJobList(const AHTMGameState* GS);
	void DrawCrosshairAndPrompts(const AMechanicCharacter* Me);
	void DrawWorldMarkers(const AMechanicCharacter* Me);
	void DrawSnapGhost(const AMechanicCharacter* Me);
	void DrawBubbles(const AMechanicCharacter* Me);
	void DrawStateOverlay(const AMechanicCharacter* Me);
	void DrawDrivingPanel(const AMechanicCharacter* Me);
	void DrawToasts();
	void DrawEvaluationPanel(const AHTMGameState* GS);
	void DrawDaySummary(const AHTMGameState* GS);

	// Utilidades.
	bool WorldToScreen(const FVector& World, FVector2D& OutScreen) const;
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border);
	/** Texto con sombra; Align 0 = izquierda, 0.5 = centrado, 1 = derecha. */
	FVector2D DrawLabel(const FString& Str, float X, float Y, const FLinearColor& InColor, UFont* Font, float Scale = 1.f, float Align = 0.f);
	void DrawRing(const FVector2D& Center, float Radius, const FLinearColor& InColor, float Thickness = 2.f, int32 Segments = 20);
	void DrawBar(float X, float Y, float W, float H, float Alpha, const FLinearColor& Fill);
	float UIScale() const;

	TArray<FToast> Toasts;
	float EvaluationShownAt = -100.f;
	bool bBoundToGameState = false;
	TWeakObjectPtr<AHTMGameState> BoundGameState;
};
