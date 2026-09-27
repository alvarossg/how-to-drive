#include "Vehicle/ModularVehicleMovement.h"
#include "Vehicle/ModularCar.h"
#include "Tools/Tool.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMTelemetrySubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

UModularVehicleMovement::UModularVehicleMovement()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

AModularCar* UModularVehicleMovement::GetCar() const
{
	return Cast<AModularCar>(GetOwner());
}

int32 UModularVehicleMovement::GetNumGrounded() const
{
	int32 N = 0;
	for (int32 i = 0; i < 4; ++i) { N += bGrounded[i] ? 1 : 0; }
	return N;
}

void UModularVehicleMovement::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AModularCar* Car = GetCar();
	UBoxComponent* Body = Car ? Car->GetChassis() : nullptr;
	if (!Body || !Body->IsSimulatingPhysics() || DeltaTime <= 0.f)
	{
		return;
	}
	const bool bAuthority = Car->HasAuthority();
	const UHTMTuningData& T = UHTMTuningData::Get();
	const FCarComputedStats& S = Car->GetStats();
	const FTransform CarT = Car->GetActorTransform();
	const FVector Up = CarT.GetUnitAxis(EAxis::Z);
	const FVector CarFwd = CarT.GetUnitAxis(EAxis::X);
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	const float Mass = Body->GetMass();
	const float QuarterMass = Mass * 0.25f;
	SpeedKmh = Velocity.Size() * 0.036f;

	// ------------------------------------------------------------------ Entradas
	const bool bDriver = Car->GetDriver() != nullptr;
	float Throttle = 0.f;
	float BrakeInput = 0.f;
	if (Car->IsEngineRunning() && S.bEngineWorks)
	{
		if (bDriver)
		{
			const float FwdSpeed = FVector::DotProduct(Velocity, CarFwd);
			Throttle = Car->GetInputThrottle();
			// Freno = marcha atrás cuando está casi parado.
			if (Car->GetInputBrake() > 0.f && FwdSpeed < 50.f && Throttle <= 0.f)
			{
				Throttle = -0.5f * Car->GetInputBrake();
			}
			else
			{
				BrakeInput = Car->GetInputBrake();
			}
		}
		else if (!Car->IsHandbrakeOn())
		{
			// "En marcha" sin conductor: avanza despacito... y se escapa (GDD §13).
			Throttle = T.IdleCreepThrottle;
		}
	}
	else if (bDriver)
	{
		BrakeInput = Car->GetInputBrake();
	}
	Throttle *= S.EngineDirection;
	const bool bHandbrake = Car->IsHandbrakeOn() || (bDriver && Car->GetInputBrake() > 0.95f && Car->GetInputThrottle() > 0.95f);
	const float Steer = FMath::Clamp((bDriver ? Car->GetInputSteer() : 0.f) + S.SteerBias, -1.f, 1.f);
	const float SteerAngle = Steer * T.MaxSteerAngle / (1.f + Velocity.Size() / 2500.f);

	int32 DrivenPresent = 0;
	for (int32 i = 0; i < 4; ++i) { DrivenPresent += (S.bWheelPresent[i] && S.bDriven[i]) ? 1 : 0; }

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMWheelRay), false, Car);
	TArray<AActor*> Attached;
	Car->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_Vehicle);

	ToolUnderCarCooldown -= DeltaTime;

	// ------------------------------------------------------------------ Ruedas
	for (int32 i = 0; i < 4; ++i)
	{
		bGrounded[i] = false;
		if (!S.bWheelPresent[i])
		{
			PrevCompression[i] = 0.f;
			continue;
		}
		const float Radius = FMath::Max(5.f, S.WheelRadius[i]);
		const float Rest = FMath::Max(2.f, S.RestLength[i]);
		const FVector Anchor = CarT.TransformPosition(S.WheelAnchorLocal[i]);
		const FVector End = Anchor - Up * (Rest + Radius);

		FHitResult Hit;
		if (!Car->GetWorld()->LineTraceSingleByObjectType(Hit, Anchor, End, Objects, Params))
		{
			PrevCompression[i] = 0.f;
			continue;
		}
		bGrounded[i] = true;
		if (!bAuthority)
		{
			continue;
		}

		// Muelle-amortiguador. Rigidez normalizada: a Stiffness 1 la esquina se hunde media carrera en reposo.
		const float Compression = FMath::Max(0.f, (Rest + Radius) - Hit.Distance);
		const float K = FMath::Max(0.1f, S.Stiffness[i]) * (QuarterMass * 980.f) / (Rest * 0.5f);
		const float C = S.Damping[i] * 2.f * FMath::Sqrt(K * QuarterMass);
		const float CompressionVel = (Compression - PrevCompression[i]) / DeltaTime;
		PrevCompression[i] = Compression;
		const float SpringForce = FMath::Max(0.f, K * Compression + C * CompressionVel);
		Body->AddForceAtLocation(Up * SpringForce, Anchor);
		const float Load = SpringForce;

		// Lo que hay debajo recibe el golpe: herramientas olvidadas, piezas... (GDD §13)
		if (UPrimitiveComponent* Under = Hit.GetComponent())
		{
			if (Under->IsSimulatingPhysics())
			{
				Under->AddImpulseAtLocation(-Up * FMath::Min(SpringForce * DeltaTime, Under->GetMass() * 400.f), Hit.ImpactPoint);
			}
			if (Cast<ATool>(Hit.GetActor()) && ToolUnderCarCooldown <= 0.f)
			{
				ToolUnderCarCooldown = 10.f;
				if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(Car))
				{
					Tel->RecordSituation(Car, EHTMSituation::ToolForgottenUnderCar, FString(), Hit.ImpactPoint);
				}
			}
		}

		// Marco de la rueda sobre el suelo.
		FVector WheelFwd = CarFwd;
		if (S.bSteers[i])
		{
			WheelFwd = FQuat(Up, FMath::DegreesToRadians(SteerAngle)).RotateVector(CarFwd);
		}
		WheelFwd = FVector::VectorPlaneProject(WheelFwd, Hit.ImpactNormal).GetSafeNormal();
		const FVector WheelRight = FVector::CrossProduct(Hit.ImpactNormal, WheelFwd).GetSafeNormal();
		const FVector PointVel = Body->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
		const float VLong = FVector::DotProduct(PointVel, WheelFwd);
		const float VLat = FVector::DotProduct(PointVel, WheelRight);
		const float GripLimit = FMath::Max(0.f, S.Grip[i]) * Load;
		const bool bRear = i >= 2;

		float FLong = 0.f;
		// Tracción: par del motor → fuerza en la rueda (N → UE ×100), limitada por velocidad máxima.
		if (S.bDriven[i] && DrivenPresent > 0 && !FMath::IsNearlyZero(Throttle))
		{
			const float MaxSpeed = FMath::Max(10.f, S.MaxSpeedKmh) / 0.036f;
			const float SpeedRatio = FMath::Clamp(FMath::Abs(VLong) / MaxSpeed, 0.f, 1.f);
			const float Limiter = (FMath::Sign(VLong) == FMath::Sign(Throttle)) ? (1.f - SpeedRatio * SpeedRatio) : 1.f;
			const float FinalDrive = Car->GetModel() ? Car->GetModel()->FinalDriveRatio : 7.f;
			FLong += Throttle * S.EngineTorqueNm * FinalDrive / (Radius / 100.f) / DrivenPresent * 100.f * Limiter;
		}
		// Freno de servicio.
		if (BrakeInput > 0.f)
		{
			const float BrakeF = T.BrakeForceNewtons * 100.f * 0.25f * S.BrakePower[i] * BrakeInput;
			FLong -= FMath::Sign(VLong) * FMath::Min(BrakeF, FMath::Abs(VLong) * QuarterMass / DeltaTime);
		}
		// Rodadura.
		FLong -= FMath::Sign(VLong) * FMath::Min(T.RollingResistance * 100.f, FMath::Abs(VLong) / DeltaTime) * QuarterMass;

		// Agarre lateral.
		float LatGrip = GripLimit;
		if (bHandbrake && bRear)
		{
			// Freno de mano: bloquea atrás y deja derrapar.
			FLong = -FMath::Sign(VLong) * FMath::Min(FMath::Abs(VLong) * QuarterMass / DeltaTime, GripLimit * T.HandbrakeGrip);
			LatGrip *= 0.5f;
		}
		float FLat = FMath::Clamp(-VLat * QuarterMass / DeltaTime * T.LateralGripFactor, -LatGrip, LatGrip);
		FLong = FMath::Clamp(FLong, -GripLimit * 1.1f, GripLimit * 1.1f);

		// Círculo de fricción.
		const float Total = FMath::Sqrt(FLong * FLong + FLat * FLat);
		if (Total > GripLimit * 1.2f && Total > KINDA_SMALL_NUMBER)
		{
			const float Scale = GripLimit * 1.2f / Total;
			FLong *= Scale;
			FLat *= Scale;
		}
		// En el punto de contacto: genera el cabeceo que hace los caballitos y los "morros".
		Body->AddForceAtLocation(WheelFwd * FLong + WheelRight * FLat, Hit.ImpactPoint);
	}

	if (!bAuthority)
	{
		return;
	}

	// ------------------------------------------------------------------ Aerodinámica
	const float V10 = Velocity.Size() / 1000.f; // en unidades de 10 m/s
	if (V10 > 0.05f)
	{
		const float DragAccel = (0.3f + S.Drag) * T.AirDragScale * V10 * V10 * 100.f;
		Body->AddForce(-Velocity.GetSafeNormal() * DragAccel * Mass);
		// Alerón: pega o (al revés) levanta el coche.
		Body->AddForce(-Up * S.Downforce * V10 * V10 * 100.f * Mass);
	}

	// ------------------------------------------------------------------ Tiempo en el aire (encargo "que salte la rampa")
	if (GetNumGrounded() == 0 && Velocity.Size() > 300.f)
	{
		AirTimer += DeltaTime;
	}
	else if (AirTimer > 0.f)
	{
		if (AirTimer > Car->GetMaxAirTime())
		{
			Car->RecordAirTime(AirTimer);
		}
		AirTimer = 0.f;
	}
}
