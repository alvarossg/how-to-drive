#include "Character/MechanicCharacter.h"
#include "Character/ActiveRagdollComponent.h"
#include "Character/MechanicExpressionComponent.h"
#include "Character/HTMInputConfig.h"
#include "Interaction/InteractionComponent.h"
#include "Interaction/GrabbableActor.h"
#include "Parts/CarPart.h"
#include "Tools/Tool.h"
#include "Vehicle/ModularCar.h"
#include "Core/HTMPlayerState.h"
#include "Core/HTMTuningData.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Core/HTMTelemetrySubsystem.h"
#include "Core/HTMDataSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HTMCharacter"

namespace MechanicBody
{
	// Alturas del capsule por postura (semialtura, radio).
	constexpr float StandHalfHeight = 60.f;
	constexpr float StandRadius = 34.f;
	constexpr float CrouchHalfHeight = 42.f;
	constexpr float ProneHalfHeight = 26.f;
	constexpr float ProneRadius = 26.f;
}

AMechanicCharacter::AMechanicCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(MechanicBody::StandRadius, MechanicBody::StandHalfHeight);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Interaction, ECR_Block);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->AirControl = 0.3f;
	Move->bEnablePhysicsInteraction = true;
	Move->bPushForceScaledToMass = false;
	Move->bScalePushForceToVelocity = true;
	Move->bPushForceUsingZOffset = false;
	Move->SetCrouchedHalfHeight(MechanicBody::CrouchHalfHeight);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 430.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 70.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(80.f);

	Interaction = CreateDefaultSubobject<UInteractionComponent>(TEXT("Interaction"));
	Ragdoll = CreateDefaultSubobject<UActiveRagdollComponent>(TEXT("ActiveRagdoll"));
	Expression = CreateDefaultSubobject<UMechanicExpressionComponent>(TEXT("Expression"));

	CarryPointOneHand = CreateDefaultSubobject<USceneComponent>(TEXT("CarryPointOneHand"));
	CarryPointOneHand->SetupAttachment(RootComponent);
	CarryPointOneHand->SetRelativeLocation(FVector(38.f, 34.f, -8.f));

	CarryPointTwoHands = CreateDefaultSubobject<USceneComponent>(TEXT("CarryPointTwoHands"));
	CarryPointTwoHands->SetupAttachment(RootComponent);
	CarryPointTwoHands->SetRelativeLocation(FVector(62.f, 0.f, 0.f));

	// Malla esquelética: vacía por defecto; BP_MechanicCharacter asigna SK_ + ABP_ (ver EDITOR_STEPS).
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -MechanicBody::StandHalfHeight));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	BuildPlaceholderBody();
}

void AMechanicCharacter::BuildPlaceholderBody()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	PlaceholderRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PlaceholderRoot"));
	PlaceholderRoot->SetupAttachment(RootComponent);

	auto MakePart = [this](const TCHAR* PartName, UStaticMesh* ShapeMesh, USceneComponent* Parent, const FVector& Loc, const FVector& SizeCm, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(PartName);
		Comp->SetupAttachment(Parent);
		Comp->SetStaticMesh(ShapeMesh);
		Comp->SetRelativeLocation(Loc);
		Comp->SetRelativeRotation(Rot);
		Comp->SetRelativeScale3D(SizeCm / 100.f);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCanEverAffectNavigation(false);
		return Comp;
	};

	UStaticMesh* Sphere = SphereFinder.Object;
	UStaticMesh* Cube = CubeFinder.Object;
	UStaticMesh* Cylinder = CylinderFinder.Object;

	// 120 cm: pies en -60. Cuerpo pera, cabeza ≈ 1/3 (44 cm), manos y botas grandes.
	BodyMesh   = MakePart(TEXT("PH_Body"), Sphere, PlaceholderRoot, FVector(0.f, 0.f, -18.f), FVector(58.f, 62.f, 80.f));
	OutfitMesh = MakePart(TEXT("PH_Outfit"), Cube, PlaceholderRoot, FVector(24.f, 0.f, -24.f), FVector(12.f, 46.f, 52.f));
	HeadMesh   = MakePart(TEXT("PH_Head"), Sphere, PlaceholderRoot, FVector(2.f, 0.f, 40.f), FVector(44.f, 42.f, 46.f));
	EyeL       = MakePart(TEXT("PH_EyeL"), Sphere, HeadMesh, FVector(0.38f, -0.2f, 0.08f) * 100.f, FVector(18.f / 44.f, 18.f / 42.f, 20.f / 46.f) * 100.f);
	EyeR       = MakePart(TEXT("PH_EyeR"), Sphere, HeadMesh, FVector(0.38f, 0.2f, 0.08f) * 100.f, FVector(18.f / 44.f, 18.f / 42.f, 20.f / 46.f) * 100.f);
	PupilL     = MakePart(TEXT("PH_PupilL"), Sphere, EyeL, FVector(0.42f, 0.f, 0.f) * 100.f, FVector(35.f));
	PupilR     = MakePart(TEXT("PH_PupilR"), Sphere, EyeR, FVector(0.42f, 0.f, 0.f) * 100.f, FVector(35.f));
	BrowL      = MakePart(TEXT("PH_BrowL"), Cube, HeadMesh, FVector(0.45f, -0.2f, 0.34f) * 100.f, FVector(3.f / 44.f, 14.f / 42.f, 3.5f / 46.f) * 100.f);
	BrowR      = MakePart(TEXT("PH_BrowR"), Cube, HeadMesh, FVector(0.45f, 0.2f, 0.34f) * 100.f, FVector(3.f / 44.f, 14.f / 42.f, 3.5f / 46.f) * 100.f);
	MouthMesh  = MakePart(TEXT("PH_Mouth"), Cube, HeadMesh, FVector(0.47f, 0.f, -0.22f) * 100.f, FVector(2.f / 44.f, 9.f / 42.f, 2.5f / 46.f) * 100.f);
	HandL      = MakePart(TEXT("PH_HandL"), Sphere, PlaceholderRoot, FVector(10.f, -42.f, -20.f), FVector(22.f, 20.f, 24.f));
	HandR      = MakePart(TEXT("PH_HandR"), Sphere, PlaceholderRoot, FVector(10.f, 42.f, -20.f), FVector(22.f, 20.f, 24.f));
	BootL      = MakePart(TEXT("PH_BootL"), Cube, PlaceholderRoot, FVector(6.f, -14.f, -53.f), FVector(32.f, 19.f, 14.f));
	BootR      = MakePart(TEXT("PH_BootR"), Cube, PlaceholderRoot, FVector(6.f, 14.f, -53.f), FVector(32.f, 19.f, 14.f));
	HatMesh    = MakePart(TEXT("PH_Hat"), Cylinder, HeadMesh, FVector(0.f, 0.f, 0.42f) * 100.f, FVector(1.f, 1.f, 0.35f) * 100.f);
}

void AMechanicCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMechanicCharacter, MechanicState);
	DOREPLIFETIME(AMechanicCharacter, LastImpactDirection);
	DOREPLIFETIME(AMechanicCharacter, Stance);
	DOREPLIFETIME(AMechanicCharacter, bSprinting);
	DOREPLIFETIME(AMechanicCharacter, CurrentCar);
}

void AMechanicCharacter::BeginPlay()
{
	Super::BeginPlay();

	const UHTMTuningData& T = UHTMTuningData::Get();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->JumpZVelocity = T.JumpZVelocity;
	Move->PushForceFactor = T.PushForceFactor;
	DefaultGroundFriction = Move->GroundFriction;
	DefaultBrakingDecel = Move->BrakingDecelerationWalking;

	// Placeholder: colores de la paleta. Si hay malla esquelética, se oculta el placeholder.
	const bool bHasSkeletal = GetMesh()->GetSkeletalMeshAsset() != nullptr;
	PlaceholderRoot->SetVisibility(!bHasSkeletal, true);
	GetMesh()->SetVisibility(bHasSkeletal);
	if (bHasSkeletal && Expression)
	{
		// Arte final: la cara es el hueco de material "Face" de SK_Mechanic (texturas EyesTex/BrowsTex/MouthTex).
		const int32 FaceIndex = GetMesh()->GetMaterialIndex(TEXT("Face"));
		if (FaceIndex != INDEX_NONE)
		{
			Expression->SetFaceMaterial(GetMesh()->CreateAndSetMaterialInstanceDynamic(FaceIndex));
		}
	}

	auto Paint = [this](UStaticMeshComponent* Comp, const FLinearColor& Color, FName Key)
	{
		PartMIDs.Add(Key, UHTMVisualLibrary::ApplyPlaceholderMaterial(Comp, Color));
	};
	Paint(BodyMesh, HTMPalette::White(), TEXT("Body"));
	Paint(OutfitMesh, HTMPalette::SafetyOrange(), TEXT("Outfit"));
	Paint(HeadMesh, HTMPalette::Skin(), TEXT("Head"));
	Paint(EyeL, HTMPalette::White(), TEXT("EyeL"));
	Paint(EyeR, HTMPalette::White(), TEXT("EyeR"));
	Paint(PupilL, FLinearColor::Black, TEXT("PupilL"));
	Paint(PupilR, FLinearColor::Black, TEXT("PupilR"));
	Paint(BrowL, FLinearColor(0.02f, 0.02f, 0.02f), TEXT("BrowL"));
	Paint(BrowR, FLinearColor(0.02f, 0.02f, 0.02f), TEXT("BrowR"));
	Paint(MouthMesh, FLinearColor(0.02f, 0.02f, 0.02f), TEXT("Mouth"));
	Paint(HandL, HTMPalette::Skin(), TEXT("HandL"));
	Paint(HandR, HTMPalette::Skin(), TEXT("HandR"));
	Paint(BootL, HTMPalette::Rubber(), TEXT("BootL"));
	Paint(BootR, HTMPalette::Rubber(), TEXT("BootR"));
	Paint(HatMesh, HTMPalette::ToolBlue(), TEXT("Hat"));

	Ragdoll->Setup(GetMesh(), PlaceholderRoot);
	Expression->SetupPlaceholder(EyeL, EyeR, BrowL, BrowR, MouthMesh);

	ApplyStance(Stance);
	ApplyCosmetics();
	UpdateMovementSpeed();
}

void AMechanicCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ApplyCosmetics();
}

void AMechanicCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyCosmetics();
}

AHTMPlayerState* AMechanicCharacter::GetHTMPlayerState() const
{
	return GetPlayerState<AHTMPlayerState>();
}

FString AMechanicCharacter::GetPlayerNameSafe() const
{
	if (const APlayerState* PS = GetPlayerState())
	{
		return PS->GetPlayerName();
	}
	return GetName();
}

// ============================================================================ Input

void AMechanicCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(UHTMInputConfig::Get()->OnFootContext, 0);
		}
	}
}

void AMechanicCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	const UHTMInputConfig* Cfg = UHTMInputConfig::Get();

	Input->BindAction(Cfg->Move, ETriggerEvent::Triggered, this, &AMechanicCharacter::InputMove);
	Input->BindAction(Cfg->Look, ETriggerEvent::Triggered, this, &AMechanicCharacter::InputLook);
	Input->BindAction(Cfg->Jump, ETriggerEvent::Started, this, &AMechanicCharacter::InputJump);
	Input->BindAction(Cfg->Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	Input->BindAction(Cfg->Sprint, ETriggerEvent::Started, this, &AMechanicCharacter::InputSprintStart);
	Input->BindAction(Cfg->Sprint, ETriggerEvent::Completed, this, &AMechanicCharacter::InputSprintStop);
	Input->BindAction(Cfg->Crouch, ETriggerEvent::Started, this, &AMechanicCharacter::InputCrouch);
	Input->BindAction(Cfg->Prone, ETriggerEvent::Started, this, &AMechanicCharacter::InputProne);
	Input->BindAction(Cfg->Grab, ETriggerEvent::Started, this, &AMechanicCharacter::InputGrab);
	Input->BindAction(Cfg->Throw, ETriggerEvent::Started, this, &AMechanicCharacter::InputThrow);
	Input->BindAction(Cfg->Use, ETriggerEvent::Started, this, &AMechanicCharacter::InputUseStart);
	Input->BindAction(Cfg->Use, ETriggerEvent::Completed, this, &AMechanicCharacter::InputUseStop);
	Input->BindAction(Cfg->AltUse, ETriggerEvent::Started, this, &AMechanicCharacter::InputAltUseStart);
	Input->BindAction(Cfg->AltUse, ETriggerEvent::Completed, this, &AMechanicCharacter::InputAltUseStop);
	Input->BindAction(Cfg->Enter, ETriggerEvent::Started, this, &AMechanicCharacter::InputEnter);
	Input->BindAction(Cfg->Shout, ETriggerEvent::Started, this, &AMechanicCharacter::InputShout);
	Input->BindAction(Cfg->Emote, ETriggerEvent::Started, this, &AMechanicCharacter::InputEmote);
}

void AMechanicCharacter::InputMove(const FInputActionValue& Value)
{
	if (!CanAct() || !Controller)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Axis.X);
}

void AMechanicCharacter::InputLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void AMechanicCharacter::InputJump()
{
	if (CanAct() && Stance == EMechanicStance::Standing && Interaction->GetCarryMode() != ECarryMode::Heavy)
	{
		Jump();
	}
}

void AMechanicCharacter::InputSprintStart()
{
	bSprinting = true;
	UpdateMovementSpeed();
	ServerSetSprinting(true);
}

void AMechanicCharacter::InputSprintStop()
{
	bSprinting = false;
	UpdateMovementSpeed();
	ServerSetSprinting(false);
}

void AMechanicCharacter::InputCrouch()
{
	if (!CanAct()) { return; }
	const EMechanicStance Target = Stance == EMechanicStance::Crouching ? EMechanicStance::Standing : EMechanicStance::Crouching;
	if (CanChangeStanceTo(Target))
	{
		ApplyStance(Target);
		ServerSetStance(Target);
	}
}

void AMechanicCharacter::InputProne()
{
	if (!CanAct() || Interaction->GetCarryMode() == ECarryMode::Heavy || Interaction->GetCarryMode() == ECarryMode::TwoHands)
	{
		return;
	}
	const EMechanicStance Target = Stance == EMechanicStance::Prone ? EMechanicStance::Standing : EMechanicStance::Prone;
	if (CanChangeStanceTo(Target))
	{
		ApplyStance(Target);
		ServerSetStance(Target);
	}
}

void AMechanicCharacter::InputGrab()        { Interaction->InputGrab(); }
void AMechanicCharacter::InputThrow()       { Interaction->InputThrow(); }
void AMechanicCharacter::InputUseStart()    { Interaction->InputUse(true); }
void AMechanicCharacter::InputUseStop()     { Interaction->InputUse(false); }
void AMechanicCharacter::InputAltUseStart() { Interaction->InputAltUse(true); }
void AMechanicCharacter::InputAltUseStop()  { Interaction->InputAltUse(false); }
void AMechanicCharacter::InputEnter()       { Interaction->InputEnter(); }

void AMechanicCharacter::InputShout()
{
	ServerShout();
}

void AMechanicCharacter::InputEmote()
{
	static int32 NextEmote = 0;
	ServerEmote(NextEmote == 0 ? 0 : 2);
	NextEmote = 1 - NextEmote;
}

void AMechanicCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	bSprinting = bNewSprinting;
	UpdateMovementSpeed();
}

void AMechanicCharacter::ServerSetStance_Implementation(EMechanicStance NewStance)
{
	if (CanAct() && CanChangeStanceTo(NewStance))
	{
		ApplyStance(NewStance);
	}
	else
	{
		// Corrige al cliente que se había adelantado.
		OnRep_Stance();
	}
}

void AMechanicCharacter::ServerShout_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastShoutServerTime < UHTMTuningData::Get().ShoutCooldown)
	{
		return;
	}
	LastShoutServerTime = Now;
	MulticastShout(MechanicState == EMechanicState::Trapped ? LOCTEXT("ShoutHelp", "¡AYUDA!") : LOCTEXT("ShoutHey", "¡EH!"));
}

void AMechanicCharacter::ServerEmote_Implementation(int32 EmoteIndex)
{
	if (CanAct())
	{
		PlayEmote(EmoteIndex);
	}
}

void AMechanicCharacter::PlayEmote(int32 EmoteIndex)
{
	if (HasAuthority())
	{
		MulticastEmote(EmoteIndex);
	}
}

void AMechanicCharacter::MulticastShout_Implementation(const FText& Text)
{
	LastShoutTime = GetWorld()->GetTimeSeconds();
	BubbleText = Text;
	Expression->PlayOverride(EFaceExpression::Surprised, 0.6f);
}

void AMechanicCharacter::MulticastEmote_Implementation(int32 EmoteIndex)
{
	LastShoutTime = GetWorld()->GetTimeSeconds();
	switch (EmoteIndex)
	{
	case 0:
		BubbleText = LOCTEXT("EmoteCelebrate", "\\o/");
		Expression->PlayOverride(EFaceExpression::Happy, 1.5f);
		UHTMVisualLibrary::SpawnFX(this, EHTMFX::Confetti, GetActorLocation() + FVector(0, 0, 60), 0.6f);
		break;
	case 1:
		BubbleText = LOCTEXT("EmoteShove", "¡Aparta!");
		Expression->PlayOverride(EFaceExpression::Angry, 0.8f);
		break;
	default:
		BubbleText = LOCTEXT("EmoteGrr", "¡Grrr!");
		Expression->PlayOverride(EFaceExpression::Angry, 1.5f);
		Ragdoll->Stagger(GetActorRightVector() * 100.f, 0.4f);
		break;
	}
}

// ============================================================================ Postura

bool AMechanicCharacter::CanChangeStanceTo(EMechanicStance NewStance) const
{
	if (NewStance == Stance)
	{
		return false;
	}
	const float NewHalf = NewStance == EMechanicStance::Standing ? MechanicBody::StandHalfHeight
		: NewStance == EMechanicStance::Crouching ? MechanicBody::CrouchHalfHeight : MechanicBody::ProneHalfHeight;
	const float CurrentHalf = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	if (NewHalf <= CurrentHalf)
	{
		return true;
	}
	// Levantarse: ¿hay sitio? (bajo un coche no).
	const float NewRadius = NewStance == EMechanicStance::Prone ? MechanicBody::ProneRadius : MechanicBody::StandRadius;
	const FVector Center = GetActorLocation() + FVector(0.f, 0.f, NewHalf - CurrentHalf + 2.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMStance), false, this);
	return !GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(NewRadius - 2.f, NewHalf - 2.f), Params);
}

void AMechanicCharacter::ApplyStance(EMechanicStance NewStance)
{
	const float OldHalf = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	float Half = MechanicBody::StandHalfHeight;
	float Radius = MechanicBody::StandRadius;
	FTransform Rest = FTransform::Identity;

	switch (NewStance)
	{
	case EMechanicStance::Crouching:
		Half = MechanicBody::CrouchHalfHeight;
		Rest.SetScale3D(FVector(1.f, 1.f, 0.75f));
		Rest.SetTranslation(FVector(0.f, 0.f, -Half + 60.f * 0.75f));
		break;
	case EMechanicStance::Prone:
		Half = MechanicBody::ProneHalfHeight;
		Radius = MechanicBody::ProneRadius;
		// Tumbado boca arriba: el grosor del cuerpo (X) pasa a ser la altura.
		Rest.SetTranslation(FVector(0.f, 0.f, -Half + 29.f));
		break;
	default:
		break;
	}

	Stance = NewStance;
	GetCapsuleComponent()->SetCapsuleSize(Radius, Half, true);
	if (!FMath::IsNearlyEqual(OldHalf, Half))
	{
		AddActorWorldOffset(FVector(0.f, 0.f, Half - OldHalf), false, nullptr, ETeleportType::TeleportPhysics);
	}
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -Half));
	Ragdoll->SetPlaceholderRest(Rest);
	Ragdoll->SetLyingDown(NewStance == EMechanicStance::Prone);
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 70.f + (MechanicBody::StandHalfHeight - Half) * -0.5f);
	UpdateMovementSpeed();
}

void AMechanicCharacter::OnRep_Stance()
{
	ApplyStance(Stance);
}

void AMechanicCharacter::OnRep_Sprinting()
{
	UpdateMovementSpeed();
}

// ============================================================================ Movimiento y equilibrio

float AMechanicCharacter::GetBalance() const
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	float Balance = 1.f;
	switch (Interaction ? Interaction->GetCarryMode() : ECarryMode::None)
	{
	case ECarryMode::TwoHands: Balance -= T.MediumBalancePenalty; break;
	case ECarryMode::Heavy:    Balance -= T.HeavyBalancePenalty; break;
	default: break;
	}
	if (IsWet())
	{
		Balance -= T.WetBalancePenalty;
	}
	return FMath::Clamp(Balance, 0.f, 1.f);
}

void AMechanicCharacter::UpdateMovementSpeed()
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	float Speed = bSprinting ? T.SprintSpeed : T.WalkSpeed;
	if (Stance == EMechanicStance::Crouching) { Speed = T.CrouchSpeed; }
	if (Stance == EMechanicStance::Prone) { Speed = T.ProneSpeed; }

	if (Interaction)
	{
		switch (Interaction->GetCarryMode())
		{
		case ECarryMode::TwoHands:
			Speed *= T.MediumCarrySpeedMult;
			break;
		case ECarryMode::Heavy:
		{
			const AGrabbableActor* Held = Interaction->GetHeldObject();
			const int32 Carriers = Held ? Held->GetNumCarriers() : 1;
			const float Custom = Held ? Held->GetCarrySpeedMultiplier(Carriers) : -1.f;
			Speed *= Custom >= 0.f ? Custom : (Carriers >= 2 ? T.HeavyTeamCarrySpeedMult : T.HeavySoloDragSpeedMult);
			break;
		}
		default:
			break;
		}
	}
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = Speed;
}

void AMechanicCharacter::OnCarryChanged()
{
	UpdateMovementSpeed();

	// Peor visión con objetos medios/pesados: la cámara se acerca y baja un poco.
	const ECarryMode Mode = Interaction->GetCarryMode();
	CameraBoom->TargetArmLength = (Mode == ECarryMode::TwoHands || Mode == ECarryMode::Heavy) ? 340.f : 430.f;

	// Color de herramientas del jugador (cosmético).
	if (ATool* Tool = Interaction->GetHeldTool())
	{
		if (const AHTMPlayerState* PS = GetHTMPlayerState())
		{
			if (const FCosmeticRow* Row = UHTMDataSubsystem::Get(this) ? UHTMDataSubsystem::Get(this)->FindCosmetic(PS->Loadout.ToolColor) : nullptr)
			{
				Tool->SetTint(Row->Color);
			}
		}
	}
}

void AMechanicCharacter::AddWetOverlap(int32 Delta)
{
	WetOverlaps = FMath::Max(0, WetOverlaps + Delta);
	const UHTMTuningData& T = UHTMTuningData::Get();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->GroundFriction = IsWet() ? T.WetGroundFriction : DefaultGroundFriction;
	Move->BrakingDecelerationWalking = IsWet() ? T.WetBrakingDecel : DefaultBrakingDecel;
}

// ============================================================================ Tick

void AMechanicCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateMovementSpeed();
	Ragdoll->SetInstability(1.f - GetBalance() + (MechanicState == EMechanicState::Stumbling ? 0.5f : 0.f));

	if (HasAuthority())
	{
		ServerCheckStateTimeout();
		ServerCheckTrip(DeltaSeconds);
		LastTrappedCheck += DeltaSeconds;
		if (LastTrappedCheck > 0.15f)
		{
			LastTrappedCheck = 0.f;
			ServerCheckTrapped();
			ServerCheckPushingCar();
		}
	}
}

void AMechanicCharacter::ServerCheckStateTimeout()
{
	if (StateEndTime > 0.f && GetWorld()->GetTimeSeconds() >= StateEndTime)
	{
		if (MechanicState == EMechanicState::KnockedOut || MechanicState == EMechanicState::Stumbling)
		{
			SetMechanicState(EMechanicState::Normal);
		}
	}
}

void AMechanicCharacter::ServerCheckTrip(float DeltaSeconds)
{
	if (!CanAct() || GetCharacterMovement()->IsFalling())
	{
		return;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	const FVector Vel = GetVelocity();
	const float Speed = Vel.Size2D();
	const float Now = GetWorld()->GetTimeSeconds();
	if (Speed < T.TripMinSpeed || Now - LastTripTime < T.TripCooldown)
	{
		return;
	}

	const FVector Dir = Vel.GetSafeNormal2D();
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, Half - 12.f);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMTrip), false, this);
	if (Interaction->GetHeldObject())
	{
		Params.AddIgnoredActor(Interaction->GetHeldObject());
	}

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Feet, Feet + Dir * 45.f, FQuat(FRotator(0.f, Dir.Rotation().Yaw, 0.f)), Objects,
		FCollisionShape::MakeBox(FVector(10.f, 22.f, 10.f)), Params);

	for (const FHitResult& Hit : Hits)
	{
		AGrabbableActor* Obstacle = Cast<AGrabbableActor>(Hit.GetActor());
		if (!Obstacle || !Obstacle->IsTripHazard())
		{
			continue;
		}
		const float Height = Obstacle->GetComponentsBoundingBox().GetSize().Z;
		if (Height > T.TripObjectMaxHeight)
		{
			continue;
		}
		LastTripTime = Now; // una tirada por obstáculo y enfriamiento
		const float Chance = T.GetTripChance(this) * (2.f - GetBalance()) * FMath::Clamp(Speed / T.SprintSpeed, 0.5f, 1.5f);

		// Patada al objeto siempre: rueda, se desplaza, es un obstáculo vivo.
		if (UPrimitiveComponent* Body = Obstacle->GetPhysicsBody())
		{
			if (Body->IsSimulatingPhysics())
			{
				Body->AddImpulse(Dir * Speed * 0.6f + FVector(0.f, 0.f, 120.f), NAME_None, true);
			}
		}

		if (FMath::FRand() < Chance)
		{
			const ECarryMode Mode = Interaction->GetCarryMode();
			if (Mode == ECarryMode::Heavy || Mode == ECarryMode::TwoHands)
			{
				if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
				{
					Tel->RecordSituation(this, EHTMSituation::TripCarryingHeavy, GetPlayerNameSafe(), GetActorLocation());
				}
				KnockOut(Dir * 350.f, TEXT("Tropezón cargando"));
			}
			else
			{
				Stumble(Dir * 380.f);
			}
		}
		break;
	}
}

void AMechanicCharacter::ServerCheckTrapped()
{
	if (Stance != EMechanicStance::Prone || (MechanicState != EMechanicState::Normal && MechanicState != EMechanicState::Trapped))
	{
		return;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float FeetZ = GetActorLocation().Z - Half;
	const FVector Start = GetActorLocation() - FVector(0.f, 0.f, Half * 0.5f);
	const FVector End = GetActorLocation() + FVector(0.f, 0.f, T.TrappedClearance + 60.f);

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMTrapped), false, this);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, Objects,
		FCollisionShape::MakeBox(FVector(18.f, 18.f, 2.f)), Params);

	const float Clearance = bHit ? (Hit.bStartPenetrating ? 0.f : Hit.ImpactPoint.Z - FeetZ) : 1000.f;
	if (MechanicState == EMechanicState::Normal && bHit && Cast<AModularCar>(Hit.GetActor()) && Clearance < T.TrappedClearance)
	{
		SetTrapped(true);
	}
	else if (MechanicState == EMechanicState::Trapped && Clearance > T.TrappedClearance + 12.f)
	{
		SetTrapped(false);
	}
}

void AMechanicCharacter::ServerCheckPushingCar()
{
	if (!CanAct() || GetVelocity().Size2D() < 30.f || GetCharacterMovement()->GetCurrentAcceleration().IsNearlyZero())
	{
		return;
	}
	const FVector Dir = GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMPush), false, this);
	FHitResult Hit;
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	if (GetWorld()->SweepSingleByObjectType(Hit, GetActorLocation(), GetActorLocation() + Dir * 25.f, FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(Radius), Params))
	{
		if (AModularCar* Car = Cast<AModularCar>(Hit.GetActor()))
		{
			Car->RegisterPusher(this);
		}
	}
}

// ============================================================================ Golpes, tambaleo, KO

void AMechanicCharacter::ReceiveImpact(float ImpactScore, const FVector& Direction, AActor* Source)
{
	if (!HasAuthority() || MechanicState == EMechanicState::KnockedOut || MechanicState == EMechanicState::Driving)
	{
		return;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = -GetActorForwardVector();
	}

	if (ImpactScore >= T.GetKOThreshold(this))
	{
		FString Cause = Source ? Source->GetClass()->GetName() : TEXT("Golpe");
		if (const ACarPart* Part = Cast<ACarPart>(Source))
		{
			Cause = FString::Printf(TEXT("%s (%s)"), *Part->GetDisplayName().ToString(),
				Part->GetCategory() == EPartCategory::Engine ? TEXT("Engine") : TEXT("Part"));
			if (Part->GetCategory() == EPartCategory::Engine)
			{
				if (AHTMPlayerState* PS = GetHTMPlayerState())
				{
					PS->AddAchievement(EHTMAchievement::EngineFellOnYou);
				}
			}
		}
		KnockOut(Dir * FMath::Min(ImpactScore * 6.f, 900.f), Cause);
	}
	else if (ImpactScore >= T.GetStumbleThreshold(this))
	{
		Stumble(Dir * FMath::Min(ImpactScore * 8.f, 500.f));
	}
}

void AMechanicCharacter::Stumble(const FVector& Impulse)
{
	if (!HasAuthority() || !CanAct())
	{
		return;
	}
	LastImpactDirection = Impulse.GetSafeNormal();
	// Con algo pesado o mediano en las manos, se cae.
	const ECarryMode Mode = Interaction->GetCarryMode();
	if (Mode == ECarryMode::Heavy || Mode == ECarryMode::TwoHands)
	{
		Interaction->ReleaseHeld(Impulse * 0.3f, false);
	}
	Interaction->CancelHold();
	SetMechanicState(EMechanicState::Stumbling, UHTMTuningData::Get().StumbleDuration);
	LaunchCharacter(Impulse + FVector(0.f, 0.f, 120.f), true, false);
	if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
	{
		Tel->Record(this, ETelemetryEvent::Stumble, GetPlayerNameSafe(), TEXT(""), GetActorLocation());
	}
}

void AMechanicCharacter::KnockOut(const FVector& Impulse, const FString& Cause)
{
	if (!HasAuthority() || MechanicState == EMechanicState::KnockedOut || MechanicState == EMechanicState::Driving)
	{
		return;
	}
	const UHTMTuningData& T = UHTMTuningData::Get();
	Interaction->ReleaseHeld(Impulse * 0.2f, false);
	Interaction->CancelHold();
	LastImpactDirection = Impulse.GetSafeNormal();
	SetMechanicState(EMechanicState::KnockedOut, FMath::FRandRange(T.KODurationMin, T.KODurationMax));
	LaunchCharacter(Impulse.GetClampedToMaxSize(900.f) + FVector(0.f, 0.f, 200.f), true, true);

	if (AHTMPlayerState* PS = GetHTMPlayerState())
	{
		PS->AddAchievement(EHTMAchievement::TimesKO);
	}
	if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
	{
		Tel->Record(this, ETelemetryEvent::KO, GetPlayerNameSafe(), Cause, GetActorLocation());
	}
}

void AMechanicCharacter::SetTrapped(bool bTrapped)
{
	if (!HasAuthority())
	{
		return;
	}
	if (bTrapped && MechanicState == EMechanicState::Normal)
	{
		Interaction->ReleaseHeld(FVector::ZeroVector, false);
		Interaction->CancelHold();
		SetMechanicState(EMechanicState::Trapped);
		MulticastShout(LOCTEXT("Trapped", "¡AYUDA!"));
		if (AHTMPlayerState* PS = GetHTMPlayerState())
		{
			PS->AddAchievement(EHTMAchievement::TimesTrapped);
		}
		if (UHTMTelemetrySubsystem* Tel = UHTMTelemetrySubsystem::Get(this))
		{
			Tel->Record(this, ETelemetryEvent::Trapped, GetPlayerNameSafe(), TEXT(""), GetActorLocation());
			Tel->RecordSituation(this, EHTMSituation::TrappedUnderCar, GetPlayerNameSafe(), GetActorLocation());
		}
	}
	else if (!bTrapped && MechanicState == EMechanicState::Trapped)
	{
		SetMechanicState(EMechanicState::Normal);
	}
}

void AMechanicCharacter::FreeFromTrap()
{
	if (!HasAuthority() || MechanicState != EMechanicState::Trapped)
	{
		return;
	}
	// Buscar el coche encima y sacar al jugador por el lado más cercano de su caja.
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HTMFree), false, this);
	FHitResult Hit;
	const FVector Loc = GetActorLocation();
	FVector Exit = Loc + GetActorRightVector() * 200.f;
	if (GetWorld()->SweepSingleByObjectType(Hit, Loc, Loc + FVector(0.f, 0.f, 200.f), FQuat::Identity, Objects, FCollisionShape::MakeSphere(20.f), Params))
	{
		if (const AActor* Car = Hit.GetActor())
		{
			const FTransform CarT = Car->GetActorTransform();
			const FBox LocalBox = Car->CalculateComponentsBoundingBoxInLocalSpace(false);
			const FVector Local = CarT.InverseTransformPosition(Loc);
			// Distancias a cada borde; salir por el más cercano.
			const float Dists[4] = { LocalBox.Max.X - Local.X, Local.X - LocalBox.Min.X, LocalBox.Max.Y - Local.Y, Local.Y - LocalBox.Min.Y };
			int32 Best = 0;
			for (int32 i = 1; i < 4; ++i) { if (Dists[i] < Dists[Best]) { Best = i; } }
			FVector LocalExit = Local;
			switch (Best)
			{
			case 0: LocalExit.X = LocalBox.Max.X + 70.f; break;
			case 1: LocalExit.X = LocalBox.Min.X - 70.f; break;
			case 2: LocalExit.Y = LocalBox.Max.Y + 70.f; break;
			default: LocalExit.Y = LocalBox.Min.Y - 70.f; break;
			}
			Exit = CarT.TransformPosition(LocalExit);
			Exit.Z = Loc.Z + 10.f;
		}
	}
	SetMechanicState(EMechanicState::Normal);
	TeleportTo(Exit, GetActorRotation(), false, true);
	if (CanChangeStanceTo(EMechanicStance::Standing))
	{
		ApplyStance(EMechanicStance::Standing);
	}
	PlayEmote(0);
}

void AMechanicCharacter::SetMechanicState(EMechanicState NewState, float Duration)
{
	MechanicState = NewState;
	StateEndTime = Duration > 0.f ? GetWorld()->GetTimeSeconds() + Duration : 0.f;
	OnRep_MechanicState();
	UpdateMovementSpeed();
}

void AMechanicCharacter::OnRep_MechanicState()
{
	const EMechanicState Prev = PrevLocalState;
	PrevLocalState = MechanicState;

	if (Prev == EMechanicState::KnockedOut && MechanicState != EMechanicState::KnockedOut)
	{
		Ragdoll->StopRagdoll();
		UHTMVisualLibrary::StopFX(KOStarsFX.Get());
		KOStarsFX = nullptr;
	}

	switch (MechanicState)
	{
	case EMechanicState::KnockedOut:
		Ragdoll->StartRagdoll(FVector(LastImpactDirection) * 900.f);
		KOStarsFX = UHTMVisualLibrary::SpawnFX(this, EHTMFX::KOStars, FVector(0.f, 0.f, 70.f), 1.f, RootComponent, -1.f);
		break;
	case EMechanicState::Stumbling:
		Ragdoll->Stagger(FVector(LastImpactDirection) * 500.f, UHTMTuningData::Get().StumbleDuration);
		break;
	case EMechanicState::Trapped:
		Expression->PlayOverride(EFaceExpression::Scared, 1.f);
		break;
	default:
		break;
	}

	const bool bDriving = MechanicState == EMechanicState::Driving;
	Ragdoll->SetSeated(bDriving);
	SetActorEnableCollision(!bDriving);
	if (bDriving)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_None);
	}
	else if (Prev == EMechanicState::Driving)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

// ============================================================================ Coche

void AMechanicCharacter::EnterCar(AModularCar* Car, USceneComponent* Seat)
{
	if (!HasAuthority() || !Car || !Seat)
	{
		return;
	}
	if (Interaction->GetCarryMode() != ECarryMode::OneHand)
	{
		Interaction->ReleaseHeld(FVector::ZeroVector, false);
	}
	Interaction->CancelHold();
	if (Stance != EMechanicStance::Standing)
	{
		ApplyStance(EMechanicStance::Standing);
	}
	CurrentCar = Car;
	SetMechanicState(EMechanicState::Driving);
	AttachToComponent(Seat, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
}

void AMechanicCharacter::ExitCar(const FVector& ExitLocation)
{
	if (!HasAuthority() || MechanicState != EMechanicState::Driving)
	{
		return;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	CurrentCar = nullptr;
	SetMechanicState(EMechanicState::Normal);
	SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	TeleportTo(ExitLocation, GetActorRotation(), false, true);
}

void AMechanicCharacter::EjectFromCar(const FVector& Impulse)
{
	if (!HasAuthority() || MechanicState != EMechanicState::Driving)
	{
		return;
	}
	const FVector Up = GetActorLocation() + FVector(0.f, 0.f, 160.f);
	ExitCar(Up);
	KnockOut(Impulse, TEXT("Salió volando del coche"));
}

// ============================================================================ Cosméticos

void AMechanicCharacter::ApplyCosmetics()
{
	const AHTMPlayerState* PS = GetHTMPlayerState();
	const UHTMDataSubsystem* Data = UHTMDataSubsystem::Get(this);
	if (!PS || !Data || PartMIDs.Num() == 0)
	{
		return;
	}
	auto SetColor = [this](FName Key, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* MID = PartMIDs.FindRef(Key))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
		}
	};

	// Sombrero
	if (const FCosmeticRow* Hat = Data->FindCosmetic(PS->Loadout.Hat))
	{
		const FName Shape = Hat->ShapeId;
		HatMesh->SetVisibility(Shape != TEXT("None"));
		if (Shape == TEXT("Helmet"))
		{
			HatMesh->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Sphere")));
			HatMesh->SetRelativeLocation(FVector(0.f, 0.f, 22.f));
			HatMesh->SetRelativeScale3D(FVector(1.12f, 1.12f, 0.8f));
		}
		else if (Shape == TEXT("Beanie"))
		{
			HatMesh->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Sphere")));
			HatMesh->SetRelativeLocation(FVector(0.f, 0.f, 28.f));
			HatMesh->SetRelativeScale3D(FVector(1.02f, 1.02f, 0.7f));
		}
		else if (Shape == TEXT("Goggles"))
		{
			HatMesh->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cylinder")));
			HatMesh->SetRelativeLocation(FVector(20.f, 0.f, 8.f));
			HatMesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
			HatMesh->SetRelativeScale3D(FVector(0.5f, 0.95f, 0.2f));
		}
		else // Cap
		{
			HatMesh->SetStaticMesh(UHTMVisualLibrary::GetShapeMesh(TEXT("Cylinder")));
			HatMesh->SetRelativeLocation(FVector(6.f, 0.f, 42.f));
			HatMesh->SetRelativeRotation(FRotator::ZeroRotator);
			HatMesh->SetRelativeScale3D(FVector(1.15f, 1.f, 0.3f));
		}
		SetColor(TEXT("Hat"), Hat->Color);
	}

	// Ropa
	if (const FCosmeticRow* Outfit = Data->FindCosmetic(PS->Loadout.Outfit))
	{
		const FName Shape = Outfit->ShapeId;
		if (Shape == TEXT("Hoodie"))
		{
			OutfitMesh->SetVisibility(false);
			SetColor(TEXT("Body"), Outfit->Color);
		}
		else
		{
			OutfitMesh->SetVisibility(true);
			OutfitMesh->SetRelativeScale3D(Shape == TEXT("Overalls") ? FVector(0.14f, 0.5f, 0.66f) : FVector(0.12f, 0.46f, 0.52f));
			SetColor(TEXT("Outfit"), Outfit->Color);
			SetColor(TEXT("Body"), HTMPalette::White());
		}
	}

	// Guantes
	if (const FCosmeticRow* Gloves = Data->FindCosmetic(PS->Loadout.Gloves))
	{
		const FLinearColor Color = Gloves->ShapeId == TEXT("None") ? HTMPalette::Skin() : Gloves->Color;
		SetColor(TEXT("HandL"), Color);
		SetColor(TEXT("HandR"), Color);
	}
}

// ============================================================================ IInteractable: rescatar

bool AMechanicCharacter::CanInteract(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return Verb == EInteractionVerb::Use && Who != this && MechanicState == EMechanicState::Trapped && Who && Who->Interaction->HasHandFree();
}

FText AMechanicCharacter::GetInteractionText(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return FText::Format(LOCTEXT("FreeTrapped", "Sacar a {0} de debajo del coche"), FText::FromString(GetPlayerNameSafe()));
}

float AMechanicCharacter::GetHoldDuration(const AMechanicCharacter* Who, EInteractionVerb Verb) const
{
	return UHTMTuningData::Get().FreeTrappedTime;
}

void AMechanicCharacter::Interact(AMechanicCharacter* Who, EInteractionVerb Verb)
{
	if (Verb == EInteractionVerb::Use && MechanicState == EMechanicState::Trapped)
	{
		FreeFromTrap();
	}
}

#undef LOCTEXT_NAMESPACE
