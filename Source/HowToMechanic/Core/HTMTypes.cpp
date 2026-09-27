#include "Core/HTMTypes.h"

#define LOCTEXT_NAMESPACE "HTM"

FText HTM::VerbKeyHint(EInteractionVerb Verb)
{
	switch (Verb)
	{
	case EInteractionVerb::Grab:   return LOCTEXT("VerbGrab", "[E / X]");
	case EInteractionVerb::Use:    return LOCTEXT("VerbUse", "[Clic izq. / RT]");
	case EInteractionVerb::AltUse: return LOCTEXT("VerbAltUse", "[Clic der. / LT]");
	case EInteractionVerb::Enter:  return LOCTEXT("VerbEnter", "[F / Y]");
	default:                       return FText::GetEmpty();
	}
}

FText HTM::ToolName(EToolType Tool)
{
	switch (Tool)
	{
	case EToolType::Wrench:       return LOCTEXT("ToolWrench", "Llave inglesa");
	case EToolType::TireIron:     return LOCTEXT("ToolTireIron", "Llave de ruedas");
	case EToolType::Screwdriver:  return LOCTEXT("ToolScrewdriver", "Destornillador");
	case EToolType::Stethoscope:  return LOCTEXT("ToolStethoscope", "Estetoscopio");
	case EToolType::Scanner:      return LOCTEXT("ToolScanner", "Escáner");
	case EToolType::Extinguisher: return LOCTEXT("ToolExtinguisher", "Extintor");
	case EToolType::Hose:         return LOCTEXT("ToolHose", "Manguera");
	case EToolType::Sponge:       return LOCTEXT("ToolSponge", "Esponja");
	case EToolType::Sander:       return LOCTEXT("ToolSander", "Lijadora");
	case EToolType::PaintGun:     return LOCTEXT("ToolPaintGun", "Pistola de pintura");
	default:                      return LOCTEXT("ToolNone", "Manos");
	}
}

FText HTM::SituationName(EHTMSituation Situation)
{
	switch (Situation)
	{
	case EHTMSituation::SimultaneousSnap:          return LOCTEXT("SitSnap", "Dos colocando la misma pieza a empujones");
	case EHTMSituation::ToolForgottenUnderCar:     return LOCTEXT("SitTool", "Herramienta olvidada bajo el coche");
	case EHTMSituation::EngineStartedWhileWorking: return LOCTEXT("SitStart", "Alguien arrancó con un compañero trabajando");
	case EHTMSituation::CarRanAway:                return LOCTEXT("SitRun", "El coche se escapó solo");
	case EHTMSituation::WheelRollingAway:          return LOCTEXT("SitWheel", "Rueda rodando por la calle");
	case EHTMSituation::TripCarryingHeavy:         return LOCTEXT("SitTrip", "Tropezón cargando algo pesado");
	case EHTMSituation::FreshPaintRuined:          return LOCTEXT("SitPaint", "Pintura fresca estropeada");
	case EHTMSituation::PartMountedReversed:       return LOCTEXT("SitReversed", "Pieza montada al revés");
	case EHTMSituation::ReturnedWithoutPart:       return LOCTEXT("SitLost", "Pieza perdida durante la prueba");
	case EHTMSituation::EngineSmoking:             return LOCTEXT("SitSmoke", "Motor echando humo");
	case EHTMSituation::TeammateCarBroken:         return LOCTEXT("SitCrash", "Choque entre coches del taller");
	case EHTMSituation::TrappedUnderCar:           return LOCTEXT("SitTrapped", "Atrapado bajo un coche");
	case EHTMSituation::GroupPush:                 return LOCTEXT("SitPush", "Empujando el coche en grupo");
	case EHTMSituation::DependencyCascade:         return LOCTEXT("SitCascade", "Quitar una pieza soltó otra");
	default:                                       return FText::GetEmpty();
	}
}

FText HTM::FaultName(EHiddenFault Fault)
{
	switch (Fault)
	{
	case EHiddenFault::EngineKnock:      return LOCTEXT("FaultKnock", "Golpeteo de motor");
	case EHiddenFault::WornBearing:      return LOCTEXT("FaultBearing", "Rodamiento gastado");
	case EHiddenFault::ExhaustLeak:      return LOCTEXT("FaultExhaust", "Fuga de escape");
	case EHiddenFault::BrakesWorn:       return LOCTEXT("FaultBrakes", "Frenos gastados");
	case EHiddenFault::WaterPumpFailing: return LOCTEXT("FaultPump", "Bomba de agua fallando");
	case EHiddenFault::SlowPuncture:     return LOCTEXT("FaultPuncture", "Pinchazo lento");
	default:                             return FText::GetEmpty();
	}
}

bool HTM::IsNoisyFault(EHiddenFault Fault)
{
	return Fault == EHiddenFault::EngineKnock || Fault == EHiddenFault::WornBearing || Fault == EHiddenFault::ExhaustLeak;
}

bool HTM::IsBoltTool(EToolType Tool)
{
	return Tool == EToolType::Wrench || Tool == EToolType::TireIron || Tool == EToolType::Screwdriver;
}

#undef LOCTEXT_NAMESPACE
