#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/Interactable.h"
#include "Core/HTMTypes.h"
#include "MechanicCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UInteractionComponent;
class UActiveRagdollComponent;
class UMechanicExpressionComponent;
class UMaterialInstanceDynamic;
class AModularCar;
class AHTMPlayerState;
struct FInputActionValue;

/**
 * Mecánico jugable (GDD §5). Tercera persona, active ragdoll, sin muerte.
 *
 * Estado replicado (servidor autoritativo): MechanicState (Normal/Tambaleo/KO/Atrapado/Conduciendo),
 * Stance (de pie/agachado/tumbado), bSprinting. Todo lo visual se deriva localmente de ese estado.
 *
 * Proporciones placeholder (ART_DIRECTION §6): 120 cm, cabeza ≈ 1/3, cuerpo pera, manos-manopla,
 * botas grandes, ojos blancos enormes, piel #2F2A38.
 */
UCLASS()
class HOWTOMECHANIC_API AMechanicCharacter : public ACharacter, public IInteractable
{
	GENERATED_BODY()

public:
	AMechanicCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void OnRep_PlayerState() override;
	virtual void PossessedBy(AController* NewController) override;

	// ------------------------------------------------------------------ Consultas
	UInteractionComponent* GetInteraction() const { return Interaction; }
	UActiveRagdollComponent* GetRagdoll() const { return Ragdoll; }
	UMechanicExpressionComponent* GetExpression() const { return Expression; }
	USceneComponent* GetCarryPointOneHand() const { return CarryPointOneHand; }
	USceneComponent* GetCarryPointTwoHands() const { return CarryPointTwoHands; }
	EMechanicState GetMechanicState() const { return MechanicState; }
	EMechanicStance GetStance() const { return Stance; }
	bool IsProne() const { return Stance == EMechanicStance::Prone; }
	bool IsWet() const { return WetOverlaps > 0; }
	bool IsSprinting() const { return bSprinting; }
	FVector GetLastImpactDirection() const { return LastImpactDirection; }
	AModularCar* GetCurrentCar() const { return CurrentCar; }
	/** Puede moverse e interactuar. */
	bool CanAct() const { return MechanicState == EMechanicState::Normal; }
	/** 1 = estable; baja al cargar peso o pisar mojado. */
	float GetBalance() const;
	FString GetPlayerNameSafe() const;
	AHTMPlayerState* GetHTMPlayerState() const;

	/** Último grito "¡EH!" (para el bocadillo del HUD). */
	float GetLastShoutTime() const { return LastShoutTime; }
	const FText& GetBubbleText() const { return BubbleText; }

	// ------------------------------------------------------------------ Servidor: física del cuerpo
	/** Golpe físico. Score = velocidad relativa (m/s) × masa (kg, con tope). */
	void ReceiveImpact(float ImpactScore, const FVector& Direction, AActor* Source);
	void Stumble(const FVector& Impulse);
	void KnockOut(const FVector& Impulse, const FString& Cause);
	void SetTrapped(bool bTrapped);
	/** Libera a un atrapado sacándolo por el lado con más sitio. */
	void FreeFromTrap();

	// ------------------------------------------------------------------ Servidor: coche
	void EnterCar(AModularCar* Car, USceneComponent* Seat);
	void ExitCar(const FVector& ExitLocation);
	/** Salir disparado del coche (vuelco o choque fuerte). */
	void EjectFromCar(const FVector& Impulse);

	// ------------------------------------------------------------------ Local / ambos
	/** Recalcula velocidad y equilibrio al coger/soltar algo. */
	void OnCarryChanged();
	void AddWetOverlap(int32 Delta);
	void ApplyCosmetics();
	void PlayEmote(int32 EmoteIndex);

	// ------------------------------------------------------------------ IInteractable (liberar al atrapado)
	virtual bool CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual FText GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual float GetHoldDuration(const AMechanicCharacter* Who, EInteractionVerb Verb) const override;
	virtual void Interact(AMechanicCharacter* Who, EInteractionVerb Verb) override;
	virtual FVector GetInteractionLocation() const override { return GetActorLocation(); }

protected:
	// Input
	void InputMove(const FInputActionValue& Value);
	void InputLook(const FInputActionValue& Value);
	void InputJump();
	void InputSprintStart();
	void InputSprintStop();
	void InputCrouch();
	void InputProne();
	void InputGrab();
	void InputThrow();
	void InputUseStart();
	void InputUseStop();
	void InputAltUseStart();
	void InputAltUseStop();
	void InputEnter();
	void InputShout();
	void InputEmote();

	UFUNCTION(Server, Reliable) void ServerSetSprinting(bool bNewSprinting);
	UFUNCTION(Server, Reliable) void ServerSetStance(EMechanicStance NewStance);
	UFUNCTION(Server, Unreliable) void ServerShout();
	UFUNCTION(Server, Unreliable) void ServerEmote(int32 EmoteIndex);
	UFUNCTION(NetMulticast, Unreliable) void MulticastShout(const FText& Text);
	UFUNCTION(NetMulticast, Unreliable) void MulticastEmote(int32 EmoteIndex);

	UFUNCTION() void OnRep_MechanicState();
	UFUNCTION() void OnRep_Stance();
	UFUNCTION() void OnRep_Sprinting();

	void SetMechanicState(EMechanicState NewState, float Duration = 0.f);
	void ApplyStance(EMechanicStance NewStance);
	bool CanChangeStanceTo(EMechanicStance NewStance) const;
	void UpdateMovementSpeed();
	void ServerCheckTrip(float DeltaSeconds);
	void ServerCheckTrapped();
	void ServerCheckStateTimeout();
	void ServerCheckPushingCar();
	void BuildPlaceholderBody();

	// ------------------------------------------------------------------ Componentes
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera") TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera") TObjectPtr<UCameraComponent> FollowCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mechanic") TObjectPtr<UInteractionComponent> Interaction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mechanic") TObjectPtr<UActiveRagdollComponent> Ragdoll;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mechanic") TObjectPtr<UMechanicExpressionComponent> Expression;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mechanic") TObjectPtr<USceneComponent> CarryPointOneHand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mechanic") TObjectPtr<USceneComponent> CarryPointTwoHands;

	/** Raíz del cuerpo placeholder (se oculta si el BP asigna una malla esquelética). */
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<USceneComponent> PlaceholderRoot;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> OutfitMesh;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> HeadMesh;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> EyeL;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> EyeR;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> PupilL;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> PupilR;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> BrowL;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> BrowR;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> MouthMesh;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> HandL;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> HandR;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> BootL;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> BootR;
	UPROPERTY(VisibleAnywhere, Category = "Placeholder") TObjectPtr<UStaticMeshComponent> HatMesh;

	// ------------------------------------------------------------------ Estado replicado
	UPROPERTY(ReplicatedUsing = OnRep_MechanicState) EMechanicState MechanicState = EMechanicState::Normal;
	UPROPERTY(Replicated) FVector_NetQuantize LastImpactDirection;
	UPROPERTY(ReplicatedUsing = OnRep_Stance) EMechanicStance Stance = EMechanicStance::Standing;
	UPROPERTY(ReplicatedUsing = OnRep_Sprinting) bool bSprinting = false;
	UPROPERTY(Replicated) TObjectPtr<AModularCar> CurrentCar;

	// Servidor
	float StateEndTime = 0.f;
	float LastTripTime = -100.f;
	float LastTrappedCheck = 0.f;
	float LastShoutServerTime = -100.f;

	// Local
	int32 WetOverlaps = 0;
	float LastShoutTime = -100.f;
	FText BubbleText;
	float DefaultGroundFriction = 8.f;
	float DefaultBrakingDecel = 2048.f;
	EMechanicState PrevLocalState = EMechanicState::Normal;
	TWeakObjectPtr<USceneComponent> KOStarsFX;

	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInstanceDynamic>> PartMIDs;
};
