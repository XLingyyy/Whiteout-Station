#include "Flow/WhiteoutGameMode.h"
#include "Settings/WhiteoutSettingsSubsystem.h"
#include "State/WindStationStateSubsystem.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void AWhiteoutGameMode::SubmitV15OnlineRouteDialogue(const FWSActionRequest& AuthoredRequest)
{
	UWindStationStateSubsystem* State = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>();
	const TArray<FWSAuthoredChoice> Choices = State->GetAuthoredDialogueChoices(AuthoredRequest.ActionId);
	const FWSAuthoredChoice* Choice = Choices.FindByPredicate([&](const FWSAuthoredChoice& Item)
		{ return Item.ChoiceId == AuthoredRequest.AuthoredChoiceId; });
	if (!Choice)
	{
		bAutomationRouteSucceeded = false;
		FinishAutomationRoute(AutomationRouteRunId);
		return;
	}
	const FString Text = Choice->Text;
	const bool bPromise = AuthoredRequest.DialogueAct == EWSDialogueAct::Promise;
	const FString OldProvider = Settings->GetLLMProviderId(), OldUrl = Settings->GetLLMBaseUrl();
	const FString OldModel = Settings->GetLLMModelId(), OldKey = Settings->GetSessionLLMApiKey();
	const bool OldEnabled = Settings->IsLLMEnabled();
	FString Error;
	if (!Settings->SetLLMConfiguration(TEXT("deepseek"), TEXT("https://api.deepseek.com"),
		FPlatformMisc::GetEnvironmentVariable(TEXT("WHITEOUT_V15_TEST_KEY")), TEXT("deepseek-v4-flash"), true, Error))
	{
		bAutomationRouteSucceeded = false;
		FinishAutomationRoute(AutomationRouteRunId);
		return;
	}
	const FGuid Session = FGuid::NewGuid(), RunId = AutomationRouteRunId;
	const FName Action = AuthoredRequest.ActionId;
	const int32 APBefore = State->GetStateSnapshot().PhaseActionPoints;
	const int32 PromisesBefore = State->GetStateSnapshot().Promises.Num();
	const int64 RevisionBefore = State->GetStateRevision();
	TWeakObjectPtr<AWhiteoutGameMode> WeakThis(this);
	const auto Finish = [WeakThis, State, Settings, Session, RunId, Action, OldProvider, OldUrl, OldModel, OldKey, OldEnabled](bool Committed)
	{
		State->EndDialogueSession(Session);
		FString RestoreError;
		Settings->SetLLMConfiguration(OldProvider, OldUrl, OldKey, OldModel, OldEnabled, RestoreError);
		if (!WeakThis.IsValid() || WeakThis->AutomationRouteRunId != RunId) return;
		WeakThis->bAutomationRouteSucceeded &= Committed;
		UE_LOG(LogTemp, Display, TEXT("WhiteoutStation OnlineRoute: %s committed=%d source=%s"),
			*Action.ToString(), Committed, *State->GetLatestDialogue().AnswerSource);
		if (Committed) WeakThis->ScheduleAutomationRouteContinuation(RunId);
		else WeakThis->FinishAutomationRoute(RunId);
	};
	const auto Commit = [State, Session, Action, Finish](bool Ready, const FWSCanonicalIntent& Intent, const FString& PlayerText)
	{
		if (!Ready) { Finish(false); return; }
		FWSActionRequest Request;
		Request.ActionId = Action; Request.PlayerSaid = PlayerText;
		Request.DialogueSessionId = Session; Request.TransactionId = FGuid::NewGuid();
		Intent.ApplyTo(Request);
		State->SubmitDialogueAction(Request, [Finish](const FWSActionResult& Result) { Finish(Result.bCommitted); });
	};
	State->ResolveOnlineIntent(Action, Text, Session,
		[State, Session, Action, Text, bPromise, APBefore, PromisesBefore, RevisionBefore, Commit, Finish]
		(bool Ready, const FWSCanonicalIntent& Intent, const FString& Status)
		{
			if (!bPromise) { Commit(Ready, Intent, Text); return; }
			const bool bProposalOnly = !Ready && Status.StartsWith(TEXT("请确认："))
				&& State->GetStateSnapshot().PhaseActionPoints == APBefore
				&& State->GetStateSnapshot().Promises.Num() == PromisesBefore
				&& State->GetStateRevision() == RevisionBefore;
			UE_LOG(LogTemp, Display, TEXT("WhiteoutStation OnlineRoute: promise_proposal_no_effect=%d"), bProposalOnly);
			if (!bProposalOnly) { Finish(false); return; }
			const FString Confirmation = TEXT("我确认同意刚才的承诺：下一阶段给维修间供暖。");
			State->ResolveOnlineIntent(Action, Confirmation, Session,
				[Commit, Confirmation](bool Confirmed, const FWSCanonicalIntent& ConfirmedIntent, const FString&)
				{ Commit(Confirmed && ConfirmedIntent.Commitment == EWSCommitmentIntent::ConfirmPending, ConfirmedIntent, Confirmation); });
		});
}

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
	FString Provider = TEXT("deepseek"), Url = TEXT("https://api.deepseek.com"), Model = TEXT("deepseek-v4-flash");
	Input->TryGetStringField(TEXT("provider"), Provider);
	Input->TryGetStringField(TEXT("base_url"), Url);
	Input->TryGetStringField(TEXT("model"), Model);
	const FString OldProvider = Settings->GetLLMProviderId(), OldUrl = Settings->GetLLMBaseUrl();
	const FString OldModel = Settings->GetLLMModelId(), OldKey = Settings->GetSessionLLMApiKey();
	const bool OldEnabled = Settings->IsLLMEnabled();
	FString Error;
	if (!Settings->SetLLMConfiguration(Provider, Url,
		Provider == TEXT("loopback") ? FString() : FPlatformMisc::GetEnvironmentVariable(TEXT("WHITEOUT_V15_TEST_KEY")), Model, true, Error))
	{ FPlatformMisc::RequestExitWithStatus(false, 3); return; }
	State->NewGame();
	EWSReasonCode Reason; TArray<FString> Changes;
	State->BeginDayPhase(EWSHeatingZone::MedicalRoom, Reason, Changes);
	const int32 Before = State->GetStateSnapshot().PhaseActionPoints;
	const double Started = FPlatformTime::Seconds();
	TWeakObjectPtr<AWhiteoutGameMode> WeakThis(this);
	const TSharedRef<bool> Finished = MakeShared<bool>(false);
	FString Lifecycle;
	Input->TryGetStringField(TEXT("lifecycle"), Lifecycle);
	const auto Finish = [WeakThis, Finished, Batch, Count, CaseIndex, State, Settings, OldProvider, OldUrl, OldModel, OldKey, OldEnabled, Before, Started, InputPath](bool Committed, const FString& Status)
	{
		if (*Finished) return;
		*Finished = true;
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("committed"), Committed);
		Report->SetStringField(TEXT("status"), Status);
		Report->SetNumberField(TEXT("ap_before"), Before);
		Report->SetNumberField(TEXT("ap_after"), State->GetStateSnapshot().PhaseActionPoints);
		Report->SetNumberField(TEXT("model_calls"), State->GetStateSnapshot().ModelCalls);
		Report->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - Started);
		Report->SetBoolField(TEXT("diagnosed"), State->GetStateSnapshot().Flags.bGuHengDiagnosed);
		Report->SetNumberField(TEXT("promises"), State->GetStateSnapshot().Promises.Num());
		Report->SetBoolField(TEXT("pending"), State->HasPendingDialogue());
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
		[State, Session, Action, Text, Finish, Lifecycle](bool Ready, const FWSCanonicalIntent& Intent, const FString& Status)
		{
			if (!Ready) { Finish(false, Status); return; }
			FWSActionRequest Request; Request.ActionId = FName(*Action); Request.PlayerSaid = Text;
			Request.DialogueSessionId = Session; Request.TransactionId = FGuid::NewGuid(); Intent.ApplyTo(Request);
			State->SubmitDialogueAction(Request, [Finish, Lifecycle](const FWSActionResult& Result)
			{
				if (!Lifecycle.IsEmpty() && !Result.bCommitted) return;
				Finish(Result.bCommitted, StaticEnum<EWSReasonCode>()->GetNameStringByValue(static_cast<int64>(Result.ReasonCode)));
			});
		});
	if (!Lifecycle.IsEmpty())
	{
		FTimerHandle CancelTimer;
		GetWorldTimerManager().SetTimer(CancelTimer, [WeakThis, Finished, State, Lifecycle, Finish]()
		{
			if (!WeakThis.IsValid() || *Finished) return;
			if (Lifecycle == TEXT("new_game")) State->NewGame();
			else State->CancelPendingDialogue();
			FTimerHandle LateReplyTimer;
			WeakThis->GetWorldTimerManager().SetTimer(LateReplyTimer, [Finish]() { Finish(false, TEXT("lifecycle_settled")); }, 2.0f, false);
		}, 0.4f, false);
	}
}
