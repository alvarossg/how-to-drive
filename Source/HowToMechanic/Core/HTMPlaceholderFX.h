#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HTMTypes.h"
#include "HTMPlaceholderFX.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Efecto placeholder local (no replicado) hecho de primitivas: "bolas de dibujo" (ART_DIRECTION §11).
 * Pocas partículas, formas grandes. Se sustituye por NS_* cuando se asignen en UHTMSettings::Effects.
 */
UCLASS(NotBlueprintable, Transient)
class HOWTOMECHANIC_API AHTMPlaceholderFX : public AActor
{
	GENERATED_BODY()

public:
	AHTMPlaceholderFX();

	void Init(EHTMFX InType, float InScale, float InDuration, FLinearColor InTint);

	virtual void Tick(float DeltaSeconds) override;

private:
	struct FBlob
	{
		TObjectPtr<UStaticMeshComponent> Comp = nullptr;
		FVector Velocity = FVector::ZeroVector;
		float Phase = 0.f;
		float BaseSize = 1.f;
	};

	TArray<FBlob> Blobs;

	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BlobComponents;

	EHTMFX Type = EHTMFX::DustPoof;
	float Scale = 1.f;
	float Duration = 1.f;
	float Age = 0.f;
	bool bLoop = false;
};
