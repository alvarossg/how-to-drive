#include "Core/HTMPlayerState.h"
#include "Character/MechanicCharacter.h"
#include "Net/UnrealNetwork.h"

AHTMPlayerState::AHTMPlayerState()
{
	Achievements.SetNumZeroed((int32)EHTMAchievement::MAX);
}

void AHTMPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHTMPlayerState, Loadout);
	DOREPLIFETIME(AHTMPlayerState, Achievements);
}

void AHTMPlayerState::AddAchievement(EHTMAchievement Achievement, int32 Amount)
{
	if (!HasAuthority())
	{
		return;
	}
	const int32 Index = (int32)Achievement;
	if (!Achievements.IsValidIndex(Index))
	{
		Achievements.SetNumZeroed((int32)EHTMAchievement::MAX);
	}
	Achievements[Index] += Amount;
}

int32 AHTMPlayerState::GetAchievementCount(EHTMAchievement Achievement) const
{
	const int32 Index = (int32)Achievement;
	return Achievements.IsValidIndex(Index) ? Achievements[Index] : 0;
}

void AHTMPlayerState::SetLoadout(const FMechanicLoadout& NewLoadout)
{
	if (HasAuthority())
	{
		Loadout = NewLoadout;
		OnRep_Loadout();
	}
}

void AHTMPlayerState::OnRep_Loadout()
{
	if (AMechanicCharacter* Char = GetPawn<AMechanicCharacter>())
	{
		Char->ApplyCosmetics();
	}
}
