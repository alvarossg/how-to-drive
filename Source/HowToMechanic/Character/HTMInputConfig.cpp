#include "Character/HTMInputConfig.h"
#include "Core/HTMSettings.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, FName(Name));
		Action->ValueType = Type;
		return Action;
	}

	void MapAxis2DKey(UInputMappingContext* IMC, UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = IMC->MapKey(Action, Key);
		if (bSwizzle)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(IMC);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(IMC));
		}
	}

	void MapStick(UInputMappingContext* IMC, UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = IMC->MapKey(Action, Key);
		UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(IMC);
		DeadZone->LowerThreshold = 0.2f;
		Mapping.Modifiers.Add(DeadZone);
	}

	void MapNegated(UInputMappingContext* IMC, UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = IMC->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(IMC));
	}
}

UHTMInputConfig* UHTMInputConfig::Get()
{
	static TStrongObjectPtr<UHTMInputConfig> Instance;
	if (!Instance.IsValid())
	{
		Instance.Reset(NewObject<UHTMInputConfig>(GetTransientPackage(), TEXT("HTMInputConfig")));
		Instance->Build();
	}
	return Instance.Get();
}

void UHTMInputConfig::Build()
{
	// ------------------------------------------------------------------ Acciones
	Move      = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	Look      = MakeAction(this, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	Jump      = MakeAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	Sprint    = MakeAction(this, TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	Crouch    = MakeAction(this, TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	Prone     = MakeAction(this, TEXT("IA_Prone"), EInputActionValueType::Boolean);
	Grab      = MakeAction(this, TEXT("IA_Grab"), EInputActionValueType::Boolean);
	Throw     = MakeAction(this, TEXT("IA_Throw"), EInputActionValueType::Boolean);
	Use       = MakeAction(this, TEXT("IA_Use"), EInputActionValueType::Boolean);
	AltUse    = MakeAction(this, TEXT("IA_AltUse"), EInputActionValueType::Boolean);
	Enter     = MakeAction(this, TEXT("IA_Enter"), EInputActionValueType::Boolean);
	Shout     = MakeAction(this, TEXT("IA_Shout"), EInputActionValueType::Boolean);
	Emote     = MakeAction(this, TEXT("IA_Emote"), EInputActionValueType::Boolean);

	Throttle  = MakeAction(this, TEXT("IA_Throttle"), EInputActionValueType::Axis1D);
	Brake     = MakeAction(this, TEXT("IA_Brake"), EInputActionValueType::Axis1D);
	Steer     = MakeAction(this, TEXT("IA_Steer"), EInputActionValueType::Axis1D);
	Handbrake = MakeAction(this, TEXT("IA_Handbrake"), EInputActionValueType::Boolean);
	Horn      = MakeAction(this, TEXT("IA_Horn"), EInputActionValueType::Boolean);
	ExitCar   = MakeAction(this, TEXT("IA_ExitCar"), EInputActionValueType::Boolean);
	CarLook   = MakeAction(this, TEXT("IA_CarLook"), EInputActionValueType::Axis2D);

	const UHTMSettings* Settings = UHTMSettings::Get();

	// ------------------------------------------------------------------ A pie
	if (UInputMappingContext* Override = Settings->OnFootMappingOverride.LoadSynchronous())
	{
		OnFootContext = Override;
	}
	else
	{
		OnFootContext = NewObject<UInputMappingContext>(this, TEXT("IMC_OnFoot"));
		UInputMappingContext* IMC = OnFootContext;

		MapAxis2DKey(IMC, Move, EKeys::W, true, false);
		MapAxis2DKey(IMC, Move, EKeys::S, true, true);
		MapAxis2DKey(IMC, Move, EKeys::D, false, false);
		MapAxis2DKey(IMC, Move, EKeys::A, false, true);
		MapStick(IMC, Move, EKeys::Gamepad_Left2D);

		IMC->MapKey(Look, EKeys::Mouse2D);
		MapStick(IMC, Look, EKeys::Gamepad_Right2D);

		IMC->MapKey(Jump, EKeys::SpaceBar);
		IMC->MapKey(Jump, EKeys::Gamepad_FaceButton_Bottom);
		IMC->MapKey(Sprint, EKeys::LeftShift);
		IMC->MapKey(Sprint, EKeys::Gamepad_LeftThumbstick);
		IMC->MapKey(Crouch, EKeys::C);
		IMC->MapKey(Crouch, EKeys::Gamepad_FaceButton_Right);
		IMC->MapKey(Prone, EKeys::X);
		IMC->MapKey(Prone, EKeys::Gamepad_DPad_Down);
		IMC->MapKey(Grab, EKeys::E);
		IMC->MapKey(Grab, EKeys::Gamepad_FaceButton_Left);
		IMC->MapKey(Throw, EKeys::Q);
		IMC->MapKey(Throw, EKeys::Gamepad_RightShoulder);
		IMC->MapKey(Use, EKeys::LeftMouseButton);
		IMC->MapKey(Use, EKeys::Gamepad_RightTrigger);
		IMC->MapKey(AltUse, EKeys::RightMouseButton);
		IMC->MapKey(AltUse, EKeys::Gamepad_LeftTrigger);
		IMC->MapKey(Enter, EKeys::F);
		IMC->MapKey(Enter, EKeys::Gamepad_FaceButton_Top);
		IMC->MapKey(Shout, EKeys::T);
		IMC->MapKey(Shout, EKeys::Gamepad_LeftShoulder);
		IMC->MapKey(Emote, EKeys::G);
		IMC->MapKey(Emote, EKeys::Gamepad_DPad_Up);
	}

	// ------------------------------------------------------------------ Conduciendo
	if (UInputMappingContext* Override = Settings->DrivingMappingOverride.LoadSynchronous())
	{
		DrivingContext = Override;
	}
	else
	{
		DrivingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Driving"));
		UInputMappingContext* IMC = DrivingContext;

		IMC->MapKey(Throttle, EKeys::W);
		IMC->MapKey(Throttle, EKeys::Gamepad_RightTriggerAxis);
		IMC->MapKey(Brake, EKeys::S);
		IMC->MapKey(Brake, EKeys::Gamepad_LeftTriggerAxis);
		IMC->MapKey(Steer, EKeys::D);
		MapNegated(IMC, Steer, EKeys::A);
		MapStick(IMC, Steer, EKeys::Gamepad_LeftX);
		IMC->MapKey(Handbrake, EKeys::SpaceBar);
		IMC->MapKey(Handbrake, EKeys::Gamepad_FaceButton_Bottom);
		IMC->MapKey(Horn, EKeys::H);
		IMC->MapKey(Horn, EKeys::Gamepad_RightShoulder);
		IMC->MapKey(ExitCar, EKeys::F);
		IMC->MapKey(ExitCar, EKeys::Gamepad_FaceButton_Top);
		IMC->MapKey(CarLook, EKeys::Mouse2D);
		MapStick(IMC, CarLook, EKeys::Gamepad_Right2D);
	}
}
