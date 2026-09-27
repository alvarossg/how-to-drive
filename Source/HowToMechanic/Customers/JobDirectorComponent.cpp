#include "Customers/JobDirectorComponent.h"
#include "Customers/JobEvaluator.h"
#include "Customers/CustomerNPC.h"
#include "Vehicle/ModularCar.h"
#include "Vehicle/CarFactory.h"
#include "Parts/CarPart.h"
#include "Character/MechanicCharacter.h"
#include "Progression/WorkshopBlockout.h"
#include "Core/HTMGameState.h"
#include "Core/HTMPlayerState.h"
#include "Core/HTMDataSubsystem.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMTelemetrySubsystem.h"
#include "Core/HTMPalette.h"
#include "Core/HTMLog.h"
#include "Engine/World.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "HTMJobDirector"

UJobDirectorComponent::UJobDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.5f;
	Rng.GenerateNewSeed();
}

FName UJobDirectorComponent::PickJob(FRandomStream& InRng, float Reputation, bool bWantAbsurd) const
{
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	if (!Data)
	{
		return NAME_None;
	}
	TArray<FName> Candidates;
	float Total = 0.f;
	for (const FName& Id : Data->GetJobIds())
	{
		const FJobDefinitionRow* Row = Data->FindJob(Id);
		if (Row && Row->MinReputation <= Reputation && (!bWantAbsurd || Row->bAbsurd))
		{
			Candidates.Add(Id);
			Total += Row->Weight;
		}
	}
	float Pick = InRng.FRandRange(0.f, Total);
	for (const FName& Id : Candidates)
	{
		Pick -= Data->FindJob(Id)->Weight;
		if (Pick <= 0.f)
		{
			return Id;
		}
	}
	return Candidates.Num() > 0 ? Candidates.Last() : NAME_None;
}

void UJobDirectorComponent::StartDay(int32 DayNumber)
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	const AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	const int32 Players = GS ? FMath::Max(1, GS->GetNumPlayers()) : 1;
	const float Reputation = GS ? GS->Reputation : 0.f;
	const int32 NumJobs = T.JobsPerDayBase + T.JobsPerExtraPlayer * Players;

	ArrivalQueue.Reset();
	DayFull = DayPartial = DayAngry = 0;
	for (int32 i = 0; i < NumJobs; ++i)
	{
		// Al menos un cliente absurdo al día a partir del tercero.
		const bool bWantAbsurd = (i == 2);
		FName Job = PickJob(Rng, Reputation, bWantAbsurd);
		if (Job.IsNone())
		{
			Job = PickJob(Rng, Reputation, false);
		}
		if (!Job.IsNone())
		{
			ArrivalQueue.Add(Job);
		}
	}
	// El primer día empieza siempre por el trabajo más fácil (tutorial implícito).
	if (DayNumber == 1)
	{
		if (const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this))
		{
			if (Data->FindJob(TEXT("JOB_FLAT")))
			{
				ArrivalQueue.Insert(TEXT("JOB_FLAT"), 0);
			}
		}
	}
	NextArrivalTime = GetWorld()->GetTimeSeconds() + T.FirstCustomerDelay;
	UE_LOG(LogHTM, Log, TEXT("Día %d: %d clientes en cola (%d jugadores)"), DayNumber, ArrivalQueue.Num(), Players);
}

void UJobDirectorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	if (!GS || GS->DayPhase != EDayPhase::Open)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (ArrivalQueue.Num() > 0 && Now >= NextArrivalTime)
	{
		const FName Job = ArrivalQueue[0];
		ArrivalQueue.RemoveAt(0);
		SpawnCustomer(Job);
		NextArrivalTime = Now + UHTMTuningData::Get().CustomerInterval;
	}

	// Plazos vencidos: el cliente se va enfadado y se lleva el coche.
	const float ServerNow = GS->GetServerWorldTimeSeconds();
	for (const FActiveJob& Job : TArray<FActiveJob>(GS->ActiveJobs))
	{
		if (Job.Outcome == EJobOutcome::Pending && Job.DeadlineServerTime > 0.f && ServerNow > Job.DeadlineServerTime + 30.f)
		{
			FJobEvaluation Eval;
			Eval.JobUid = Job.JobUid;
			Eval.CustomerName = Job.CustomerName;
			Eval.Labels.Add(LOCTEXT("TooLate", "Dentro de plazo"));
			Eval.Passed.Add(false);
			Eval.Outcome = EJobOutcome::Angry;
			Eval.ReputationDelta = UHTMTuningData::Get().RepAngry;
			Eval.ServerTime = ServerNow;
			ResolveJob(Job.JobUid, Eval, nullptr);
		}
	}
}

void UJobDirectorComponent::SpawnCustomer(FName JobId)
{
	AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	const FJobDefinitionRow* Def = Data ? Data->FindJob(JobId) : nullptr;
	AWorkshopBlockout* Blockout = AWorkshopBlockout::Get(this);
	if (!GS || !Def || !Blockout)
	{
		return;
	}

	// Primera plaza de clientes libre.
	FTransform Spot;
	bool bFound = false;
	for (const FTransform& Candidate : Blockout->GetCustomerSpots())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMSpot), false);
		if (!GetWorld()->OverlapBlockingTestByChannel(Candidate.GetLocation() + FVector(0.f, 0.f, 80.f), FQuat::Identity, ECC_Vehicle, FCollisionShape::MakeBox(FVector(220.f, 110.f, 50.f)), Params))
		{
			Spot = Candidate;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		// Sin sitio: vuelve a la cola y lo intenta más tarde.
		ArrivalQueue.Insert(JobId, 0);
		GS->MulticastToast(LOCTEXT("NoRoom", "Un cliente no encuentra sitio para aparcar... ¡haced hueco!"), HTMPalette::SafetyOrange());
		return;
	}

	const int32 Uid = NextUid++;
	FCarSpawnOptions Options;
	Options.ForcedFault = Def->ForcedFault;
	Options.ForcedSlot = Def->ForcedSlot;
	Options.ForcedPartId = Def->ForcedPartId;
	Options.JobUid = Uid;
	Options.Seed = Rng.RandRange(1, INT32_MAX - 1);
	const FName Model = UCarFactory::PickModel(Rng, Def->CarModels, this);
	AModularCar* Car = UCarFactory::SpawnCar(this, Model, Spot, Options);
	if (!Car)
	{
		return;
	}

	const UHTMTuningData& T = UHTMTuningData::Get();
	const int32 Players = FMath::Max(1, GS->GetNumPlayers());
	const float TimeMult = FMath::Lerp(T.SoloTimeLimitMult, 1.f, (Players - 1) / 3.f);

	FActiveJob Job;
	Job.JobUid = Uid;
	Job.JobId = JobId;
	Job.Car = Car;
	Job.CustomerName = Def->CustomerName;
	Job.RequestText = Def->RequestText;
	Job.RequirementLabels = FJobEvaluator::BuildLabels(*Def);
	Job.Budget = Def->Budget;
	Job.StartServerTime = GS->GetServerWorldTimeSeconds();
	Job.DeadlineServerTime = Def->TimeLimitSeconds > 0.f ? Job.StartServerTime + FMath::Min(Def->TimeLimitSeconds * TimeMult, T.MaxJobSeconds) : 0.f;
	GS->ActiveJobs.Add(Job);
	GS->NotifyJobsChanged();

	// El cliente se queda junto a su coche con su bocadillo.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector NpcLoc = Spot.TransformPosition(FVector(0.f, -230.f, 60.f));
	if (ACustomerNPC* Npc = GetWorld()->SpawnActor<ACustomerNPC>(NpcLoc, Spot.Rotator(), Params))
	{
		Npc->InitCustomer(Uid, Def->CustomerName, Def->RequestText, Def->CustomerColor);
		Customers.Add(Uid, Npc);
	}

	GS->MulticastToast(FText::Format(LOCTEXT("NewCustomer", "Nuevo cliente: {0} - «{1}»"), Def->CustomerName, Def->RequestText), HTMPalette::CarSky());
}

bool UJobDirectorComponent::DeliverCar(AModularCar* Car, AMechanicCharacter* Who)
{
	AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	if (!GS || !Car || !Data)
	{
		return false;
	}
	const FActiveJob* Job = GS->FindJobForCar(Car);
	const FJobDefinitionRow* Def = Job ? Data->FindJob(Job->JobId) : nullptr;
	if (!Job || !Def)
	{
		return false;
	}
	const FJobEvaluation Eval = FJobEvaluator::Evaluate(*Def, *Job, Car, GS->GetServerWorldTimeSeconds());
	ResolveJob(Job->JobUid, Eval, Who);
	return true;
}

void UJobDirectorComponent::ResolveJob(int32 JobUid, const FJobEvaluation& Eval, AMechanicCharacter* Who)
{
	AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	FActiveJob* Job = GS ? GS->FindJobByUid(JobUid) : nullptr;
	if (!Job || Job->Outcome != EJobOutcome::Pending)
	{
		return;
	}
	Job->Outcome = Eval.Outcome;
	DayFull += Eval.Outcome == EJobOutcome::FullPay ? 1 : 0;
	DayPartial += Eval.Outcome == EJobOutcome::PartialPay ? 1 : 0;
	DayAngry += Eval.Outcome == EJobOutcome::Angry ? 1 : 0;
	const float Duration = GS->GetServerWorldTimeSeconds() - Job->StartServerTime;

	if (Eval.Payment > 0)
	{
		GS->AddMoney(Eval.Payment, FText::Format(LOCTEXT("PayReason", "Pago de {0}"), Job->CustomerName));
	}
	GS->AddReputation(Eval.ReputationDelta);
	GS->LastEvaluation = Eval;
	GS->NotifyEvaluation();
	GS->NotifyJobsChanged();

	if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
	{
		const ETelemetryEvent Event = Eval.Outcome == EJobOutcome::FullPay ? ETelemetryEvent::JobCompleted
			: Eval.Outcome == EJobOutcome::PartialPay ? ETelemetryEvent::JobPartial : ETelemetryEvent::JobFailed;
		Tel->Record(this, Event, Who ? Who->GetPlayerNameSafe() : FString(), Job->JobId.ToString(), FVector::ZeroVector, Duration);
	}
	if (Eval.Outcome == EJobOutcome::FullPay)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (AHTMPlayerState* HPS = Cast<AHTMPlayerState>(PS))
			{
				HPS->AddAchievement(EHTMAchievement::PerfectJobs);
			}
		}
	}

	if (TObjectPtr<ACustomerNPC>* Npc = Customers.Find(JobUid))
	{
		if (*Npc)
		{
			(*Npc)->React(Eval.Outcome);
		}
	}
	DismissCustomer(JobUid, 4.f);
}

void UJobDirectorComponent::DismissCustomer(int32 JobUid, float Delay)
{
	TWeakObjectPtr<UJobDirectorComponent> WeakThis(this);
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, [WeakThis, JobUid]()
	{
		UJobDirectorComponent* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		AHTMGameState* GS = Self->GetWorld()->GetGameState<AHTMGameState>();
		if (FActiveJob* Job = GS ? GS->FindJobByUid(JobUid) : nullptr)
		{
			if (Job->Car)
			{
				// Soltar a quien siga dentro y llevarse el coche (y sus piezas).
				if (Job->Car->GetDriver())
				{
					Job->Car->EjectDriver(FVector(0.f, 0.f, 200.f));
				}
				for (ACarPart* Part : Job->Car->GetMountedParts())
				{
					Part->Destroy();
				}
				Job->Car->Destroy();
			}
			GS->ActiveJobs.RemoveAll([JobUid](const FActiveJob& J) { return J.JobUid == JobUid; });
			GS->NotifyJobsChanged();
		}
		if (TObjectPtr<ACustomerNPC>* Npc = Self->Customers.Find(JobUid))
		{
			if (*Npc)
			{
				(*Npc)->Destroy();
			}
			Self->Customers.Remove(JobUid);
		}
	}, Delay, false);
}

void UJobDirectorComponent::EndDay()
{
	AHTMGameState* GS = GetWorld()->GetGameState<AHTMGameState>();
	ArrivalQueue.Reset();
	if (!GS)
	{
		return;
	}
	for (const FActiveJob& Job : TArray<FActiveJob>(GS->ActiveJobs))
	{
		if (Job.Outcome == EJobOutcome::Pending)
		{
			FJobEvaluation Eval;
			Eval.JobUid = Job.JobUid;
			Eval.CustomerName = Job.CustomerName;
			Eval.Labels.Add(LOCTEXT("Closed", "Atendido antes del cierre"));
			Eval.Passed.Add(false);
			Eval.Outcome = EJobOutcome::Angry;
			Eval.ReputationDelta = UHTMTuningData::Get().RepAngry * 0.5f;
			Eval.ServerTime = GS->GetServerWorldTimeSeconds();
			ResolveJob(Job.JobUid, Eval, nullptr);
		}
	}
}

#undef LOCTEXT_NAMESPACE
