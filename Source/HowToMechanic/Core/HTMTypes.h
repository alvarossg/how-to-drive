#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "HTMTypes.generated.h"

/** Canal de traza de interacción (definido en DefaultEngine.ini). */
#define ECC_Interaction ECC_GameTraceChannel1

/** Categoría de peso: decide cómo se coge un objeto (GDD §6). */
UENUM(BlueprintType)
enum class EHTMWeightClass : uint8
{
	Light,   // una mano, se lanza
	Medium,  // dos manos, más lento y peor visión
	Heavy    // dos jugadores o herramienta; solo se arrastra
};

/** Verbo de interacción. Todo pasa por UInteractionComponent + IInteractable. */
UENUM(BlueprintType)
enum class EInteractionVerb : uint8
{
	Grab,    // coger / soltar / arrancar pieza sin tornillos
	Use,     // usar (mantener): apretar, arrancar motor, bombear gato, liberar atrapado...
	AltUse,  // uso secundario (mantener): aflojar, freno de mano, bajar...
	Enter    // subir / bajar del coche
};

UENUM(BlueprintType)
enum class EToolType : uint8
{
	None,
	Wrench,       // motor, escape, suspensión
	TireIron,     // ruedas
	Screwdriver,  // carrocería, luces, cristales, interior, accesorios
	Stethoscope,
	Scanner,
	Extinguisher,
	Hose,
	Sponge,
	Sander,
	PaintGun
};

UENUM(BlueprintType)
enum class EPartCategory : uint8
{
	Engine,
	Exhaust,
	Wheel,
	Suspension,
	Hood,
	Door,
	Trunk,
	Bumper,
	Spoiler,
	Headlight,
	Taillight,
	Windshield,
	Window,
	Seat,
	SteeringWheel,
	Radio,
	Horn,
	Accessory
};

/** Estado legible de una pieza (derivado de condición, óxido y suciedad). */
UENUM(BlueprintType)
enum class EPartState : uint8
{
	Good,
	Damaged,
	Broken,
	Rusty,
	Dirty
};

/** Averías ocultas (GDD §7.4). Viven en la pieza: cambiar la pieza las repara. */
UENUM(BlueprintType)
enum class EHiddenFault : uint8
{
	None,
	EngineKnock,      // ruido; menos par y más calor
	WornBearing,      // ruido; la rueda se suelta con más facilidad
	ExhaustLeak,      // ruido; más calor
	BrakesWorn,       // los frenos fallan en la prueba
	WaterPumpFailing, // se sobrecalienta el doble
	SlowPuncture      // la rueda se va deshinchando durante la prueba
};

/** Rasgos de personalidad del coche (GDD §7.5). */
UENUM(BlueprintType)
enum class ECarTrait : uint8
{
	None,
	HornSticks,
	DriverDoorJammed,
	SmellsWeird,
	PullsLeft,
	PullsRight,
	RadioStuckOn,
	WobblyWheels
};

UENUM(BlueprintType)
enum class EMechanicState : uint8
{
	Normal,
	Stumbling,
	KnockedOut,
	Trapped,
	Driving
};

UENUM(BlueprintType)
enum class EMechanicStance : uint8
{
	Standing,
	Crouching,
	Prone
};

UENUM(BlueprintType)
enum class ECarryMode : uint8
{
	None,
	OneHand,
	TwoHands,
	Heavy
};

UENUM(BlueprintType)
enum class EFaceExpression : uint8
{
	Happy,
	Effort,
	Scared,
	KO,
	Angry,
	Surprised
};

UENUM(BlueprintType)
enum class EDayPhase : uint8
{
	Closed,   // antes de abrir: se puede preparar el taller
	Open,     // jornada en marcha, llegan clientes
	Summary   // resumen de fin de día
};

/** Situaciones de la tabla GDD §13. La telemetría las cuenta para el playtest (fase 5). */
UENUM(BlueprintType)
enum class EHTMSituation : uint8
{
	SimultaneousSnap,        // dos jugadores colocan una pieza a la vez y se empujan
	ToolForgottenUnderCar,   // herramienta olvidada bajo un coche
	EngineStartedWhileWorking,
	CarRanAway,              // coche sale disparado del taller
	WheelRollingAway,
	TripCarryingHeavy,
	FreshPaintRuined,
	PartMountedReversed,
	ReturnedWithoutPart,     // volver de la prueba sin puerta
	EngineSmoking,
	TeammateCarBroken,
	TrappedUnderCar,
	GroupPush,
	DependencyCascade,       // reparación simple que empeora todo
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EHTMFX : uint8
{
	Smoke,
	Fire,
	Sparks,
	KOStars,
	DustPoof,
	WaterSplash,
	PaintSplash,
	Scrap,
	Confetti
};

UENUM(BlueprintType)
enum class ECosmeticSlot : uint8
{
	Hat,
	Outfit,
	Gloves,
	ToolColor
};

/** Logros absurdos que desbloquean cosméticos (GDD §12). */
UENUM(BlueprintType)
enum class EHTMAchievement : uint8
{
	EngineFellOnYou,
	TimesTrapped,
	WheelsLost,
	FiresStarted,
	TimesKO,
	CarsFlipped,
	PerfectJobs,
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ETelemetryEvent : uint8
{
	SessionStart,
	DayStart,
	DayEnd,
	PartDetached,
	KO,
	Stumble,
	CarFlipped,
	JobCompleted,
	JobPartial,
	JobFailed,
	EngineSmoke,
	EngineFire,
	Trapped,
	PaintRuined,
	CarCrash,
	CarBought,
	CarSold,
	PartBought,
	Situation
};

/** Opciones de sala (GDD §14). Las decide el anfitrión. */
USTRUCT(BlueprintType)
struct FHTMRoomOptions
{
	GENERATED_BODY()

	/** Física "caótica": más desprendimientos, tropiezos y KOs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bChaoticPhysics = false;

	/** Fuego amigo de empujones: los objetos lanzados por compañeros y los empujones tumban. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bFriendlyPush = true;
};

namespace HTM
{
	/** Nombre legible de un verbo para el HUD. */
	HOWTOMECHANIC_API FText VerbKeyHint(EInteractionVerb Verb);
	HOWTOMECHANIC_API FText ToolName(EToolType Tool);
	HOWTOMECHANIC_API FText SituationName(EHTMSituation Situation);
	HOWTOMECHANIC_API FText FaultName(EHiddenFault Fault);
	HOWTOMECHANIC_API bool IsNoisyFault(EHiddenFault Fault);
	HOWTOMECHANIC_API bool IsBoltTool(EToolType Tool);
}
