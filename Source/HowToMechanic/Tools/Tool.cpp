#include "Tools/Tool.h"
#include "Tools/WaterPatch.h"
#include "Character/MechanicCharacter.h"
#include "Parts/CarPart.h"
#include "Parts/SurfaceTreatable.h"
#include "Vehicle/ModularCar.h"
#include "Vehicle/CarFireActor.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Core/HTMGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HTMTool"

ATool::ATool()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	bTripHazard = true;
	Spec = MakeSpecFor(ToolType);
}

void ATool::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATool, ToolType);
	DOREPLIFETIME(ATool, bInUse);
	DOREPLIFETIME(ATool, PaintColor);
	DOREPLIFETIME(ATool, PaintColorIndex);
}

FGrabbablePropSpec ATool::MakeSpecFor(EToolType Type)
{
	// Herramientas exageradas 1,2–1,5× (ART_DIRECTION §2). Rojo/azul de la paleta.
	FGrabbablePropSpec S;
	S.bForceWeightClass = true;
	S.WeightClass = EHTMWeightClass::Light;
	S.DisplayName = HTM::ToolName(Type);
	switch (Type)
	{
	case EToolType::Wrench:       S.Shape = TEXT("Cube");     S.SizeCm = FVector(55.f, 10.f, 5.f);  S.Color = HTMPalette::ToolRed();  S.MassKg = 1.2f; break;
	case EToolType::TireIron:     S.Shape = TEXT("Cube");     S.SizeCm = FVector(60.f, 30.f, 5.f);  S.Color = HTMPalette::ToolBlue(); S.MassKg = 1.8f; break;
	case EToolType::Screwdriver:  S.Shape = TEXT("Cylinder"); S.SizeCm = FVector(7.f, 7.f, 40.f);   S.Color = HTMPalette::SignalYellow(); S.MassKg = 0.4f; S.MeshRotation = FRotator(90.f, 0.f, 0.f); break;
	case EToolType::Stethoscope:  S.Shape = TEXT("Sphere");   S.SizeCm = FVector(18.f, 18.f, 8.f);  S.Color = HTMPalette::Chrome(); S.MassKg = 0.3f; break;
	case EToolType::Scanner:      S.Shape = TEXT("Cube");     S.SizeCm = FVector(25.f, 16.f, 8.f);  S.Color = HTMPalette::SafetyOrange(); S.MassKg = 0.8f; break;
	case EToolType::Extinguisher: S.Shape = TEXT("Cylinder"); S.SizeCm = FVector(22.f, 22.f, 55.f); S.Color = HTMPalette::ToolRed(); S.MassKg = 6.f; break;
	case EToolType::Hose:         S.Shape = TEXT("Cylinder"); S.SizeCm = FVector(10.f, 10.f, 45.f); S.Color = HTMPalette::CarLime(); S.MassKg = 1.5f; S.MeshRotation = FRotator(90.f, 0.f, 0.f); break;
	case EToolType::Sponge:       S.Shape = TEXT("Cube");     S.SizeCm = FVector(22.f, 14.f, 10.f); S.Color = HTMPalette::SignalYellow(); S.MassKg = 0.2f; break;
	case EToolType::Sander:       S.Shape = TEXT("Cube");     S.SizeCm = FVector(30.f, 18.f, 16.f); S.Color = HTMPalette::ToolBlue(); S.MassKg = 2.5f; break;
	case EToolType::PaintGun:     S.Shape = TEXT("Cone");     S.SizeCm = FVector(20.f, 20.f, 35.f); S.Color = HTMPalette::White(); S.MassKg = 1.4f; S.MeshRotation = FRotator(-90.f, 0.f, 0.f); break;
	default: break;
	}
	return S;
}

void ATool::InitTool(EToolType InType)
{
	ToolType = InType;
	SetPropSpec(MakeSpecFor(InType));
	PaintColor = HTMPalette::PaintGunColors()[0];
}

void ATool::OnRep_ToolType()
{
	ApplyVisuals();
}

bool ATool::IsContinuous() const
{
	return !HTM::IsBoltTool(ToolType) && ToolType != EToolType::Scanner && ToolType != EToolType::None;
}

FText ATool::GetDisplayName() const
{
	return HTM::ToolName(ToolType);
}

FRotator ATool::GetCarryRotationOffset() const
{
	return Spec.MeshRotation;
}

void ATool::SetTint(const FLinearColor& Tint)
{
	if (MID && HTM::IsBoltTool(ToolType))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Tint);
		MID->SetVectorParameterValue(TEXT("BaseColor"), Tint);
	}
}

void ATool::OnDropped(AMechanicCharacter* Who, const FVector& Velocity, bool bThrown)
{
	if (bInUse)
	{
		SetUsing(Who, false, false);
	}
	Super::OnDropped(Who, Velocity, bThrown);
}

void ATool::SetUsing(AMechanicCharacter* User, bool bActive, bool bAlt)
{
	check(HasAuthority());
	if (ToolType == EToolType::Scanner)
	{
		if (bActive)
		{
			ActiveUser = User;
			UseScanner();
		}
		return;
	}
	bInUse = bActive;
	ActiveUser = bActive ? User : nullptr;
	OnRep_InUse();
}

void ATool::AltUse(AMechanicCharacter* User)
{
	if (ToolType == EToolType::PaintGun)
	{
		const TArray<FLinearColor>& Colors = HTMPalette::PaintGunColors();
		PaintColorIndex = (PaintColorIndex + 1) % Colors.Num();
		PaintColor = Colors[PaintColorIndex];
		OnRep_InUse();
	}
}

void ATool::OnRep_InUse()
{
	// Efecto visible del chorro (local).
	UHTMVisualLibrary::StopFX(SprayFX.Get());
	SprayFX = nullptr;
	if (ToolType == EToolType::PaintGun && MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(HTMPalette::White(), PaintColor, 0.6f));
	}
	if (!bInUse)
	{
		return;
	}
	EHTMFX Type = EHTMFX::DustPoof;
	FLinearColor Tint = FLinearColor::White;
	switch (ToolType)
	{
	case EToolType::Hose:         Type = EHTMFX::WaterSplash; break;
	case EToolType::PaintGun:     Type = EHTMFX::PaintSplash; Tint = PaintColor; break;
	case EToolType::Extinguisher: Type = EHTMFX::Smoke; break;
	case EToolType::Sander:       Type = EHTMFX::Sparks; break;
	default: return;
	}
	SprayFX = UHTMVisualLibrary::SpawnFX(this, Type, FVector(40.f, 0.f, 0.f), 0.6f, GetRootComponent(), -1.f, Tint);
}

void ATool::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && bInUse)
	{
		if (!ActiveUser || !IsCarriedBy(ActiveUser) || !ActiveUser->CanAct())
		{
			SetUsing(nullptr, false, false);
			return;
		}
		ServerToolTick(DeltaSeconds);
	}
}

bool ATool::TraceFromUser(float Range, FHitResult& OutHit) const
{
	if (!ActiveUser)
	{
		return false;
	}
	const FVector Start = ActiveUser->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const FVector Dir = ActiveUser->GetBaseAimRotation().Vector();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMToolTrace), false, ActiveUser);
	Params.AddIgnoredActor(this);
	return GetWorld()->SweepSingleByChannel(OutHit, Start, Start + Dir * Range, FQuat::Identity, ECC_Interaction,
		FCollisionShape::MakeSphere(20.f), Params);
}

void ATool::ServerToolTick(float DeltaSeconds)
{
	switch (ToolType)
	{
	case EToolType::Stethoscope:  TickStethoscope(DeltaSeconds); break;
	case EToolType::Extinguisher: TickExtinguisher(DeltaSeconds); break;
	case EToolType::Hose:
	case EToolType::Sponge:
	case EToolType::Sander:
	case EToolType::PaintGun:     TickSpray(DeltaSeconds); break;
	default: break;
	}
}

void ATool::TickStethoscope(float DeltaSeconds)
{
	StethoscopeTimer -= DeltaSeconds;
	if (StethoscopeTimer > 0.f)
	{
		return;
	}
	StethoscopeTimer = 0.4f;

	FHitResult Hit;
	const bool bHit = TraceFromUser(220.f, Hit);
	const FVector Point = bHit ? Hit.ImpactPoint : ActiveUser->GetActorLocation() + ActiveUser->GetBaseAimRotation().Vector() * 150.f;

	// Escuchar: revela las averías RUIDOSAS de las piezas cercanas al punto (GDD §7.4).
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMStetho), false, this);
	GetWorld()->OverlapMultiByChannel(Overlaps, Point, FQuat::Identity, ECC_Interaction, FCollisionShape::MakeSphere(80.f), Params);
	bool bFound = false;
	for (const FOverlapResult& O : Overlaps)
	{
		if (ACarPart* Part = Cast<ACarPart>(O.GetActor()))
		{
			if (HTM::IsNoisyFault(Part->GetHiddenFault()))
			{
				Part->RevealFault(ActiveUser);
				MulticastBubble(Part->GetActorLocation() + FVector(0, 0, 40), FText::Format(LOCTEXT("Noise", "¡CLONC CLONC! {0}"), HTM::FaultName(Part->GetHiddenFault())));
				bFound = true;
				break;
			}
		}
	}
	if (!bFound && bHit && (Cast<ACarPart>(Hit.GetActor()) || Cast<AModularCar>(Hit.GetActor())))
	{
		MulticastBubble(Point, LOCTEXT("Silence", "...suena bien"));
	}
}

void ATool::UseScanner()
{
	FHitResult Hit;
	if (!TraceFromUser(300.f, Hit))
	{
		return;
	}
	AModularCar* Car = Cast<AModularCar>(Hit.GetActor());
	if (!Car)
	{
		if (ACarPart* Part = Cast<ACarPart>(Hit.GetActor()))
		{
			Car = Part->GetOwningCar();
		}
	}
	if (!Car)
	{
		return;
	}
	// Códigos de avería: revela todo lo que no sea mecánico puro (rodamientos sí: sensor ABS).
	const int32 Found = Car->RevealAllFaults(ActiveUser);
	MulticastBubble(Car->GetActorLocation() + FVector(0, 0, 150),
		Found > 0 ? FText::Format(LOCTEXT("ScanFound", "ESCÁNER: {0} código(s) de avería"), FText::AsNumber(Found)) : LOCTEXT("ScanOk", "ESCÁNER: sin códigos"));
}

void ATool::TickSpray(float DeltaSeconds)
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	const bool bContact = ToolType == EToolType::Sponge || ToolType == EToolType::Sander;
	FHitResult Hit;
	if (!TraceFromUser(bContact ? 170.f : T.SprayRange, Hit))
	{
		return;
	}

	ESurfaceTreatment Treatment = ESurfaceTreatment::Water;
	float Rate = T.HoseCleanRate;
	switch (ToolType)
	{
	case EToolType::PaintGun: Treatment = ESurfaceTreatment::Paint;  Rate = T.PaintRate; break;
	case EToolType::Sponge:   Treatment = ESurfaceTreatment::Sponge; Rate = T.SpongeCleanRate; break;
	case EToolType::Sander:   Treatment = ESurfaceTreatment::Sand;   Rate = T.SandRate; break;
	default: break;
	}

	if (ISurfaceTreatable* Surface = Cast<ISurfaceTreatable>(Hit.GetActor()))
	{
		Surface->TreatSurface(Treatment, Rate * DeltaSeconds, PaintColor, ActiveUser);
		return;
	}

	// Manguera contra el suelo: el agua moja el suelo y hace resbalar (GDD §8).
	if (ToolType == EToolType::Hose && Hit.ImpactNormal.Z > 0.7f)
	{
		WaterPatchTimer -= DeltaSeconds;
		if (WaterPatchTimer <= 0.f)
		{
			WaterPatchTimer = 0.5f;
			AWaterPatch::SpawnOrGrow(this, Hit.ImpactPoint);
		}
	}
	else if (ToolType == EToolType::Hose)
	{
		if (AMechanicCharacter* Victim = Cast<AMechanicCharacter>(Hit.GetActor()))
		{
			Victim->Stumble(ActiveUser->GetBaseAimRotation().Vector().GetSafeNormal2D() * 60.f);
		}
	}
}

void ATool::TickExtinguisher(float DeltaSeconds)
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	const FVector Start = ActiveUser->GetActorLocation() + FVector(0.f, 0.f, 30.f);
	const FVector Dir = ActiveUser->GetBaseAimRotation().Vector();

	for (TActorIterator<ACarFireActor> It(GetWorld()); It; ++It)
	{
		ACarFireActor* Fire = *It;
		const FVector ToFire = Fire->GetActorLocation() - Start;
		const float Dist = ToFire.Size();
		// Cono de ~30º y alcance de la manguera.
		if (Dist < T.SprayRange && FVector::DotProduct(ToFire.GetSafeNormal(), Dir) > 0.85f)
		{
			Fire->Extinguish(T.ExtinguishPerSec * DeltaSeconds);
		}
	}

	// La espuma empuja cosas ligeras (y a los compañeros despistados).
	FHitResult Hit;
	if (TraceFromUser(T.SprayRange * 0.6f, Hit))
	{
		if (UPrimitiveComponent* Comp = Hit.GetComponent())
		{
			if (Comp->IsSimulatingPhysics() && Comp->GetMass() < 30.f)
			{
				Comp->AddImpulse(Dir * 120.f * DeltaSeconds * 60.f, NAME_None, true);
			}
		}
	}
}

void ATool::MulticastBubble_Implementation(FVector_NetQuantize Location, const FText& Text)
{
	if (AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>())
	{
		GS->OnToast.Broadcast(Text, HTMPalette::SignalYellow());
	}
	UHTMVisualLibrary::SpawnFX(this, EHTMFX::DustPoof, Location, 0.3f, nullptr, 0.4f);
}

#undef LOCTEXT_NAMESPACE
