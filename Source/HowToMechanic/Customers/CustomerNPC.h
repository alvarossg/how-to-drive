#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Customers/JobTypes.h"
#include "CustomerNPC.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USceneComponent;

/**
 * Cliente (GDD §10.1): personalidad visual (color), bocadillo con su petición y reacción física
 * al recibir el coche (salta de alegría, se encoge de hombros o patalea). Sin IA: está donde aparca.
 * Voces tipo murmullo sin idioma: pendiente de audio (ASSET_LIST).
 */
UCLASS()
class HOWTOMECHANIC_API ACustomerNPC : public AActor
{
	GENERATED_BODY()

public:
	ACustomerNPC();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Servidor. */
	void InitCustomer(int32 InJobUid, const FText& InName, const FText& InRequest, const FLinearColor& InColor);
	/** Servidor: reacción visible para todos. */
	void React(EJobOutcome Outcome);

protected:
	UFUNCTION() void OnRep_Customer();
	UFUNCTION() void OnRep_Mood();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> BodyRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> EyeL;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> EyeR;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Bubble;

	UPROPERTY(ReplicatedUsing = OnRep_Customer) int32 JobUid = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Customer) FText CustomerName;
	UPROPERTY(ReplicatedUsing = OnRep_Customer) FText Request;
	UPROPERTY(ReplicatedUsing = OnRep_Customer) FLinearColor Color = FLinearColor(1.f, 0.54f, 0.24f);
	UPROPERTY(ReplicatedUsing = OnRep_Mood) EJobOutcome Mood = EJobOutcome::Pending;

	float Time = 0.f;
	float ReactTime = 0.f;
};
