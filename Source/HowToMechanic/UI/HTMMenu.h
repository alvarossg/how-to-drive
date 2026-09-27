#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HTMMenu.generated.h"

class UFont;

/** Modo de juego de L_MainMenu: sin pawn, ratón visible, HUD de menú. */
UCLASS()
class HOWTOMECHANIC_API AHTMMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHTMMenuGameMode();
};

/** Controlador del menú: ratón + navegación con teclado/mando (arriba/abajo/aceptar/atrás). */
UCLASS()
class HOWTOMECHANIC_API AHTMMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHTMMenuPlayerController();
	virtual void BeginPlay() override;
	virtual bool InputKey(const FInputKeyParams& Params) override;
};

/**
 * Menú principal dibujado con Canvas (placeholder hasta WBP_MainMenu). Pantallas:
 *  Principal: crear taller (online/LAN), buscar talleres, opciones de sala, nueva partida, salir.
 *  Buscar: lista de salas encontradas → unirse.
 */
UCLASS()
class HOWTOMECHANIC_API AHTMMenuHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;
	virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;

	void MoveSelection(int32 Delta);
	void ActivateSelection();
	void Back();

protected:
	enum class EScreen : uint8 { Main, Browse };

	struct FButton
	{
		FName Id;
		FString Label;
		bool bEnabled = true;
	};

	void BuildButtons(TArray<FButton>& Out) const;
	void Activate(FName Id);
	float UIScale() const;

	EScreen Screen = EScreen::Main;
	int32 Selected = 0;
	/** Botones del último frame (para navegar con mando). */
	TArray<FName> LastButtonIds;
};
