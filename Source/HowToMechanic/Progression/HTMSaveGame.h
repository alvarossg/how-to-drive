#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/HTMTypes.h"
#include "Core/HTMPlayerState.h"
#include "Interaction/GrabbableActor.h"
#include "Parts/PartTypes.h"
#include "HTMSaveGame.generated.h"

USTRUCT()
struct FSavedPart
{
	GENERATED_BODY()

	UPROPERTY() FName SlotName;
	UPROPERTY() FName PartId;
	UPROPERTY() FTransform Transform;
	UPROPERTY() float Condition = 100.f;
	UPROPERTY() uint8 BoltsTight = 0;
	UPROPERTY() bool bReversed = false;
	UPROPERTY() FPartSurfaceState Surface;
	UPROPERTY() EHiddenFault Fault = EHiddenFault::None;
	UPROPERTY() bool bFaultRevealed = false;
};

USTRUCT()
struct FSavedCar
{
	GENERATED_BODY()

	UPROPERTY() FName ModelId;
	UPROPERTY() FTransform Transform;
	UPROPERTY() FText Name;
	UPROPERTY() FString Plate;
	UPROPERTY() TArray<ECarTrait> Traits;
	UPROPERTY() FPartSurfaceState Chassis;
	UPROPERTY() TArray<FSavedPart> Parts;
	UPROPERTY() float BestTopSpeedKmh = 0.f;
	UPROPERTY() float MaxAirTime = 0.f;
};

USTRUCT()
struct FSavedDecor
{
	GENERATED_BODY()

	UPROPERTY() FGrabbablePropSpec Spec;
	UPROPERTY() FTransform Transform;
};

USTRUCT()
struct FSavedPlayer
{
	GENERATED_BODY()

	UPROPERTY() FString PlayerName;
	UPROPERTY() FMechanicLoadout Loadout;
	UPROPERTY() TArray<int32> Achievements;
};

/**
 * Progreso del taller, guardado por el ANFITRIÓN (GDD §3). Se guarda al cerrar cada día y al mejorar
 * el taller. Los coches propios y las piezas sueltas dentro del taller se conservan tal cual.
 */
UCLASS()
class HOWTOMECHANIC_API UHTMSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;
	static const TCHAR* SlotName() { return TEXT("HowToMechanic_Workshop"); }

	UPROPERTY() int32 Version = CurrentVersion;
	UPROPERTY() int32 Money = 0;
	UPROPERTY() float Reputation = 0.f;
	UPROPERTY() int32 WorkshopLevel = 1;
	UPROPERTY() int32 DayNumber = 0;
	UPROPERTY() int32 WallColorIndex = 0;
	UPROPERTY() TArray<FName> UnlockedCosmetics;
	UPROPERTY() FHTMRoomOptions RoomOptions;
	UPROPERTY() TArray<FSavedCar> Cars;
	UPROPERTY() TArray<FSavedPart> LooseParts;
	UPROPERTY() TArray<FSavedDecor> Decor;
	UPROPERTY() TArray<FSavedPlayer> Players;
};
