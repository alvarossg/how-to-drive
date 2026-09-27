#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ModularVehicleMovement.generated.h"

class AModularCar;

/**
 * Conducción de raycast sobre Chaos Physics (ADR-001). Cada rueda MONTADA es un rayo de suspensión
 * con muelle-amortiguador, tracción limitada por agarre×carga y agarre lateral. Las fuerzas se
 * aplican en el punto de contacto, así que un motor enorme en un coche ligero hace caballitos,
 * ruedas gigantes suben el centro de masas y vuelcan, y una suspensión baja roza (GDD §7.6).
 *
 * Solo aplica fuerzas en el servidor. En clientes calcula el contacto para efectos visuales.
 */
UCLASS(ClassGroup = (HTM), meta = (BlueprintSpawnableComponent))
class HOWTOMECHANIC_API UModularVehicleMovement : public UActorComponent
{
	GENERATED_BODY()

public:
	UModularVehicleMovement();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool IsWheelGrounded(int32 Index) const { return Index >= 0 && Index < 4 && bGrounded[Index]; }
	int32 GetNumGrounded() const;
	float GetSpeedKmh() const { return SpeedKmh; }

private:
	AModularCar* GetCar() const;

	float PrevCompression[4] = { 0.f, 0.f, 0.f, 0.f };
	bool bGrounded[4] = { false, false, false, false };
	float AirTimer = 0.f;
	float SpeedKmh = 0.f;
	float ToolUnderCarCooldown = 0.f;
};
