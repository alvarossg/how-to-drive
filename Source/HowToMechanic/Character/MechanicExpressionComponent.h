#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "Core/HTMTypes.h"
#include "MechanicExpressionComponent.generated.h"

class UTexture2D;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** Texturas de una expresión (ojos + cejas + boca). ART_DIRECTION §6. */
USTRUCT(BlueprintType)
struct FFaceExpressionTextures
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> Eyes = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> Brows = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> Mouth = nullptr;
};

/** DA_FaceExpressions: una entrada por expresión. Lo rellena el artista. */
UCLASS(BlueprintType)
class HOWTOMECHANIC_API UFaceExpressionSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<EFaceExpression, FFaceExpressionTextures> Expressions;
};

/**
 * Expresiones por textura intercambiable, sin rig facial (GDD §5).
 * - Con FaceMaterial + ExpressionSet: cambia los parámetros de textura EyesTex / BrowsTex / MouthTex.
 * - Placeholder: mueve ojos, cejas y boca de primitivas (las cejas hacen el 80 % del trabajo).
 * Local: la expresión se deduce del estado replicado del personaje.
 */
UCLASS(ClassGroup = (HTM), meta = (BlueprintSpawnableComponent))
class HOWTOMECHANIC_API UMechanicExpressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMechanicExpressionComponent();

	void SetupPlaceholder(UStaticMeshComponent* InEyeL, UStaticMeshComponent* InEyeR, UStaticMeshComponent* InBrowL,
		UStaticMeshComponent* InBrowR, UStaticMeshComponent* InMouth);

	/** Material de la cara de la malla final (opcional). */
	void SetFaceMaterial(UMaterialInstanceDynamic* InFaceMID) { FaceMID = InFaceMID; }

	/** Fuerza una expresión durante un tiempo (emotes, enfado del cliente...). */
	void PlayOverride(EFaceExpression Expression, float Duration);

	EFaceExpression GetCurrentExpression() const { return Current; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Face") TObjectPtr<UFaceExpressionSet> ExpressionSet;

private:
	EFaceExpression ComputeExpression() const;
	void Apply(EFaceExpression Expression);

	UPROPERTY() TObjectPtr<UStaticMeshComponent> EyeL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> EyeR;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BrowL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BrowR;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mouth;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FaceMID;

	FTransform EyeLRest, EyeRRest, BrowLRest, BrowRRest, MouthRest;
	EFaceExpression Current = EFaceExpression::Happy;
	EFaceExpression OverrideExpression = EFaceExpression::Happy;
	float OverrideTime = 0.f;
	float BlinkTimer = 3.f;
};
