#include "Progression/ProgressionActors.h"
#include "Character/MechanicCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Parts/CarPart.h"
#include "Parts/SurfaceTreatable.h"
#include "Vehicle/ModularCar.h"
#include "Core/HTMGameMode.h"
#include "Core/HTMGameState.h"
#include "Core/HTMPlayerState.h"
#include "Core/HTMDataSubsystem.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HTMProgression"

// ============================================================================ Ampliación

AWorkshopUpgradeTerminal::AWorkshopUpgradeTerminal()
{
	PrimaryActorTick.bCanEverTick = true;
	Label->SetWorldSize(16.f);
	Label->SetTextRenderColor(FColor::White);
}

void AWorkshopUpgradeTerminal::GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const
{
	OutShape = TEXT("Cube");
	OutSize = FVector(40.f, 80.f, 140.f);
	OutColor = HTMPalette::ToolBlue();
}

void AWorkshopUpgradeTerminal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Timer -= DeltaSeconds;
	if (Timer > 0.f)
	{
		return;
	}
	Timer = 1.f;
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	const FWorkshopLevelRow* Next = (GS && Data && GS->WorkshopLevel < Data->GetMaxWorkshopLevel()) ? Data->FindWorkshopLevel(GS->WorkshopLevel + 1) : nullptr;
	if (!Next)
	{
		SetLabel(LOCTEXT("MaxLevel", "TALLER AL MÁXIMO"));
		return;
	}
	SetLabel(FText::Format(LOCTEXT("NextLevel", "AMPLIAR:\n{0}\n{1} EUR · Rep. {2}"), Next->DisplayName, FText::AsNumber(Next->UpgradeCost), FText::AsNumber(FMath::RoundToInt(Next->MinReputation))));
}

bool AWorkshopUpgradeTerminal::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	return Verb == EInteractionVerb::Use && GS && Data && GS->WorkshopLevel < Data->GetMaxWorkshopLevel() && GS->DayPhase != EDayPhase::Open
		&& Who && Who->GetInteraction()->HasHandFree();
}

FText AWorkshopUpgradeTerminal::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return LOCTEXT("Upgrade", "Ampliar el taller (con el taller cerrado)");
}

void AWorkshopUpgradeTerminal::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	if (AHTMGameMode* GM = GetWorld()->GetAuthGameMode<AHTMGameMode>())
	{
		GM->UpgradeWorkshop(Who);
	}
}

// ============================================================================ Armario

AWardrobe::AWardrobe()
{
	Label->SetWorldSize(14.f);
}

void AWardrobe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWardrobe, Slot);
}

void AWardrobe::GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const
{
	OutShape = TEXT("Cube");
	OutSize = FVector(60.f, 90.f, 180.f);
	OutColor = HTMPalette::CarCream();
}

void AWardrobe::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Slot();
}

void AWardrobe::SetSlot(ECosmeticSlot InSlot)
{
	Slot = InSlot;
	OnRep_Slot();
}

void AWardrobe::OnRep_Slot()
{
	switch (Slot)
	{
	case ECosmeticSlot::Hat:       SetLabel(LOCTEXT("Hats", "GORROS")); break;
	case ECosmeticSlot::Outfit:    SetLabel(LOCTEXT("Outfits", "ROPA")); break;
	case ECosmeticSlot::Gloves:    SetLabel(LOCTEXT("Gloves", "GUANTES")); break;
	case ECosmeticSlot::ToolColor: SetLabel(LOCTEXT("ToolColor", "COLOR HERRAMIENTAS")); break;
	}
}

bool AWardrobe::IsUnlocked(const UObject* WorldContext, FName CosmeticId, const AHTMPlayerState* PS)
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(WorldContext);
	const FCosmeticRow* Row = Data ? Data->FindCosmetic(CosmeticId) : nullptr;
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AHTMGameState* GS = World ? World->GetGameState<AHTMGameState>() : nullptr;
	if (!Row)
	{
		return false;
	}
	if (Row->bUnlockedByDefault || (GS && GS->UnlockedCosmetics.Contains(CosmeticId)))
	{
		return true;
	}
	return Row->AchievementCount > 0 && PS && PS->GetAchievementCount(Row->Achievement) >= Row->AchievementCount;
}

FName AWardrobe::FindNext(const AMechanicCharacter* Who, bool bUnlocked) const
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	const AHTMPlayerState* PS = Who ? Who->GetHTMPlayerState() : nullptr;
	if (!Data || !PS)
	{
		return NAME_None;
	}
	TArray<FName> Ids = Data->GetCosmeticIds(Slot);
	Ids.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	FName Current;
	switch (Slot)
	{
	case ECosmeticSlot::Hat:       Current = PS->Loadout.Hat; break;
	case ECosmeticSlot::Outfit:    Current = PS->Loadout.Outfit; break;
	case ECosmeticSlot::Gloves:    Current = PS->Loadout.Gloves; break;
	case ECosmeticSlot::ToolColor: Current = PS->Loadout.ToolColor; break;
	}
	const int32 Start = Ids.IndexOfByKey(Current);
	for (int32 i = 1; i <= Ids.Num(); ++i)
	{
		const FName Id = Ids[(FMath::Max(Start, 0) + i) % Ids.Num()];
		const bool bIsUnlocked = IsUnlocked(this, Id, PS);
		if (bIsUnlocked == bUnlocked)
		{
			if (!bUnlocked)
			{
				// Solo sirven para comprar los que tienen precio.
				const FCosmeticRow* Row = Data->FindCosmetic(Id);
				if (!Row || Row->Price <= 0)
				{
					continue;
				}
			}
			return Id;
		}
	}
	return NAME_None;
}

bool AWardrobe::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	if (!Who || !Who->GetInteraction()->HasHandFree())
	{
		return false;
	}
	return (Verb == EInteractionVerb::Use && !FindNext(Who, true).IsNone()) || (Verb == EInteractionVerb::AltUse && !FindNext(Who, false).IsNone());
}

FText AWardrobe::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	const FCosmeticRow* Row = Data ? Data->FindCosmetic(FindNext(Who, Verb == EInteractionVerb::Use)) : nullptr;
	if (!Row)
	{
		return FText::GetEmpty();
	}
	return Verb == EInteractionVerb::Use ? FText::Format(LOCTEXT("Wear", "Ponerse: {0}"), Row->DisplayName)
		: FText::Format(LOCTEXT("BuyCosmetic", "Comprar {0} ({1} EUR)"), Row->DisplayName, FText::AsNumber(Row->Price));
}

void AWardrobe::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	AHTMPlayerState* PS = Who ? Who->GetHTMPlayerState() : nullptr;
	AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	if (!PS || !GS || !Data)
	{
		return;
	}
	const FName Id = FindNext(Who, Verb == EInteractionVerb::Use);
	const FCosmeticRow* Row = Data->FindCosmetic(Id);
	if (!Row)
	{
		return;
	}
	if (Verb == EInteractionVerb::AltUse)
	{
		if (!GS->TrySpend(Row->Price, Row->DisplayName))
		{
			return;
		}
		GS->UnlockedCosmetics.AddUnique(Id);
	}
	FMechanicLoadout Loadout = PS->Loadout;
	switch (Slot)
	{
	case ECosmeticSlot::Hat:       Loadout.Hat = Id; break;
	case ECosmeticSlot::Outfit:    Loadout.Outfit = Id; break;
	case ECosmeticSlot::Gloves:    Loadout.Gloves = Id; break;
	case ECosmeticSlot::ToolColor: Loadout.ToolColor = Id; break;
	}
	PS->SetLoadout(Loadout);
	Who->PlayEmote(0);
}

// ============================================================================ Color de paredes

AWallColorPanel::AWallColorPanel()
{
	Label->SetText(LOCTEXT("WallColor", "PINTURA\nPAREDES"));
	Label->SetWorldSize(12.f);
}

void AWallColorPanel::GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const
{
	OutShape = TEXT("Cube");
	OutSize = FVector(10.f, 60.f, 60.f);
	OutColor = HTMPalette::WallMint();
}

bool AWallColorPanel::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return Verb == EInteractionVerb::Use && Who && Who->GetInteraction()->HasHandFree();
}

FText AWallColorPanel::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return LOCTEXT("ChangeWall", "Cambiar el color de las paredes");
}

void AWallColorPanel::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	if (AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>())
	{
		GS->WallColorIndex = (GS->WallColorIndex + 1) % HTMPalette::WallColors().Num();
		GS->NotifyWallColorChanged();
	}
}

// ============================================================================ Cabina de pintura

APaintBooth::APaintBooth()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	SetRootComponent(Volume);
	Volume->SetBoxExtent(FVector(450.f, 320.f, 220.f));
	Volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Volume->SetCollisionResponseToAllChannels(ECR_Ignore);
	Volume->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
	Volume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
}

void APaintBooth::BeginPlay()
{
	Super::BeginPlay();
	// Cabina: marco azul claro y techo con luces (se ve desde fuera).
	const FLinearColor Frame = HTMPalette::CarSky();
	UHTMVisualLibrary::AddShape(this, Volume, TEXT("Cube"), FVector(-450.f, 0.f, 0.f), FVector(15.f, 640.f, 440.f), FRotator::ZeroRotator, Frame, true);
	UHTMVisualLibrary::AddShape(this, Volume, TEXT("Cube"), FVector(0.f, -320.f, 0.f), FVector(900.f, 15.f, 440.f), FRotator::ZeroRotator, Frame, true);
	UHTMVisualLibrary::AddShape(this, Volume, TEXT("Cube"), FVector(0.f, 320.f, 0.f), FVector(900.f, 15.f, 440.f), FRotator::ZeroRotator, Frame, true);
	UHTMVisualLibrary::AddShape(this, Volume, TEXT("Cube"), FVector(0.f, 0.f, 220.f), FVector(900.f, 640.f, 10.f), FRotator::ZeroRotator, HTMPalette::White(), false);
	SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, 220.f));
}

void APaintBooth::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	Timer += DeltaSeconds;
	if (Timer < 0.5f)
	{
		return;
	}
	TArray<AActor*> Inside;
	Volume->GetOverlappingActors(Inside);
	for (AActor* Actor : Inside)
	{
		if (AModularCar* Car = Cast<AModularCar>(Actor))
		{
			Car->SplashAllSurfaces(ESurfaceTreatment::BoothDry, Timer);
		}
		else if (ACarPart* Part = Cast<ACarPart>(Actor))
		{
			Part->TreatSurface(ESurfaceTreatment::BoothDry, Timer, FLinearColor::White, nullptr);
		}
	}
	Timer = 0.f;
}

#undef LOCTEXT_NAMESPACE
