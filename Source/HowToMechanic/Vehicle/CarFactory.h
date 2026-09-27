#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/HTMTypes.h"
#include "CarFactory.generated.h"

class AModularCar;
class ACarPart;
class ATool;
class AGrabbableActor;
struct FGrabbablePropSpec;

/** Cómo generar un coche entrante (GDD §7.5): modelo + estado de cada pieza + rasgos + nombre. */
USTRUCT(BlueprintType)
struct FCarSpawnOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ConditionMin = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ConditionMax = 95.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MissingPartChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RustChance = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DirtChance = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LooseBoltChance = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MinFaults = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxFaults = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EHiddenFault ForcedFault = EHiddenFault::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ForcedSlot;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ForcedPartId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MinTraits = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxTraits = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 JobUid = 0;
};

/** Genera coches, piezas, herramientas y objetos (siempre en el servidor). */
UCLASS()
class HOWTOMECHANIC_API UCarFactory : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static AModularCar* SpawnCar(UObject* WorldContext, FName ModelId, const FTransform& Where, const FCarSpawnOptions& Options);

	/** Coche guardado: modelo + piezas exactas (Progression/HTMSaveGame). */
	static AModularCar* SpawnEmptyCar(UObject* WorldContext, FName ModelId, const FTransform& Where, const FLinearColor& Color);

	static ACarPart* SpawnPart(UObject* WorldContext, FName PartId, const FTransform& Where, float Condition = 100.f,
		EHiddenFault Fault = EHiddenFault::None, const FLinearColor* BodyColor = nullptr, float Rust = 0.f, float Dirt = 0.f);

	static ATool* SpawnTool(UObject* WorldContext, EToolType Type, const FTransform& Where);

	static AGrabbableActor* SpawnProp(UObject* WorldContext, const FGrabbablePropSpec& Spec, const FTransform& Where);

	static FName PickModel(FRandomStream& Rng, const TArray<FName>& Allowed, const UObject* WorldContext);
	static FString MakePlate(FRandomStream& Rng);

private:
	static EPartCategory CategoryForFault(EHiddenFault Fault);
};
