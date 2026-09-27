#include "Economy/EconomyActors.h"
#include "Character/MechanicCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Parts/CarPart.h"
#include "Tools/Tool.h"
#include "Vehicle/ModularCar.h"
#include "Vehicle/CarFactory.h"
#include "Core/HTMGameMode.h"
#include "Core/HTMGameState.h"
#include "Core/HTMDataSubsystem.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Core/HTMTelemetrySubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HTMEconomy"

// ============================================================================ Base diegética

AHTMDiegeticActor::AHTMDiegeticActor()
{
	bReplicates = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BodyMesh->SetCollisionResponseToChannel(ECC_Interaction, ECR_Block);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextBottom);
	Label->SetWorldSize(20.f);
	Label->SetTextRenderColor(FColor::White);
}

void AHTMDiegeticActor::GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const
{
	OutShape = TEXT("Cube");
	OutSize = FVector(60.f);
	OutColor = HTMPalette::ToolRed();
}

void AHTMDiegeticActor::BeginPlay()
{
	Super::BeginPlay();
	FName Shape;
	FVector Size;
	FLinearColor Color;
	GetBodySpec(Shape, Size, Color);
	BodyMesh->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(Shape));
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, Size.Z * 0.5f));
	BodyMesh->SetRelativeScale3D(Size / 100.f);
	BodyMID = UHTMVisualLibrary::ApplyPlaceholderMaterial(BodyMesh, Color);
	Label->SetRelativeLocation(FVector(Size.X * 0.5f + 2.f, 0.f, Size.Z + 10.f));
}

void AHTMDiegeticActor::SetLabel(const FText& Text)
{
	Label->SetText(Text);
}

void AHTMDiegeticActor::SetHighlighted(bool bHighlighted)
{
	UHTMVisualLibrary::SetHighlighted(BodyMesh, BodyMID, bHighlighted);
}

FVector AHTMDiegeticActor::GetInteractionLocation() const
{
	return Label->GetComponentLocation();
}

// ============================================================================ Estantería

AShelfSlot::AShelfSlot()
{
	Display = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Display"));
	Display->SetupAttachment(Root);
	Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetWorldSize(12.f);
	Label->SetTextRenderColor(FColor::Black);
}

void AShelfSlot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShelfSlot, PartId);
}

void AShelfSlot::GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const
{
	// Balda de estantería (la estructura la pone el blockout).
	OutShape = TEXT("Cube");
	OutSize = FVector(55.f, 110.f, 8.f);
	OutColor = HTMPalette::ToolBlue();
}

void AShelfSlot::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Stock();
}

void AShelfSlot::SetStock(FName InPartId)
{
	PartId = InPartId;
	OnRep_Stock();
}

void AShelfSlot::OnRep_Stock()
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	const FPartDefinitionRow* Row = Data ? Data->FindPart(PartId) : nullptr;
	Display->SetVisibility(Row != nullptr);
	if (!Row)
	{
		SetLabel(LOCTEXT("SoldOut", "AGOTADO"));
		return;
	}
	// Miniatura de la pieza sobre la balda + etiqueta de precio colgando.
	Display->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(Row->PlaceholderShape));
	const FVector Size = Row->PlaceholderSize.ComponentMin(FVector(50.f, 100.f, 60.f));
	Display->SetRelativeScale3D(Size / 100.f);
	Display->SetRelativeLocation(FVector(0.f, 0.f, 8.f + Size.Z * 0.5f));
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Display, Row->PlaceholderColor);
	SetLabel(FText::Format(LOCTEXT("PriceTag", "{0}\n{1} EUR"), Row->DisplayName, FText::AsNumber(Row->Price)));
	Label->SetRelativeLocation(FVector(30.f, 0.f, -4.f));
	Label->SetVerticalAlignment(EVRTA_TextTop);
}

bool AShelfSlot::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return Verb == EInteractionVerb::Grab && !PartId.IsNone() && Who && Who->GetInteraction()->GetHeldObject() == nullptr;
}

FText AShelfSlot::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	const FPartDefinitionRow* Row = Data ? Data->FindPart(PartId) : nullptr;
	return Row ? FText::Format(LOCTEXT("Buy", "Comprar {0} ({1} EUR)"), Row->DisplayName, FText::AsNumber(Row->Price)) : FText::GetEmpty();
}

void AShelfSlot::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	const FPartDefinitionRow* Row = Data ? Data->FindPart(PartId) : nullptr;
	if (!GS || !Row || !GS->TrySpend(Row->Price, Row->DisplayName))
	{
		return;
	}
	const FTransform Where(GetActorRotation(), Display->GetComponentLocation() + FVector(0.f, 0.f, 20.f));
	if (ACarPart* Part = UCarFactory::SpawnPart(this, PartId, Where))
	{
		Who->GetInteraction()->PickUp(Part);
		if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
		{
			Tel->Record(this, ETelemetryEvent::PartBought, Who->GetPlayerNameSafe(), PartId.ToString(), GetActorLocation(), Row->Price);
		}
	}
	SetStock(NAME_None);
}

// ============================================================================ Caja registradora

ACashRegister::ACashRegister()
{
	PrimaryActorTick.bCanEverTick = true;
	Label->SetWorldSize(28.f);
	Label->SetTextRenderColor(FColor(185, 224, 106));
}

void ACashRegister::GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const
{
	OutShape = TEXT("Cube");
	OutSize = FVector(70.f, 90.f, 110.f);
	OutColor = HTMPalette::ToolRed();
}

void ACashRegister::BeginPlay()
{
	Super::BeginPlay();
	RefreshDisplay();
}

void ACashRegister::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Timer -= DeltaSeconds;
	if (Timer <= 0.f)
	{
		Timer = 0.5f;
		RefreshDisplay();
	}
}

void ACashRegister::RefreshDisplay()
{
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	if (!GS)
	{
		return;
	}
	FText Phase;
	switch (GS->DayPhase)
	{
	case EDayPhase::Closed:  Phase = LOCTEXT("PhaseClosed", "CERRADO"); break;
	case EDayPhase::Open:
	{
		const int32 Left = FMath::RoundToInt(GS->GetDayTimeRemaining());
		Phase = FText::Format(LOCTEXT("PhaseOpen", "ABIERTO {0}:{1}"), FText::AsNumber(Left / 60), FText::FromString(FString::Printf(TEXT("%02d"), Left % 60)));
		break;
	}
	default:                 Phase = LOCTEXT("PhaseSummary", "FIN DEL DÍA"); break;
	}
	SetLabel(FText::Format(LOCTEXT("Register", "{0} EUR\nDía {1} · {2}\nRep. {3}"), FText::AsNumber(GS->Money), FText::AsNumber(GS->DayNumber),
		Phase, FText::AsNumber(FMath::RoundToInt(GS->Reputation))));
}

bool ACashRegister::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	return Verb == EInteractionVerb::Use && GS && Who && Who->GetInteraction()->HasHandFree();
}

FText ACashRegister::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	if (!GS)
	{
		return FText::GetEmpty();
	}
	switch (GS->DayPhase)
	{
	case EDayPhase::Closed:  return LOCTEXT("OpenShop", "Abrir el taller (empieza la jornada)");
	case EDayPhase::Open:    return LOCTEXT("CloseShop", "Cerrar antes de hora");
	default:                 return LOCTEXT("NextDay", "Preparar el día siguiente");
	}
}

void ACashRegister::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	AHTMGameMode* GM = GetWorld()->GetAuthGameMode<AHTMGameMode>();
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	if (!GM || !GS)
	{
		return;
	}
	switch (GS->DayPhase)
	{
	case EDayPhase::Closed:  GM->StartDay(); break;
	case EDayPhase::Open:    GM->EndDay(); break;
	default:                 GM->PrepareNextDay(); break;
	}
}

// ============================================================================ Desguace

AJunkyardOfferSign::AJunkyardOfferSign()
{
	PrimaryActorTick.bCanEverTick = true;
	Label->SetWorldSize(18.f);
	Label->SetTextRenderColor(FColor::Black);
}

void AJunkyardOfferSign::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJunkyardOfferSign, OfferIndex);
}

void AJunkyardOfferSign::GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const
{
	OutShape = TEXT("Cube");
	OutSize = FVector(10.f, 140.f, 90.f);
	OutColor = HTMPalette::SignalYellow();
}

const FJunkyardOffer* AJunkyardOfferSign::GetOffer() const
{
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	return GS && GS->JunkyardOffers.IsValidIndex(OfferIndex) ? &GS->JunkyardOffers[OfferIndex] : nullptr;
}

void AJunkyardOfferSign::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Timer -= DeltaSeconds;
	if (Timer > 0.f)
	{
		return;
	}
	Timer = 1.f;
	const FJunkyardOffer* Offer = GetOffer();
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	const FCarModelRow* Model = (Offer && Data) ? Data->FindCarModel(Offer->ModelId) : nullptr;
	if (!Offer || !Model)
	{
		SetLabel(LOCTEXT("NoOffer", "DESGUACE\n(sin ofertas)"));
	}
	else if (Offer->bSold)
	{
		SetLabel(LOCTEXT("Sold", "VENDIDO"));
	}
	else
	{
		SetLabel(FText::Format(LOCTEXT("Offer", "{0}\n{1} EUR\n\"{2}\""), Model->DisplayName, FText::AsNumber(Offer->Price), Offer->Hint));
	}
}

bool AJunkyardOfferSign::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	const FJunkyardOffer* Offer = GetOffer();
	return Verb == EInteractionVerb::Use && Offer && !Offer->bSold && Who && Who->GetInteraction()->HasHandFree();
}

FText AJunkyardOfferSign::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	const FJunkyardOffer* Offer = GetOffer();
	return Offer ? FText::Format(LOCTEXT("BuyCar", "Comprar este coche ({0} EUR) - estado: ???"), FText::AsNumber(Offer->Price)) : FText::GetEmpty();
}

void AJunkyardOfferSign::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	if (AHTMGameMode* GM = GetWorld()->GetAuthGameMode<AHTMGameMode>())
	{
		GM->BuyJunkyardOffer(OfferIndex, Who);
	}
}

// ============================================================================ Venta

ASellPoint::ASellPoint()
{
	PrimaryActorTick.bCanEverTick = true;
	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetupAttachment(Root);
	Zone->SetBoxExtent(FVector(300.f, 250.f, 150.f));
	Zone->SetRelativeLocation(FVector(0.f, -330.f, 150.f));
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);

	FloorMark = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMark"));
	FloorMark->SetupAttachment(Zone);
	FloorMark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetWorldSize(20.f);
	Label->SetTextRenderColor(FColor::Black);
}

void ASellPoint::GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const
{
	OutShape = TEXT("Cube");
	OutSize = FVector(10.f, 160.f, 120.f);
	OutColor = HTMPalette::CarLime();
}

void ASellPoint::BeginPlay()
{
	Super::BeginPlay();
	FloorMark->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cube")));
	FloorMark->SetRelativeLocation(FVector(0.f, 0.f, -148.f));
	FloorMark->SetRelativeScale3D(FVector(6.f, 5.f, 0.02f));
	UHTMVisualLibrary::ApplyPlaceholderMaterial(FloorMark, HTMPalette::CarLime());
}

void ASellPoint::Tick(float DeltaSeconds)
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
	const FBuyerProfileRow* Buyer = (GS && Data) ? Data->FindBuyer(GS->TodaysBuyer) : nullptr;
	const AModularCar* Car = FindCarInZone();
	FText Quote = FText::GetEmpty();
	if (Car && Buyer && Car->IsOwnedByWorkshop())
	{
		Quote = FText::Format(LOCTEXT("Quote", "\nOferta: {0} EUR"), FText::AsNumber(QuotePrice(Car, GS->TodaysBuyer, this)));
	}
	SetLabel(Buyer ? FText::Format(LOCTEXT("SellSign", "VENTA · Hoy compra: {0}\n{1}{2}"), Buyer->DisplayName, Buyer->LikesText, Quote) : LOCTEXT("SellNone", "VENTA"));
}

AModularCar* ASellPoint::FindCarInZone() const
{
	TArray<AActor*> Overlapping;
	Zone->GetOverlappingActors(Overlapping, AModularCar::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		AModularCar* Car = Cast<AModularCar>(Actor);
		if (Car && !Car->GetDriver())
		{
			return Car;
		}
	}
	return nullptr;
}

int32 ASellPoint::QuotePrice(const AModularCar* Car, FName BuyerId, const UObject* WorldContext)
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(WorldContext);
	const FBuyerProfileRow* Buyer = Data ? Data->FindBuyer(BuyerId) : nullptr;
	const FCarModelRow* Model = Car ? Car->GetModel() : nullptr;
	if (!Model)
	{
		return 0;
	}
	float Price = (float)Car->EstimateValue();
	if (Buyer)
	{
		const float Cond = Car->GetAverageCondition() / 100.f;
		Price *= FMath::Lerp(1.f, 0.5f + Cond, FMath::Clamp(Buyer->ConditionWeight - 1.f, 0.f, 1.f));
		float Bonus = 0.f;
		for (const FName& Tag : Buyer->LikedTags)
		{
			Bonus += Car->HasPartWithTag(Tag) ? Buyer->TagBonus : 0.f;
		}
		Bonus += Car->HasVividPaint() ? Buyer->VividPaintBonus : 0.f;
		Bonus += Car->GetMinWheelRadius() >= 40.f ? Buyer->BigWheelsBonus : 0.f;
		Bonus += Car->GetFuelUse() <= 6.f ? Buyer->LowFuelBonus : 0.f;
		Price *= 1.f + Bonus;
	}
	return FMath::Max(50, FMath::RoundToInt(Price));
}

bool ASellPoint::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	const AModularCar* Car = FindCarInZone();
	return Verb == EInteractionVerb::Use && Car && Car->IsOwnedByWorkshop() && Who && Who->GetInteraction()->HasHandFree();
}

FText ASellPoint::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	const AModularCar* Car = FindCarInZone();
	return (GS && Car) ? FText::Format(LOCTEXT("Sell", "Vender {0} por {1} EUR"), Car->GetCarName(), FText::AsNumber(QuotePrice(Car, GS->TodaysBuyer, this))) : FText::GetEmpty();
}

void ASellPoint::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	AHTMGameMode* GM = GetWorld()->GetAuthGameMode<AHTMGameMode>();
	if (AModularCar* Car = FindCarInZone())
	{
		if (GM)
		{
			GM->SellCar(Car, Who);
		}
	}
}

// ============================================================================ Chatarra

AScrapBin::AScrapBin()
{
	Mouth = CreateDefaultSubobject<UBoxComponent>(TEXT("Mouth"));
	Mouth->SetupAttachment(Root);
	Mouth->SetBoxExtent(FVector(70.f, 110.f, 30.f));
	Mouth->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Mouth->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mouth->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mouth->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	Label->SetText(LOCTEXT("Scrap", "CHATARRA"));
	Label->SetTextRenderColor(FColor::Black);
}

void AScrapBin::GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const
{
	OutShape = TEXT("Cube");
	OutSize = FVector(150.f, 230.f, 100.f);
	OutColor = HTMPalette::Rust();
}

void AScrapBin::BeginPlay()
{
	Super::BeginPlay();
	// Solo el marco sólido: la boca queda abierta arriba (la colisión del cuerpo es la caja entera).
	if (HasAuthority())
	{
		Mouth->OnComponentBeginOverlap.AddDynamic(this, &AScrapBin::OnOverlap);
	}
}

void AScrapBin::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACarPart* Part = Cast<ACarPart>(OtherActor);
	if (!Part || Part->IsMounted() || Part->IsCarried())
	{
		// Herramientas y otros objetos: el contenedor los escupe (siempre se recuperan, GDD §14).
		if (AGrabbableActor* Other = Cast<AGrabbableActor>(OtherActor))
		{
			if (!Other->IsCarried() && Other->GetPhysicsBody()->IsSimulatingPhysics())
			{
				Other->GetPhysicsBody()->AddImpulse(FVector(0.f, 0.f, 600.f) + GetActorForwardVector() * 300.f, NAME_None, true);
			}
		}
		return;
	}
	AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	const float Fraction = UHTMTuningData::Get().ScrapValueFraction * FMath::Lerp(0.3f, 1.f, Part->GetCondition() / 100.f);
	const int32 Value = FMath::Max(1, FMath::RoundToInt(Part->GetPrice() * Fraction));
	if (GS)
	{
		GS->AddMoney(Value, FText::Format(LOCTEXT("ScrapReason", "Chatarra: {0}"), Part->GetDisplayName()));
	}
	UHTMVisualLibrary::SpawnFX(this, EHTMFX::Scrap, Part->GetActorLocation(), 1.f);
	Part->Destroy();
}

// ============================================================================ Grúa

ATowPhone::ATowPhone()
{
	Label->SetText(LOCTEXT("TowLabel", "GRÚA"));
	Label->SetWorldSize(16.f);
}

void ATowPhone::GetBodySpec(FName& OutShape, FVector& OutSize, FLinearColor& OutColor) const
{
	OutShape = TEXT("Cube");
	OutSize = FVector(20.f, 35.f, 50.f);
	OutColor = HTMPalette::SafetyOrange();
}

bool ATowPhone::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return Verb == EInteractionVerb::Use && Who && Who->GetInteraction()->HasHandFree();
}

FText ATowPhone::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return FText::Format(LOCTEXT("Tow", "Llamar a la grúa ({0} EUR): trae los coches perdidos o volcados"), FText::AsNumber(UHTMTuningData::Get().TowFee));
}

void ATowPhone::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	if (AHTMGameMode* GM = GetWorld()->GetAuthGameMode<AHTMGameMode>())
	{
		GM->CallTow(Who);
	}
}

#undef LOCTEXT_NAMESPACE
