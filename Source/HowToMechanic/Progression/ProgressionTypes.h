#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/HTMTypes.h"
#include "ProgressionTypes.generated.h"

/** Nivel del taller (DT_WorkshopLevels, GDD §11). */
USTRUCT(BlueprintType)
struct FWorkshopLevelRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 UpgradeCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinReputation = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Bays = 1;
	/** Tamaño interior del taller (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D FloorSize = FVector2D(1400.f, 1200.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bHasJackStands = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bHasLift = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bHasEngineCrane = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bHasPaintBooth = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bHasScanner = false;
	/** Herramientas que aparecen en el panel de herramientas a este nivel (acumulativo). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<EToolType> Tools;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ShelfSlots = 6;
};

/** Cosmético (DT_Cosmetics, GDD §12). Solo estético. */
USTRUCT(BlueprintType)
struct FCosmeticRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) ECosmeticSlot Slot = ECosmeticSlot::Hat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	/** Forma placeholder: Cap, Helmet, Beanie, Goggles, None / Apron, Overalls, Hoodie / Mitts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ShapeId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Price = 0;
	/** Logro que lo desbloquea (si AchievementCount > 0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EHTMAchievement Achievement = EHTMAchievement::TimesKO;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 AchievementCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bUnlockedByDefault = false;
};

/** Comprador de coches reparados (DT_BuyerProfiles, GDD §10.2). */
USTRUCT(BlueprintType)
struct FBuyerProfileRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText LikesText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> LikedTags;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ConditionWeight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CompletenessWeight = 1.f;
	/** Bonus (fracción del valor base) por cada condición que le gusta. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TagBonus = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float VividPaintBonus = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BigWheelsBonus = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LowFuelBonus = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Weight = 1.f;
};

/** Nombres y matrículas para los coches generados (DT_CarNames). */
USTRUCT(BlueprintType)
struct FCarNameRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name;
};
