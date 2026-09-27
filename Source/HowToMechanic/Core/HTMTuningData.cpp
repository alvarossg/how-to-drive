#include "Core/HTMTuningData.h"
#include "Core/HTMSettings.h"
#include "Core/HTMGameState.h"
#include "Engine/World.h"

const UHTMTuningData& UHTMTuningData::Get()
{
	static TWeakObjectPtr<const UHTMTuningData> Cached;
	// Se intenta cargar DA_Tuning una sola vez (y otra si el asset se descarga): se consulta cada frame
	// y un asset que aún no existe no debe provocar intentos de carga continuos.
	static bool bTriedLoad = false;
	if (const UHTMTuningData* Asset = Cached.Get())
	{
		return *Asset;
	}
	if (!bTriedLoad)
	{
		bTriedLoad = true;
		const UHTMSettings* Settings = UHTMSettings::Get();
		if (!Settings->Tuning.IsNull())
		{
			if (const UHTMTuningData* Loaded = Settings->Tuning.LoadSynchronous())
			{
				Cached = Loaded;
				bTriedLoad = false; // si se descarga (GC tras recargar), se vuelve a intentar
				return *Loaded;
			}
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
