#include "Customers/JobBoard.h"
#include "Core/HTMGameState.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Vehicle/ModularCar.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "HTMJobBoard"

AJobBoard::AJobBoard()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(Root);
	Board = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
	Board->SetupAttachment(Root);
	Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	auto MakeText = [this](const TCHAR* Name, float Size)
	{
		UTextRenderComponent* T = CreateDefaultSubobject<UTextRenderComponent>(Name);
		T->SetupAttachment(Root);
		T->SetHorizontalAlignment(EHTA_Left);
		T->SetVerticalAlignment(EVRTA_TextTop);
		T->SetWorldSize(Size);
		T->SetTextRenderColor(FColor::White);
		return T;
	};
	Title = MakeText(TEXT("Title"), 26.f);
	Jobs = MakeText(TEXT("Jobs"), 13.f);
	LastResult = MakeText(TEXT("LastResult"), 13.f);
}

void AJobBoard::BeginPlay()
{
	Super::BeginPlay();
	// Pizarra verde oscura con marco de madera: se lee a distancia.
	Frame->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cube")));
	Frame->SetRelativeScale3D(FVector(0.06f, 3.4f, 2.1f));
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Frame, HTMPalette::Rust());
	Board->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cube")));
	Board->SetRelativeLocation(FVector(4.f, 0.f, 0.f));
	Board->SetRelativeScale3D(FVector(0.02f, 3.2f, 1.9f));
	UHTMVisualLibrary::ApplyPlaceholderMaterial(Board, FLinearColor(0.03f, 0.08f, 0.05f));

	Title->SetRelativeLocation(FVector(6.f, 150.f, 90.f));
	Title->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	Title->SetText(LOCTEXT("Title", "ENCARGOS"));
	Title->SetTextRenderColor(FColor(255, 210, 63));
	Jobs->SetRelativeLocation(FVector(6.f, 150.f, 60.f));
	LastResult->SetRelativeLocation(FVector(6.f, -20.f, 60.f));
	// El texto de TextRender mira a +X: la pizarra mira a +X local.
	for (UTextRenderComponent* T : { Title.Get(), Jobs.Get(), LastResult.Get() })
	{
		T->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	}
	Refresh();
}

void AJobBoard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshTimer -= DeltaSeconds;
	if (RefreshTimer <= 0.f)
	{
		RefreshTimer = 1.f;
		Refresh();
	}
}

void AJobBoard::Refresh()
{
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	if (!GS)
	{
		return;
	}
	const float Now = GS->GetServerWorldTimeSeconds();

	FString JobsText;
	int32 Shown = 0;
	for (const FActiveJob& Job : GS->ActiveJobs)
	{
		if (Job.Outcome != EJobOutcome::Pending || Shown >= 4)
		{
			continue;
		}
		++Shown;
		JobsText += FString::Printf(TEXT("%s  (%d EUR)\n  \"%s\"\n"), *Job.CustomerName.ToString(), Job.Budget, *Job.RequestText.ToString());
		if (Job.Car)
		{
			JobsText += FString::Printf(TEXT("  Coche: %s  %s\n"), *Job.Car->GetCarName().ToString(), *Job.Car->GetPlate());
		}
		if (Job.DeadlineServerTime > 0.f)
		{
			const int32 Left = FMath::Max(0, FMath::RoundToInt(Job.DeadlineServerTime - Now));
			JobsText += FString::Printf(TEXT("  Plazo %d:%02d\n"), Left / 60, Left % 60);
		}
		for (const FText& Label : Job.RequirementLabels)
		{
			JobsText += FString::Printf(TEXT("   [ ] %s\n"), *Label.ToString());
		}
		JobsText += TEXT("\n");
	}
	if (Shown == 0)
	{
		JobsText = GS->DayPhase == EDayPhase::Open ? TEXT("Sin encargos... de momento.") : TEXT("Taller cerrado.\nAbrid con la caja registradora.");
	}
	Jobs->SetText(FText::FromString(JobsText));

	const FJobEvaluation& Eval = GS->LastEvaluation;
	if (Eval.JobUid != 0)
	{
		FString Result = FString::Printf(TEXT("ÚLTIMA ENTREGA: %s\n"), *Eval.CustomerName.ToString());
		for (int32 i = 0; i < Eval.Labels.Num(); ++i)
		{
			Result += FString::Printf(TEXT(" %s %s\n"), Eval.Passed.IsValidIndex(i) && Eval.Passed[i] ? TEXT("[OK]") : TEXT("[X]"), *Eval.Labels[i].ToString());
		}
		const TCHAR* Outcome = Eval.Outcome == EJobOutcome::FullPay ? TEXT("PAGO COMPLETO") : Eval.Outcome == EJobOutcome::PartialPay ? TEXT("PAGO PARCIAL") : TEXT("CLIENTE ENFADADO");
		Result += FString::Printf(TEXT("\n%s: %d EUR"), Outcome, Eval.Payment);
		LastResult->SetText(FText::FromString(Result));
	}
}

#undef LOCTEXT_NAMESPACE
