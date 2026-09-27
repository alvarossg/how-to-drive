#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HTMTuningData.generated.h"

/**
 * Panel de tuning del caos (fase 5). Toda cifra de juego vive aquí; el código solo lee.
 * DA_Tuning es una instancia de esta clase. Si no existe, se usan estos valores por defecto.
 *
 * Unidades: cm, s, kg, grados. "Impacto" = velocidad relativa (m/s) × masa (kg, con tope).
 */
UCLASS(BlueprintType)
class HOWTOMECHANIC_API UHTMTuningData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Devuelve DA_Tuning o, si no existe, el CDO con los valores por defecto. */
	static const UHTMTuningData& Get();

	/** Multiplicador según opciones de sala: física caótica. */
	static bool IsChaotic(const UObject* WorldContext);

	// ---------------------------------------------------------------- Personaje
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character") float WalkSpeed = 380.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character") float SprintSpeed = 620.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character") float CrouchSpeed = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character") float ProneSpeed = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character") float JumpZVelocity = 520.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character") float MediumCarrySpeedMult = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character") float HeavyTeamCarrySpeedMult = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character") float HeavySoloDragSpeedMult = 0.3f;
	/** Fuerza con la que un personaje empuja objetos físicos al andar contra ellos (se suma entre jugadores). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character") float PushForceFactor = 180000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character") float ShoutCooldown = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character") float ShoveImpulse = 520.f;

	// ---------------------------------------------------------------- Interacción
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float MaxInteractDistance = 260.f;
	/** Holgura extra que acepta el servidor (latencia). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float ServerDistanceSlack = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float InteractTraceRadius = 18.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float ThrowSpeedLight = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float ThrowSpeedMedium = 750.f;
	/** Masa (kg) a partir de la cual un objeto es medio / pesado si no se indica categoría. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float MediumMassKg = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float HeavyMassKg = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float HeavyHandleStiffness = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float HeavyHandleDamping = 250.f;
	/** Altura objetivo (sobre la pelvis) al cargar pesado entre dos, y en solitario (arrastre). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float HeavyTeamHoldHeight = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float HeavySoloHoldHeight = -70.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float HeavyHoldForward = 95.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction") float HandleBreakDistance = 220.f;

	// ---------------------------------------------------------------- Impactos, KO y tropiezos
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float ImpactMassCapKg = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float StumbleImpactThreshold = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float KOImpactThreshold = 95.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float KODurationMin = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float KODurationMax = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float StumbleDuration = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float ImpactCooldown = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float TripMinSpeed = 240.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float TripChance = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float TripObjectMaxHeight = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float TripCooldown = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float MediumBalancePenalty = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float HeavyBalancePenalty = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float WetBalancePenalty = 0.35f;
	/** Tiempo que un objeto lanzado cuenta como "lanzado por" alguien (fuego amigo). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos") float ThrowAttributionTime = 3.0f;

	// ---------------------------------------------------------------- Piezas
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts") float BoltTime = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts") float SnapRadius = 75.f;
	/** Probabilidad por segundo base de que una pieza floja se suelte con el coche en marcha. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts") float LooseDetachChancePerSec = 0.015f;
	/** Suma por cada 1000 cm/s de velocidad. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts") float LooseDetachSpeedFactor = 0.12f;
	/** Suma por unidad de vibración (motor en marcha = 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts") float LooseDetachVibrationFactor = 0.03f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts") float LooseImpactDetachChance = 0.4f;
	/** Multiplicador si la pieza no tiene NINGÚN tornillo apretado. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts") float ZeroBoltsDetachMult = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts") float CarDamageSpeedThreshold = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts") float PartDamagePerSpeed = 0.035f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts") float DamageRadius = 170.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts") float SlowPunctureRate = 0.02f;

	// ---------------------------------------------------------------- Motor
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engine") float AmbientTemp = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engine") float WarnTemp = 105.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engine") float SmokeTemp = 115.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engine") float FireTemp = 130.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engine") float HeatScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engine") float CoolingScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engine") float FireDamagePerSec = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engine") float ExtinguishPerSec = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engine") float FireGrowPerSec = 0.05f;

	// ---------------------------------------------------------------- Vehículo
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float LateralGripFactor = 0.55f;
	/** Deceleración por rodadura (m/s²). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float RollingResistance = 0.6f;
	/** Escala de la deceleración aerodinámica: Drag 1 = 1 m/s² a 10 m/s (cuadrática). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float AirDragScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float BrakeForceNewtons = 9000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float IdleCreepThrottle = 0.07f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float MaxSteerAngle = 34.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float EjectDeltaV = 1400.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float FlippedUpDot = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float FlippedTime = 1.5f;
	/** Impulso angular (rad/s) que da cada empujón para enderezar un coche volcado (se suma entre jugadores). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float FlipImpulsePerPush = 0.9f;
	/** Agarre longitudinal de las ruedas traseras con el freno de mano (fracción del agarre). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float HandbrakeGrip = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float DefaultWheelRadius = 30.f;
	/** Fuerza con la que ruedas y ventilador empujan a quien esté cerca con el motor en marcha. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float RunningEngineShove = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float RunawaySpeed = 300.f;
	/** Velocidad lateral (cm/s) del punto de apoyo a partir de la cual el gato vuelca. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float JackTipLateralSpeed = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float JackTipOffset = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float JackLiftSpeed = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float JackMaxHeight = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float LiftSpeed = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float LiftMaxHeight = 170.f;
	/** Holgura mínima bajo el coche para no quedar atrapado (tumbado). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float TrappedClearance = 38.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float FreeTrappedTime = 1.5f;

	// ---------------------------------------------------------------- Pintura y limpieza
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") float SprayRange = 380.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") float PaintRate = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") float PaintDryTime = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") float PaintBoothDryMult = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") float WetDryTime = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") float HoseCleanRate = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") float SpongeCleanRate = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") float SandRate = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") float WaterPatchLifetime = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") float WetGroundFriction = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") float WetBrakingDecel = 150.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint") int32 MaxWaterPatches = 24;

	// ---------------------------------------------------------------- Economía y jornada
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy") int32 StartMoney = 600;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy") int32 TowFee = 150;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy") int32 LostItemFee = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy") float ScrapValueFraction = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy") float PartialPayFraction = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy") float DeadlineBonusFraction = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy") float RepFull = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy") float RepPartial = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy") float RepAngry = -6.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day") float DayLengthSeconds = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day") int32 JobsPerDayBase = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day") int32 JobsPerExtraPlayer = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day") float FirstCustomerDelay = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day") float CustomerInterval = 170.f;
	/** Los plazos se multiplican por esto con 1 jugador (y se interpola hasta 1 con 4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day") float SoloTimeLimitMult = 1.6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day") int32 JunkyardOffersPerDay = 3;
	/** Ningún trabajo individual debería superar esto (GDD §14). Se usa como plazo máximo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day") float MaxJobSeconds = 900.f;

	// ---------------------------------------------------------------- Física: presupuesto
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics Budget") int32 MaxActiveBodies = 150;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics Budget") float FreezeDistance = 6000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics Budget") float UnfreezeDistance = 4500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics Budget") float FreezeMaxMassKg = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics Budget") float BudgetInterval = 0.5f;

	// ---------------------------------------------------------------- Física caótica (opción de sala)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaotic Mode") float ChaoticDetachMult = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaotic Mode") float ChaoticTripMult = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaotic Mode") float ChaoticImpactThresholdMult = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaotic Mode") float ChaoticThrowMult = 1.3f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaotic Mode") float ChaoticHeatMult = 1.5f;

	// ---------------------------------------------------------------- Accesos con modo caótico aplicado
	float GetDetachMult(const UObject* Ctx) const { return IsChaotic(Ctx) ? ChaoticDetachMult : 1.f; }
	float GetTripChance(const UObject* Ctx) const { return TripChance * (IsChaotic(Ctx) ? ChaoticTripMult : 1.f); }
	float GetStumbleThreshold(const UObject* Ctx) const { return StumbleImpactThreshold * (IsChaotic(Ctx) ? ChaoticImpactThresholdMult : 1.f); }
	float GetKOThreshold(const UObject* Ctx) const { return KOImpactThreshold * (IsChaotic(Ctx) ? ChaoticImpactThresholdMult : 1.f); }
	float GetThrowMult(const UObject* Ctx) const { return IsChaotic(Ctx) ? ChaoticThrowMult : 1.f; }
	float GetHeatMult(const UObject* Ctx) const { return HeatScale * (IsChaotic(Ctx) ? ChaoticHeatMult : 1.f); }
};
