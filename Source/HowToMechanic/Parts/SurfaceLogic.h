#pragma once

#include "CoreMinimal.h"
#include "Parts/PartTypes.h"
#include "Parts/SurfaceTreatable.h"

namespace HTMSurface
{
	/**
	 * Aplica un tratamiento a un estado de superficie (GDD §8). Compartido por piezas y carrocería.
	 * @return true si este tratamiento acaba de ESTROPEAR pintura fresca.
	 */
	HOWTOMECHANIC_API bool Apply(FPartSurfaceState& Surface, ESurfaceTreatment Treatment, float Amount, const FLinearColor& Color, bool bPaintable);

	/** Secado natural (agua y pintura fresca). @return true si ha cambiado algo. */
	HOWTOMECHANIC_API bool TickDrying(FPartSurfaceState& Surface, float DeltaSeconds);
}
