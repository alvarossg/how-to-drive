#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SurfaceTreatable.generated.h"

class AMechanicCharacter;

UENUM(BlueprintType)
enum class ESurfaceTreatment : uint8
{
	Paint,   // pistola: cubre con color; pintura fresca
	Water,   // manguera: moja, limpia un poco, estropea pintura fresca
	Sponge,  // esponja: limpia mucho si está mojado
	Sand,    // lijadora: quita óxido (y pintura)
	Dirt,    // barro/charco de tierra: ensucia (estropea pintura fresca)
	BoothDry // cabina de pintura: secado acelerado y protegido
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USurfaceTreatable : public UInterface
{
	GENERATED_BODY()
};

/** Algo con estado de superficie (pieza o carrocería del chasis). GDD §8. Solo servidor. */
class HOWTOMECHANIC_API ISurfaceTreatable
{
	GENERATED_BODY()

public:
	virtual void TreatSurface(ESurfaceTreatment Treatment, float Amount, const FLinearColor& Color, AMechanicCharacter* By) = 0;
	virtual bool IsPaintable() const { return true; }
};
