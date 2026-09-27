#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/HTMTypes.h"
#include "CarModelTypes.generated.h"

class UStaticMesh;

/**
 * Un slot del chasis (GDD §7.1). Transform relativo al centro del chasis.
 * Convención de la pieza: X adelante, Z arriba. Ruedas: el eje de giro es Z local de la pieza,
 * por eso los slots de rueda llevan Roll = 90.
 */
USTRUCT(BlueprintType)
struct FCarSlotDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SlotName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EPartCategory Category = EPartCategory::Accessory;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector Location = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FRotator Rotation = FRotator::ZeroRotator;
	/** Pieza de serie que monta el modelo (vacío = slot vacío de fábrica). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName DefaultPartId;
	/** Quitar la pieza de ParentSlot suelta la de este slot ("quitar X suelta Y", GDD §13). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ParentSlot;
	/** Este slot no es accesible mientras BlockedBySlot tenga pieza (p. ej. motor bajo el capó). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName BlockedBySlot;
	/** Hace falta para que el coche esté "completo" al entregarlo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bRequired = true;
	/** 0 FL, 1 FR, 2 RL, 3 RR para Wheel y Suspension; -1 en el resto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 WheelIndex = -1;
};

/** Modelo base de coche (DT_CarModels). */
USTRUCT(BlueprintType)
struct FCarModelRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	/** Semiextensión de la caja física del chasis (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector ChassisExtent = FVector(160.f, 80.f, 28.f);
	/** Cabina (colisión soldada al chasis) y su desplazamiento. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector CabinExtent = FVector(80.f, 72.f, 40.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector CabinOffset = FVector(-15.f, 0.f, 65.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ChassisMassKg = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 BaseValue = 1500;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 JunkyardPriceMin = 250;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 JunkyardPriceMax = 600;
	/** 0 = trasera, 1 = delantera, 2 = total. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 DriveType = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector SeatOffset = FVector(-20.f, -30.f, 30.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector ExitOffset = FVector(-20.f, -190.f, 60.f);
	/** Relación final para convertir par del motor en fuerza en la rueda. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FinalDriveRatio = 7.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FCarSlotDefinition> Slots;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UStaticMesh> BodyMesh;
	/** Probabilidad relativa de aparecer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SpawnWeight = 1.f;
};

/** Rasgo de personalidad (DT_Traits). */
USTRUCT(BlueprintType)
struct FCarTraitRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) ECarTrait Trait = ECarTrait::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Weight = 1.f;
};
