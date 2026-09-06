#include "Flow/WhiteoutGameMode.h"
#include "Settings/WhiteoutSettingsSubsystem.h"
#include "State/WindStationStateSubsystem.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void AWhiteoutGameMode::RunV15DialogueProbe(const FString& InputPath, const int32 CaseIndex)
{
	FString Json, Text, Action;
	TSharedPtr<FJsonObject> Input;
	if (!FFileHelper::LoadFileToString(Json, *InputPath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Input))
	{ FPlatformMisc::RequestExitWithStatus(false, 2); return; }
	const TArray<TSharedPtr<FJsonValue>>* Cases = nullptr;
	const bool Batch = Input->TryGetArrayField(TEXT("cases"), Cases);
	const int32 Count = Batch ? Cases->Num() : 1;
	if (Batch)
	{
		if (!Cases->IsValidIndex(CaseIndex)) { FPlatformMisc::RequestExit(false); return; }
		Input = (*Cases)[CaseIndex]->AsObject();
	}
	if (!Input || !Input->TryGetStringField(TEXT("text"), Text) || !Input->TryGetStringField(TEXT("action"), Action))
	{ FPlatformMisc::RequestExitWithStatus(false, 2); return; }
	UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>();
	UWindStationStateSubsystem* State = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	const FString OldProvider = Settings->GetLLMProviderId(), OldUrl = Settings->GetLLMBaseUrl();
	const FString OldModel = Settings->GetLLMModelId(), OldKey = Settings->GetSessionLLMApiKey();
	const bool OldEnabled = Settings->IsLLMEnabled();
	FString Error;
	if (!Settings->SetLLMConfiguration(TEXT("deepseek"), TEXT("https://api.deepseek.com"),
		FPlatformMisc::GetEnvironmentVariable(TEXT("WHITEOUT_V15_TEST_KEY")), TEXT("deepseek-v4-flash"), true, Error))
	{ FPlatformMisc::RequestExitWithStatus(false, 3); return; }
	State->NewGame();
	EWSReasonCode Reason; TArray<FString> Changes;
	State->BeginDayPhase(EWSHeatingZone::MedicalRoom, Reason, Changes);
	const int32 Before = State->GetStateSnapshot().PhaseActionPoints;
	const double Started = FPlatformTime::Seconds();
	TWeakObjectPtr<AWhiteoutGameMode> WeakThis(this);
	const auto Finish = [WeakThis, Batch, Count, CaseIndex, State, Settings, OldProvider, OldUrl, OldModel, OldKey, OldEnabled, Before, Started, InputPath](bool Committed, const FString& Status)
	{
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("committed"), Committed);
		Report->SetStringField(TEXT("status"), Status);
		Report->SetNumberField(TEXT("ap_before"), Before);
		Report->SetNumberField(TEXT("ap_after"), State->GetStateSnapshot().PhaseActionPoints);
		Report->SetNumberField(TEXT("model_calls"), State->GetStateSnapshot().ModelCalls);
		Report->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - Started);
		Report->SetBoolField(TEXT("diagnosed"), State->GetStateSnapshot().Flags.bGuHengDiagnosed);
		Report->SetNumberField(TEXT("promises"), State->GetStateSnapshot().Promises.Num());
		Report->SetStringField(TEXT("line"), State->GetLatestDialogue().Utterance);
		Report->SetStringField(TEXT("source"), State->GetLatestDialogue().AnswerSource);
		Report->SetStringField(TEXT("validation"), State->GetLatestDialogue().ValidationReason);
		FString Output; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Output));
		FFileHelper::SaveStringToFile(Output, *(InputPath + (Batch ? FString::Printf(TEXT(".%03d.result.json"), CaseIndex) : FString(TEXT(".result.json")))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString RestoreError;
		Settings->SetLLMConfiguration(OldProvider, OldUrl, OldKey, OldModel, OldEnabled, RestoreError);
		if (Batch && CaseIndex + 1 < Count && WeakThis.IsValid())
		{
			FTimerHandle Next;
			WeakThis->GetWorldTimerManager().SetTimer(Next, [WeakThis, InputPath, CaseIndex]()
			{ if (WeakThis.IsValid()) WeakThis->RunV15DialogueProbe(InputPath, CaseIndex + 1); }, 0.05f, false);
		}
		else FPlatformMisc::RequestExit(false);
	};
	const FGuid Session = FGuid::NewGuid();
	State->ResolveOnlineIntent(FName(*Action), Text, Session,
		[State, Session, Action, Text, Finish](bool Ready, const FWSCanonicalIntent& Intent, const FString& Status)
		{
			if (!Ready) { Finish(false, Status); return; }
			FWSActionRequest Request; Request.ActionId = FName(*Action); Request.PlayerSaid = Text;
			Request.DialogueSessionId = Session; Request.TransactionId = FGuid::NewGuid(); Intent.ApplyTo(Request);
			State->SubmitDialogueAction(Request, [Finish](const FWSActionResult& Result)
			{
				Finish(Result.bCommitted, StaticEnum<EWSReasonCode>()->GetNameStringByValue(static_cast<int64>(Result.ReasonCode)));
			});
		});
}
