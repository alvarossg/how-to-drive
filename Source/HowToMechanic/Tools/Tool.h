#pragma once

#include "CoreMinimal.h"
#include "Interaction/GrabbableActor.h"
#include "Core/HTMTypes.h"
#include "Tool.generated.h"

class AMechanicCharacter;
class USceneComponent;

/**
 * Herramienta física (GDD §6): cada una sirve para una cosa y hay que ir a buscarla.
 *
 * - Llaves (inglesa, de ruedas, destornillador): no hacen nada solas; las piezas comprueban la
 *   herramienta en la mano al atornillar (IInteractable de ACarPart).
 * - Continuas (manguera, pintura, extintor, lijadora, esponja, estetoscopio): mientras se mantiene
 *   "usar", el servidor traza desde la mirada del portador y aplica el efecto.
 * - Escáner: uso instantáneo sobre el coche enfocado.
 */
UCLASS()
class HOWTOMECHANIC_API ATool : public AGrabbableActor
{
	GENERATED_BODY()

public:
	ATool();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Configura tipo y placeholder (servidor, al spawnear). */
	void InitTool(EToolType InType);
	static FGrabbablePropSpec MakeSpecFor(EToolType Type);

	EToolType GetToolType() const { return ToolType; }
	virtual bool IsContinuous() const;
	virtual bool HandlesAltUseItself() const { return ToolType == EToolType::PaintGun; }

	/** Servidor. */
	void SetUsing(AMechanicCharacter* User, bool bActive, bool bAlt);
	/** Servidor: uso secundario instantáneo (pistola: siguiente color). */
	void AltUse(AMechanicCharacter* User);

	/** Color cosmético del jugador (local). */
	void SetTint(const FLinearColor& Tint);

	FLinearColor GetPaintColor() const { return PaintColor; }
	bool IsInUse() const { return bInUse; }

	virtual FText GetDisplayName() const override;
	virtual FRotator GetCarryRotationOffset() const override;
	virtual EHTMWeightClass GetWeightClass() const override { return EHTMWeightClass::Light; }

protected:
	virtual void OnDropped(AMechanicCharacter* Who, const FVector& Velocity, bool bThrown) override;

	void ServerToolTick(float DeltaSeconds);
	bool TraceFromUser(float Range, FHitResult& OutHit) const;
	void TickStethoscope(float DeltaSeconds);
	void TickSpray(float DeltaSeconds);
	void TickExtinguisher(float DeltaSeconds);
	void UseScanner();

	UFUNCTION() void OnRep_ToolType();
	UFUNCTION() void OnRep_InUse();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBubble(FVector_NetQuantize Location, const FText& Text);

	UPROPERTY(ReplicatedUsing = OnRep_ToolType, EditAnywhere, Category = "Tool")
	EToolType ToolType = EToolType::Wrench;

	UPROPERTY(ReplicatedUsing = OnRep_InUse)
	bool bInUse = false;

	UPROPERTY(ReplicatedUsing = OnRep_InUse)
	FLinearColor PaintColor = FLinearColor::Red;

	UPROPERTY(Replicated)
	int32 PaintColorIndex = 0;

	UPROPERTY(Transient)
	TObjectPtr<AMechanicCharacter> ActiveUser;

	TWeakObjectPtr<USceneComponent> SprayFX;
	float StethoscopeTimer = 0.f;
	float WaterPatchTimer = 0.f;
};
