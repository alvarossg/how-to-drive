#include "Core/HTMTelemetrySubsystem.h"
#include "Core/HTMLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "HTMTelemetry"

UHTMTelemetrySubsystem* UHTMTelemetrySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UHTMTelemetrySubsystem>() : nullptr;
}

void UHTMTelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SessionStamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Telemetry");
	EventsFile = Dir / FString::Printf(TEXT("events_%s.csv"), *SessionStamp);
	SummaryFile = Dir / FString::Printf(TEXT("situations_%s.csv"), *SessionStamp);
}

void UHTMTelemetrySubsystem::Deinitialize()
{
	Flush();
	Super::Deinitialize();
}

void UHTMTelemetrySubsystem::Record(const UObject* WorldContext, ETelemetryEvent Event, const FString& Who, const FString& Detail,
	const FVector& Location, float Value)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	FHTMTelemetryRecord R;
	R.Time = World->GetTimeSeconds();
	R.Day = CurrentDay;
	R.Event = Event;
	R.Who = Who;
	R.Detail = Detail;
	R.Location = Location;
	R.Value = Value;
	Pending.Add(R);
	DayRecords.Add(R);

	if (Pending.Num() >= 64)
	{
		Flush();
	}
}

void UHTMTelemetrySubsystem::RecordSituation(const UObject* WorldContext, EHTMSituation Situation, const FString& Who, const FVector& Location)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || Situation == EHTMSituation::MAX)
	{
		return;
	}
	const int32 Index = (int32)Situation;
	const double Now = World->GetTimeSeconds();
	if (Now - LastSituationTime[Index] < 3.0 && LastSituationTime[Index] > 0.0)
	{
		return;
	}
	LastSituationTime[Index] = Now;
	DaySituationCounts[Index]++;
	SessionSituationCounts[Index]++;

	FHTMTelemetryRecord R;
	R.Time = Now;
	R.Day = CurrentDay;
	R.Event = ETelemetryEvent::Situation;
	R.Situation = Situation;
	R.Who = Who;
	R.Detail = HTM::SituationName(Situation).ToString();
	R.Location = Location;
	Pending.Add(R);
	DayRecords.Add(R);
	UE_LOG(LogHTM, Log, TEXT("[Situación §13] %s (%s)"), *R.Detail, *Who);
}

void UHTMTelemetrySubsystem::BeginDay(int32 DayNumber)
{
	CurrentDay = DayNumber;
	DayRecords.Reset();
	FMemory::Memzero(DaySituationCounts, sizeof(DaySituationCounts));
}

int32 UHTMTelemetrySubsystem::GetDistinctSituationsToday() const
{
	int32 N = 0;
	for (int32 i = 0; i < (int32)EHTMSituation::MAX; ++i) { N += DaySituationCounts[i] > 0 ? 1 : 0; }
	return N;
}

int32 UHTMTelemetrySubsystem::GetDistinctSituationsSession() const
{
	int32 N = 0;
	for (int32 i = 0; i < (int32)EHTMSituation::MAX; ++i) { N += SessionSituationCounts[i] > 0 ? 1 : 0; }
	return N;
}

int32 UHTMTelemetrySubsystem::DisasterScore(const FHTMTelemetryRecord& R)
{
	switch (R.Event)
	{
	case ETelemetryEvent::EngineFire:   return 90;
	case ETelemetryEvent::CarFlipped:   return 80;
	case ETelemetryEvent::Trapped:      return 70;
	case ETelemetryEvent::KO:           return R.Detail.Contains(TEXT("Engine")) ? 85 : 50;
	case ETelemetryEvent::PartDetached: return R.Detail.Contains(TEXT("Wheel")) ? 65 : 45;
	case ETelemetryEvent::JobFailed:    return 40;
	case ETelemetryEvent::CarCrash:     return 60;
	case ETelemetryEvent::PaintRuined:  return 55;
	case ETelemetryEvent::Situation:    return 35;
	default:                            return 0;
	}
}

FText UHTMTelemetrySubsystem::DescribeDisaster(const FHTMTelemetryRecord& R)
{
	const FText Who = FText::FromString(R.Who.IsEmpty() ? TEXT("Alguien") : R.Who);
	switch (R.Event)
	{
	case ETelemetryEvent::EngineFire:   return FText::Format(LOCTEXT("DFire", "Fuego en el motor de {0}"), FText::FromString(R.Detail));
	case ETelemetryEvent::CarFlipped:   return FText::Format(LOCTEXT("DFlip", "{0} volcó un coche"), Who);
	case ETelemetryEvent::Trapped:      return FText::Format(LOCTEXT("DTrap", "{0} quedó atrapado bajo un coche"), Who);
	case ETelemetryEvent::KO:           return FText::Format(LOCTEXT("DKO", "{0} acabó KO ({1})"), Who, FText::FromString(R.Detail));
	case ETelemetryEvent::PartDetached: return FText::Format(LOCTEXT("DPart", "Se soltó: {0}"), FText::FromString(R.Detail));
	case ETelemetryEvent::JobFailed:    return FText::Format(LOCTEXT("DJob", "Cliente enfadado: {0}"), FText::FromString(R.Detail));
	case ETelemetryEvent::CarCrash:     return FText::Format(LOCTEXT("DCrash", "Choque: {0}"), FText::FromString(R.Detail));
	case ETelemetryEvent::PaintRuined:  return LOCTEXT("DPaint", "Pintura fresca estropeada");
	default:                            return FText::FromString(R.Detail);
	}
}

TArray<FHTMDisaster> UHTMTelemetrySubsystem::EndDay(int32 MaxDisasters)
{
	TArray<FHTMTelemetryRecord> Sorted = DayRecords;
	Sorted.Sort([](const FHTMTelemetryRecord& A, const FHTMTelemetryRecord& B) { return DisasterScore(A) > DisasterScore(B); });

	TArray<FHTMDisaster> Result;
	TSet<FString> Seen;
	for (const FHTMTelemetryRecord& R : Sorted)
	{
		if (Result.Num() >= MaxDisasters || DisasterScore(R) <= 0)
		{
			break;
		}
		const FText Desc = DescribeDisaster(R);
		if (Seen.Contains(Desc.ToString()))
		{
			continue;
		}
		Seen.Add(Desc.ToString());
		FHTMDisaster D;
		D.Description = Desc;
		D.Who = R.Who;
		D.Score = DisasterScore(R);
		Result.Add(D);
	}
	Flush();
	return Result;
}

void UHTMTelemetrySubsystem::Flush()
{
	if (Pending.Num() > 0)
	{
		FString Out;
		if (!FPaths::FileExists(EventsFile))
		{
			Out += TEXT("time,day,event,situation,who,detail,x,y,z,value\n");
		}
		const UEnum* EventEnum = StaticEnum<ETelemetryEvent>();
		const UEnum* SitEnum = StaticEnum<EHTMSituation>();
		for (const FHTMTelemetryRecord& R : Pending)
		{
			Out += FString::Printf(TEXT("%.2f,%d,%s,%s,\"%s\",\"%s\",%.0f,%.0f,%.0f,%.2f\n"),
				R.Time, R.Day,
				*EventEnum->GetNameStringByValue((int64)R.Event),
				R.Situation == EHTMSituation::MAX ? TEXT("") : *SitEnum->GetNameStringByValue((int64)R.Situation),
				*R.Who.Replace(TEXT("\""), TEXT("'")), *R.Detail.Replace(TEXT("\""), TEXT("'")),
				R.Location.X, R.Location.Y, R.Location.Z, R.Value);
		}
		FFileHelper::SaveStringToFile(Out, *EventsFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		Pending.Reset();
	}

	// Resumen de situaciones §13 (se reescribe entero).
	FString Summary = TEXT("situation,description,count_session\n");
	const UEnum* SitEnum = StaticEnum<EHTMSituation>();
	for (int32 i = 0; i < (int32)EHTMSituation::MAX; ++i)
	{
		Summary += FString::Printf(TEXT("%s,\"%s\",%d\n"), *SitEnum->GetNameStringByValue(i),
			*HTM::SituationName((EHTMSituation)i).ToString(), SessionSituationCounts[i]);
	}
	Summary += FString::Printf(TEXT("DISTINCT,\"Situaciones distintas (aceptación fase 5: >= 5)\",%d\n"), GetDistinctSituationsSession());
	FFileHelper::SaveStringToFile(Summary, *SummaryFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

#undef LOCTEXT_NAMESPACE
