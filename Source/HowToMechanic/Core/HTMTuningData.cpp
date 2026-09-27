#include "Core/HTMTuningData.h"
#include "Core/HTMSettings.h"
#include "Core/HTMGameState.h"
#include "Engine/World.h"

const UHTMTuningData& UHTMTuningData::Get()
{
	static TWeakObjectPtr<const UHTMTuningData> Cached;
	if (const UHTMTuningData* Asset = Cached.Get())
	{
		return *Asset;
	}

	const UHTMSettings* Settings = UHTMSettings::Get();
	if (!Settings->Tuning.IsNull())
	{
		if (const UHTMTuningData* Loaded = Settings->Tuning.LoadSynchronous())
		{
			Cached = Loaded;
			return *Loaded;
		}
	}
	return *GetDefault<UHTMTuningData>();
}

bool UHTMTuningData::IsChaotic(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AHTMGameState* GS = World ? World->GetGameState<AHTMGameState>() : nullptr;
	return GS && GS->RoomOptions.bChaoticPhysics;
}
