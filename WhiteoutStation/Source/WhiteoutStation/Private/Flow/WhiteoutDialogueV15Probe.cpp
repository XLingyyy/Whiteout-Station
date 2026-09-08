#include "Flow/WhiteoutGameMode.h"
#include "Settings/WhiteoutSettingsSubsystem.h"
#include "State/WindStationStateSubsystem.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	void RunSemanticProbeStep(UWindStationStateSubsystem* State, FGuid Session, FName Action,
		const TArray<FString>& Steps, int32 Index, bool Reopen, TSharedRef<TArray<TSharedPtr<FJsonValue>>> Reports,
		TFunction<void(bool, const FString&)> Finish)
	{
		if (Index >= Steps.Num()) { Finish(false, TEXT("sequence_completed")); return; }
		if (Reopen && Index > 0) { State->EndDialogueSession(Session); Session = FGuid::NewGuid(); }
		const FString Text = Steps[Index];
		const auto Before = State->GetStateSnapshot();
		if (Text.StartsWith(TEXT("@")))
		{
			bool Success = false;
			if (Text == TEXT("@save_load")) Success = State->SaveSnapshot() && State->LoadSnapshot();
			else if (Text == TEXT("@reopen")) { State->EndDialogueSession(Session); Session = FGuid::NewGuid(); Success = true; }
			else if (Text == TEXT("@talk_gu")) { State->EndDialogueSession(Session); Session = FGuid::NewGuid(); Action = TEXT("talk_gu_heng"); Success = true; }
			else if (Text == TEXT("@talk_ye")) { State->EndDialogueSession(Session); Session = FGuid::NewGuid(); Action = TEXT("talk_ye_cheng"); Success = true; }
			else if (Text == TEXT("@rest_doctor"))
			{
				FWSActionRequest Request; Request.ActionId = TEXT("rest"); Request.RestTarget = EWSCharacterId::YeCheng; Request.RestLocation = EWSCharacterLocation::MedicalRoom;
				Success = State->CommitAction(Request).bCommitted;
			}
			else if (Text == TEXT("@reveal_heatpack")) Success = State->SubmitAuthoredDialogueChoice(TEXT("talk_ye_cheng"), TEXT("ye_alternative"), FGuid::NewGuid()).bCommitted;
			else if (Text == TEXT("@treat_full") || Text == TEXT("@treat_support") || Text == TEXT("@bandage"))
			{
				FWSActionRequest Request; Request.ActionId = TEXT("treat_character"); Request.TreatmentTarget = EWSCharacterId::GuHeng;
				Request.TreatmentMethod = Text == TEXT("@treat_full") ? EWSTreatmentMethod::Full : Text == TEXT("@bandage") ? EWSTreatmentMethod::Bandage : EWSTreatmentMethod::HeatPack;
				Request.bHasCollaborator = true; Request.Collaborator = EWSCharacterId::Player;
				Success = State->CommitAction(Request).bCommitted;
			}
			const auto After = State->GetStateSnapshot();
			auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("text"), Text);
			Row->SetStringField(TEXT("kind"), TEXT("fixture_action")); Row->SetBoolField(TEXT("committed"), Success);
			Row->SetArrayField(TEXT("parsed"), {}); Row->SetStringField(TEXT("line"), TEXT(""));
			Row->SetNumberField(TEXT("ap_before"), Before.PhaseActionPoints); Row->SetNumberField(TEXT("ap_after"), After.PhaseActionPoints);
			Row->SetBoolField(TEXT("treated"), After.Flags.bGuHengTreated); Row->SetNumberField(TEXT("medicine"), After.Resources.Medicine);
			Row->SetNumberField(TEXT("turns"), State->GetDialogueTurnsUsed(Action)); Reports->Add(MakeShared<FJsonValueObject>(Row));
			if (!Success) { Finish(false, TEXT("fixture_action_failed")); return; }
			RunSemanticProbeStep(State, Session, Action, Steps, Index + 1, Reopen, Reports, Finish); return;
		}
		const double MessageStarted = FPlatformTime::Seconds();
		State->ResolveOnlineIntent(Action, Text, Session,
			[State, Session, Action, Steps, Index, Reopen, Reports, Finish, Before, Text, MessageStarted](bool Ready, const FWSCanonicalIntent& Intent, const FString& Status)
			{
				auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("text"), Text);
				Row->SetBoolField(TEXT("ready"), Ready); Row->SetStringField(TEXT("resolution_status"), Status);
				Row->SetBoolField(TEXT("diagnosed_before"), Before.Flags.bGuHengDiagnosed);
				Row->SetNumberField(TEXT("ye_trust_before"), Before.Characters.FindRef(EWSCharacterId::YeCheng).Trust);
				Row->SetNumberField(TEXT("ye_pressure_before"), Before.Characters.FindRef(EWSCharacterId::YeCheng).Pressure);
				TArray<TSharedPtr<FJsonValue>> Parsed;
				if (const auto* Ledger = State->GetDialogueSessionState(Session); Ledger && Ledger->LastParsedMessage.IsSet())
				{
					const auto& Original = Ledger->LastParsedMessage.GetValue();
					const auto Parts = Original.Parts.IsEmpty() ? TArray<FWSCanonicalIntent>{Original} : Original.Parts;
					for (const auto& Part : Parts)
					{
						auto P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("topic"), Part.TopicId.ToString());
						P->SetNumberField(TEXT("speech_act"), static_cast<int32>(Part.Frame.SpeechAct));
						P->SetNumberField(TEXT("target"), static_cast<int32>(Part.Frame.TargetCharacter));
						P->SetNumberField(TEXT("polarity"), static_cast<int32>(Part.Polarity));
						P->SetNumberField(TEXT("commitment"), static_cast<int32>(Part.Commitment));
						P->SetBoolField(TEXT("clarify"), Part.bNeedsClarification);
						P->SetStringField(TEXT("clarification"), Part.Clarification);
						TArray<TSharedPtr<FJsonValue>> Terms;
						for (const auto& T : Part.Terms)
						{
							auto Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("kind"), T.Kind.ToString());
							Item->SetNumberField(TEXT("zone"), static_cast<int32>(T.Zone)); Item->SetNumberField(TEXT("offset"), T.PhaseOffset);
							Item->SetStringField(TEXT("prerequisite"), T.Prerequisite); Item->SetStringField(TEXT("description"), T.Description);
							Terms.Add(MakeShared<FJsonValueObject>(Item));
						}
						P->SetArrayField(TEXT("terms"), Terms); Parsed.Add(MakeShared<FJsonValueObject>(P));
					}
				}
				Row->SetArrayField(TEXT("parsed"), Parsed);
				const auto Record = [State, Session, Action, Steps, Index, Reopen, Reports, Finish, Before, Row, Ready, MessageStarted](bool Committed, const FString& Result)
				{
					const auto After = State->GetStateSnapshot();
					Row->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - MessageStarted);
					Row->SetNumberField(TEXT("model_calls"), After.ModelCalls - Before.ModelCalls);
					Row->SetNumberField(TEXT("turns_used"), State->GetDialogueTurnsUsed(Action));
					Row->SetNumberField(TEXT("state_revision"), State->GetStateRevision());
					Row->SetNumberField(TEXT("history_before"), Before.ConversationHistory.Num());
					Row->SetNumberField(TEXT("history_after"), After.ConversationHistory.Num());
					Row->SetBoolField(TEXT("committed"), Committed); Row->SetStringField(TEXT("result"), Result);
					Row->SetNumberField(TEXT("ap_before"), Before.PhaseActionPoints); Row->SetNumberField(TEXT("ap_after"), After.PhaseActionPoints);
					Row->SetNumberField(TEXT("promises"), After.Promises.Num()); Row->SetNumberField(TEXT("generator_progress"), After.Tasks.GeneratorProgress);
					Row->SetNumberField(TEXT("forced_actions"), After.Flags.ForcedActionCount);
					Row->SetBoolField(TEXT("treated"), After.Flags.bGuHengTreated); Row->SetBoolField(TEXT("diagnosed"), After.Flags.bGuHengDiagnosed);
					Row->SetNumberField(TEXT("medicine"), After.Resources.Medicine); Row->SetNumberField(TEXT("relay"), After.Resources.ReplacementRelay);
					Row->SetStringField(TEXT("line"), Committed ? State->GetLatestDialogue().Utterance : FString());
					Row->SetStringField(TEXT("source"), Committed ? State->GetLatestDialogue().AnswerSource : FString());
					if (const auto* Ledger = State->GetDialogueSessionState(Session))
					{
						Row->SetNumberField(TEXT("messages"), Ledger->MessageCount); Row->SetNumberField(TEXT("turns"), Ledger->CommittedTurns);
						Row->SetBoolField(TEXT("pending_proposal"), Ledger->PendingCommitment.IsSet());
						if (Ledger->PendingCommitment.IsSet()) Row->SetNumberField(TEXT("proposal_version"), Ledger->PendingCommitment->ProposalVersion);
					}
					Reports->Add(MakeShared<FJsonValueObject>(Row));
					RunSemanticProbeStep(State, Session, Action, Steps, Index + 1, Reopen, Reports, Finish);
				};
				if (!Ready) { Record(false, Status); return; }
				FWSActionRequest Request; Intent.ApplyTo(Request); Request.ActionId = Action; Request.DialogueSessionId = Session;
				Request.PlayerSaid = Text; Request.TransactionId = FGuid::NewGuid();
				State->SubmitDialogueAction(Request, [Record](const FWSActionResult& R)
					{ Record(R.bCommitted, StaticEnum<EWSReasonCode>()->GetNameStringByValue(static_cast<int64>(R.ReasonCode))); });
			});
	}
}

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
	const FGuid Session = FGuid::NewGuid();
	const auto StepReports = MakeShared<TArray<TSharedPtr<FJsonValue>>>();
	const double Started = FPlatformTime::Seconds();
	TWeakObjectPtr<AWhiteoutGameMode> WeakThis(this);
	const TSharedRef<bool> Finished = MakeShared<bool>(false);
	FString Lifecycle;
	Input->TryGetStringField(TEXT("lifecycle"), Lifecycle);
	const auto Finish = [WeakThis, Finished, Batch, Count, CaseIndex, State, Settings, OldProvider, OldUrl, OldModel, OldKey, OldEnabled, Before, Started, InputPath, StepReports](bool Committed, const FString& Status)
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
		Report->SetArrayField(TEXT("steps"), *StepReports);
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
	TArray<FString> Steps;
	if (Input->TryGetStringArrayField(TEXT("steps"), Steps))
	{
		bool SetupDiagnosis = false; Input->TryGetBoolField(TEXT("setup_diagnosis"), SetupDiagnosis);
		if (SetupDiagnosis && !State->SubmitAuthoredDialogueChoice(TEXT("talk_ye_cheng"), TEXT("ye_diagnosis"), FGuid::NewGuid()).bCommitted)
		{ Finish(false, TEXT("authored_diagnosis_setup_failed")); return; }
		bool SetupCooperation = false; Input->TryGetBoolField(TEXT("setup_cooperation"), SetupCooperation);
		if (SetupCooperation && !State->SubmitAuthoredDialogueChoice(TEXT("talk_ye_cheng"), TEXT("ye_reassure"), FGuid::NewGuid()).bCommitted)
		{ Finish(false, TEXT("authored_cooperation_setup_failed")); return; }
		bool IncorrectHistory = false; Input->TryGetBoolField(TEXT("setup_incorrect_history"), IncorrectHistory);
		if (IncorrectHistory)
		{
			FWSConversationEntry Entry; Entry.EntryId = FGuid::NewGuid(); Entry.SessionId = Session;
			Entry.SpeakerId = TEXT("ye_cheng"); Entry.PlayerLine = TEXT("你给顾衡治疗了吗？");
			Entry.NpcLine = TEXT("已经处理过了，我刚给他的手做了完整治疗。");
			Entry.bCommitted = true; Entry.bCountedTurn = false; Entry.ReplySource = TEXT("legacy_error_test_fixture");
			State->RulesEngine.RecordConversationEntry(Entry);
		}
		int32 SetupTurns = 0; Input->TryGetNumberField(TEXT("setup_turns"), SetupTurns);
		for (int32 I = 0; I < SetupTurns; ++I)
		{
			const FName Choice = I == 0 ? FName(TEXT("gu_person")) : FName(TEXT("gu_generator"));
			if (!State->SubmitAuthoredDialogueChoice(TEXT("talk_gu_heng"), Choice, Session).bCommitted)
			{ Finish(false, TEXT("authored_setup_failed")); return; }
		}
		bool Reopen = false; Input->TryGetBoolField(TEXT("reopen_each_step"), Reopen);
		RunSemanticProbeStep(State, Session, FName(*Action), Steps, 0, Reopen, StepReports, Finish);
		return;
	}
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
