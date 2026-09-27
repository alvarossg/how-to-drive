#include "Customers/JobEvaluator.h"
#include "Vehicle/ModularCar.h"
#include "Core/HTMTuningData.h"

#define LOCTEXT_NAMESPACE "HTMJobs"

FText FJobEvaluator::DescribeRequirement(const FJobRequirement& R)
{
	if (!R.Label.IsEmpty())
	{
		return R.Label;
	}
	switch (R.Type)
	{
	case EJobRequirementType::FixNoisyFaults:             return LOCTEXT("ReqNoisy", "La avería que hace ruido está reparada");
	case EJobRequirementType::FixAllFaults:               return LOCTEXT("ReqFaults", "Sin averías ocultas");
	case EJobRequirementType::TopSpeedAtLeast:            return FText::Format(LOCTEXT("ReqSpeed", ">= {0} km/h en la recta"), FText::AsNumber(FMath::RoundToInt(R.Value)));
	case EJobRequirementType::HasPartId:                  return FText::Format(LOCTEXT("ReqPart", "Lleva: {0}"), FText::FromName(R.Param));
	case EJobRequirementType::HasPartWithTag:             return FText::Format(LOCTEXT("ReqTag", "Pieza {0}"), FText::FromName(R.Param));
	case EJobRequirementType::HasCategory:                return FText::Format(LOCTEXT("ReqCat", "Tiene {0}"), FText::FromName(R.Param));
	case EJobRequirementType::VividPaint:                 return LOCTEXT("ReqVivid", "Carrocería de color vivo");
	case EJobRequirementType::DistinctPaintColorsAtLeast: return FText::Format(LOCTEXT("ReqColors", "Pintado de {0} colores"), FText::AsNumber(FMath::RoundToInt(R.Value)));
	case EJobRequirementType::FuelUseAtMost:              return FText::Format(LOCTEXT("ReqFuel", "Consumo <= {0}"), FText::AsNumber(R.Value));
	case EJobRequirementType::TotalMassAtMost:            return FText::Format(LOCTEXT("ReqMass", "Peso <= {0} kg"), FText::AsNumber(FMath::RoundToInt(R.Value)));
	case EJobRequirementType::WheelRadiusAtLeast:         return FText::Format(LOCTEXT("ReqWheel", "Ruedas de radio >= {0} cm"), FText::AsNumber(FMath::RoundToInt(R.Value)));
	case EJobRequirementType::AirTimeAtLeast:             return FText::Format(LOCTEXT("ReqAir", "Salta la rampa (>= {0} s en el aire)"), FText::AsNumber(R.Value));
	case EJobRequirementType::AllRequiredSlotsFilled:     return LOCTEXT("ReqComplete", "Coche completo");
	case EJobRequirementType::NoLooseParts:               return LOCTEXT("ReqTight", "Sin piezas flojas");
	case EJobRequirementType::NoBrokenParts:              return LOCTEXT("ReqBroken", "Sin piezas rotas");
	case EJobRequirementType::MinAverageCondition:        return FText::Format(LOCTEXT("ReqCond", "Estado medio >= {0} %"), FText::AsNumber(FMath::RoundToInt(R.Value)));
	default:                                              return FText::GetEmpty();
	}
}

bool FJobEvaluator::CheckRequirement(const FJobRequirement& R, const AModularCar* Car, float Tolerance)
{
	if (!Car)
	{
		return false;
	}
	const float Lo = R.Value * (1.f - Tolerance);
	const float Hi = R.Value * (1.f + Tolerance);
	switch (R.Type)
	{
	case EJobRequirementType::FixNoisyFaults:             return !Car->HasFault(true);
	case EJobRequirementType::FixAllFaults:               return !Car->HasFault(false);
	case EJobRequirementType::TopSpeedAtLeast:            return Car->GetBestTopSpeedKmh() >= Lo;
	case EJobRequirementType::HasPartId:                  return Car->HasPartId(R.Param);
	case EJobRequirementType::HasPartWithTag:             return Car->HasPartWithTag(R.Param);
	case EJobRequirementType::HasCategory:
	{
		const int64 Value = StaticEnum<EPartCategory>()->GetValueByNameString(R.Param.ToString());
		return Value != INDEX_NONE && Car->HasCategory((EPartCategory)Value);
	}
	case EJobRequirementType::VividPaint:                 return Car->HasVividPaint();
	case EJobRequirementType::DistinctPaintColorsAtLeast: return Car->CountDistinctPaintColors() >= FMath::RoundToInt(R.Value);
	case EJobRequirementType::FuelUseAtMost:              return Car->GetFuelUse() <= Hi;
	case EJobRequirementType::TotalMassAtMost:            return Car->GetStats().TotalMassKg <= Hi;
	case EJobRequirementType::WheelRadiusAtLeast:         return Car->GetMinWheelRadius() >= Lo;
	case EJobRequirementType::AirTimeAtLeast:             return Car->GetMaxAirTime() >= Lo;
	case EJobRequirementType::AllRequiredSlotsFilled:     return Car->AllRequiredSlotsFilled();
	case EJobRequirementType::NoLooseParts:               return !Car->HasLooseParts();
	case EJobRequirementType::NoBrokenParts:              return !Car->HasBrokenParts();
	case EJobRequirementType::MinAverageCondition:        return Car->GetAverageCondition() >= Lo;
	default:                                              return false;
	}
}

TArray<FText> FJobEvaluator::BuildLabels(const FJobDefinitionRow& Definition)
{
	TArray<FText> Labels;
	for (const FJobRequirement& R : Definition.Requirements)
	{
		Labels.Add(DescribeRequirement(R));
	}
	Labels.Add(LOCTEXT("ImplicitComplete", "Coche completo"));
	Labels.Add(LOCTEXT("ImplicitTight", "Sin piezas flojas"));
	if (Definition.TimeLimitSeconds > 0.f)
	{
		Labels.Add(LOCTEXT("ImplicitDeadline", "Dentro de plazo"));
	}
	return Labels;
}

FJobEvaluation FJobEvaluator::Evaluate(const FJobDefinitionRow& Definition, const FActiveJob& Job, const AModularCar* Car, float ServerNow)
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	FJobEvaluation Eval;
	Eval.JobUid = Job.JobUid;
	Eval.CustomerName = Job.CustomerName;
	Eval.ServerTime = ServerNow;

	for (const FJobRequirement& R : Definition.Requirements)
	{
		Eval.Labels.Add(DescribeRequirement(R));
		Eval.Passed.Add(CheckRequirement(R, Car, Definition.Tolerance));
	}
	Eval.Labels.Add(LOCTEXT("ImplicitComplete", "Coche completo"));
	Eval.Passed.Add(Car && Car->AllRequiredSlotsFilled());
	Eval.Labels.Add(LOCTEXT("ImplicitTight", "Sin piezas flojas"));
	Eval.Passed.Add(Car && !Car->HasLooseParts());
	const bool bHasDeadline = Job.DeadlineServerTime > 0.f;
	if (bHasDeadline)
	{
		Eval.Labels.Add(LOCTEXT("ImplicitDeadline", "Dentro de plazo"));
		Eval.Passed.Add(ServerNow <= Job.DeadlineServerTime);
	}

	int32 NumPassed = 0;
	for (bool b : Eval.Passed) { NumPassed += b ? 1 : 0; }
	const float Fraction = Eval.Passed.Num() > 0 ? (float)NumPassed / Eval.Passed.Num() : 0.f;

	if (NumPassed == Eval.Passed.Num())
	{
		Eval.Outcome = EJobOutcome::FullPay;
		Eval.Payment = Job.Budget + (bHasDeadline ? FMath::RoundToInt(Job.Budget * T.DeadlineBonusFraction) : 0);
		Eval.ReputationDelta = T.RepFull;
	}
	else if (Fraction >= 0.5f)
	{
		Eval.Outcome = EJobOutcome::PartialPay;
		Eval.Payment = FMath::RoundToInt(Job.Budget * T.PartialPayFraction * Fraction);
		Eval.ReputationDelta = T.RepPartial;
	}
	else
	{
		Eval.Outcome = EJobOutcome::Angry;
		Eval.Payment = 0;
		Eval.ReputationDelta = T.RepAngry;
	}
	return Eval;
}

#undef LOCTEXT_NAMESPACE
