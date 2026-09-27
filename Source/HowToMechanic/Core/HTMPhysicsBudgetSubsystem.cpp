#include "Core/HTMPhysicsBudgetSubsystem.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMLog.h"
#include "Interaction/GrabbableActor.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarPhysicsBudgetStats(
	TEXT("htm.PhysicsBudget.Stats"), 0, TEXT("Muestra en el log los cuerpos activos del presupuesto de física."));

void UHTMPhysicsBudgetSubsystem::Register(AGrabbableActor* Actor)
{
	if (Actor)
	{
		Tracked.AddUnique(Actor);
	}
}

void UHTMPhysicsBudgetSubsystem::Unregister(AGrabbableActor* Actor)
{
	Tracked.Remove(Actor);
}

TStatId UHTMPhysicsBudgetSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHTMPhysicsBudgetSubsystem, STATGROUP_Tickables);
}

bool UHTMPhysicsBudgetSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHTMPhysicsBudgetSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	Accumulator += DeltaTime;
	if (Accumulator >= UHTMTuningData::Get().BudgetInterval)
	{
		Accumulator = 0.f;
		Evaluate();
	}
}

void UHTMPhysicsBudgetSubsystem::Evaluate()
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	UWorld* World = GetWorld();

	TArray<FVector> PlayerLocations;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PC = It->Get())
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				PlayerLocations.Add(Pawn->GetActorLocation());
			}
		}
	}

	struct FCandidate { AGrabbableActor* Actor; float Score; };
	TArray<FCandidate> Awake;

	Tracked.RemoveAll([](const TWeakObjectPtr<AGrabbableActor>& W) { return !W.IsValid(); });
	for (const TWeakObjectPtr<AGrabbableActor>& Weak : Tracked)
	{
		AGrabbableActor* Actor = Weak.Get();
		if (!Actor || Actor->IsCarried())
		{
			continue;
		}
		float NearestSq = TNumericLimits<float>::Max();
		for (const FVector& P : PlayerLocations)
		{
			NearestSq = FMath::Min(NearestSq, FVector::DistSquared(P, Actor->GetActorLocation()));
		}
		const float Nearest = FMath::Sqrt(NearestSq);

		// Congelar pequeños y lejanos en reposo; descongelar al acercarse alguien.
		if (Actor->IsFrozenByBudget())
		{
			if (Nearest < T.UnfreezeDistance)
			{
				Actor->SetFrozenByBudget(false);
			}
			continue;
		}
		UPrimitiveComponent* Body = Actor->GetPhysicsBody();
		if (!Body || !Body->IsSimulatingPhysics())
		{
			continue;
		}
		const bool bAwake = Body->IsAnyRigidBodyAwake();
		if (!bAwake && Nearest > T.FreezeDistance && Actor->GetMassKg() <= T.FreezeMaxMassKg)
		{
			Actor->SetFrozenByBudget(true);
			continue;
		}
		if (bAwake)
		{
			Awake.Add({ Actor, Nearest / FMath::Max(1.f, Actor->GetMassKg()) });
		}
	}

	LastActiveCount = Awake.Num();
	if (Awake.Num() > T.MaxActiveBodies)
	{
		// Dormir primero los más lejanos y ligeros.
		Awake.Sort([](const FCandidate& A, const FCandidate& B) { return A.Score > B.Score; });
		const int32 Excess = Awake.Num() - T.MaxActiveBodies;
		for (int32 i = 0; i < Excess; ++i)
		{
			if (UPrimitiveComponent* Body = Awake[i].Actor->GetPhysicsBody())
			{
				Body->PutAllRigidBodiesToSleep();
			}
		}
	}

	if (CVarPhysicsBudgetStats.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogHTM, Log, TEXT("PhysicsBudget: %d despiertos / %d registrados (máx %d)"), LastActiveCount, Tracked.Num(), T.MaxActiveBodies);
	}
}
