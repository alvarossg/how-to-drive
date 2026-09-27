#include "Interaction/Interactable.h"
#include "GameFramework/Actor.h"

FVector IInteractable::GetInteractionLocation() const
{
	if (const AActor* Actor = Cast<AActor>(_getUObject()))
	{
		FVector Origin, Extent;
		Actor->GetActorBounds(true, Origin, Extent);
		return Origin + FVector(0.f, 0.f, Extent.Z);
	}
	return FVector::ZeroVector;
}
