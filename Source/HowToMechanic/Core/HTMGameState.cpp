#include "Core/HTMGameState.h"
#include "Core/HTMPalette.h"
#include "Vehicle/ModularCar.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HTMGameState"

AHTMGameState::AHTMGameState()
{
}

void AHTMGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHTMGameState, Money);
	DOREPLIFETIME(AHTMGameState, Reputation);
	DOREPLIFETIME(AHTMGameState, WorkshopLevel);
	DOREPLIFETIME(AHTMGameState, DayNumber);
	DOREPLIFETIME(AHTMGameState, DayPhase);
	DOREPLIFETIME(AHTMGameState, DayEndServerTime);
	DOREPLIFETIME(AHTMGameState, ActiveJobs);
	DOREPLIFETIME(AHTMGameState, LastEvaluation);
	DOREPLIFETIME(AHTMGameState, LastDaySummary);
	DOREPLIFETIME(AHTMGameState, RoomOptions);
	DOREPLIFETIME(AHTMGameState, WallColorIndex);
	DOREPLIFETIME(AHTMGameState, JunkyardOffers);
	DOREPLIFETIME(AHTMGameState, TodaysBuyer);
	DOREPLIFETIME(AHTMGameState, UnlockedCosmetics);
}

void AHTMGameState::AddMoney(int32 Delta, const FText& Reason)
{
	if (!HasAuthority() || Delta == 0)
	{
		return;
	}
	Money += Delta;
	if (Delta > 0) { DayMoneyEarned += Delta; }
	else { DayMoneySpent += -Delta; }
	OnRep_Money();

	const FText Text = FText::Format(LOCTEXT("MoneyToast", "{0}{1} EUR · {2}"),
		FText::FromString(Delta > 0 ? TEXT("+") : TEXT("")), FText::AsNumber(Delta), Reason);
	MulticastToast(Text, Delta > 0 ? HTMPalette::CarLime() : HTMPalette::ToolRed());
}

bool AHTMGameState::TrySpend(int32 Amount, const FText& Reason)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (Amount <= 0)
	{
		return true;
	}
	if (Money < Amount)
	{
		MulticastToast(FText::Format(LOCTEXT("NoMoney", "No hay dinero para: {0} ({1} EUR)"), Reason, FText::AsNumber(Amount)), HTMPalette::ToolRed());
		return false;
	}
	AddMoney(-Amount, Reason);
	return true;
}

void AHTMGameState::AddReputation(float Delta)
{
	if (!HasAuthority())
	{
		return;
	}
	Reputation = FMath::Max(0.f, Reputation + Delta);
	DayReputationDelta += Delta;
}

void AHTMGameState::MulticastToast_Implementation(const FText& Text, FLinearColor Color)
{
	OnToast.Broadcast(Text, Color);
}

float AHTMGameState::GetDayTimeRemaining() const
{
	return DayPhase == EDayPhase::Open ? FMath::Max(0.f, DayEndServerTime - GetServerWorldTimeSeconds()) : 0.f;
}

const FActiveJob* AHTMGameState::FindJobForCar(const AModularCar* Car) const
{
	if (!Car)
	{
		return nullptr;
	}
	return ActiveJobs.FindByPredicate([Car](const FActiveJob& Job) { return Job.Car == Car && Job.Outcome == EJobOutcome::Pending; });
}

FActiveJob* AHTMGameState::FindJobByUid(int32 Uid)
{
	return ActiveJobs.FindByPredicate([Uid](const FActiveJob& Job) { return Job.JobUid == Uid; });
}

void AHTMGameState::OnRep_Money() { OnMoneyChanged.Broadcast(); }
void AHTMGameState::OnRep_WorkshopLevel() { OnWorkshopLevelChanged.Broadcast(); }
void AHTMGameState::OnRep_DayPhase() { OnDayPhaseChanged.Broadcast(); }
void AHTMGameState::OnRep_Jobs() { OnJobsChanged.Broadcast(); }
void AHTMGameState::OnRep_LastEvaluation() { OnEvaluation.Broadcast(); }
void AHTMGameState::OnRep_DaySummary() { OnDaySummary.Broadcast(); }
void AHTMGameState::OnRep_WallColor() { OnWallColorChanged.Broadcast(); }

#undef LOCTEXT_NAMESPACE
