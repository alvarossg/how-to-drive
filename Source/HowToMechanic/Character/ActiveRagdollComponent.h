#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveRagdollComponent.generated.h"

class USkeletalMeshComponent;
class UPhysicalAnimationComponent;
class USceneComponent;

/**
 * Active ragdoll (GDD §5, ART_DIRECTION §6): animación mezclada con física.
 *
 * - Con malla esquelética + Physics Asset: UPhysicalAnimationComponent sobre el tronco y brazos
 *   (se tambalean al cargar peso y al recibir golpes); KO = ragdoll completo y vuelta a la animación.
 * - Sin malla (placeholder de primitivas): muelle procedural sobre la raíz visual que imita lo mismo
 *   (inclinación al acelerar/cargar, bamboleo al tambalearse, caída de muñeco en el KO).
 *
 * Todo es COSMÉTICO y local en cada máquina: el estado (KO, tambaleo) lo replica el personaje.
 */
UCLASS(ClassGroup = (HTM), meta = (BlueprintSpawnableComponent))
class HOWTOMECHANIC_API UActiveRagdollComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActiveRagdollComponent();

	void Setup(USkeletalMeshComponent* InMesh, USceneComponent* InPlaceholderRoot);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool UsesSkeletalPhysics() const { return bSkeletalPhysics; }

	/** 0 = firme, 1 = muy inestable (cargar pesado, suelo mojado...). */
	void SetInstability(float InInstability) { Instability = FMath::Clamp(InInstability, 0.f, 1.f); }

	/** Tambaleo breve (golpe, tropiezo). */
	void Stagger(const FVector& WorldImpulse, float Duration);

	/** Ragdoll completo (KO). */
	void StartRagdoll(const FVector& WorldImpulse);

	/** Vuelta a la animación. */
	void StopRagdoll();

	/** Tumbado boca arriba (para meterse bajo el coche). */
	void SetLyingDown(bool bLying) { bLyingDown = bLying; }

	/** Transform de reposo del cuerpo placeholder (cambia con la postura). */
	void SetPlaceholderRest(const FTransform& Rest) { PlaceholderRestTransform = Rest; }

	/** Sentado (conduciendo). */
	void SetSeated(bool bSeated) { bSitting = bSeated; }

	UPROPERTY(EditAnywhere, Category = "Ragdoll") FName PelvisBone = TEXT("pelvis");
	UPROPERTY(EditAnywhere, Category = "Ragdoll") FName UpperBodyBone = TEXT("spine_01");
	UPROPERTY(EditAnywhere, Category = "Ragdoll") float OrientationStrength = 1500.f;
	UPROPERTY(EditAnywhere, Category = "Ragdoll") float AngularVelocityStrength = 120.f;
	UPROPERTY(EditAnywhere, Category = "Ragdoll") float BaseBlendWeight = 0.35f;

private:
	void TickPlaceholder(float DeltaTime);
	void TickSkeletal(float DeltaTime);

	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<USceneComponent> PlaceholderRoot;
	UPROPERTY() TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimation;

	FTransform PlaceholderRestTransform;
	FTransform MeshRestRelativeTransform;
	bool bSkeletalPhysics = false;
	bool bRagdoll = false;
	bool bLyingDown = false;
	bool bSitting = false;
	float Instability = 0.f;
	float StaggerTime = 0.f;
	float StaggerDuration = 0.f;
	float RecoverAlpha = 1.f;
	FVector StaggerDir = FVector::ZeroVector;
	FVector LastVelocity = FVector::ZeroVector;

	// Muelle del placeholder (inclinación en grados).
	FVector2D Lean = FVector2D::ZeroVector;
	FVector2D LeanVelocity = FVector2D::ZeroVector;
	float FallAlpha = 0.f;
	float LieAlpha = 0.f;
	float Time = 0.f;
};
