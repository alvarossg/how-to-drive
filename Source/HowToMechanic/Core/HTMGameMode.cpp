#include "Core/HTMGameMode.h"
#include "Core/HTMGameState.h"
#include "Core/HTMPlayerState.h"
#include "Core/HTMPlayerController.h"
#include "Core/HTMDataSubsystem.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMTelemetrySubsystem.h"
#include "Core/HTMSettings.h"
#include "Core/HTMPalette.h"
#include "Core/HTMLog.h"
#include "Character/MechanicCharacter.h"
#include "Customers/JobDirectorComponent.h"
#include "Customers/JobBoard.h"
#include "Customers/DeliveryBay.h"
#include "Economy/EconomyActors.h"
#include "Parts/CarPart.h"
#include "Progression/HTMSaveGame.h"
#include "Progression/ProgressionActors.h"
#include "Progression/WorkshopBlockout.h"
#include "Tools/LiftingDevices.h"
#include "Tools/Tool.h"
#include "Tools/WaterPatch.h"
#include "UI/MechanicHUD.h"
#include "Vehicle/CarFactory.h"
#include "Vehicle/ModularCar.h"
#include "Vehicle/TestZoneActors.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "HTMGameMode"

namespace
{
	const FName TagDecor(TEXT("Decor"));
	const FName TagTestZone(TEXT("TestZoneProp"));

	/** Zona "de casa": taller + explanada. Lo que queda fuera se considera perdido. */
	FBox HomeBox(const FBox& WorkshopBounds)
	{
		return FBox(FVector(WorkshopBounds.Min.X - 50.f, -2100.f, -500.f), FVector(1400.f, 3100.f, 1500.f));
	}
}

AHTMGameMode::AHTMGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	DefaultPawnClass = AMechanicCharacter::StaticClass();
	PlayerControllerClass = AHTMPlayerController::StaticClass();
	GameStateClass = AHTMGameState::StaticClass();
	PlayerStateClass = AHTMPlayerState::StaticClass();
	HUDClass = AMechanicHUD::StaticClass();
	bUseSeamlessTravel = false;
	JobDirector = CreateDefaultSubobject<UJobDirectorComponent>(TEXT("JobDirector"));
	Rng.GenerateNewSeed();
}

void AHTMGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	bNewGameRequested = UGameplayStatics::GetIntOption(Options, TEXT("NewGame"), 0) != 0;
	bChaoticOption = UGameplayStatics::GetIntOption(Options, TEXT("Chaotic"), 0) != 0;
	bFriendlyPushOption = UGameplayStatics::GetIntOption(Options, TEXT("FriendlyPush"), 1) != 0;
}

UClass* AHTMGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const TSoftClassPtr<APawn>& Override = UHTMSettings::Get()->MechanicCharacterClass;
	if (!Override.IsNull())
	{
		if (UClass* Class = Override.LoadSynchronous())
		{
			return Class;
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

// ============================================================================ Arranque

void AHTMGameMode::EnsureBlockout()
{
	Blockout = AWorkshopBlockout::Get(this);
	if (!Blockout)
	{
		FActorSpawnParameters Params;
		Params.Name = TEXT("WorkshopBlockout");
		Blockout = GetWorld()->SpawnActor<AWorkshopBlockout>(AWorkshopBlockout::StaticClass(), FTransform::Identity, Params);
	}
}

template <typename T>
T* AHTMGameMode::SpawnLayout(const FTransform& Where)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	T* Actor = GetWorld()->SpawnActor<T>(T::StaticClass(), Where, Params);
	if (Actor)
	{
		LayoutActors.Add(Actor);
	}
	return Actor;
}

void AHTMGameMode::StartPlay()
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	EnsureBlockout();
	LoadWorkshop();

	SpawnLayoutActors(GS ? GS->WorkshopLevel : 1, 0);
	SpawnTestZone();
	if (LoadedSave && !bNewGameRequested)
	{
		RestoreSavedObjects(LoadedSave);
	}
	else
	{
		SpawnStarterContent();
	}

	// Puntos de aparición si el mapa no trae.
	bool bHasStarts = false;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		bHasStarts = true;
		break;
	}
	if (!bHasStarts && Blockout)
	{
		for (const FTransform& T : Blockout->GetLayout(GS ? GS->WorkshopLevel : 1).PlayerStarts)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), T, Params);
		}
		// Quien entró antes de que existieran los puntos de aparición (el anfitrión en PIE) se quedó sin cuerpo.
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && !PC->GetPawn() && PlayerCanRestart(PC))
			{
				RestartPlayer(PC);
			}
		}
	}

	GenerateJunkyardOffers();
	if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
	{
		Tel->Record(this, ETelemetryEvent::SessionStart, FString(), GetWorld()->GetMapName());
	}
	Super::StartPlay();
}

void AHTMGameMode::SpawnLayoutActors(int32 Level, int32 PreviousLevel)
{
	if (!Blockout)
	{
		return;
	}
	for (AActor* Actor : LayoutActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	LayoutActors.Reset();

	const FWorkshopLayout L = Blockout->GetLayout(Level);
	const FWorkshopLayout Prev = PreviousLevel > 0 ? Blockout->GetLayout(PreviousLevel) : FWorkshopLayout();

	for (const FTransform& T : L.ShelfSlots) { SpawnLayout<AShelfSlot>(T); }
	SpawnLayout<ACashRegister>(L.CashRegister);
	SpawnLayout<AJobBoard>(L.JobBoard);
	SpawnLayout<ATowPhone>(L.TowPhone);
	SpawnLayout<AScrapBin>(L.ScrapBin);
	SpawnLayout<AWorkshopUpgradeTerminal>(L.UpgradeTerminal);
	SpawnLayout<AWallColorPanel>(L.WallPanel);
	SpawnLayout<ADeliveryBay>(L.DeliveryBay);
	SpawnLayout<ASellPoint>(L.SellPoint);
	for (int32 i = 0; i < L.Wardrobes.Num(); ++i)
	{
		if (AWardrobe* W = SpawnLayout<AWardrobe>(L.Wardrobes[i]))
		{
			W->SetSlot((ECosmeticSlot)FMath::Clamp(i, 0, (int32)ECosmeticSlot::ToolColor));
		}
	}
	for (int32 i = 0; i < L.JunkyardSigns.Num(); ++i)
	{
		if (AJunkyardOfferSign* S = SpawnLayout<AJunkyardOfferSign>(L.JunkyardSigns[i]))
		{
			S->SetOfferIndex(i);
		}
	}
	for (const FTransform& T : L.Lifts) { SpawnLayout<AHydraulicLift>(T); }
	for (const FTransform& T : L.PaintBooths) { SpawnLayout<APaintBooth>(T); }

	// Equipo físico nuevo de este nivel (lo anterior sigue donde lo dejasteis).
	for (int32 i = Prev.Tools.Num(); i < L.Tools.Num(); ++i)
	{
		UCarFactory::SpawnTool(this, L.Tools[i].Key, L.Tools[i].Value);
	}
	for (int32 i = Prev.Jacks.Num(); i < L.Jacks.Num(); ++i)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		GetWorld()->SpawnActor<AJack>(AJack::StaticClass(), L.Jacks[i], Params);
	}
	for (int32 i = Prev.JackStands.Num(); i < L.JackStands.Num(); ++i)
	{
		FGrabbablePropSpec Stand;
		Stand.Shape = TEXT("Cylinder");
		Stand.SizeCm = FVector(32.f, 32.f, 48.f);
		Stand.Color = HTMPalette::ToolRed();
		Stand.MassKg = 7.f;
		Stand.DisplayName = LOCTEXT("JackStand", "Borriqueta");
		UCarFactory::SpawnProp(this, Stand, L.JackStands[i]);
	}
	for (int32 i = Prev.Cranes.Num(); i < L.Cranes.Num(); ++i)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		GetWorld()->SpawnActor<AEngineCrane>(AEngineCrane::StaticClass(), L.Cranes[i], Params);
	}
	RestockShelves();
}

void AHTMGameMode::SpawnTestZone()
{
	if (!Blockout)
	{
		return;
	}
	TArray<FTransform> Lamps, Cones, Tires, Clutter;
	FTransform SpeedTrap;
	TArray<TPair<FTransform, FVector2D>> Puddles, Mud;
	Blockout->GetTestZoneSpawns(Lamps, SpeedTrap, Puddles, Mud, Cones, Tires, Clutter);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FTransform& T : Lamps) { GetWorld()->SpawnActor<AKnockableLamp>(AKnockableLamp::StaticClass(), T, Params); }
	GetWorld()->SpawnActor<ASpeedTrap>(ASpeedTrap::StaticClass(), SpeedTrap, Params);
	for (const auto& P : Puddles)
	{
		if (AWaterPatch* W = GetWorld()->SpawnActor<AWaterPatch>(AWaterPatch::StaticClass(), P.Key, Params)) { W->InitPermanent(P.Value, false); }
	}
	for (const auto& P : Mud)
	{
		if (AWaterPatch* W = GetWorld()->SpawnActor<AWaterPatch>(AWaterPatch::StaticClass(), P.Key, Params)) { W->InitPermanent(P.Value, true); }
	}

	FGrabbablePropSpec Cone;
	Cone.Shape = TEXT("Cone");
	Cone.SizeCm = FVector(40.f, 40.f, 60.f);
	Cone.Color = HTMPalette::SafetyOrange();
	Cone.MassKg = 2.f;
	Cone.DisplayName = LOCTEXT("Cone", "Cono");
	for (const FTransform& T : Cones)
	{
		if (AGrabbableActor* A = UCarFactory::SpawnProp(this, Cone, T)) { A->Tags.Add(TagTestZone); }
	}
	FGrabbablePropSpec Tire;
	Tire.Shape = TEXT("Cylinder");
	Tire.SizeCm = FVector(75.f, 75.f, 26.f);
	Tire.Color = HTMPalette::Rubber();
	Tire.MassKg = 11.f;
	Tire.DisplayName = LOCTEXT("OldTire", "Neumático viejo");
	for (const FTransform& T : Tires)
	{
		if (AGrabbableActor* A = UCarFactory::SpawnProp(this, Tire, T)) { A->Tags.Add(TagTestZone); }
	}
	// Desorden legible: cajas y bidones (obstáculos para tropezar).
	for (int32 i = 0; i < Clutter.Num(); ++i)
	{
		FGrabbablePropSpec Spec;
		if (i % 2 == 0)
		{
			Spec.Shape = TEXT("Cube"); Spec.SizeCm = FVector(45.f); Spec.Color = HTMPalette::Dirt(); Spec.MassKg = 5.f; Spec.DisplayName = LOCTEXT("Crate", "Caja");
		}
		else
		{
			Spec.Shape = TEXT("Cylinder"); Spec.SizeCm = FVector(55.f, 55.f, 85.f); Spec.Color = HTMPalette::ToolBlue(); Spec.MassKg = 25.f; Spec.DisplayName = LOCTEXT("Barrel", "Bidón");
		}
		UCarFactory::SpawnProp(this, Spec, Clutter[i]);
	}
}

void AHTMGameMode::SpawnStarterContent()
{
	if (!Blockout)
	{
		return;
	}
	const FWorkshopLayout L = Blockout->GetLayout(1);

	// Coche de prueba de la fase 2: utilitario diminuto con una rueda pinchada.
	for (const FTransform& T : L.StarterCars)
	{
		FCarSpawnOptions Options;
		Options.ForcedSlot = TEXT("Wheel_FL");
		Options.ForcedPartId = TEXT("Wheel_Flat");
		Options.MinFaults = 0;
		Options.MaxFaults = 0;
		Options.ConditionMin = 60.f;
		Options.ConditionMax = 90.f;
		Options.LooseBoltChance = 0.f;
		UCarFactory::SpawnCar(this, TEXT("Car_Tiny"), T, Options);
	}

	// Objetos de prueba de la fase 1: rueda de repuesto, bloque motor, caja.
	UCarFactory::SpawnPart(this, TEXT("Wheel_Standard"), FTransform(FRotator(0.f, 0.f, 90.f), FVector(-250.f, 250.f, 50.f)));
	UCarFactory::SpawnPart(this, TEXT("Engine_I3"), FTransform(FVector(-450.f, 100.f, 60.f)), 70.f);
	FGrabbablePropSpec Crate;
	Crate.Shape = TEXT("Cube");
	Crate.SizeCm = FVector(50.f);
	Crate.Color = HTMPalette::Dirt();
	Crate.MassKg = 6.f;
	Crate.DisplayName = LOCTEXT("TestCrate", "Caja");
	UCarFactory::SpawnProp(this, Crate, FTransform(FVector(-100.f, -300.f, 40.f)));

	// Decoración movible (personalización del taller; se guarda dónde la dejéis).
	struct FDecorDef { FName Shape; FVector Size; FLinearColor Color; float Mass; FText Name; FVector Where; };
	const FDecorDef Decor[] = {
		{ TEXT("Cylinder"), FVector(40.f, 40.f, 70.f), HTMPalette::CarLime(), 8.f, LOCTEXT("Plant", "Planta"), FVector(-600.f, 450.f, 50.f) },
		{ TEXT("Cube"), FVector(35.f, 20.f, 25.f), HTMPalette::ToolRed(), 2.f, LOCTEXT("Radio", "Radio del taller"), FVector(-640.f, 300.f, 110.f) },
		{ TEXT("Cylinder"), FVector(35.f, 35.f, 50.f), HTMPalette::SignalYellow(), 4.f, LOCTEXT("Stool", "Taburete"), FVector(-400.f, 450.f, 40.f) },
	};
	for (const FDecorDef& D : Decor)
	{
		FGrabbablePropSpec Spec;
		Spec.Shape = D.Shape; Spec.SizeCm = D.Size; Spec.Color = D.Color; Spec.MassKg = D.Mass; Spec.DisplayName = D.Name;
		if (AGrabbableActor* A = UCarFactory::SpawnProp(this, Spec, FTransform(D.Where)))
		{
			A->Tags.Add(TagDecor);
		}
	}
}

// ============================================================================ Jugadores

void AHTMGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	AHTMPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AHTMPlayerState>() : nullptr;
	if (!PS || !LoadedSave)
	{
		return;
	}
	for (const FSavedPlayer& Saved : LoadedSave->Players)
	{
		if (Saved.PlayerName == PS->GetPlayerName())
		{
			PS->SetLoadout(Saved.Loadout);
			PS->Achievements = Saved.Achievements;
			PS->Achievements.SetNumZeroed((int32)EHTMAchievement::MAX);
			break;
		}
	}
}

void AHTMGameMode::Logout(AController* Exiting)
{
	// Soltar lo que llevara y bajar del coche antes de irse.
	if (APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		if (AModularCar* Car = Cast<AModularCar>(PC->GetPawn()))
		{
			Car->EjectDriver(FVector::ZeroVector);
		}
	}
	Super::Logout(Exiting);
}

// ============================================================================ Jornada

void AHTMGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AHTMGameState* GS = GetGameState<AHTMGameState>();
	if (GS && GS->DayPhase == EDayPhase::Open && GS->GetServerWorldTimeSeconds() >= GS->DayEndServerTime)
	{
		EndDay();
	}
}

void AHTMGameMode::StartDay()
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	if (!GS || GS->DayPhase == EDayPhase::Open)
	{
		return;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	GS->DayNumber++;
	GS->DayMoneyEarned = 0;
	GS->DayMoneySpent = 0;
	GS->DayReputationDelta = 0.f;
	GS->DayPhase = EDayPhase::Open;
	GS->DayEndServerTime = GS->GetServerWorldTimeSeconds() + T.DayLengthSeconds;
	GS->NotifyDayPhase();

	if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
	{
		Tel->BeginDay(GS->DayNumber);
		Tel->Record(this, ETelemetryEvent::DayStart, FString(), FString::Printf(TEXT("Día %d, %d jugadores"), GS->DayNumber, GS->GetNumPlayers()));
	}
	RestockShelves();
	GenerateJunkyardOffers();
	JobDirector->StartDay(GS->DayNumber);
	GS->MulticastToast(FText::Format(LOCTEXT("DayStart", "¡Día {0}! El taller está abierto."), FText::AsNumber(GS->DayNumber)), HTMPalette::SignalYellow());
}

void AHTMGameMode::EndDay()
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	if (!GS || GS->DayPhase != EDayPhase::Open)
	{
		return;
	}
	JobDirector->EndDay();

	const int32 Lost = CollectLostItems();
	const int32 Fee = Lost * UHTMTuningData::Get().LostItemFee;
	if (Fee > 0)
	{
		GS->AddMoney(-Fee, FText::Format(LOCTEXT("LostFee", "Recoger {0} objetos perdidos"), FText::AsNumber(Lost)));
	}

	FHTMDaySummary Summary;
	Summary.Day = GS->DayNumber;
	Summary.MoneyEarned = GS->DayMoneyEarned;
	Summary.MoneySpent = GS->DayMoneySpent;
	Summary.ReputationDelta = GS->DayReputationDelta;
	Summary.JobsFull = JobDirector->DayFull;
	Summary.JobsPartial = JobDirector->DayPartial;
	Summary.JobsAngry = JobDirector->DayAngry;
	Summary.LostItemsRecovered = Lost;
	Summary.LostItemsFee = Fee;
	if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
	{
		Summary.DistinctSituations = Tel->GetDistinctSituationsToday();
		Tel->Record(this, ETelemetryEvent::DayEnd, FString(), FString::Printf(TEXT("Día %d"), GS->DayNumber), FVector::ZeroVector, (float)Summary.MoneyEarned);
		Summary.Disasters = Tel->EndDay(3);
	}
	GS->LastDaySummary = Summary;
	GS->NotifyDaySummary();
	GS->DayPhase = EDayPhase::Summary;
	GS->NotifyDayPhase();
	SaveWorkshop();
}

void AHTMGameMode::PrepareNextDay()
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	if (GS && GS->DayPhase == EDayPhase::Summary)
	{
		GS->DayPhase = EDayPhase::Closed;
		GS->NotifyDayPhase();
	}
}

void AHTMGameMode::RestockShelves()
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	if (!GS || !Data)
	{
		return;
	}
	// Primero lo imprescindible (ruedas, suspensión, escape), luego el resto al azar.
	TArray<FName> Essentials, Others;
	for (const FName& Id : Data->GetPartIds())
	{
		const FPartDefinitionRow* Row = Data->FindPart(Id);
		if (!Row || Row->ShelfLevel <= 0 || Row->ShelfLevel > GS->WorkshopLevel)
		{
			continue;
		}
		(Row->Tags.Contains(TEXT("Essential")) ? Essentials : Others).Add(Id);
	}
	for (int32 i = Others.Num() - 1; i > 0; --i)
	{
		Others.Swap(i, Rng.RandRange(0, i));
	}
	TArray<FName> Stock = Essentials;
	Stock.Append(Others);
	int32 Index = 0;
	for (AActor* Actor : LayoutActors)
	{
		if (AShelfSlot* Slot = Cast<AShelfSlot>(Actor))
		{
			Slot->SetStock(Stock.Num() > 0 ? Stock[Index++ % Stock.Num()] : NAME_None);
		}
	}
}

void AHTMGameMode::GenerateJunkyardOffers()
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	if (!GS || !Data)
	{
		return;
	}
	// Pistas vagas del vendedor: información incompleta (GDD §10.2).
	const FText Hints[] = {
		LOCTEXT("Hint0", "Arranca... a veces"),
		LOCTEXT("Hint1", "Solo lo usaba mi abuela"),
		LOCTEXT("Hint2", "Hace un ruidito, nada grave"),
		LOCTEXT("Hint3", "Le falta alguna cosilla"),
		LOCTEXT("Hint4", "Como nuevo (no)"),
		LOCTEXT("Hint5", "Estuvo en un río, pero poco"),
	};
	GS->JunkyardOffers.Reset();
	for (int32 i = 0; i < UHTMTuningData::Get().JunkyardOffersPerDay; ++i)
	{
		FJunkyardOffer Offer;
		Offer.OfferId = i;
		Offer.ModelId = UCarFactory::PickModel(Rng, {}, this);
		const FCarModelRow* Model = Data->FindCarModel(Offer.ModelId);
		Offer.Price = Model ? Rng.RandRange(Model->JunkyardPriceMin, Model->JunkyardPriceMax) : 300;
		Offer.Hint = Hints[Rng.RandRange(0, UE_ARRAY_COUNT(Hints) - 1)];
		Offer.Seed = Rng.RandRange(1, INT32_MAX - 1);
		GS->JunkyardOffers.Add(Offer);
	}
	const TArray<FName> Buyers = Data->GetBuyerIds();
	GS->TodaysBuyer = Buyers.Num() > 0 ? Buyers[Rng.RandRange(0, Buyers.Num() - 1)] : NAME_None;
	GS->NotifyJobsChanged();
}

// ============================================================================ Compraventa, grúa, perdidos

bool AHTMGameMode::BuyJunkyardOffer(int32 OfferIndex, AMechanicCharacter* Who)
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	if (!GS || !Blockout || !GS->JunkyardOffers.IsValidIndex(OfferIndex) || GS->JunkyardOffers[OfferIndex].bSold)
	{
		return false;
	}
	FJunkyardOffer& Offer = GS->JunkyardOffers[OfferIndex];
	if (!GS->TrySpend(Offer.Price, LOCTEXT("JunkCar", "Coche del desguace")))
	{
		return false;
	}
	FCarSpawnOptions Options;
	Options.ConditionMin = 10.f;
	Options.ConditionMax = 75.f;
	Options.MissingPartChance = 0.12f;
	Options.RustChance = 0.6f;
	Options.DirtChance = 0.85f;
	Options.LooseBoltChance = 0.2f;
	Options.MinFaults = 1;
	Options.MaxFaults = 2;
	Options.Seed = Offer.Seed;

	FTransform Where = Blockout->GetJunkyardDropSpot();
	for (int32 Try = 0; Try < 4; ++Try)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMJunkDrop), false);
		if (!GetWorld()->OverlapBlockingTestByChannel(Where.GetLocation() + FVector(0.f, 0.f, 80.f), FQuat::Identity, ECC_Vehicle, FCollisionShape::MakeBox(FVector(200.f, 100.f, 50.f)), Params))
		{
			break;
		}
		Where.AddToTranslation(FVector(-450.f, 0.f, 0.f));
	}
	AModularCar* Car = UCarFactory::SpawnCar(this, Offer.ModelId, Where, Options);
	Offer.bSold = true;
	GS->NotifyJobsChanged();
	if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
	{
		Tel->Record(this, ETelemetryEvent::CarBought, Who ? Who->GetPlayerNameSafe() : FString(), Offer.ModelId.ToString(), Where.GetLocation(), (float)Offer.Price);
	}
	GS->MulticastToast(LOCTEXT("Bought", "Coche comprado: os espera en el desguace"), HTMPalette::CarSky());
	return Car != nullptr;
}

bool AHTMGameMode::SellCar(AModularCar* Car, AMechanicCharacter* Who)
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	if (!GS || !Car || !Car->IsOwnedByWorkshop() || Car->GetDriver())
	{
		return false;
	}
	const int32 Price = ASellPoint::QuotePrice(Car, GS->TodaysBuyer, this);
	const FText Name = Car->GetCarName();
	for (ACarPart* Part : Car->GetMountedParts())
	{
		Part->Destroy();
	}
	Car->Destroy();
	GS->AddMoney(Price, FText::Format(LOCTEXT("SoldReason", "Venta de {0}"), Name));
	if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
	{
		Tel->Record(this, ETelemetryEvent::CarSold, Who ? Who->GetPlayerNameSafe() : FString(), Name.ToString(), FVector::ZeroVector, (float)Price);
	}
	return true;
}

void AHTMGameMode::CallTow(AMechanicCharacter* Who)
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	if (!GS || !Blockout)
	{
		return;
	}
	const FBox Home = HomeBox(Blockout->GetWorkshopBounds(GS->WorkshopLevel));
	TArray<AModularCar*> ToTow;
	for (TActorIterator<AModularCar> It(GetWorld()); It; ++It)
	{
		if (It->IsFlipped() || !Home.IsInsideXY(It->GetActorLocation()) || It->GetActorLocation().Z < -200.f)
		{
			ToTow.Add(*It);
		}
	}
	if (ToTow.Num() == 0)
	{
		GS->MulticastToast(LOCTEXT("NoTow", "La grúa no ve ningún coche que remolcar"), HTMPalette::White());
		return;
	}
	if (!GS->TrySpend(UHTMTuningData::Get().TowFee, LOCTEXT("TowReason", "Grúa")))
	{
		return;
	}
	const TArray<FTransform> Spots = Blockout->GetTowSpots();
	for (int32 i = 0; i < ToTow.Num(); ++i)
	{
		FTransform Where = Spots[i % Spots.Num()];
		Where.AddToTranslation(FVector(0.f, 0.f, 80.f + (i / Spots.Num()) * 250.f));
		ToTow[i]->ResetTo(Where);
	}
	GS->MulticastToast(FText::Format(LOCTEXT("Towed", "La grúa ha traído {0} coche(s)"), FText::AsNumber(ToTow.Num())), HTMPalette::SafetyOrange());
}

int32 AHTMGameMode::CollectLostItems()
{
	const AHTMGameState* GS = GetGameState<AHTMGameState>();
	if (!GS || !Blockout)
	{
		return 0;
	}
	const FWorkshopLayout L = Blockout->GetLayout(GS->WorkshopLevel);
	const FBox Home = HomeBox(L.WorkshopBounds);
	int32 Count = 0;
	for (TActorIterator<AGrabbableActor> It(GetWorld()); It; ++It)
	{
		AGrabbableActor* Item = *It;
		if (Item->IsCarried() || Item->GetAttachParentActor() || Item->Tags.Contains(TagTestZone))
		{
			continue;
		}
		if (Home.IsInsideXY(Item->GetActorLocation()) && Item->GetActorLocation().Z > -200.f)
		{
			continue;
		}
		// Siempre hay forma de recuperar lo perdido (GDD §14).
		const FVector Offset(Rng.FRandRange(-150.f, 150.f), Rng.FRandRange(-150.f, 150.f), 40.f + Count * 5.f);
		Item->SetActorLocation(L.LostAndFound + Offset, false, nullptr, ETeleportType::ResetPhysics);
		if (UPrimitiveComponent* Body = Item->GetPhysicsBody())
		{
			if (Body->IsSimulatingPhysics())
			{
				Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			}
		}
		++Count;
	}
	return Count;
}

bool AHTMGameMode::UpgradeWorkshop(AMechanicCharacter* Who)
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	if (!GS || !Data || GS->DayPhase == EDayPhase::Open)
	{
		return false;
	}
	const FWorkshopLevelRow* Next = Data->FindWorkshopLevel(GS->WorkshopLevel + 1);
	if (!Next || Next->Level != GS->WorkshopLevel + 1)
	{
		return false;
	}
	if (GS->Reputation < Next->MinReputation)
	{
		GS->MulticastToast(FText::Format(LOCTEXT("NeedRep", "Hace falta reputación {0} para ampliar"), FText::AsNumber(FMath::RoundToInt(Next->MinReputation))), HTMPalette::ToolRed());
		return false;
	}
	if (!GS->TrySpend(Next->UpgradeCost, FText::Format(LOCTEXT("UpgradeReason", "Ampliación: {0}"), Next->DisplayName)))
	{
		return false;
	}
	const int32 Previous = GS->WorkshopLevel;
	GS->WorkshopLevel = Next->Level;
	GS->NotifyWorkshopLevelChanged();
	SpawnLayoutActors(GS->WorkshopLevel, Previous);
	GS->MulticastToast(FText::Format(LOCTEXT("Upgraded", "¡Nuevo taller: {0}!"), Next->DisplayName), HTMPalette::CarLime());
	SaveWorkshop();
	return true;
}

// ============================================================================ Guardado (anfitrión)

void AHTMGameMode::LoadWorkshop()
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	if (!GS)
	{
		return;
	}
	if (!bNewGameRequested && UGameplayStatics::DoesSaveGameExist(UHTMSaveGame::SlotName(), 0))
	{
		LoadedSave = Cast<UHTMSaveGame>(UGameplayStatics::LoadGameFromSlot(UHTMSaveGame::SlotName(), 0));
	}
	if (LoadedSave && LoadedSave->Version == UHTMSaveGame::CurrentVersion)
	{
		GS->Money = LoadedSave->Money;
		GS->Reputation = LoadedSave->Reputation;
		GS->WorkshopLevel = FMath::Max(1, LoadedSave->WorkshopLevel);
		GS->DayNumber = LoadedSave->DayNumber;
		GS->WallColorIndex = LoadedSave->WallColorIndex;
		GS->UnlockedCosmetics = LoadedSave->UnlockedCosmetics;
		GS->RoomOptions = LoadedSave->RoomOptions;
		UE_LOG(LogHTM, Log, TEXT("Partida cargada: día %d, %d €, nivel %d"), GS->DayNumber, GS->Money, GS->WorkshopLevel);
	}
	else
	{
		LoadedSave = nullptr;
		GS->Money = UHTMTuningData::Get().StartMoney;
		GS->WorkshopLevel = 1;
		GS->DayNumber = 0;
	}
	// Las opciones de la URL mandan sobre las guardadas.
	const FString& Options = OptionsString;
	if (UGameplayStatics::HasOption(Options, TEXT("Chaotic"))) { GS->RoomOptions.bChaoticPhysics = bChaoticOption; }
	if (UGameplayStatics::HasOption(Options, TEXT("FriendlyPush"))) { GS->RoomOptions.bFriendlyPush = bFriendlyPushOption; }
	GS->NotifyWorkshopLevelChanged();
	GS->NotifyWallColorChanged();
}

void AHTMGameMode::RestoreSavedObjects(const UHTMSaveGame* Save)
{
	for (const FSavedCar& Saved : Save->Cars)
	{
		AModularCar* Car = UCarFactory::SpawnEmptyCar(this, Saved.ModelId, Saved.Transform, Saved.Chassis.BaseColor);
		if (!Car)
		{
			continue;
		}
		Car->SetIdentity(Saved.Name, Saved.Plate, Saved.Traits);
		Car->RestoreRecords(Saved.Chassis, Saved.BestTopSpeedKmh, Saved.MaxAirTime);
		for (const FSavedPart& P : Saved.Parts)
		{
			ACarPart* Part = UCarFactory::SpawnPart(this, P.PartId, FTransform(Car->GetActorRotation(), Car->GetSlotWorldLocation(P.SlotName)), P.Condition, P.Fault);
			if (Part)
			{
				Part->SetSurface(P.Surface);
				if (P.bFaultRevealed) { Part->RevealFault(nullptr); }
				if (Car->MountPart(Part, P.SlotName, P.bReversed, nullptr, false))
				{
					Car->SetBolts(P.SlotName, P.BoltsTight);
				}
			}
		}
		Car->SetCustomerJob(0);
		Car->RecalculateStats();
	}
	for (const FSavedPart& P : Save->LooseParts)
	{
		if (ACarPart* Part = UCarFactory::SpawnPart(this, P.PartId, P.Transform, P.Condition, P.Fault))
		{
			Part->SetSurface(P.Surface);
		}
	}
	for (const FSavedDecor& D : Save->Decor)
	{
		if (AGrabbableActor* A = UCarFactory::SpawnProp(this, D.Spec, D.Transform))
		{
			A->Tags.Add(TagDecor);
		}
	}
	// Las herramientas se reponen en el banco (siempre se pueden recuperar).
}

void AHTMGameMode::SaveWorkshop()
{
	AHTMGameState* GS = GetGameState<AHTMGameState>();
	if (!GS || !Blockout)
	{
		return;
	}
	UHTMSaveGame* Save = Cast<UHTMSaveGame>(UGameplayStatics::CreateSaveGameObject(UHTMSaveGame::StaticClass()));
	Save->Money = GS->Money;
	Save->Reputation = GS->Reputation;
	Save->WorkshopLevel = GS->WorkshopLevel;
	Save->DayNumber = GS->DayNumber;
	Save->WallColorIndex = GS->WallColorIndex;
	Save->UnlockedCosmetics = GS->UnlockedCosmetics;
	Save->RoomOptions = GS->RoomOptions;

	const FBox Home = HomeBox(Blockout->GetWorkshopBounds(GS->WorkshopLevel));
	for (TActorIterator<AModularCar> It(GetWorld()); It; ++It)
	{
		AModularCar* Car = *It;
		if (!Car->IsOwnedByWorkshop())
		{
			continue;
		}
		FSavedCar Saved;
		Saved.ModelId = Car->GetModelId();
		Saved.Transform = Home.IsInsideXY(Car->GetActorLocation()) && !Car->IsFlipped() ? Car->GetActorTransform() : Blockout->GetTowSpots()[0];
		Saved.Name = Car->GetCarName();
		Saved.Plate = Car->GetPlate();
		Saved.Traits = Car->GetTraits();
		Saved.Chassis = Car->GetChassisSurface();
		Saved.BestTopSpeedKmh = Car->GetBestTopSpeedKmh();
		Saved.MaxAirTime = Car->GetMaxAirTime();
		for (const FCarSlotState& S : Car->GetSlots())
		{
			if (!S.Part)
			{
				continue;
			}
			FSavedPart P;
			P.SlotName = S.SlotName;
			P.PartId = S.Part->GetPartId();
			P.Condition = S.Part->GetCondition();
			P.BoltsTight = S.BoltsTight;
			P.bReversed = S.bReversed;
			P.Surface = S.Part->GetSurface();
			P.Fault = S.Part->GetHiddenFault();
			P.bFaultRevealed = S.Part->IsFaultRevealed();
			Saved.Parts.Add(P);
		}
		Save->Cars.Add(Saved);
	}
	for (TActorIterator<AGrabbableActor> It(GetWorld()); It; ++It)
	{
		AGrabbableActor* Item = *It;
		if (Item->GetAttachParentActor() || !Home.IsInsideXY(Item->GetActorLocation()))
		{
			continue;
		}
		if (const ACarPart* Part = Cast<ACarPart>(Item))
		{
			if (Part->IsMounted())
			{
				continue;
			}
			FSavedPart P;
			P.PartId = Part->GetPartId();
			P.Transform = Part->GetActorTransform();
			P.Condition = Part->GetCondition();
			P.Surface = Part->GetSurface();
			P.Fault = Part->GetHiddenFault();
			Save->LooseParts.Add(P);
		}
		else if (Item->Tags.Contains(TagDecor))
		{
			FSavedDecor D;
			D.Spec = Item->GetPropSpec();
			D.Transform = Item->GetActorTransform();
			Save->Decor.Add(D);
		}
	}

	// Perfiles: los guardados + los jugadores actuales.
	if (LoadedSave)
	{
		Save->Players = LoadedSave->Players;
	}
	for (APlayerState* PSBase : GS->PlayerArray)
	{
		if (const AHTMPlayerState* PS = Cast<AHTMPlayerState>(PSBase))
		{
			FSavedPlayer* Existing = Save->Players.FindByPredicate([PS](const FSavedPlayer& P) { return P.PlayerName == PS->GetPlayerName(); });
			FSavedPlayer& Target = Existing ? *Existing : Save->Players.AddDefaulted_GetRef();
			Target.PlayerName = PS->GetPlayerName();
			Target.Loadout = PS->Loadout;
			Target.Achievements = PS->Achievements;
		}
	}

	if (UGameplayStatics::SaveGameToSlot(Save, UHTMSaveGame::SlotName(), 0))
	{
		LoadedSave = Save;
		UE_LOG(LogHTM, Log, TEXT("Taller guardado (día %d, %d coches, %d piezas sueltas)"), Save->DayNumber, Save->Cars.Num(), Save->LooseParts.Num());
	}
}

void AHTMGameMode::ResetSave()
{
	UGameplayStatics::DeleteGameInSlot(UHTMSaveGame::SlotName(), 0);
	LoadedSave = nullptr;
}

// ============================================================================ Depuración

void AHTMGameMode::DebugSpawnCar(FName ModelId, AMechanicCharacter* Near)
{
	if (!Near)
	{
		return;
	}
	const FTransform Where(FRotator(0.f, Near->GetActorRotation().Yaw, 0.f), Near->GetActorLocation() + Near->GetActorForwardVector() * 450.f);
	FCarSpawnOptions Options;
	UCarFactory::SpawnCar(this, ModelId.IsNone() ? UCarFactory::PickModel(Rng, {}, this) : ModelId, Where, Options);
}

void AHTMGameMode::DebugSpawnPart(FName PartId, AMechanicCharacter* Near)
{
	if (Near)
	{
		UCarFactory::SpawnPart(this, PartId, FTransform(Near->GetActorLocation() + Near->GetActorForwardVector() * 150.f + FVector(0.f, 0.f, 50.f)));
	}
}

#undef LOCTEXT_NAMESPACE
