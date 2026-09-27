#include "Progression/WorkshopBlockout.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Core/HTMGameState.h"
#include "Core/HTMSettings.h"
#include "Core/HTMDataSubsystem.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "HTMBlockout"

static TAutoConsoleVariable<int32> CVarOutline(TEXT("htm.Outline"), 0,
	TEXT("Prueba A/B del contorno de objetos (ART_DIRECTION §10). 0 = sin contorno, 1 = con contorno."));

namespace Layout
{
	// Exterior fijo (no depende del nivel del taller).
	constexpr float ApronMinX = 700.f, ApronMaxX = 1400.f;
	constexpr float StreetMinX = 1400.f, StreetMaxX = 2300.f;
	constexpr float StraightY = -2000.f;
	constexpr float ReturnY = 1900.f;
	constexpr float NorthRoadX = 8900.f;
	constexpr float RoadWidth = 600.f;
	const FVector2D CurveCenter(8300.f, -1400.f);
	constexpr float CurveRadius = 600.f;
	constexpr float BayCenterX = 150.f;
}

AWorkshopBlockout::AWorkshopBlockout()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;
	// Postproceso mínimo: ligera saturación, sin grano, sin viñeta fuerte, bloom muy suave (ART_DIRECTION §10).
	FPostProcessSettings& S = PostProcess->Settings;
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(1.12f, 1.12f, 1.12f, 1.f);
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 0.25f;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.12f;
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = 0.f;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.f;
}

AWorkshopBlockout* AWorkshopBlockout::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AWorkshopBlockout> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AWorkshopBlockout::BeginPlay()
{
	Super::BeginPlay();
	BuildStaticWorld();
	BuildLighting();

	AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	BuildWorkshop(GS ? GS->WorkshopLevel : 1);
	if (GS)
	{
		LevelChangedHandle = GS->OnWorkshopLevelChanged.AddWeakLambda(this, [this]()
		{
			if (const AHTMGameState* State = GetWorld()->GetGameState<AHTMGameState>())
			{
				BuildWorkshop(State->WorkshopLevel);
			}
		});
		WallColorHandle = GS->OnWallColorChanged.AddUObject(this, &AWorkshopBlockout::RecolorWalls);
	}

	// Asignar el material de contorno como blendable (peso 0 hasta activarlo).
	if (UMaterialInterface* Outline = UHTMSettings::Get()->OutlinePostProcessMaterial.LoadSynchronous())
	{
		PostProcess->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(0.f, Outline));
	}
}

void AWorkshopBlockout::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AHTMGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHTMGameState>() : nullptr)
	{
		GS->OnWorkshopLevelChanged.Remove(LevelChangedHandle);
		GS->OnWallColorChanged.Remove(WallColorHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AWorkshopBlockout::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const int32 Outline = CVarOutline.GetValueOnGameThread();
	if (Outline != LastOutlineValue)
	{
		LastOutlineValue = Outline;
		SetOutlineEnabled(Outline != 0);
	}
}

void AWorkshopBlockout::SetOutlineEnabled(bool bEnabled)
{
	for (FWeightedBlendable& B : PostProcess->Settings.WeightedBlendables.Array)
	{
		B.Weight = bEnabled ? 1.f : 0.f;
	}
}

UStaticMeshComponent* AWorkshopBlockout::Box(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollision,
	const FRotator& Rotation, bool bWorkshopPart, FName Shape)
{
	UStaticMeshComponent* C = UHTMVisualLibrary::AddShape(this, Root, Shape, Center, Size, Rotation, Color, bCollision);
	if (bWorkshopPart)
	{
		WorkshopComponents.Add(C);
	}
	return C;
}

// ============================================================================ Mundo estático

void AWorkshopBlockout::BuildStaticWorld()
{
	if (bStaticWorldBuilt)
	{
		return;
	}
	bStaticWorldBuilt = true;
	using namespace Layout;
	const FLinearColor Asphalt = HTMPalette::Asphalt();
	const FLinearColor Curb = HTMPalette::FloorWarmGrey();

	// Suelo general, explanada y calle.
	Box(FVector(3300.f, 0.f, -10.f), FVector(14000.f, 8200.f, 20.f), HTMPalette::Grass());
	Box(FVector((ApronMinX + ApronMaxX) * 0.5f, 500.f, 1.f), FVector(ApronMaxX - ApronMinX, 5000.f, 2.f), Asphalt * 1.25f);
	Box(FVector((StreetMinX + StreetMaxX) * 0.5f, 0.f, 1.5f), FVector(StreetMaxX - StreetMinX, 7000.f, 3.f), Asphalt);
	for (float Y = -3300.f; Y < 3300.f; Y += 400.f)
	{
		Box(FVector((StreetMinX + StreetMaxX) * 0.5f, Y, 3.5f), FVector(20.f, 180.f, 1.f), HTMPalette::SignalYellow(), false);
	}
	// Bordillos: los coches con la suspensión rebajada se quedan enganchados (GDD §7.6). Huecos de entrada.
	Box(FVector(StreetMinX, -2400.f, 6.f), FVector(12.f, 1600.f, 12.f), Curb);
	Box(FVector(StreetMinX, -350.f, 6.f), FVector(12.f, 1300.f, 12.f), Curb);
	Box(FVector(StreetMinX, 3000.f, 6.f), FVector(12.f, 800.f, 12.f), Curb);
	Box(FVector(StreetMaxX, 0.f, 6.f), FVector(12.f, 2800.f, 12.f), Curb);

	// ------------------------------------------------------------------ Zona de pruebas (circuito compacto)
	Box(FVector(5300.f, StraightY, 1.5f), FVector(6000.f, RoadWidth, 3.f), Asphalt);                 // recta
	Box(FVector(NorthRoadX, 250.f, 1.5f), FVector(RoadWidth, 3300.f, 3.f), Asphalt);                  // subida norte
	Box(FVector(5600.f, ReturnY, 1.5f), FVector(6600.f, RoadWidth, 3.f), Asphalt);                   // vuelta
	for (int32 i = 0; i < 6; ++i)                                                                      // curva peligrosa
	{
		const float A = FMath::DegreesToRadians(-90.f + (i + 0.5f) * 15.f);
		const FVector P(CurveCenter.X + CurveRadius * FMath::Cos(A), CurveCenter.Y + CurveRadius * FMath::Sin(A), 1.6f);
		Box(P, FVector(200.f, RoadWidth, 3.2f), Asphalt, true, FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
	}
	for (float X = 2600.f; X < 8200.f; X += 500.f)
	{
		Box(FVector(X, StraightY, 3.5f), FVector(200.f, 20.f, 1.f), HTMPalette::White(), false);
	}

	// Camino de tierra.
	Box(FVector(4500.f, 800.f, 1.f), FVector(3000.f, 600.f, 2.f), HTMPalette::Dirt());

	// Cuesta: subida, meseta y bajada (con relleno macizo debajo).
	const float SlopeAngle = FMath::RadiansToDegrees(FMath::Atan2(240.f, 600.f));
	Box(FVector(4750.f, -650.f, 100.f), FVector(900.f, 700.f, 200.f), HTMPalette::Dirt() * 0.9f);
	Box(FVector(4300.f, -650.f, 110.f), FVector(650.f, 700.f, 40.f), Asphalt, true, FRotator(SlopeAngle, 0.f, 0.f));
	Box(FVector(4750.f, -650.f, 220.f), FVector(320.f, 700.f, 40.f), Asphalt);
	Box(FVector(5200.f, -650.f, 110.f), FVector(650.f, 700.f, 40.f), Asphalt, true, FRotator(-SlopeAngle, 0.f, 0.f));

	// Rampa de salto en la subida norte (encargo "que salte la rampa").
	Box(FVector(NorthRoadX, -300.f, 70.f), FVector(520.f, 420.f, 30.f), HTMPalette::SafetyOrange(), true, FRotator(17.f, 90.f, 0.f));
	Box(FVector(NorthRoadX, -120.f, 50.f), FVector(420.f, 160.f, 100.f), HTMPalette::SafetyOrange() * 0.8f);

	// Barreras bajas y vallas de límite.
	Box(FVector(5750.f, StraightY - 330.f, 30.f), FVector(6900.f, 20.f, 60.f), HTMPalette::White());
	Box(FVector(5750.f, ReturnY + 330.f, 30.f), FVector(6900.f, 20.f, 60.f), HTMPalette::White());
	Box(FVector(NorthRoadX + 340.f, 0.f, 30.f), FVector(20.f, 4400.f, 60.f), HTMPalette::White());
	Box(FVector(3300.f, -4100.f, 100.f), FVector(14000.f, 30.f, 200.f), HTMPalette::WallMint() * 0.8f);
	Box(FVector(3300.f, 4100.f, 100.f), FVector(14000.f, 30.f, 200.f), HTMPalette::WallMint() * 0.8f);
	Box(FVector(-3700.f, 0.f, 100.f), FVector(30.f, 8200.f, 200.f), HTMPalette::WallMint() * 0.8f);
	Box(FVector(10300.f, 0.f, 100.f), FVector(30.f, 8200.f, 200.f), HTMPalette::WallMint() * 0.8f);

	// ------------------------------------------------------------------ Desguace
	const FLinearColor Fence = HTMPalette::Rust() * 0.9f;
	Box(FVector(-800.f, 1500.f, 90.f), FVector(1200.f, 20.f, 180.f), Fence);   // sur (izquierda de la puerta)
	Box(FVector(350.f, 1500.f, 90.f), FVector(100.f, 20.f, 180.f), Fence);     // sur (derecha de la puerta)
	Box(FVector(-500.f, 2600.f, 90.f), FVector(1800.f, 20.f, 180.f), Fence);   // norte
	Box(FVector(-1400.f, 2050.f, 90.f), FVector(20.f, 1100.f, 180.f), Fence);  // oeste
	Box(FVector(400.f, 2050.f, 90.f), FVector(20.f, 1100.f, 180.f), Fence);    // este
	const FVector JunkPiles[] = { FVector(-1250, 2450, 60), FVector(-1050, 2480, 45), FVector(-850, 2420, 70), FVector(-1250, 2250, 40) };
	for (const FVector& P : JunkPiles)
	{
		Box(P, FVector(180.f, 160.f, P.Z * 2.f), HTMPalette::Rust(), true, FRotator(0.f, P.X * 0.1f, 0.f));
	}
	UTextRenderComponent* JunkSign = NewObject<UTextRenderComponent>(this);
	JunkSign->SetupAttachment(Root);
	JunkSign->SetRelativeLocation(FVector(50.f, 1480.f, 220.f));
	JunkSign->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	JunkSign->SetHorizontalAlignment(EHTA_Center);
	JunkSign->SetWorldSize(60.f);
	JunkSign->SetTextRenderColor(FColor(229, 72, 77));
	JunkSign->SetText(LOCTEXT("Junkyard", "DESGUACE"));
	JunkSign->RegisterComponent();
}

void AWorkshopBlockout::BuildLighting()
{
	// Solo si el mapa no trae su propia iluminación.
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		return;
	}
	// Luz principal cálida y soleada, sombras suaves y definidas, cielo de relleno (ART_DIRECTION §10).
	UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(this, TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetWorldRotation(FRotator(-42.f, -35.f, 0.f));
	Sun->SetIntensity(7.5f);
	Sun->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f));
	Sun->SetAtmosphereSunLight(true);
	Sun->SetDynamicShadowDistanceMovableLight(9000.f);
	Sun->RegisterComponent();

	USkyAtmosphereComponent* Atmosphere = NewObject<USkyAtmosphereComponent>(this, TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);
	Atmosphere->RegisterComponent();

	USkyLightComponent* Sky = NewObject<USkyLightComponent>(this, TEXT("SkyLight"));
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->bRealTimeCapture = true;
	Sky->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	Sky->SetIntensity(1.2f);
	Sky->RegisterComponent();

	UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(this, TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.005f);
	Fog->RegisterComponent();
}

// ============================================================================ Taller por niveles

FVector2D AWorkshopBlockout::GetFloorSize(int32 Level) const
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	const FWorkshopLevelRow* Row = Data ? Data->FindWorkshopLevel(Level) : nullptr;
	return Row ? Row->FloorSize : FVector2D(1400.f, 1200.f);
}

int32 AWorkshopBlockout::GetBays(int32 Level) const
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	const FWorkshopLevelRow* Row = Data ? Data->FindWorkshopLevel(Level) : nullptr;
	return Row ? FMath::Max(1, Row->Bays) : 1;
}

FBox AWorkshopBlockout::GetWorkshopBounds(int32 Level) const
{
	const FVector2D Size = GetFloorSize(Level);
	return FBox(FVector(DoorX - Size.X, -Size.Y * 0.5f, -100.f), FVector(DoorX, Size.Y * 0.5f, 600.f));
}

FBox AWorkshopBlockout::GetPlayableBounds() const
{
	return FBox(FVector(-3650.f, -4050.f, -500.f), FVector(10250.f, 4050.f, 3000.f));
}

void AWorkshopBlockout::ClearWorkshop()
{
	for (UActorComponent* C : WorkshopComponents)
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}
	WorkshopComponents.Reset();
	WallComponents.Reset();
}

void AWorkshopBlockout::RecolorWalls()
{
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	const TArray<FLinearColor>& Colors = HTMPalette::WallColors();
	const FLinearColor Color = Colors[GS ? FMath::Clamp(GS->WallColorIndex, 0, Colors.Num() - 1) : 0];
	for (UStaticMeshComponent* Wall : WallComponents)
	{
		UHTMVisualLibrary::ApplyPlaceholderMaterial(Wall, Color);
	}
}

void AWorkshopBlockout::BuildWorkshop(int32 Level)
{
	if (Level == BuiltLevel)
	{
		return;
	}
	ClearWorkshop();
	BuiltLevel = Level;

	const FVector2D Size = GetFloorSize(Level);
	const float MinX = DoorX - Size.X;
	const float HalfY = Size.Y * 0.5f;
	const float MidX = (MinX + DoorX) * 0.5f;
	const float H = 450.f;
	const float T = 30.f;
	const int32 Bays = GetBays(Level);
	const float DoorWidth = FMath::Min(Size.Y - 200.f, 250.f + Bays * 350.f);
	const FLinearColor Floor = HTMPalette::FloorWarmGrey();

	// Cada nivel se ve claramente más grande, limpio y equipado (ART_DIRECTION §9).
	Box(FVector(MidX, 0.f, 2.f), FVector(Size.X, Size.Y, 4.f), Floor, true, FRotator::ZeroRotator, true);
	WallComponents.Add(Box(FVector(MinX - T * 0.5f, 0.f, H * 0.5f), FVector(T, Size.Y + 2.f * T, H), HTMPalette::WallMint(), true, FRotator::ZeroRotator, true));
	WallComponents.Add(Box(FVector(MidX, -HalfY - T * 0.5f, H * 0.5f), FVector(Size.X, T, H), HTMPalette::WallMint(), true, FRotator::ZeroRotator, true));
	WallComponents.Add(Box(FVector(MidX, HalfY + T * 0.5f, H * 0.5f), FVector(Size.X, T, H), HTMPalette::WallMint(), true, FRotator::ZeroRotator, true));
	const float SideLen = HalfY - DoorWidth * 0.5f;
	WallComponents.Add(Box(FVector(DoorX + T * 0.5f, -HalfY + SideLen * 0.5f, H * 0.5f), FVector(T, SideLen, H), HTMPalette::WallMint(), true, FRotator::ZeroRotator, true));
	WallComponents.Add(Box(FVector(DoorX + T * 0.5f, HalfY - SideLen * 0.5f, H * 0.5f), FVector(T, SideLen, H), HTMPalette::WallMint(), true, FRotator::ZeroRotator, true));
	WallComponents.Add(Box(FVector(DoorX + T * 0.5f, 0.f, 415.f), FVector(T, DoorWidth, 70.f), HTMPalette::WallMint(), true, FRotator::ZeroRotator, true));
	RecolorWalls();

	// Cartel GARAGE azul sobre la puerta (referencia 01_portada).
	Box(FVector(DoorX + 45.f, 0.f, 520.f), FVector(20.f, FMath::Min(DoorWidth, 700.f), 110.f), HTMPalette::ToolBlue(), false, FRotator::ZeroRotator, true);
	UTextRenderComponent* Garage = NewObject<UTextRenderComponent>(this);
	Garage->SetupAttachment(Root);
	Garage->SetRelativeLocation(FVector(DoorX + 57.f, 0.f, 490.f));
	Garage->SetHorizontalAlignment(EHTA_Center);
	Garage->SetWorldSize(80.f);
	Garage->SetTextRenderColor(FColor(255, 210, 63));
	Garage->SetText(LOCTEXT("Garage", "GARAGE"));
	Garage->RegisterComponent();
	WorkshopComponents.Add(Garage);

	// Vigas del techo (sin techo: vista de diorama).
	for (float X = MinX + 300.f; X < DoorX; X += 600.f)
	{
		Box(FVector(X, 0.f, H - 10.f), FVector(25.f, Size.Y, 25.f), HTMPalette::ToolBlue() * 0.7f, false, FRotator::ZeroRotator, true);
	}

	// Plazas: líneas amarillas, manchas de aceite y luz de fluorescente cálida.
	const float BaySpacing = Size.Y / Bays;
	for (int32 i = 0; i < Bays; ++i)
	{
		const float Y = -HalfY + (i + 0.5f) * BaySpacing;
		const FVector C(Layout::BayCenterX, Y, 5.f);
		Box(C + FVector(0.f, -BaySpacing * 0.5f + 20.f, 0.f), FVector(560.f, 10.f, 1.f), HTMPalette::SignalYellow(), false, FRotator::ZeroRotator, true);
		Box(C + FVector(0.f, BaySpacing * 0.5f - 20.f, 0.f), FVector(560.f, 10.f, 1.f), HTMPalette::SignalYellow(), false, FRotator::ZeroRotator, true);
		Box(C + FVector(-60.f, 40.f * (i % 2 ? 1.f : -1.f), 0.2f), FVector(120.f, 90.f, 1.f), FLinearColor(0.05f, 0.05f, 0.06f), false, FRotator::ZeroRotator, true, TEXT("Cylinder"));

		UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
		Lamp->SetupAttachment(Root);
		Lamp->SetMobility(EComponentMobility::Movable);
		Lamp->SetRelativeLocation(FVector(Layout::BayCenterX, Y, H - 40.f));
		Lamp->SetIntensity(9000.f);
		Lamp->SetAttenuationRadius(900.f);
		Lamp->SetLightColor(FLinearColor(1.f, 0.9f, 0.75f));
		Lamp->SetCastShadows(false);
		Lamp->RegisterComponent();
		WorkshopComponents.Add(Lamp);
		Box(FVector(Layout::BayCenterX, Y, H - 30.f), FVector(200.f, 20.f, 8.f), HTMPalette::White(), false, FRotator::ZeroRotator, true);
	}

	// Banco de herramientas (cajas rojas), estantería azul y decoración legible.
	Box(FVector(MinX + 45.f, 90.f, 45.f), FVector(70.f, 440.f, 90.f), HTMPalette::ToolRed(), true, FRotator::ZeroRotator, true);
	const int32 ShelfCols = 4 + (Level - 1) * 3;
	Box(FVector(MinX + 150.f + ShelfCols * 62.5f - 62.5f, -HalfY + 30.f, 90.f), FVector(ShelfCols * 125.f, 25.f, 180.f), HTMPalette::ToolBlue(), true, FRotator::ZeroRotator, true);
	// Neumáticos apilados en la esquina (referencia 03_taller).
	for (int32 i = 0; i < 4; ++i)
	{
		Box(FVector(DoorX - 80.f, -HalfY + 80.f, 15.f + i * 28.f), FVector(90.f, 90.f, 26.f), HTMPalette::Rubber(), true, FRotator::ZeroRotator, true, TEXT("Cylinder"));
	}
	// Carteles en la pared.
	Box(FVector(MinX + 2.f, -HalfY * 0.5f, 250.f), FVector(4.f, 150.f, 100.f), HTMPalette::SafetyOrange(), false, FRotator::ZeroRotator, true);
	Box(FVector(MinX + 2.f, HalfY * 0.5f, 260.f), FVector(4.f, 120.f, 160.f), HTMPalette::CarSky(), false, FRotator::ZeroRotator, true);
}

// ============================================================================ Layout para el GameMode

FWorkshopLayout AWorkshopBlockout::GetLayout(int32 Level) const
{
	FWorkshopLayout L;
	L.Level = Level;
	L.WorkshopBounds = GetWorkshopBounds(Level);
	const FVector2D Size = GetFloorSize(Level);
	const float MinX = DoorX - Size.X;
	const float HalfY = Size.Y * 0.5f;
	const int32 Bays = GetBays(Level);
	const float BaySpacing = Size.Y / Bays;
	auto BayY = [&](int32 i) { return -HalfY + (i + 0.5f) * BaySpacing; };

	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	const FWorkshopLevelRow* Row = Data ? Data->FindWorkshopLevel(Level) : nullptr;

	for (int32 i = 0; i < 4; ++i)
	{
		L.PlayerStarts.Add(FTransform(FRotator(0.f, 0.f, 0.f), FVector(-150.f - (i / 2) * 150.f, (i % 2 ? 1.f : -1.f) * 120.f, 110.f)));
	}

	const int32 ShelfSlots = Row ? Row->ShelfSlots : 6;
	for (int32 i = 0; i < ShelfSlots; ++i)
	{
		const int32 Col = i / 2;
		const int32 RowIdx = i % 2;
		L.ShelfSlots.Add(FTransform(FRotator(0.f, 90.f, 0.f), FVector(MinX + 150.f + Col * 125.f, -HalfY + 60.f, 30.f + RowIdx * 80.f)));
	}

	// Herramientas del nivel (acumulativas) sobre el banco.
	TArray<EToolType> Tools;
	if (Data)
	{
		for (int32 Lv = 1; Lv <= Level; ++Lv)
		{
			if (const FWorkshopLevelRow* R = Data->FindWorkshopLevel(Lv))
			{
				if (R->Level == Lv)
				{
					for (EToolType T : R->Tools) { Tools.Add(T); }
				}
			}
		}
	}
	for (int32 i = 0; i < Tools.Num(); ++i)
	{
		const float Y = -110.f + (i % 7) * 60.f;
		const float Z = 110.f + (i / 7) * 30.f;
		L.Tools.Add(TPair<EToolType, FTransform>(Tools[i], FTransform(FRotator::ZeroRotator, FVector(MinX + 45.f, Y, Z))));
	}

	L.Jacks.Add(FTransform(FRotator::ZeroRotator, FVector(Layout::BayCenterX - 100.f, BayY(0) + BaySpacing * 0.5f - 60.f, 20.f)));
	if (Row && Row->bHasJackStands)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			L.JackStands.Add(FTransform(FRotator::ZeroRotator, FVector(Layout::BayCenterX - 250.f + i * 45.f, BayY(0) + BaySpacing * 0.5f - 60.f, 30.f)));
		}
		L.Jacks.Add(FTransform(FRotator::ZeroRotator, FVector(Layout::BayCenterX + 150.f, BayY(Bays - 1) + BaySpacing * 0.5f - 60.f, 20.f)));
	}
	if (Row && Row->bHasLift && Bays >= 2)
	{
		L.Lifts.Add(FTransform(FRotator::ZeroRotator, FVector(Layout::BayCenterX, BayY(1), 4.f)));
		if (Bays >= 4)
		{
			L.Lifts.Add(FTransform(FRotator::ZeroRotator, FVector(Layout::BayCenterX, BayY(3), 4.f)));
		}
	}
	if (Row && Row->bHasEngineCrane)
	{
		L.Cranes.Add(FTransform(FRotator::ZeroRotator, FVector(Layout::BayCenterX - 450.f, BayY(0), 30.f)));
	}
	if (Row && Row->bHasPaintBooth)
	{
		L.PaintBooths.Add(FTransform(FRotator::ZeroRotator, FVector(MinX + 700.f, 0.f, 0.f)));
	}

	for (int32 i = 0; i < 4; ++i)
	{
		L.Wardrobes.Add(FTransform(FRotator::ZeroRotator, FVector(MinX + 50.f, -HalfY + 170.f + i * 110.f, 0.f)));
	}
	L.CashRegister = FTransform(FRotator(0.f, 180.f, 0.f), FVector(DoorX - 120.f, HalfY - 120.f, 0.f));
	L.JobBoard = FTransform(FRotator(0.f, -90.f, 0.f), FVector(Layout::BayCenterX - 100.f, HalfY - 8.f, 200.f));
	L.TowPhone = FTransform(FRotator(0.f, -90.f, 0.f), FVector(DoorX - 300.f, HalfY - 20.f, 100.f));
	L.ScrapBin = FTransform(FRotator::ZeroRotator, FVector(MinX + 160.f, HalfY - 150.f, 0.f));
	L.UpgradeTerminal = FTransform(FRotator::ZeroRotator, FVector(MinX + 50.f, HalfY - 360.f, 0.f));
	L.WallPanel = FTransform(FRotator::ZeroRotator, FVector(MinX + 20.f, HalfY - 480.f, 120.f));
	L.LostAndFound = FVector(MinX + 350.f, HalfY - 300.f, 60.f);

	// Exterior (fijo).
	L.DeliveryBay = FTransform(FRotator::ZeroRotator, FVector(1050.f, 750.f, 150.f));
	L.SellPoint = FTransform(FRotator::ZeroRotator, FVector(1050.f, -420.f, 0.f));
	for (int32 i = 0; i < 3; ++i)
	{
		L.JunkyardSigns.Add(FTransform(FRotator(0.f, -90.f, 0.f), FVector(-1100.f + i * 350.f, 1700.f, 0.f)));
	}
	if (Level == 1)
	{
		L.StarterCars.Add(FTransform(FRotator::ZeroRotator, FVector(Layout::BayCenterX, BayY(0), 20.f)));
	}
	return L;
}

TArray<FTransform> AWorkshopBlockout::GetCustomerSpots() const
{
	TArray<FTransform> Spots;
	for (int32 i = 0; i < 4; ++i)
	{
		Spots.Add(FTransform(FRotator(0.f, 180.f, 0.f), FVector(1050.f, 1450.f + i * 450.f, 20.f)));
	}
	return Spots;
}

TArray<FTransform> AWorkshopBlockout::GetTowSpots() const
{
	return {
		FTransform(FRotator(0.f, 180.f, 0.f), FVector(1050.f, -1300.f, 20.f)),
		FTransform(FRotator(0.f, 180.f, 0.f), FVector(1050.f, -1750.f, 20.f)),
		FTransform(FRotator(0.f, -90.f, 0.f), FVector(-900.f, 2150.f, 20.f)),
		FTransform(FRotator(0.f, -90.f, 0.f), FVector(-450.f, 2150.f, 20.f))
	};
}

FTransform AWorkshopBlockout::GetJunkyardDropSpot() const
{
	return FTransform(FRotator(0.f, -90.f, 0.f), FVector(50.f, 2150.f, 20.f));
}

void AWorkshopBlockout::GetTestZoneSpawns(TArray<FTransform>& OutLamps, FTransform& OutSpeedTrap, TArray<TPair<FTransform, FVector2D>>& OutPuddles,
	TArray<TPair<FTransform, FVector2D>>& OutMud, TArray<FTransform>& OutCones, TArray<FTransform>& OutTires, TArray<FTransform>& OutClutter) const
{
	using namespace Layout;
	for (float X = 3000.f; X <= 7500.f; X += 1500.f)
	{
		OutLamps.Add(FTransform(FVector(X, StraightY + 360.f, 225.f)));
	}
	for (float Y = -2500.f; Y <= 2000.f; Y += 1500.f)
	{
		OutLamps.Add(FTransform(FVector(StreetMaxX - 40.f, Y, 225.f)));
	}
	OutLamps.Add(FTransform(FVector(StreetMinX + 40.f, -1800.f, 225.f)));
	OutLamps.Add(FTransform(FVector(StreetMinX + 40.f, 2800.f, 225.f)));

	OutSpeedTrap = FTransform(FVector(6900.f, StraightY, 200.f));

	// Charco grande en la vuelta y barro en el camino de tierra.
	OutPuddles.Add(TPair<FTransform, FVector2D>(FTransform(FVector(7400.f, ReturnY, 20.f)), FVector2D(900.f, 550.f)));
	OutMud.Add(TPair<FTransform, FVector2D>(FTransform(FVector(3800.f, 800.f, 20.f)), FVector2D(600.f, 500.f)));
	OutMud.Add(TPair<FTransform, FVector2D>(FTransform(FVector(5200.f, 800.f, 20.f)), FVector2D(600.f, 500.f)));

	// Conos en eslalon.
	for (int32 i = 0; i < 8; ++i)
	{
		OutCones.Add(FTransform(FVector(6200.f - i * 400.f, ReturnY + (i % 2 ? 150.f : -150.f), 30.f)));
	}
	// Muro de neumáticos por fuera de la curva peligrosa.
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = FMath::DegreesToRadians(-85.f + i * 11.f);
		const FVector P(CurveCenter.X + 1000.f * FMath::Cos(A), CurveCenter.Y + 1000.f * FMath::Sin(A), 20.f);
		OutTires.Add(FTransform(FRotator(0.f, 0.f, 0.f), P));
		OutTires.Add(FTransform(FRotator(0.f, 0.f, 0.f), P + FVector(0.f, 0.f, 30.f)));
	}
	// Desorden legible en el taller y la explanada (obstáculos para tropezar).
	OutClutter.Add(FTransform(FVector(-300.f, 420.f, 30.f)));
	OutClutter.Add(FTransform(FVector(-420.f, -380.f, 30.f)));
	OutClutter.Add(FTransform(FVector(900.f, 250.f, 30.f)));
	OutClutter.Add(FTransform(FVector(1200.f, -900.f, 30.f)));
}

#undef LOCTEXT_NAMESPACE
