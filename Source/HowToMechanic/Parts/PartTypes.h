#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/HTMTypes.h"
#include "PartTypes.generated.h"

class UStaticMesh;

/**
 * Estadísticas físicas de una pieza. Las del coche se CALCULAN a partir de estas (GDD §7.6).
 * Solo tiene sentido rellenar las de la categoría correspondiente.
 */
USTRUCT(BlueprintType)
struct FPartStats
{
	GENERATED_BODY()

	// Motor
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TorqueNm = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxSpeedKmh = 0.f;
	/** Consumo (L/100 km a masa de referencia 1000 kg). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FuelUse = 0.f;
	/** Calor generado (°C/s a plena carga). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HeatGeneration = 0.f;
	/** Capacidad de refrigeración (fracción por segundo de la diferencia con el ambiente). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Cooling = 0.f;

	// Rueda
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float WheelRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Grip = 0.f;

	// Suspensión
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RestLength = 0.f;
	/** Rigidez (N/cm por esquina a masa de referencia 1000 kg; se escala con la masa real). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Stiffness = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damping = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BrakePower = 0.f;

	// Carrocería / aero / escape
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Downforce = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Drag = 0.f;
	/** Escape: multiplica el par (deportivo > 1) y el calor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TorqueMult = 0.f;
};

/** Definición de pieza (DT_Parts). */
USTRUCT(BlueprintType)
struct FPartDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EPartCategory Category = EPartCategory::Accessory;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MassKg = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Price = 50;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 BoltCount = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EToolType RequiredTool = EToolType::Wrench;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FPartStats Stats;
	/** Etiquetas para encargos y compradores: Sport, Eco, Offroad, Absurd, Original... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Tags;
	/** Nivel de taller a partir del cual aparece en la estantería (0 = nunca). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ShelfLevel = 1;
	/** Sonido/bocadillo de bocina u otro efecto característico ("Ship", "Clown"...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SoundTag;

	// --- Visual (placeholder hasta tener SM_ final) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UStaticMesh> Mesh;
	/** Cube, Cylinder, Sphere o Cone (/Engine/BasicShapes). Cylinder: eje Z = eje de la rueda. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName PlaceholderShape = TEXT("Cube");
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector PlaceholderSize = FVector(40.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor PlaceholderColor = FLinearColor::Gray;
	/** Toma el color de carrocería del coche al generarse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bUseCarColor = false;
	/** Se puede pintar con la pistola. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bPaintable = false;
};

/**
 * Estado de superficie: son estados de juego que cambian (lavar, lijar, pintar) y se
 * traducen a parámetros del material maestro (ART_DIRECTION §4).
 */
USTRUCT(BlueprintType)
struct FPartSurfaceState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) float Dirt = 0.f;
	UPROPERTY(BlueprintReadOnly) float Rust = 0.f;
	UPROPERTY(BlueprintReadOnly) float Wetness = 0.f;
	UPROPERTY(BlueprintReadOnly) FLinearColor BaseColor = FLinearColor::Gray;
	UPROPERTY(BlueprintReadOnly) FLinearColor PaintColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly) float PaintAmount = 0.f;
	/** 1 = recién pintado, 0 = seco. */
	UPROPERTY(BlueprintReadOnly) float FreshPaint = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bPaintRuined = false;
	UPROPERTY(BlueprintReadOnly) float Damage = 0.f;

	/** Color visible final (base mezclado con pintura). */
	FLinearColor GetVisibleColor() const
	{
		return FLinearColor::LerpUsingHSV(BaseColor, PaintColor, FMath::Clamp(PaintAmount, 0.f, 1.f));
	}

	/** ¿Tiene una capa de pintura completa y vistosa? */
	bool HasVividPaint() const
	{
		if (PaintAmount < 0.85f || bPaintRuined) { return false; }
		const FLinearColor HSV = PaintColor.LinearRGBToHSV();
		return HSV.G > 0.5f && HSV.B > 0.5f;
	}
};
