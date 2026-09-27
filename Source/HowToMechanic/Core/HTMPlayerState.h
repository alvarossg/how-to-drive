#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/HTMTypes.h"
#include "HTMPlayerState.generated.h"

/** Cosméticos equipados (GDD §12). Ids de DT_Cosmetics. */
USTRUCT(BlueprintType)
struct FMechanicLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Hat = TEXT("Hat_Cap_Blue");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Outfit = TEXT("Outfit_Apron_Orange");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Gloves = TEXT("Gloves_None");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ToolColor = TEXT("ToolColor_Red");
};

/** Datos por jugador: cosméticos y contadores de logros absurdos. */
UCLASS()
class HOWTOMECHANIC_API AHTMPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AHTMPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor. */
	void AddAchievement(EHTMAchievement Achievement, int32 Amount = 1);
	int32 GetAchievementCount(EHTMAchievement Achievement) const;
	/** Servidor. */
	void SetLoadout(const FMechanicLoadout& NewLoadout);

	UPROPERTY(ReplicatedUsing = OnRep_Loadout, BlueprintReadOnly) FMechanicLoadout Loadout;
	UPROPERTY(Replicated, BlueprintReadOnly) TArray<int32> Achievements;

protected:
	UFUNCTION() void OnRep_Loadout();
};
