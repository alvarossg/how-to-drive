#include "Core/HTMDataSubsystem.h"
#include "Core/HTMSettings.h"
#include "Core/HTMLog.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UHTMDataSubsystem* UHTMDataSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UHTMDataSubsystem>() : nullptr;
}

void UHTMDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UHTMSettings* S = UHTMSettings::Get();
	Parts          = LoadTable(S->PartsTable,          FPartDefinitionRow::StaticStruct(), TEXT("Parts"));
	CarModels      = LoadTable(S->CarModelsTable,      FCarModelRow::StaticStruct(),       TEXT("CarModels"));
	Jobs           = LoadTable(S->JobsTable,           FJobDefinitionRow::StaticStruct(),  TEXT("Jobs"));
	Traits         = LoadTable(S->TraitsTable,         FCarTraitRow::StaticStruct(),       TEXT("Traits"));
	Cosmetics      = LoadTable(S->CosmeticsTable,      FCosmeticRow::StaticStruct(),       TEXT("Cosmetics"));
	WorkshopLevels = LoadTable(S->WorkshopLevelsTable, FWorkshopLevelRow::StaticStruct(),  TEXT("WorkshopLevels"));
	Buyers         = LoadTable(S->BuyerProfilesTable,  FBuyerProfileRow::StaticStruct(),   TEXT("BuyerProfiles"));
	CarNames       = LoadTable(S->CarNamesTable,       FCarNameRow::StaticStruct(),        TEXT("CarNames"));
}

UDataTable* UHTMDataSubsystem::LoadTable(const TSoftObjectPtr<UDataTable>& Path, UScriptStruct* RowStruct, const TCHAR* JsonFallbackName)
{
	if (!Path.IsNull())
	{
		if (UDataTable* Table = Path.LoadSynchronous())
		{
			if (Table->GetRowStruct() == RowStruct)
			{
				return Table;
			}
			UE_LOG(LogHTM, Error, TEXT("%s tiene un RowStruct distinto de %s. Reimporta con Tools/Scripts/import_data_tables.py"),
				*Path.ToString(), *RowStruct->GetName());
		}
	}

#if WITH_EDITOR
	// Respaldo en editor: tabla transitoria desde el JSON fuente.
	const FString JsonPath = FPaths::ProjectContentDir() / TEXT("Data") / (FString(JsonFallbackName) + TEXT(".json"));
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *JsonPath))
	{
		UDataTable* Table = NewObject<UDataTable>(this, FName(*FString::Printf(TEXT("DT_%s_Transient"), JsonFallbackName)), RF_Transient);
		Table->RowStruct = RowStruct;
		const TArray<FString> Problems = Table->CreateTableFromJSONString(Json);
		for (const FString& Problem : Problems)
		{
			UE_LOG(LogHTM, Warning, TEXT("[%s.json] %s"), JsonFallbackName, *Problem);
		}
		UE_LOG(LogHTM, Log, TEXT("DT_%s no importado: usando %s (%d filas)"), JsonFallbackName, *JsonPath, Table->GetRowMap().Num());
		return Table;
	}
#endif

	UE_LOG(LogHTM, Error, TEXT("No se pudo cargar la tabla %s. Ejecuta Tools/Scripts/setup_project.py en el editor."), JsonFallbackName);
	UDataTable* Empty = NewObject<UDataTable>(this, NAME_None, RF_Transient);
	Empty->RowStruct = RowStruct;
	return Empty;
}

const FPartDefinitionRow* UHTMDataSubsystem::FindPart(FName PartId) const
{
	return (Parts && !PartId.IsNone()) ? Parts->FindRow<FPartDefinitionRow>(PartId, TEXT("FindPart"), false) : nullptr;
}

const FCarModelRow* UHTMDataSubsystem::FindCarModel(FName ModelId) const
{
	return (CarModels && !ModelId.IsNone()) ? CarModels->FindRow<FCarModelRow>(ModelId, TEXT("FindCarModel"), false) : nullptr;
}

const FJobDefinitionRow* UHTMDataSubsystem::FindJob(FName JobId) const
{
	return (Jobs && !JobId.IsNone()) ? Jobs->FindRow<FJobDefinitionRow>(JobId, TEXT("FindJob"), false) : nullptr;
}

const FWorkshopLevelRow* UHTMDataSubsystem::FindWorkshopLevel(int32 Level) const
{
	if (!WorkshopLevels) { return nullptr; }
	const FWorkshopLevelRow* Best = nullptr;
	for (const auto& Pair : WorkshopLevels->GetRowMap())
	{
		const FWorkshopLevelRow* Row = reinterpret_cast<const FWorkshopLevelRow*>(Pair.Value);
		if (Row->Level == Level)
		{
			return Row;
		}
		if (Row->Level < Level && (!Best || Row->Level > Best->Level))
		{
			Best = Row;
		}
	}
	return Best;
}

const FCosmeticRow* UHTMDataSubsystem::FindCosmetic(FName CosmeticId) const
{
	return (Cosmetics && !CosmeticId.IsNone()) ? Cosmetics->FindRow<FCosmeticRow>(CosmeticId, TEXT("FindCosmetic"), false) : nullptr;
}

const FBuyerProfileRow* UHTMDataSubsystem::FindBuyer(FName BuyerId) const
{
	return (Buyers && !BuyerId.IsNone()) ? Buyers->FindRow<FBuyerProfileRow>(BuyerId, TEXT("FindBuyer"), false) : nullptr;
}

TArray<FName> UHTMDataSubsystem::GetPartIds() const
{
	return Parts ? Parts->GetRowNames() : TArray<FName>();
}

TArray<FName> UHTMDataSubsystem::GetPartIdsInCategory(EPartCategory Category) const
{
	TArray<FName> Result;
	if (Parts)
	{
		for (const auto& Pair : Parts->GetRowMap())
		{
			if (reinterpret_cast<const FPartDefinitionRow*>(Pair.Value)->Category == Category)
			{
				Result.Add(Pair.Key);
			}
		}
	}
	return Result;
}

TArray<FName> UHTMDataSubsystem::GetCarModelIds() const
{
	return CarModels ? CarModels->GetRowNames() : TArray<FName>();
}

TArray<FName> UHTMDataSubsystem::GetJobIds() const
{
	return Jobs ? Jobs->GetRowNames() : TArray<FName>();
}

TArray<FName> UHTMDataSubsystem::GetCosmeticIds(ECosmeticSlot Slot) const
{
	TArray<FName> Result;
	if (Cosmetics)
	{
		for (const auto& Pair : Cosmetics->GetRowMap())
		{
			if (reinterpret_cast<const FCosmeticRow*>(Pair.Value)->Slot == Slot)
			{
				Result.Add(Pair.Key);
			}
		}
	}
	return Result;
}

TArray<FName> UHTMDataSubsystem::GetBuyerIds() const
{
	return Buyers ? Buyers->GetRowNames() : TArray<FName>();
}

TArray<const FCarTraitRow*> UHTMDataSubsystem::GetTraits() const
{
	TArray<const FCarTraitRow*> Result;
	if (Traits)
	{
		for (const auto& Pair : Traits->GetRowMap())
		{
			Result.Add(reinterpret_cast<const FCarTraitRow*>(Pair.Value));
		}
	}
	return Result;
}

TArray<FText> UHTMDataSubsystem::GetCarNames() const
{
	TArray<FText> Result;
	if (CarNames)
	{
		for (const auto& Pair : CarNames->GetRowMap())
		{
			Result.Add(reinterpret_cast<const FCarNameRow*>(Pair.Value)->CarName);
		}
	}
	return Result;
}

int32 UHTMDataSubsystem::GetMaxWorkshopLevel() const
{
	int32 Max = 1;
	if (WorkshopLevels)
	{
		for (const auto& Pair : WorkshopLevels->GetRowMap())
		{
			Max = FMath::Max(Max, reinterpret_cast<const FWorkshopLevelRow*>(Pair.Value)->Level);
		}
	}
	return Max;
}
