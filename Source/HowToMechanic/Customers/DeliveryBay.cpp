#include "Customers/DeliveryBay.h"
#include "Customers/JobDirectorComponent.h"
#include "Vehicle/ModularCar.h"
#include "Character/MechanicCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Core/HTMGameMode.h"
#include "Core/HTMGameState.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "HTMDelivery"

ADeliveryBay::ADeliveryBay()
{
	bReplicates = true;
	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	SetRootComponent(Zone);
	Zone->SetBoxExtent(FVector(300.f, 250.f, 150.f));
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);

	FloorMark = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMark"));
	FloorMark->SetupAttachment(Zone);
	FloorMark->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Sign = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sign"));
	Sign->SetupAttachment(Zone);
	Sign->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Sign->SetCollisionResponseToChannel(ECC_Interaction, ECR_Block);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Zone);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(30.f);
	Label->SetTextRenderColor(FColor::Black);
}

void ADeliveryBay::BeginPlay()
{
	Super::BeginPlay();
	FloorMark->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cube")));
	FloorMark->SetRelativeLocation(FVector(0.f, 0.f, -148.f));
	FloorMark->SetRelativeScale3D(FVector(6.f, 5.f, 0.02f));
	UHTMVisualLibrary::ApplyPlaceholderMaterial(FloorMark, HTMPalette::SignalYellow());
	Sign->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cube")));
	Sign->SetRelativeLocation(FVector(0.f, 290.f, 0.f));
	Sign->SetRelativeScale3D(FVector(1.2f, 0.1f, 0.8f));
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Sign, HTMPalette::SignalYellow());
	Label->SetRelativeLocation(FVector(0.f, 283.f, 10.f));
	Label->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Label->SetText(LOCTEXT("Label", "ENTREGAS"));
}

AModularCar* ADeliveryBay::FindDeliverableCar() const
{
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	TArray<AActor*> Overlapping;
	Zone->GetOverlappingActors(Overlapping, AModularCar::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		AModularCar* Car = Cast<AModularCar>(Actor);
		if (Car && !Car->GetDriver() && GS && GS->FindJobForCar(Car))
		{
			return Car;
		}
	}
	return nullptr;
}

bool ADeliveryBay::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return Verb == EInteractionVerb::Use && Who && Who->GetInteraction()->HasHandFree() && FindDeliverableCar() != nullptr;
}

FText ADeliveryBay::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	const AModularCar* Car = FindDeliverableCar();
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	const FActiveJob* Job = (GS && Car) ? GS->FindJobForCar(Car) : nullptr;
	return Job ? FText::Format(LOCTEXT("Deliver", "Entregar {0} a {1}"), Car->GetCarName(), Job->CustomerName) : FText::GetEmpty();
}

void ADeliveryBay::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	AHTMGameMode* GM = GetWorld()->GetAuthGameMode<AHTMGameMode>();
	if (AModularCar* Car = FindDeliverableCar())
	{
		if (GM && GM->GetJobDirector())
		{
			GM->GetJobDirector()->DeliverCar(Car, Who);
		}
	}
}

FVector ADeliveryBay::GetInteractionLocation() const
{
	return Sign->GetComponentLocation() + FVector(0.f, 0.f, 60.f);
}

#undef LOCTEXT_NAMESPACE
