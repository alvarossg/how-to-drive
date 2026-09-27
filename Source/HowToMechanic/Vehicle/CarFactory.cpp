#include "Vehicle/CarFactory.h"
#include "Vehicle/ModularCar.h"
#include "Parts/CarPart.h"
#include "Tools/Tool.h"
#include "Interaction/GrabbableActor.h"
#include "Core/HTMDataSubsystem.h"
#include "Core/HTMPalette.h"
#include "Core/HTMLog.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "HTMCarFactory"

EPartCategory UCarFactory::CategoryForFault(EHiddenFault Fault)
{
	switch (Fault)
	{
	case EHiddenFault::EngineKnock:
	case EHiddenFault::WaterPumpFailing: return EPartCategory::Engine;
	case EHiddenFault::ExhaustLeak:      return EPartCategory::Exhaust;
	case EHiddenFault::BrakesWorn:       return EPartCategory::Suspension;
	default:                             return EPartCategory::Wheel;
	}
}

FName UCarFactory::PickModel(FRandomStream& Rng, const TArray<FName>& Allowed, const UObject* WorldContext)
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(WorldContext);
	if (!Data)
	{
		return NAME_None;
	}
	TArray<FName> Candidates = Allowed.Num() > 0 ? Allowed : Data->GetCarModelIds();
	float Total = 0.f;
	for (const FName& Id : Candidates)
	{
		const FCarModelRow* Row = Data->FindCarModel(Id);
		Total += Row ? Row->SpawnWeight : 0.f;
	}
	float Pick = Rng.FRandRange(0.f, Total);
	for (const FName& Id : Candidates)
	{
		const FCarModelRow* Row = Data->FindCarModel(Id);
		Pick -= Row ? Row->SpawnWeight : 0.f;
		if (Pick <= 0.f)
		{
			return Id;
		}
	}
	return Candidates.Num() > 0 ? Candidates.Last() : NAME_None;
}

FString UCarFactory::MakePlate(FRandomStream& Rng)
{
	// Matrícula inventada: 4 cifras + 3 consonantes.
	static const TCHAR* Letters = TEXT("BCDFGHJKLMNPRSTVWXYZ");
	FString Plate = FString::Printf(TEXT("%04d "), Rng.RandRange(0, 9999));
	for (int32 i = 0; i < 3; ++i)
	{
		Plate.AppendChar(Letters[Rng.RandRange(0, 19)]);
	}
	return Plate;
}

ACarPart* UCarFactory::SpawnPart(UObject* WorldContext, FName PartId, const FTransform& Where, float Condition, EHiddenFault Fault,
	const FLinearColor* BodyColor, float Rust, float Dirt)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(WorldContext);
	const FPartDefinitionRow* Row = Data ? Data->FindPart(PartId) : nullptr;
	if (!World || !Row)
	{
		UE_LOG(LogHTM, Warning, TEXT("SpawnPart: pieza desconocida %s"), *PartId.ToString());
		return nullptr;
	}
	ACarPart* Part = World->SpawnActorDeferred<ACarPart>(ACarPart::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Part)
	{
		return nullptr;
	}
	FPartSurfaceState Surface;
	Surface.BaseColor = (Row->bUseCarColor && BodyColor) ? *BodyColor : Row->PlaceholderColor;
	Surface.Rust = Rust;
	Surface.Dirt = Dirt;
	Part->InitPart(PartId, Condition, Surface, Fault);
	Part->FinishSpawning(Where);
	return Part;
}

ATool* UCarFactory::SpawnTool(UObject* WorldContext, EToolType Type, const FTransform& Where)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	ATool* Tool = World->SpawnActorDeferred<ATool>(ATool::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Tool)
	{
		Tool->InitTool(Type);
		Tool->FinishSpawning(Where);
	}
	return Tool;
}

AGrabbableActor* UCarFactory::SpawnProp(UObject* WorldContext, const FGrabbablePropSpec& Spec, const FTransform& Where)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	AGrabbableActor* Prop = World->SpawnActorDeferred<AGrabbableActor>(AGrabbableActor::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Prop)
	{
		Prop->SetPropSpec(Spec);
		Prop->FinishSpawning(Where);
	}
	return Prop;
}

AModularCar* UCarFactory::SpawnEmptyCar(UObject* WorldContext, FName ModelId, const FTransform& Where, const FLinearColor& Color)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(WorldContext);
	if (!World || !Data || !Data->FindCarModel(ModelId))
	{
		UE_LOG(LogHTM, Warning, TEXT("SpawnCar: modelo desconocido %s"), *ModelId.ToString());
		return nullptr;
	}
	FTransform Raised = Where;
	Raised.AddToTranslation(FVector(0.f, 0.f, 70.f));
	AModularCar* Car = World->SpawnActorDeferred<AModularCar>(AModularCar::StaticClass(), Raised, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Car)
	{
		return nullptr;
	}
	Car->InitModel(ModelId, Color);
	Car->FinishSpawning(Raised);
	return Car;
}

AModularCar* UCarFactory::SpawnCar(UObject* WorldContext, FName ModelId, const FTransform& Where, const FCarSpawnOptions& Options)
{
	FRandomStream Rng(Options.Seed != 0 ? Options.Seed : FMath::Rand());
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(WorldContext);
	if (!Data)
	{
		return nullptr;
	}
	const TArray<FLinearColor>& Pastels = HTMPalette::CarPastels();
	const FLinearColor Color = Pastels[Rng.RandRange(0, Pastels.Num() - 1)];
	AModularCar* Car = SpawnEmptyCar(WorldContext, ModelId, Where, Color);
	const FCarModelRow* Model = Data->FindCarModel(ModelId);
	if (!Car || !Model)
	{
		return Car;
	}

	// ------------------------------------------------------------------ Piezas
	for (const FCarSlotDefinition& Def : Model->Slots)
	{
		FName PartId = Def.DefaultPartId;
		if (!Options.ForcedSlot.IsNone() && Def.SlotName == Options.ForcedSlot)
		{
			PartId = Options.ForcedPartId;
		}
		if (PartId.IsNone())
		{
			continue;
		}
		const bool bForced = Def.SlotName == Options.ForcedSlot;
		if (!bForced && Rng.FRand() < Options.MissingPartChance)
		{
			continue; // coche de desguace: le falta algo
		}
		const float Condition = Rng.FRandRange(Options.ConditionMin, Options.ConditionMax);
		const float Rust = Rng.FRand() < Options.RustChance ? Rng.FRandRange(0.5f, 1.f) : 0.f;
		const float Dirt = Rng.FRand() < Options.DirtChance ? Rng.FRandRange(0.4f, 1.f) : 0.f;
		const FTransform PartWhere(Car->GetActorRotation(), Car->GetSlotWorldLocation(Def.SlotName));
		ACarPart* Part = SpawnPart(WorldContext, PartId, PartWhere, Condition, EHiddenFault::None, &Color, Rust, Dirt);
		if (Part && Car->MountPart(Part, Def.SlotName, false, nullptr, true))
		{
			if (Rng.FRand() < Options.LooseBoltChance)
			{
				Car->SetBolts(Def.SlotName, Part->GetBoltCount() - 1);
			}
		}
	}

	// ------------------------------------------------------------------ Averías ocultas
	TArray<EHiddenFault> Faults;
	if (Options.ForcedFault != EHiddenFault::None)
	{
		Faults.Add(Options.ForcedFault);
	}
	const int32 NumRandom = Rng.RandRange(Options.MinFaults, Options.MaxFaults);
	const EHiddenFault Pool[] = { EHiddenFault::EngineKnock, EHiddenFault::WornBearing, EHiddenFault::ExhaustLeak,
		EHiddenFault::BrakesWorn, EHiddenFault::WaterPumpFailing, EHiddenFault::SlowPuncture };
	for (int32 i = 0; i < NumRandom; ++i)
	{
		Faults.AddUnique(Pool[Rng.RandRange(0, UE_ARRAY_COUNT(Pool) - 1)]);
	}
	for (EHiddenFault Fault : Faults)
	{
		const EPartCategory Cat = CategoryForFault(Fault);
		TArray<ACarPart*> Candidates = Car->GetMountedParts().FilterByPredicate([Cat](const ACarPart* P) { return P->GetCategory() == Cat && P->GetHiddenFault() == EHiddenFault::None; });
		if (Candidates.Num() > 0)
		{
			Candidates[Rng.RandRange(0, Candidates.Num() - 1)]->SetHiddenFault(Fault);
		}
	}

	// ------------------------------------------------------------------ Personalidad
	TArray<ECarTrait> Traits;
	TArray<const FCarTraitRow*> AllTraits = Data->GetTraits();
	const int32 NumTraits = Rng.RandRange(Options.MinTraits, Options.MaxTraits);
	for (int32 i = 0; i < NumTraits && AllTraits.Num() > 0; ++i)
	{
		float Total = 0.f;
		for (const FCarTraitRow* T : AllTraits) { Total += T->Weight; }
		float Pick = Rng.FRandRange(0.f, Total);
		for (int32 j = 0; j < AllTraits.Num(); ++j)
		{
			Pick -= AllTraits[j]->Weight;
			if (Pick <= 0.f)
			{
				Traits.AddUnique(AllTraits[j]->Trait);
				AllTraits.RemoveAt(j);
				break;
			}
		}
	}
	const TArray<FText> Names = Data->GetCarNames();
	const FText Name = Names.Num() > 0 ? Names[Rng.RandRange(0, Names.Num() - 1)] : Model->DisplayName;
	Car->SetIdentity(Name, MakePlate(Rng), Traits);
	Car->SetCustomerJob(Options.JobUid);
	Car->RecalculateStats();
	return Car;
}

#undef LOCTEXT_NAMESPACE
