#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Parts/PartTypes.h"
#include "Vehicle/CarModelTypes.h"
#include "Customers/JobTypes.h"
#include "Progression/ProgressionTypes.h"
#include "HTMDataSubsystem.generated.h"

class UDataTable;

/**
 * Acceso único a los datos de juego (DataTables). Carga las tablas configuradas en UHTMSettings.
 * En el editor, si un DT_* aún no se ha importado, construye una tabla transitoria desde
 * Content/Data/<Nombre>.json para que PIE funcione nada más compilar.
 */
UCLASS()
class HOWTOMECHANIC_API UHTMDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UHTMDataSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FPartDefinitionRow* FindPart(FName PartId) const;
	const FCarModelRow* FindCarModel(FName ModelId) const;
	const FJobDefinitionRow* FindJob(FName JobId) const;
	const FWorkshopLevelRow* FindWorkshopLevel(int32 Level) const;
	const FCosmeticRow* FindCosmetic(FName CosmeticId) const;
	const FBuyerProfileRow* FindBuyer(FName BuyerId) const;

	TArray<FName> GetPartIds() const;
	TArray<FName> GetPartIdsInCategory(EPartCategory Category) const;
	TArray<FName> GetCarModelIds() const;
	TArray<FName> GetJobIds() const;
	TArray<FName> GetCosmeticIds(ECosmeticSlot Slot) const;
	TArray<FName> GetBuyerIds() const;
	TArray<const FCarTraitRow*> GetTraits() const;
	TArray<FText> GetCarNames() const;
	int32 GetMaxWorkshopLevel() const;

	UDataTable* GetPartsTable() const { return Parts; }

private:
	UDataTable* LoadTable(const TSoftObjectPtr<UDataTable>& Path, UScriptStruct* RowStruct, const TCHAR* JsonFallbackName);

	UPROPERTY() TObjectPtr<UDataTable> Parts;
	UPROPERTY() TObjectPtr<UDataTable> CarModels;
	UPROPERTY() TObjectPtr<UDataTable> Jobs;
	UPROPERTY() TObjectPtr<UDataTable> Traits;
	UPROPERTY() TObjectPtr<UDataTable> Cosmetics;
	UPROPERTY() TObjectPtr<UDataTable> WorkshopLevels;
	UPROPERTY() TObjectPtr<UDataTable> Buyers;
	UPROPERTY() TObjectPtr<UDataTable> CarNames;
};
