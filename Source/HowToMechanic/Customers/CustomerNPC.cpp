#include "Customers/CustomerNPC.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HTMCustomer"

ACustomerNPC::ACustomerNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	BodyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyRoot"));
	BodyRoot->SetupAttachment(Root);

	auto Make = [this](const TCHAR* Name, USceneComponent* Parent)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return C;
	};
	Body = Make(TEXT("Body"), BodyRoot);
	Head = Make(TEXT("Head"), BodyRoot);
	EyeL = Make(TEXT("EyeL"), BodyRoot);
	EyeR = Make(TEXT("EyeR"), BodyRoot);

	Bubble = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Bubble"));
	Bubble->SetupAttachment(Root);
	Bubble->SetHorizontalAlignment(EHTA_Center);
	Bubble->SetVerticalAlignment(EVRTA_TextBottom);
	Bubble->SetWorldSize(22.f);
	Bubble->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	Bubble->SetTextRenderColor(FColor::White);
}

void ACustomerNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACustomerNPC, JobUid);
	DOREPLIFETIME(ACustomerNPC, CustomerName);
	DOREPLIFETIME(ACustomerNPC, Request);
	DOREPLIFETIME(ACustomerNPC, Color);
	DOREPLIFETIME(ACustomerNPC, Mood);
}

void ACustomerNPC::BeginPlay()
{
	Super::BeginPlay();
	auto Shape = [](UStaticMeshComponent* C, FName S, const FVector& Loc, const FVector& Size)
	{
		C->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(S));
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Size / 100.f);
	};
	// Mismo lenguaje que los mecánicos (cabeza grande, cuerpo pera) pero ropa de calle de color.
	Shape(Body, TEXT("Sphere"), FVector(0.f, 0.f, 42.f), FVector(60.f, 64.f, 84.f));
	Shape(Head, TEXT("Sphere"), FVector(0.f, 0.f, 104.f), FVector(46.f));
	Shape(EyeL, TEXT("Sphere"), FVector(18.f, -9.f, 108.f), FVector(15.f, 15.f, 17.f));
	Shape(EyeR, TEXT("Sphere"), FVector(18.f, 9.f, 108.f), FVector(15.f, 15.f, 17.f));
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Head, HTMPalette::Skin());
	UHTMVisualLibrary::ApplyPlaceholderMaterial(EyeL, HTMPalette::White());
	UHTMVisualLibrary::ApplyPlaceholderMaterial(EyeR, HTMPalette::White());
	OnRep_Customer();
}

void ACustomerNPC::InitCustomer(int32 InJobUid, const FText& InName, const FText& InRequest, const FLinearColor& InColor)
{
	JobUid = InJobUid;
	CustomerName = InName;
	Request = InRequest;
	Color = InColor;
	OnRep_Customer();
}

void ACustomerNPC::OnRep_Customer()
{
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Body, Color);
	Bubble->SetText(FText::Format(LOCTEXT("Bubble", "{0}:\n“{1}”"), CustomerName, Request));
}

void ACustomerNPC::React(EJobOutcome Outcome)
{
	Mood = Outcome;
	OnRep_Mood();
}

void ACustomerNPC::OnRep_Mood()
{
	ReactTime = 0.f;
	switch (Mood)
	{
	case EJobOutcome::FullPay:
		Bubble->SetText(LOCTEXT("Happy", "¡¡GENIAL!!"));
		UHTMVisualLibrary::SpawnFX(this, EHTMFX::Confetti, GetActorLocation() + FVector(0.f, 0.f, 150.f), 1.f);
		break;
	case EJobOutcome::PartialPay:
		Bubble->SetText(LOCTEXT("Meh", "Bueno... algo es algo"));
		break;
	case EJobOutcome::Angry:
		Bubble->SetText(LOCTEXT("Angry", "¡¡ESTO ES UN DESASTRE!!"));
		UHTMVisualLibrary::ApplyPlaceholderMaterial(Head, HTMPalette::ToolRed());
		break;
	default:
		break;
	}
}

void ACustomerNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	ReactTime += DeltaSeconds;

	// Animación procedural: balanceo de espera, saltos de alegría, pataleta.
	FVector Offset = FVector::ZeroVector;
	FRotator Rot = FRotator::ZeroRotator;
	switch (Mood)
	{
	case EJobOutcome::FullPay:
		Offset.Z = FMath::Abs(FMath::Sin(ReactTime * 8.f)) * 45.f;
		break;
	case EJobOutcome::PartialPay:
		Rot.Roll = FMath::Sin(ReactTime * 3.f) * 8.f;
		break;
	case EJobOutcome::Angry:
		Offset.Z = FMath::Abs(FMath::Sin(ReactTime * 16.f)) * 12.f;
		Rot.Roll = FMath::Sin(ReactTime * 20.f) * 10.f;
		break;
	default:
		Rot.Roll = FMath::Sin(Time * 1.5f) * 4.f;
		Offset.Z = FMath::Abs(FMath::Sin(Time * 1.5f)) * 3.f;
		break;
	}
	BodyRoot->SetRelativeLocationAndRotation(Offset, Rot);

	// El bocadillo mira siempre a la cámara local (legible a distancia).
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCam = Cam->GetCameraLocation() - Bubble->GetComponentLocation();
		Bubble->SetWorldRotation(FRotator(0.f, ToCam.Rotation().Yaw, 0.f));
	}
}

#undef LOCTEXT_NAMESPACE
