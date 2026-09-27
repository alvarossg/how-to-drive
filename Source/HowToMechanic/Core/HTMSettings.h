#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HTMSettings.generated.h"

class UDataTable;
class UHTMTuningData;
class UMaterialInterface;
class UNiagaraSystem;
class UInputMappingContext;
class APawn;

/**
 * Rutas de datos del proyecto (Project Settings > Game > How to Mechanic).
 * Valores en Config/DefaultGame.ini. Nada de rutas hardcodeadas en el resto del código.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "How to Mechanic"))
class HOWTOMECHANIC_API UHTMSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UHTMSettings* Get() { return GetDefault<UHTMSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// --- Tablas de datos (FPartDefinitionRow, FCarModelRow, ...) ---
	UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UDataTable> PartsTable;
	UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UDataTable> CarModelsTable;
	UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UDataTable> JobsTable;
	UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UDataTable> TraitsTable;
	UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UDataTable> CosmeticsTable;
	UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UDataTable> WorkshopLevelsTable;
	UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UDataTable> BuyerProfilesTable;
	UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UDataTable> CarNamesTable;

	/** Panel de tuning del caos (fase 5). Si no existe, se usan los valores por defecto de la clase. */
	UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UHTMTuningData> Tuning;

	// --- Arte ---
	/** M_Master (ART_DIRECTION §4). Si no existe, se usa BasicShapeMaterial con el parámetro "Color". */
	UPROPERTY(Config, EditAnywhere, Category = "Art") TSoftObjectPtr<UMaterialInterface> MasterMaterial;
	UPROPERTY(Config, EditAnywhere, Category = "Art") TSoftObjectPtr<UMaterialInterface> OutlinePostProcessMaterial;

	/** Efectos Niagara por tipo (EHTMFX como índice). Vacío = placeholder de primitivas. */
	UPROPERTY(Config, EditAnywhere, Category = "Art") TMap<FName, TSoftObjectPtr<UNiagaraSystem>> Effects;

	// --- Clases (opcional: BP_ con arte final). Vacío = clases C++ con placeholders ---
	UPROPERTY(Config, EditAnywhere, Category = "Classes") TSoftClassPtr<APawn> MechanicCharacterClass;

	// --- Input opcional (si está vacío se construye en C++) ---
	UPROPERTY(Config, EditAnywhere, Category = "Input") TSoftObjectPtr<UInputMappingContext> OnFootMappingOverride;
	UPROPERTY(Config, EditAnywhere, Category = "Input") TSoftObjectPtr<UInputMappingContext> DrivingMappingOverride;

	// --- Mapas ---
	UPROPERTY(Config, EditAnywhere, Category = "Maps") FString WorkshopMap = TEXT("/Game/Maps/L_Workshop");
	UPROPERTY(Config, EditAnywhere, Category = "Maps") FString MainMenuMap = TEXT("/Game/Maps/L_MainMenu");
};
