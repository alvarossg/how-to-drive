#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/HTMTypes.h"
#include "Parts/PartTypes.h"
#include "HTMVisualLibrary.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UMaterialInstanceDynamic;
class USceneComponent;

/**
 * Utilidades de arte placeholder (ART_DIRECTION §14): primitivas con la paleta oficial y el
 * material maestro. Cuando llegue el arte final, las clases BP_ asignan meshes y esto deja de usarse.
 */
UCLASS()
class HOWTOMECHANIC_API UHTMVisualLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Cube, Cylinder, Sphere, Cone o Plane de /Engine/BasicShapes (100 cm). */
	static UStaticMesh* GetShapeMesh(FName Shape);

	/** Crea un MID del material maestro (o BasicShapeMaterial como respaldo) con el color dado. */
	static UMaterialInstanceDynamic* ApplyPlaceholderMaterial(UPrimitiveComponent* Component, const FLinearColor& Color);

	/** Escribe el estado de superficie en el MID (Dirt, Rust, Paint..., y "Color" compuesto de respaldo). */
	static void ApplySurface(UMaterialInstanceDynamic* MID, const FPartSurfaceState& Surface);

	/** Resalte de interacción (custom depth + parámetro Highlight). Solo local. */
	static void SetHighlighted(UPrimitiveComponent* Component, UMaterialInstanceDynamic* MID, bool bHighlighted);

	/**
	 * Añade una primitiva escalada a SizeCm como hijo de Parent (en construcción o en runtime).
	 * bCollision = BlockAll estático; si no, sin colisión.
	 */
	static UStaticMeshComponent* AddShape(AActor* Owner, USceneComponent* Parent, FName Shape, const FVector& Location,
		const FVector& SizeCm, const FRotator& Rotation, const FLinearColor& Color, bool bCollision, FName Name = NAME_None);

	/**
	 * Efecto visual local (no replicado: llámalo desde OnRep o multicast).
	 * Usa el Niagara configurado en UHTMSettings::Effects o un placeholder de "bolas de dibujo".
	 * Duration < 0 = en bucle hasta StopFX.
	 */
	static USceneComponent* SpawnFX(const UObject* WorldContext, EHTMFX Type, const FVector& Location, float Scale = 1.f,
		USceneComponent* AttachTo = nullptr, float Duration = 1.2f, FLinearColor Tint = FLinearColor::White);

	static void StopFX(USceneComponent* FX);
};
