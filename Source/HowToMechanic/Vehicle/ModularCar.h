#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Interaction/Interactable.h"
#include "Parts/SurfaceTreatable.h"
#include "Parts/PartTypes.h"
#include "Vehicle/CarModelTypes.h"
#include "ModularCar.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;
class UModularVehicleMovement;
class UEngineTemperatureComponent;
class ACarPart;
class ACarFireActor;
class AMechanicCharacter;
struct FInputActionValue;

/** Estado replicado de un slot: QUÉ pieza, cuántos tornillos apretados, si está al revés. */
USTRUCT(BlueprintType)
struct FCarSlotState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName SlotName;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<ACarPart> Part = nullptr;
	UPROPERTY(BlueprintReadOnly) uint8 BoltsTight = 0;
	UPROPERTY(BlueprintReadOnly) uint8 BoltsTotal = 0;
	UPROPERTY(BlueprintReadOnly) bool bReversed = false;
};

/** Estadísticas CALCULADAS a partir de las piezas montadas (GDD §7.6). Local en cada máquina. */
struct FCarComputedStats
{
	float TotalMassKg = 0.f;
	FVector CenterOfMassLocal = FVector::ZeroVector;
	float EngineTorqueNm = 0.f;
	/** +1 normal, -1 motor montado al revés (funciona "a su manera"). */
	float EngineDirection = 1.f;
	float MaxSpeedKmh = 0.f;
	float FuelUse = 0.f;
	float HeatGeneration = 0.f;
	float Cooling = 0.f;
	float Downforce = 0.f;
	float Drag = 0.f;
	float SteerBias = 0.f;
	bool bEngineWorks = false;
	bool bHasHorn = false;
	FName HornSound;

	// Por rueda (0 FL, 1 FR, 2 RL, 3 RR)
	bool bWheelPresent[4] = { false, false, false, false };
	FVector WheelAnchorLocal[4] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };
	float WheelRadius[4] = { 0.f, 0.f, 0.f, 0.f };
	float Grip[4] = { 0.f, 0.f, 0.f, 0.f };
	float RestLength[4] = { 0.f, 0.f, 0.f, 0.f };
	float Stiffness[4] = { 0.f, 0.f, 0.f, 0.f };
	float Damping[4] = { 0.f, 0.f, 0.f, 0.f };
	float BrakePower[4] = { 0.f, 0.f, 0.f, 0.f };
	bool bDriven[4] = { false, false, false, false };
	bool bSteers[4] = { false, false, false, false };
};

/**
 * Coche modular (GDD §7): un chasis físico con slots; cada pieza es un actor independiente.
 *
 * Conducción: vehículo de raycast propio sobre Chaos Physics (ver ADR-001 en docs/TECH_DESIGN.md):
 * cada rueda montada es un rayo de suspensión; sin rueda, no hay fuerza en esa esquina. Así una
 * rueda floja puede salir rodando en plena prueba y las estadísticas salen de las piezas reales.
 *
 * Red: servidor autoritativo. El conductor envía entradas por RPC; el servidor simula; los demás
 * reciben ReplicatedMovement (física replicada). Slots, motor, freno de mano, rasgos... replicados.
 */
UCLASS()
class HOWTOMECHANIC_API AModularCar : public APawn, public IInteractable, public ISurfaceTreatable
{
	GENERATED_BODY()

public:
	AModularCar();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	// ------------------------------------------------------------------ Inicialización (servidor)
	void InitModel(FName InModelId, const FLinearColor& BodyColor);
	void SetIdentity(const FText& InName, const FString& InPlate, const TArray<ECarTrait>& InTraits);
	void SetCustomerJob(int32 InJobUid) { JobUid = InJobUid; bOwnedByWorkshop = InJobUid == 0; }
	/** Guardado: carrocería y récords medidos. */
	void RestoreRecords(const FPartSurfaceState& InChassis, float InTopSpeedKmh, float InAirTime);

	// ------------------------------------------------------------------ Consultas
	const FCarModelRow* GetModel() const;
	FName GetModelId() const { return ModelId; }
	UBoxComponent* GetChassis() const { return Chassis; }
	UModularVehicleMovement* GetVehicleMovement() const { return VehicleMovement; }
	UEngineTemperatureComponent* GetTemperature() const { return Temperature; }
	const FCarComputedStats& GetStats() const { return Stats; }
	const TArray<FCarSlotState>& GetSlots() const { return Slots; }
	const FCarSlotState* GetSlotState(FName SlotName) const;
	const FCarSlotDefinition* GetSlotDefinition(FName SlotName) const;
	bool IsSlotAccessible(FName SlotName) const;
	ACarPart* GetPartInSlot(FName SlotName) const;
	TArray<ACarPart*> GetMountedParts() const;
	bool IsEngineRunning() const { return bEngineRunning; }
	bool IsHandbrakeOn() const { return bHandbrake; }
	bool IsFlipped() const { return bFlipped; }
	AMechanicCharacter* GetDriver() const { return Driver; }
	const FText& GetCarName() const { return CarName; }
	const FString& GetPlate() const { return Plate; }
	const TArray<ECarTrait>& GetTraits() const { return Traits; }
	bool HasTrait(ECarTrait Trait) const { return Traits.Contains(Trait); }
	int32 GetJobUid() const { return JobUid; }
	bool IsOwnedByWorkshop() const { return bOwnedByWorkshop; }
	float GetBestTopSpeedKmh() const { return BestTopSpeedKmh; }
	float GetMaxAirTime() const { return MaxAirTime; }
	float GetForwardSpeed() const;
	/** 0..1+: vibración que suelta piezas flojas (motor en marcha, baches, velocidad). */
	float GetVibration() const;
	const FPartSurfaceState& GetChassisSurface() const { return ChassisSurface; }
	FVector GetSlotWorldLocation(FName SlotName) const;

	// ------------------------------------------------------------------ Montaje (servidor)
	bool FindSnapSlot(const ACarPart* Part, FName& OutSlot, bool& bOutReversed, float& OutDistance) const;
	bool MountPart(ACarPart* Part, FName SlotName, bool bReversed, AMechanicCharacter* Who, bool bFullyTightened);
	void DetachPart(ACarPart* Part, bool bInheritVelocity, bool bCascade);
	void TightenBolt(FName SlotName, AMechanicCharacter* Who);
	void LoosenBolt(FName SlotName, AMechanicCharacter* Who);
	void SetBolts(FName SlotName, int32 Bolts);
	void OnPartBroken(ACarPart* Part);
	void RecalculateStats();

	// ------------------------------------------------------------------ Estado (servidor)
	void SetEngineRunning(bool bRunning, AMechanicCharacter* Who);
	void SetHandbrake(bool bOn);
	void RecordTopSpeed(float Kmh);
	void DamagePartsNear(const FVector& WorldLocation, float Radius, float Amount);
	int32 RevealAllFaults(AMechanicCharacter* Who);
	void StartFire();
	void OnFireExtinguished();
	void SplashAllSurfaces(ESurfaceTreatment Treatment, float Amount);
	void RegisterPusher(AMechanicCharacter* Pusher);
	/** Grúa/recuperación: recolocar derecho en un punto. */
	void ResetTo(const FTransform& Where);
	void EjectDriver(const FVector& Impulse);
	void OnSpeedTrap(float Kmh) { RecordTopSpeed(Kmh); }
	void RecordAirTime(float Seconds) { if (HasAuthority() && Seconds > MaxAirTime) { MaxAirTime = Seconds; } }

	// ------------------------------------------------------------------ Evaluación (encargos / venta)
	bool HasFault(bool bOnlyNoisy) const;
	bool HasPartId(FName PartId) const;
	bool HasPartWithTag(FName Tag) const;
	bool HasCategory(EPartCategory Category) const;
	bool HasVividPaint() const;
	int32 CountDistinctPaintColors() const;
	float GetFuelUse() const;
	float GetMinWheelRadius() const;
	bool AllRequiredSlotsFilled() const;
	bool HasLooseParts() const;
	bool HasBrokenParts() const;
	float GetAverageCondition() const;
	int32 EstimateValue() const;

	// ------------------------------------------------------------------ IInteractable
	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual float GetHoldDuration(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual bool IsHoldRepeatable(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;
	virtual void SetHighlighted(bool bHighlighted) override;

	// ------------------------------------------------------------------ ISurfaceTreatable (carrocería)
	virtual void TreatSurface(ESurfaceTreatment Treatment, float Amount, const FLinearColor& Color, AMechanicCharacter* By) override;

	/** Entradas del conductor (locales). */
	float GetInputThrottle() const { return InputThrottle; }
	float GetInputBrake() const { return InputBrake; }
	float GetInputSteer() const { return InputSteer; }

protected:
	// Input del conductor
	void InputThrottleAxis(const FInputActionValue& Value);
	void InputBrakeAxis(const FInputActionValue& Value);
	void InputSteerAxis(const FInputActionValue& Value);
	void InputThrottleReleased();
	void InputBrakeReleased();
	void InputSteerReleased();
	void InputHandbrakeStart();
	void InputHandbrakeStop();
	void InputHorn();
	void InputExit();
	void InputLook(const FInputActionValue& Value);

	UFUNCTION(Server, Unreliable) void ServerSetDriveInput(int8 Throttle, int8 Brake, int8 Steer, bool bHandbrakeHeld);
	UFUNCTION(Server, Reliable) void ServerExit();
	UFUNCTION(Server, Unreliable) void ServerHorn();
	UFUNCTION(NetMulticast, Unreliable) void MulticastHorn(const FText& Sound);
	UFUNCTION(NetMulticast, Unreliable) void MulticastSparks(FVector_NetQuantize Location, float Scale);

	UFUNCTION() void OnRep_Model();
	UFUNCTION() void OnRep_Slots();
	UFUNCTION() void OnRep_ChassisSurface();
	UFUNCTION() void OnRep_Engine();
	UFUNCTION() void OnRep_Identity();

	UFUNCTION()
	void OnChassisHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void BuildFromModel();
	void EnterAsDriver(AMechanicCharacter* Who);
	void ExitDriver();
	FVector FindExitLocation() const;
	int32 FindSlotIndex(FName SlotName) const;
	void ServerTickLooseParts(float DeltaSeconds);
	void ServerTickFaults(float DeltaSeconds);
	void ServerTickFlip(float DeltaSeconds);
	void ServerTickRunningEngine(float DeltaSeconds);
	void ServerTickTraits(float DeltaSeconds);
	void SendDriveInputIfChanged(float DeltaSeconds);

	// ------------------------------------------------------------------ Componentes
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Chassis;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> CabinCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> BodyVisual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> CabinVisual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> DriverSeat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> PlateText;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UModularVehicleMovement> VehicleMovement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UEngineTemperatureComponent> Temperature;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BodyMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CabinMID;

	// ------------------------------------------------------------------ Estado replicado
	UPROPERTY(ReplicatedUsing = OnRep_Model) FName ModelId;
	UPROPERTY(ReplicatedUsing = OnRep_Slots) TArray<FCarSlotState> Slots;
	UPROPERTY(ReplicatedUsing = OnRep_ChassisSurface) FPartSurfaceState ChassisSurface;
	UPROPERTY(ReplicatedUsing = OnRep_Engine) bool bEngineRunning = false;
	UPROPERTY(Replicated) bool bHandbrake = true;
	UPROPERTY(Replicated) bool bFlipped = false;
	UPROPERTY(Replicated) TObjectPtr<AMechanicCharacter> Driver;
	UPROPERTY(Replicated) TObjectPtr<ACarFireActor> Fire;
	UPROPERTY(ReplicatedUsing = OnRep_Identity) FText CarName;
	UPROPERTY(ReplicatedUsing = OnRep_Identity) FString Plate;
	UPROPERTY(Replicated) TArray<ECarTrait> Traits;
	UPROPERTY(Replicated) int32 JobUid = 0;
	UPROPERTY(Replicated) bool bOwnedByWorkshop = true;
	UPROPERTY(Replicated) float BestTopSpeedKmh = 0.f;
	UPROPERTY(Replicated) float MaxAirTime = 0.f;

	FCarComputedStats Stats;

	// Entradas (local del conductor y servidor)
	float InputThrottle = 0.f;
	float InputBrake = 0.f;
	float InputSteer = 0.f;
	bool bInputHandbrake = false;
	int8 LastSentThrottle = 0, LastSentBrake = 0, LastSentSteer = 0;
	bool bLastSentHandbrake = false;
	float InputSendTimer = 0.f;

	// Servidor
	float FlippedTimer = 0.f;
	bool bFlipReported = false;
	float LooseTimer = 0.f;
	float FaultTimer = 0.f;
	float ShoveTimer = 0.f;
	float TraitTimer = 5.f;
	float RunawayCooldown = 0.f;
	float SparksCooldown = 0.f;
	TMap<TWeakObjectPtr<AMechanicCharacter>, float> RecentPushers;
	TMap<TWeakObjectPtr<AMechanicCharacter>, float> RecentFlippers;
	TWeakObjectPtr<AMechanicCharacter> LastStarter;
	float LastIgnitionAttempt = -10.f;
};
