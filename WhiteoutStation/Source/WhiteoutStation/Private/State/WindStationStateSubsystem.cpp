#include "State/WindStationStateSubsystem.h"

#include "Actions/WSActionResolver.h"
#include "Agents/WSAgentGateway.h"
#include "Agents/WSNPCContextBuilder.h"
#include "Agents/WSNPCDecisionService.h"
#include "Agents/WSRoleplayKnowledgeRepository.h"
#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Save/WindStationSaveGame.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Settings/WhiteoutSettingsSubsystem.h"

namespace
{
	FString DialogueSpeakerId(const EWSCharacterId Speaker)
	{
		switch (Speaker)
		{
		case EWSCharacterId::GuHeng:
			return TEXT("gu_heng");
		case EWSCharacterId::YeCheng:
			return TEXT("ye_cheng");
		default:
			return TEXT("player");
		}
	}

	TArray<TSharedPtr<FJsonValue>> NameIdArray(
		const TArray<FName>& Names,
		const bool bSort = true)
	{
		TArray<FString> Values;
		Values.Reserve(Names.Num());
		for (const FName Name : Names)
		{
			if (!Name.IsNone())
			{
				Values.Add(Name.ToString());
			}
		}
		if (bSort)
		{
			Values.Sort([](const FString& Left, const FString& Right)
			{
				return Left.Compare(Right, ESearchCase::CaseSensitive) < 0;
			});
		}
		TArray<TSharedPtr<FJsonValue>> Result;
		Result.Reserve(Values.Num());
		for (const FString& Value : Values)
		{
			Result.Add(MakeShared<FJsonValueString>(Value));
		}
		return Result;
	}

	FString NormalizeValidationOutcome(
		const FString& Value,
		const bool bFallback,
		const FString& FallbackReason)
	{
		FString Result = Value.TrimStartAndEnd().ToLower();
		if (Result.IsEmpty() || Result == TEXT("unknown"))
		{
			Result = bFallback
				? FString::Printf(
					TEXT("fallback_%s"),
					FallbackReason.IsEmpty()
						? TEXT("unknown")
						: *FallbackReason)
				: TEXT("accepted");
		}
		for (TCHAR& Character : Result)
		{
			const bool bAsciiLetter = Character >= TEXT('a')
				&& Character <= TEXT('z');
			const bool bAsciiDigit = Character >= TEXT('0')
				&& Character <= TEXT('9');
			if (!bAsciiLetter && !bAsciiDigit && Character != TEXT('_'))
			{
				Character = TEXT('_');
			}
		}
		while (Result.Contains(TEXT("__")))
		{
			Result.ReplaceInline(TEXT("__"), TEXT("_"));
		}
		while (Result.RemoveFromStart(TEXT("_")))
		{
		}
		while (Result.RemoveFromEnd(TEXT("_")))
		{
		}
		if (Result.IsEmpty()
			|| Result[0] < TEXT('a')
			|| Result[0] > TEXT('z'))
		{
			Result = TEXT("outcome_") + Result;
		}
		const bool bContainsSensitiveTraceLabel =
			Result.Contains(TEXT("prompt"))
			|| Result.Contains(TEXT("request"))
			|| Result.Contains(TEXT("response"))
			|| Result.Contains(TEXT("player_said"))
			|| Result.Contains(TEXT("player_input"))
			|| Result.Contains(TEXT("player_text"))
			|| Result.Contains(TEXT("npc_line"))
			|| Result.Contains(TEXT("utterance"))
			|| Result.Contains(TEXT("api_key"))
			|| Result.Contains(TEXT("credential"))
			|| Result.Contains(TEXT("authorization"))
			|| Result.Contains(TEXT("bearer"))
			|| Result.Contains(TEXT("secret"));
		if (bContainsSensitiveTraceLabel)
		{
			Result = TEXT("fallback_external_failure");
		}
		return Result.Left(96);
	}

	bool HeatingZoneForAction(
		const FName ActionId,
		EWSHeatingZone& OutHeatingZone)
	{
		if (ActionId == TEXT("heat_repair_room"))
		{
			OutHeatingZone = EWSHeatingZone::RepairRoom;
			return true;
		}
		if (ActionId == TEXT("heat_medical_room"))
		{
			OutHeatingZone = EWSHeatingZone::MedicalRoom;
			return true;
		}
		if (ActionId == TEXT("heat_kitchen"))
		{
			OutHeatingZone = EWSHeatingZone::Kitchen;
			return true;
		}
		if (ActionId == TEXT("heat_control_room"))
		{
			OutHeatingZone = EWSHeatingZone::ControlRoom;
			return true;
		}
		return false;
	}

	void AppendOfferAudit(
		const FString& Event,
		const FName ActionId,
		const FWSNegotiationOffer* Offer,
		const FWSGameState& State)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("kind"), TEXT("negotiation_offer"));
		Root->SetStringField(TEXT("event"), Event);
		Root->SetStringField(TEXT("action_id"), ActionId.ToString());
		Root->SetStringField(
			TEXT("day_phase"),
			StaticEnum<EWSDayPhase>()->GetNameStringByValue(
				static_cast<int64>(State.DayPhase)));
		Root->SetStringField(TEXT("timestamp_utc"), FDateTime::UtcNow().ToIso8601());
		if (Offer)
		{
			Root->SetStringField(TEXT("offer_id"), Offer->OfferId.ToString());
			Root->SetBoolField(TEXT("accepted"), Offer->bAccepted);
			Root->SetBoolField(TEXT("fulfilled"), Offer->bFulfilled);
			Root->SetBoolField(TEXT("broken"), Offer->bBroken);
		}
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		if (FJsonSerializer::Serialize(Root, Writer))
		{
			FFileHelper::SaveStringToFile(
				Json + LINE_TERMINATOR,
				*(FPaths::ProjectSavedDir()
					/ TEXT("Logs/WhiteoutStation_OfferAudit.jsonl")),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
				&IFileManager::Get(),
				FILEWRITE_Append);
		}
	}
}

const FString UWindStationStateSubsystem::SaveSlot(
	TEXT("WhiteoutStation_Autosave_v1_4"));
const FString UWindStationStateSubsystem::LegacySaveSlotV13(
	TEXT("WhiteoutStation_Autosave_v1_3"));
const FString UWindStationStateSubsystem::LegacySaveSlotV12(
	TEXT("WhiteoutStation_Autosave_v1_2"));
const FString UWindStationStateSubsystem::LegacySaveSlotV11(
	TEXT("WhiteoutStation_Autosave_v1_1"));

void UWindStationStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StateRevision = 1;
	DialogueGeneration = 1;
	bHasPendingDialogue = false;
	Collection.InitializeDependency<UWhiteoutSettingsSubsystem>();
	FString Error;
	const FString ConfigPath =
		FPaths::ProjectContentDir()
		/ TEXT("Rules/WhiteoutStationRules.v1.1.json");
	if (!RulesEngine.LoadConfig(ConfigPath, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Whiteout rules config fallback: %s"), *Error);
	}
	ActionResolver = NewObject<UWSActionResolver>(this);
	ActionResolver->Initialize(this);
	AgentGateway = NewObject<UWSAgentGateway>(this);
	AgentGateway->Initialize();
	RoleplayRepository = NewObject<UWSRoleplayKnowledgeRepository>(this);
	RoleplayContextBuilder = NewObject<UWSNPCContextBuilder>(this);
	FString RoleplayError;
	if (!RoleplayRepository->LoadDefault(RoleplayError))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Whiteout v1.4 roleplay content unavailable: %s"),
			*RoleplayError);
	}
	if (UWhiteoutSettingsSubsystem* Settings =
		GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		LLMSettingsChangedHandle = Settings->OnLLMSettingsChanged.AddUObject(
			this,
			&UWindStationStateSubsystem::HandleLLMSettingsChanged);
	}
}

void UWindStationStateSubsystem::Deinitialize()
{
	if (UWhiteoutSettingsSubsystem* Settings =
		GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		Settings->OnLLMSettingsChanged.Remove(LLMSettingsChangedHandle);
	}
	LLMSettingsChangedHandle.Reset();
	AbortPendingDialogue(
		EWSReasonCode::DialogueCancelled,
		false,
		true);
	DialogueSessions.Reset();
	ActionResolver = nullptr;
	AgentGateway = nullptr;
	RoleplayContextBuilder = nullptr;
	RoleplayRepository = nullptr;
	Super::Deinitialize();
}

bool UWindStationStateSubsystem::ApplyLLMRuntimeConfiguration(FString& OutError)
{
	if (bLifecycleTransitionActive || bCommitDispatchActive)
	{
		OutError = TEXT("状态切换期间不能重配模型运行时。");
		return false;
	}
	TGuardValue<bool> LifecycleGuard(bLifecycleTransitionActive, true);
	AbortPendingDialogue(
		EWSReasonCode::DialogueCancelled,
		true,
		true);
	UWhiteoutSettingsSubsystem* Settings =
		GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>();
	if (!Settings || !AgentGateway)
	{
		OutError = TEXT("模型运行时尚未初始化。");
		LLMConfigurationError = OutError;
		return false;
	}
	const FString TargetProvider =
		Settings->GetLLMProviderId().TrimStartAndEnd().ToLower();
	const FString ExistingCredentialSource = AgentGateway->GetCredentialSource();
	const bool bPreserveLegacyCredential =
		!Settings->HasSessionLLMApiKey()
		&& (ExistingCredentialSource == TEXT("environment")
			|| ExistingCredentialSource == TEXT("local_ini"))
		&& AgentGateway->GetCredentialProviderId() == TargetProvider;
	const FString SessionApiKey = Settings->HasSessionLLMApiKey()
		? Settings->GetSessionLLMApiKey()
		: FString();
	const bool bConfigured = AgentGateway->ConfigureRuntime(
		TargetProvider,
		Settings->GetLLMBaseUrl(),
		SessionApiKey,
		Settings->GetLLMModelId(),
		Settings->IsLLMEnabled(),
		bPreserveLegacyCredential,
		Settings->HasSessionLLMApiKey() ? TEXT("session_ui") : TEXT("none"),
		OutError);
	LLMConfigurationError = bConfigured ? FString() : OutError;
	return bConfigured;
}

FString UWindStationStateSubsystem::GetLLMRuntimeStatus() const
{
	if (!LLMConfigurationError.IsEmpty())
	{
		return FString::Printf(TEXT("确定性回退｜%s"), *LLMConfigurationError);
	}
	return AgentGateway
		? AgentGateway->GetRuntimeStatus()
		: TEXT("确定性回退｜模型运行时尚未初始化");
}

bool UWindStationStateSubsystem::HasLiveLLMProvider() const
{
	return LLMConfigurationError.IsEmpty()
		&& AgentGateway
		&& AgentGateway->HasLiveProvider();
}

bool UWindStationStateSubsystem::SetRequirementPinned(
	const FName ActionId,
	const bool bPinned)
{
	if (bLifecycleTransitionActive || bCommitDispatchActive)
	{
		return false;
	}
	if (!RulesEngine.SetRequirementPinned(ActionId, bPinned))
	{
		return false;
	}
	AppendOfferAudit(
		bPinned ? TEXT("pinned") : TEXT("unpinned"),
		ActionId,
		nullptr,
		RulesEngine.GetState());
	++StateRevision;
	SaveSnapshot();
	BroadcastState();
	return true;
}

bool UWindStationStateSubsystem::AcceptLatestNegotiationOffer(FString& OutMessage)
{
	if (bLifecycleTransitionActive || bCommitDispatchActive)
	{
		OutMessage = TEXT("状态正在切换，请稍后再操作。");
		return false;
	}
	if (!RulesEngine.AcceptNegotiationOffer(LatestDialogue, OutMessage))
	{
		return false;
	}
	const FWSNegotiationOffer* AcceptedOffer = RulesEngine.GetState().NegotiationOffers.IsEmpty()
		? nullptr
		: &RulesEngine.GetState().NegotiationOffers.Last();
	AppendOfferAudit(
		TEXT("accepted"),
		LatestDialogue.RequirementReport.ActionId,
		AcceptedOffer,
		RulesEngine.GetState());
	++StateRevision;
	SaveSnapshot();
	BroadcastState();
	return true;
}

void UWindStationStateSubsystem::RequestDialogueIntent(
	const FString& UserText,
	const FName CurrentDialogueActionId,
	const FName CurrentTopicActionId,
	TFunction<void(const FWSDialogueIntentResult&)> Completion)
{
	if (bLifecycleTransitionActive || bCommitDispatchActive)
	{
		FWSDialogueIntentResult LocalIntent =
			UWSAgentGateway::ClassifyLocalIntent(
				UserText,
				CurrentDialogueActionId,
				CurrentTopicActionId);
		LocalIntent.Reason = TEXT("lifecycle_transition_local");
		Completion(LocalIntent);
		return;
	}
	if (!AgentGateway)
	{
		Completion(UWSAgentGateway::ClassifyLocalIntent(
			UserText,
			CurrentDialogueActionId,
			CurrentTopicActionId));
		return;
	}
	const bool bUseLiveProvider = LLMConfigurationError.IsEmpty()
		&& AgentGateway->HasLiveProvider();
	AgentGateway->RequestDialogueIntent(
		UserText,
		CurrentDialogueActionId,
		CurrentTopicActionId,
		bUseLiveProvider,
		FWSDialogueIntentCallback::CreateLambda(
			[Completion = MoveTemp(Completion)](const FWSDialogueIntentResult& Intent)
			{
				Completion(Intent);
			}));
}

void UWindStationStateSubsystem::HandleLLMSettingsChanged()
{
	FString Error;
	ApplyLLMRuntimeConfiguration(Error);
}

void UWindStationStateSubsystem::NewGame()
{
	if (bLifecycleTransitionActive || bCommitDispatchActive)
	{
		return;
	}
	TGuardValue<bool> LifecycleGuard(bLifecycleTransitionActive, true);
	AbortPendingDialogue(
		EWSReasonCode::DialogueCancelled,
		true,
		true);
	RulesEngine.Reset();
	DialogueSessions.Reset();
	++StateRevision;
	LatestDialogue = FWSAgentReply();
	BroadcastState();
}

FWSGameState UWindStationStateSubsystem::GetStateSnapshot() const
{
	return RulesEngine.GetState();
}

void UWindStationStateSubsystem::CancelPendingDialogue()
{
	AbortPendingDialogue(
		EWSReasonCode::DialogueCancelled,
		true,
		true);
}

void UWindStationStateSubsystem::EndDialogueSession(
	const FGuid& DialogueSessionId)
{
	if (!DialogueSessionId.IsValid())
	{
		return;
	}
	DialogueSessions.Remove(DialogueSessionId);
}

bool UWindStationStateSubsystem::CanContinueDialogueSession(
	const FGuid& DialogueSessionId) const
{
	const FWSDialogueSessionRuntimeState* Session =
		DialogueSessions.Find(DialogueSessionId);
	return Session && Session->CommittedTurns < 3;
}

bool UWindStationStateSubsystem::NormalizeDialogueSessionRequest(
	FWSActionRequest& InOutRequest,
	EWSReasonCode& OutReason) const
{
	if (!InOutRequest.DialogueSessionId.IsValid())
	{
		InOutRequest.DialogueSessionId = FGuid::NewGuid();
	}
	InOutRequest.DialogueSessionMaxTurns = 3;
	InOutRequest.DialogueTurnIndex = 1;
	InOutRequest.bDialogueSessionFollowUp = false;
	if (const FWSDialogueSessionRuntimeState* Session =
			DialogueSessions.Find(InOutRequest.DialogueSessionId))
	{
		if (Session->ActionId != InOutRequest.ActionId
			|| Session->DayPhase != RulesEngine.GetState().DayPhase)
		{
			OutReason = EWSReasonCode::DialogueStateChanged;
			return false;
		}
		if (Session->CommittedTurns >= InOutRequest.DialogueSessionMaxTurns)
		{
			OutReason = EWSReasonCode::DialogueSessionComplete;
			return false;
		}
		InOutRequest.DialogueTurnIndex = Session->CommittedTurns + 1;
		InOutRequest.bDialogueSessionFollowUp = true;
	}
	OutReason = EWSReasonCode::Ok;
	return true;
}

void UWindStationStateSubsystem::RecordCommittedDialogueSession(
	const FWSActionRequest& Request)
{
	if (!Request.DialogueSessionId.IsValid())
	{
		return;
	}
	FWSDialogueSessionRuntimeState& Session =
		DialogueSessions.FindOrAdd(Request.DialogueSessionId);
	Session.ActionId = Request.ActionId;
	Session.DayPhase = RulesEngine.GetState().DayPhase;
	Session.CommittedTurns = FMath::Clamp(
		Request.DialogueTurnIndex,
		1,
		Request.DialogueSessionMaxTurns);
}

FWSActionPreview UWindStationStateSubsystem::PreviewAction(const FWSActionRequest& Request) const
{
	FWSActionRequest NormalizedRequest = Request;
	if (Request.ActionId == TEXT("talk_gu_heng")
		|| Request.ActionId == TEXT("talk_ye_cheng"))
	{
		EWSReasonCode SessionReason = EWSReasonCode::Ok;
		if (!NormalizeDialogueSessionRequest(NormalizedRequest, SessionReason))
		{
			FWSActionPreview Preview;
			Preview.ActionId = Request.ActionId;
			Preview.ReasonCode = SessionReason;
			Preview.WorkReadiness = EWSWorkReadiness::Unavailable;
			return Preview;
		}
	}
	EWSHeatingZone HeatingZone = EWSHeatingZone::None;
	if (RulesEngine.IsV11()
		&& HeatingZoneForAction(NormalizedRequest.ActionId, HeatingZone))
	{
		const FWSGameState& State = RulesEngine.GetState();
		FWSActionPreview Preview;
		Preview.ActionId = NormalizedRequest.ActionId;
		Preview.BaseAP = 0;
		Preview.RawAP = 0;
		Preview.APCost = 0;
		Preview.WorkReadiness = EWSWorkReadiness::Ready;
		Preview.PreviewText = FText::FromString(
			TEXT("锁定本阶段供暖区，消耗 1 单位燃料；本阶段内不可更改。"));
		if (State.bDayWindowClosed)
		{
			Preview.ReasonCode = EWSReasonCode::WindowClosed;
		}
		else if (State.bDayPhaseStarted || State.Heating.bLocked)
		{
			Preview.ReasonCode = EWSReasonCode::HeatingLocked;
		}
		else if (State.Resources.Fuel < 1)
		{
			Preview.ReasonCode = EWSReasonCode::NeedsFuel;
		}
		else
		{
			Preview.bCanExecute = true;
			Preview.ReasonCode = EWSReasonCode::Ok;
		}
		return Preview;
	}
	return RulesEngine.Preview(NormalizedRequest);
}

FWSActionRequirementReport UWindStationStateSubsystem::EvaluateActionRequirements(
	const FName ActionId) const
{
	FWSActionRequest Request;
	Request.ActionId = ActionId;
	const FWSActionRequirementReport MechanicalReport =
		RulesEngine.EvaluateActionRequirements(Request);
	FWSActionRequest DialogueContextRequest;
	DialogueContextRequest.ActionId = TEXT("talk_gu_heng");
	DialogueContextRequest.DialogueAct = EWSDialogueAct::Ask;
	DialogueContextRequest.SemanticFrame.SpeechAct = EWSDialogueAct::Ask;
	DialogueContextRequest.SemanticFrame.QueryType =
		EWSDialogueQueryType::Requirements;
	DialogueContextRequest.SemanticFrame.TargetActionId = ActionId;
	DialogueContextRequest.SemanticFrame.TargetCharacter =
		EWSCharacterId::GuHeng;
	return UWSNPCDecisionService::ResolveRequirementVisibility(
		MechanicalReport,
		UWSNPCDecisionService::BuildDisclosureContext(
			DialogueContextRequest,
			EWSCharacterId::GuHeng,
			RulesEngine.GetState()));
}

FWSActionResult UWindStationStateSubsystem::CommitAction(const FWSActionRequest& Request)
{
	if (bLifecycleTransitionActive || bCommitDispatchActive)
	{
		FWSActionResult Result;
		Result.ActionId = Request.ActionId;
		Result.TransactionId = Request.TransactionId.IsValid()
			? Request.TransactionId
			: FGuid::NewGuid();
		Result.DialogueAct = Request.DialogueAct;
		Result.PromiseCondition = Request.PromiseCondition;
		Result.APBefore = RulesEngine.GetState().ActionPoints;
		Result.APAfter = Result.APBefore;
		Result.ReasonCode = EWSReasonCode::DialogueCancelled;
		return Result;
	}
	if (Request.ActionId == TEXT("talk_gu_heng")
		|| Request.ActionId == TEXT("talk_ye_cheng"))
	{
		return SubmitDialogueAction(Request);
	}
	EWSHeatingZone HeatingZone = EWSHeatingZone::None;
	if (RulesEngine.IsV11()
		&& HeatingZoneForAction(Request.ActionId, HeatingZone))
	{
		FWSActionResult Result;
		Result.ActionId = Request.ActionId;
		Result.TransactionId = Request.TransactionId.IsValid()
			? Request.TransactionId
			: FGuid::NewGuid();
		Result.APBefore = RulesEngine.GetState().ActionPoints;
		Result.APAfter = Result.APBefore;
		Result.BaseAP = 0;
		Result.ActualAP = 0;
		Result.WorkReadiness = EWSWorkReadiness::Ready;
		Result.bCommitted = RulesEngine.BeginDayPhase(
			HeatingZone,
			Result.ReasonCode,
			Result.Changes);
		if (Result.bCommitted)
		{
			Result.APAfter = RulesEngine.GetState().ActionPoints;
			++StateRevision;
			SaveSnapshot();
			OnActionCommitted.Broadcast(Result);
			BroadcastState();
		}
		return Result;
	}
	TSet<FName> ActiveOfferIdsBeforeCommit;
	for (const FWSNegotiationOffer& Offer : RulesEngine.GetState().NegotiationOffers)
	{
		if (Offer.bAccepted && !Offer.bFulfilled && !Offer.bBroken)
		{
			ActiveOfferIdsBeforeCommit.Add(Offer.OfferId);
		}
	}
	FWSActionResult Result = RulesEngine.Commit(Request);
	if (Result.bCommitted)
	{
		for (const FWSNegotiationOffer& Offer : RulesEngine.GetState().NegotiationOffers)
		{
			if (ActiveOfferIdsBeforeCommit.Contains(Offer.OfferId) && Offer.bFulfilled)
			{
				AppendOfferAudit(
					TEXT("fulfilled"),
					Request.ActionId,
					&Offer,
					RulesEngine.GetState());
			}
		}
		++StateRevision;
		const int64 CommittedRevision = StateRevision;
		SaveSnapshot();
		OnActionCommitted.Broadcast(Result);
		BroadcastState();
		FWSActionRequest CommittedRequest = Request;
		CommittedRequest.TransactionId = Result.TransactionId;
		if (StateRevision == CommittedRevision)
		{
			RequestActionExpression(CommittedRequest);
		}
	}
	return Result;
}

bool UWindStationStateSubsystem::CanCommitPreparedDialogue(
	const FWSPreparedDialogue& Candidate,
	const FWSPreparedDialogue& Pending,
	const int64 CurrentStateRevision,
	const int64 CurrentGeneration,
	const TArray<FGuid>& CommittedTransactions)
{
	return Candidate.TransactionId.IsValid()
		&& Candidate.TransactionId == Pending.TransactionId
		&& Candidate.OriginalRequest.ActionId == Pending.OriginalRequest.ActionId
		&& Candidate.OriginalRequest.DialogueSessionId
			== Pending.OriginalRequest.DialogueSessionId
		&& Candidate.StateRevision == Pending.StateRevision
		&& Candidate.StateRevision == CurrentStateRevision
		&& Candidate.Generation == Pending.Generation
		&& Candidate.Generation == CurrentGeneration
		&& !CommittedTransactions.Contains(Candidate.TransactionId);
}

#if WITH_DEV_AUTOMATION_TESTS
void UWindStationStateSubsystem::SetDialogueRealizeTestHook(
	FWSDialogueRealizeTestHook Hook)
{
	DialogueRealizeTestHook = MoveTemp(Hook);
}

void UWindStationStateSubsystem::SetDialogueCommitDispatchTestHook(
	FWSDialogueCommitDispatchTestHook Hook)
{
	DialogueCommitDispatchTestHook = MoveTemp(Hook);
}

void UWindStationStateSubsystem::SetAutomationSaveSlot(FString InSaveSlot)
{
	AutomationSaveSlot = MoveTemp(InSaveSlot);
}

void UWindStationStateSubsystem::SetDialogueAuditPathForTest(FString InPath)
{
	DialogueAuditPathForTest = MoveTemp(InPath);
}

void UWindStationStateSubsystem::SetEventLogExportPathForTest(FString InPath)
{
	EventLogExportPathForTest = MoveTemp(InPath);
}
#endif

FWSActionResult UWindStationStateSubsystem::SubmitDialogueAction(
	const FWSActionRequest& Request,
	TFunction<void(const FWSActionResult&)> Completion)
{
	FWSActionRequest NormalizedRequest = Request;
	if (!NormalizedRequest.TransactionId.IsValid())
	{
		NormalizedRequest.TransactionId = FGuid::NewGuid();
	}
	EWSReasonCode SessionReason = EWSReasonCode::Ok;
	if (!NormalizeDialogueSessionRequest(NormalizedRequest, SessionReason))
	{
		FWSActionResult Rejected;
		Rejected.ActionId = NormalizedRequest.ActionId;
		Rejected.TransactionId = NormalizedRequest.TransactionId;
		Rejected.DialogueAct = NormalizedRequest.DialogueAct;
		Rejected.PromiseCondition = NormalizedRequest.PromiseCondition;
		Rejected.APBefore = RulesEngine.GetState().ActionPoints;
		Rejected.APAfter = Rejected.APBefore;
		Rejected.ReasonCode = SessionReason;
		CompleteDialogueSubmission(Rejected, MoveTemp(Completion));
		return Rejected;
	}
	const TSharedRef<TOptional<FWSActionResult>> SynchronousResult =
		MakeShared<TOptional<FWSActionResult>>();
	TFunction<void(const FWSActionResult&)> CapturingCompletion =
		[SynchronousResult, UserCompletion = MoveTemp(Completion)](
			const FWSActionResult& CompletedResult) mutable
		{
			*SynchronousResult = CompletedResult;
			if (UserCompletion)
			{
				UserCompletion(CompletedResult);
			}
		};
	FWSActionResult Result = PrepareDialogue(NormalizedRequest);
	if (!Result.bPendingDialogue)
	{
		CompleteDialogueSubmission(Result, MoveTemp(CapturingCompletion));
		return Result;
	}

	PendingDialogueCompletion = MoveTemp(CapturingCompletion);
	RealizePreparedDialogue();
	if (SynchronousResult->IsSet())
	{
		Result = SynchronousResult->GetValue();
	}
	return Result;
}

FWSActionResult UWindStationStateSubsystem::PrepareDialogue(
	const FWSActionRequest& ActionRequest)
{
	FWSActionResult Result;
	Result.ActionId = ActionRequest.ActionId;
	Result.TransactionId = ActionRequest.TransactionId;
	Result.DialogueAct = ActionRequest.DialogueAct;
	Result.PromiseCondition = ActionRequest.PromiseCondition;
	Result.APBefore = RulesEngine.GetState().ActionPoints;
	Result.APAfter = Result.APBefore;
	if (bCommitDispatchActive || bLifecycleTransitionActive)
	{
		Result.ReasonCode = EWSReasonCode::DialogueCancelled;
		return Result;
	}
	if (ActionRequest.ActionId != TEXT("talk_gu_heng")
		&& ActionRequest.ActionId != TEXT("talk_ye_cheng"))
	{
		Result.ReasonCode = EWSReasonCode::DialogueOutcomeRequired;
		return Result;
	}
	if (bHasPendingDialogue)
	{
		Result.ReasonCode = PendingDialogue.TransactionId == ActionRequest.TransactionId
			? EWSReasonCode::DuplicateTransaction
			: EWSReasonCode::DialoguePending;
		return Result;
	}
	if (RulesEngine.GetState().CommittedTransactions.Contains(
		ActionRequest.TransactionId))
	{
		Result.ReasonCode = EWSReasonCode::DuplicateTransaction;
		return Result;
	}

	const FWSActionPreview Preview = RulesEngine.Preview(ActionRequest);
	Result.ReasonCode = Preview.ReasonCode;
	Result.BaseAP = Preview.BaseAP;
	Result.ActualAP = Preview.APCost;
	Result.CostModifiers = Preview.CostModifiers;
	Result.WorkReadiness = Preview.WorkReadiness;
	if (!Preview.bCanExecute)
	{
		return Result;
	}

	FWSPreparedDialogue Prepared;
	Prepared.TransactionId = ActionRequest.TransactionId;
	Prepared.StateRevision = StateRevision;
	Prepared.Generation = ++DialogueGeneration;
	Prepared.OriginalRequest = ActionRequest;
	Prepared.ReadSnapshot = RulesEngine.GetState();
	Prepared.APCost = Preview.APCost;

	if (!RoleplayRepository || !RoleplayRepository->IsAvailable())
	{
		Result.ReasonCode = EWSReasonCode::DialogueOutcomeInvalid;
		return Result;
	}
	FWSRoleplayFallback RoleplayFallback;
	FString RoleplayError;
	if (!UWSNPCContextBuilder::BuildRequest(
			ActionRequest,
			Prepared.ReadSnapshot,
			*RoleplayRepository,
			Prepared.ReadSnapshot.DialogueMemories,
			ActionRequest.DialogueTurnIndex,
			Prepared.RoleplayRequest,
			RoleplayFallback,
			RoleplayError))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Whiteout v1.4 context build failed: %s"),
			*RoleplayError);
		Result.ReasonCode = EWSReasonCode::DialogueOutcomeInvalid;
		return Result;
	}
	Prepared.bRoleplayV14 = true;
	Prepared.Contract.PersonaStyleId =
		Prepared.RoleplayRequest.SpeakerId.ToString();
	Prepared.Contract.MaxSentences =
		Prepared.RoleplayRequest.ResponsePolicy.MaxSentences;
	Prepared.Contract.MaxCharacters =
		Prepared.RoleplayRequest.ResponsePolicy.MaxCharacters;
	Prepared.Contract.ForbiddenFactIds =
		Prepared.RoleplayRequest.ForbiddenFactIds;
	for (const FWSRoleplayKnowledgeItem& Knowledge :
		Prepared.RoleplayRequest.AvailableKnowledge)
	{
		if (Knowledge.bCreatesGameFact && !Knowledge.GameFactId.IsNone())
		{
			Prepared.AllowedFactIds.AddUnique(Knowledge.GameFactId);
		}
	}

	FWSAgentReply& Fallback = Prepared.LocalFallback;
	Fallback.Speaker = ActionRequest.ActionId == TEXT("talk_ye_cheng")
		? EWSCharacterId::YeCheng
		: EWSCharacterId::GuHeng;
	Fallback.ActionId = ActionRequest.ActionId;
	Fallback.TransactionId = ActionRequest.TransactionId;
	Fallback.DialogueSessionId = ActionRequest.DialogueSessionId;
	Fallback.DialogueTurnIndex = ActionRequest.DialogueTurnIndex;
	Fallback.DialogueSessionMaxTurns =
		ActionRequest.DialogueSessionMaxTurns;
	Fallback.ResponseType = EWSResponseType::Deflect;
	Fallback.Utterance = RoleplayFallback.Line;
	Fallback.SpeechFunction = RoleplayFallback.SpeechFunction;
	Fallback.ReferencedKnowledgeIds =
		RoleplayFallback.ReferencedKnowledgeIds;
	Fallback.MemorySummary = RoleplayFallback.Line.Left(160);
	Fallback.SemanticFrame = ActionRequest.SemanticFrame;
	Fallback.Emotion = Fallback.Speaker == EWSCharacterId::YeCheng
		? TEXT("clinical")
		: TEXT("guarded");
	Fallback.AnswerSource = TEXT("local_natural_fallback");
	Fallback.Provider = TEXT("preset");
	Fallback.ValidationReason = TEXT("local_natural_fallback");
	Fallback.bFallback = true;

	FWSDialogueOutcome SimulationOutcome;
	SimulationOutcome.FinalReply = Prepared.LocalFallback;
	SimulationOutcome.DisclosedFactIds =
		Prepared.LocalFallback.DisclosedFactIds;
	SimulationOutcome.RealizedAtomIds =
		Prepared.LocalFallback.RealizedAtomIds;
	SimulationOutcome.AnswerSource =
		Prepared.LocalFallback.AnswerSource;
	FString FallbackValidationReason;
	if (!UWSAgentGateway::ValidateDialogueOutcome(
			Prepared,
			SimulationOutcome,
			FallbackValidationReason))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Whiteout v1.4 fallback validation failed: %s"),
			*FallbackValidationReason);
		Result.ReasonCode = EWSReasonCode::DialogueOutcomeInvalid;
		return Result;
	}
	FWhiteoutRulesEngine Simulation = RulesEngine;
	const FWSActionResult SimulationResult =
		Simulation.CommitDialogueOutcome(Prepared, SimulationOutcome);
	if (SimulationResult.bCommitted)
	{
		const EWSCharacterId Speaker = Prepared.LocalFallback.Speaker;
		const FWSCharacterState Before =
			Prepared.ReadSnapshot.Characters.FindRef(Speaker);
		const FWSCharacterState After =
			Simulation.GetState().Characters.FindRef(Speaker);
		Prepared.PlannedTrustDelta = After.Trust - Before.Trust;
		Prepared.PlannedPressureDelta = After.Pressure - Before.Pressure;
	}

	PendingDialogue = MoveTemp(Prepared);
	bHasPendingDialogue = true;
	Result.bPendingDialogue = true;
	Result.ReasonCode = EWSReasonCode::Ok;
	return Result;
}

void UWindStationStateSubsystem::RealizePreparedDialogue()
{
	if (!bHasPendingDialogue)
	{
		return;
	}
	const FGuid TransactionId = PendingDialogue.TransactionId;
	const int64 Generation = PendingDialogue.Generation;
#if WITH_DEV_AUTOMATION_TESTS
	if (DialogueRealizeTestHook)
	{
		const FWSPreparedDialogue Prepared = PendingDialogue;
		TWeakObjectPtr<UWindStationStateSubsystem> WeakThis(this);
		DialogueRealizeTestHook(
			Prepared,
			[WeakThis, TransactionId, Generation](const FWSAgentReply& Reply)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandlePreparedDialogueReply(
						Reply,
						TransactionId,
						Generation);
				}
			});
		return;
	}
#endif
	const bool bLiveProviderEligible =
		LLMConfigurationError.IsEmpty()
		&& AgentGateway
		&& AgentGateway->HasLiveProvider()
		&& RulesEngine.GetState().ModelCalls
			< RulesEngine.GetConfig().ModelCallHardLimit;
	const bool bUseLiveProvider =
		bLiveProviderEligible && RulesEngine.TryRecordModelCall();
	PendingDialogue.bModelCallAttempted = bUseLiveProvider;
	if (bUseLiveProvider)
	{
		if (!bHasPendingDialogue
			|| PendingDialogue.TransactionId != TransactionId
			|| PendingDialogue.Generation != Generation)
		{
			return;
		}
		if (PendingDialogue.StateRevision != StateRevision)
		{
			HandlePreparedDialogueReply(
				PendingDialogue.LocalFallback,
				TransactionId,
				Generation);
			return;
		}
	}
	if (!AgentGateway)
	{
		HandlePreparedDialogueReply(
			PendingDialogue.LocalFallback,
			TransactionId,
			Generation);
		return;
	}

	const FWSPreparedDialogue Prepared = PendingDialogue;
	TWeakObjectPtr<UWindStationStateSubsystem> WeakThis(this);
	AgentGateway->RequestDialogueRealization(
		Prepared,
		bUseLiveProvider,
		FWSDialogueOutcomeCallback::CreateLambda(
			[WeakThis, TransactionId, Generation](const FWSDialogueOutcome& Outcome)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandlePreparedDialogueOutcome(
						Outcome,
						TransactionId,
						Generation);
				}
			}));
	if (bUseLiveProvider
		&& bHasPendingDialogue
		&& PendingDialogue.TransactionId == TransactionId
		&& PendingDialogue.Generation == Generation)
	{
		SaveSnapshot();
		BroadcastState();
	}
}

void UWindStationStateSubsystem::HandlePreparedDialogueReply(
	const FWSAgentReply& Reply,
	const FGuid TransactionId,
	const int64 Generation)
{
	FWSDialogueOutcome Outcome;
	Outcome.FinalReply = Reply;
	Outcome.DisclosedFactIds = Reply.DisclosedFactIds;
	Outcome.RealizedAtomIds = Reply.RealizedAtomIds;
	Outcome.AnswerSource = Reply.AnswerSource;
	HandlePreparedDialogueOutcome(Outcome, TransactionId, Generation);
}

void UWindStationStateSubsystem::HandlePreparedDialogueOutcome(
	const FWSDialogueOutcome& RealizedOutcome,
	const FGuid TransactionId,
	const int64 Generation)
{
	if (!bHasPendingDialogue
		|| PendingDialogue.TransactionId != TransactionId
		|| PendingDialogue.Generation != Generation)
	{
		return;
	}

	const FWSPreparedDialogue Prepared = PendingDialogue;
	FWSDialogueOutcome Outcome = RealizedOutcome;
	Outcome.ValidationOutcome = NormalizeValidationOutcome(
		Outcome.ValidationOutcome,
		Outcome.FinalReply.bFallback,
		Outcome.FinalReply.ValidationReason);
	FString ValidationReason;
	if (!UWSAgentGateway::ValidateDialogueOutcome(
			Prepared,
			Outcome,
			ValidationReason))
	{
		const FString Provider = Outcome.FinalReply.Provider;
		const FString FailureReason = Outcome.FinalReply.ValidationReason;
		Outcome.FinalReply = Prepared.LocalFallback;
		Outcome.FinalReply.Provider = Provider;
		Outcome.FinalReply.ValidationReason = FailureReason.IsEmpty()
			? TEXT("prepared_outcome_validation_failed")
			: FailureReason;
		Outcome.FinalReply.bFallback = true;
		Outcome.DisclosedFactIds =
			Prepared.LocalFallback.DisclosedFactIds;
		Outcome.RealizedAtomIds =
			Prepared.LocalFallback.RealizedAtomIds;
		Outcome.AnswerSource =
			Prepared.LocalFallback.AnswerSource;
		Outcome.ValidationOutcome =
			TEXT("fallback_prepared_outcome_validation_failed");
	}
	Outcome.FinalReply.PlannedDisclosureFacts =
		Prepared.PlannedDisclosureFacts;

	FWSActionResult Result;
	if (!CommitDialogueOutcome(Prepared, Outcome, Result))
	{
		TFunction<void(const FWSActionResult&)> Completion =
			MoveTemp(PendingDialogueCompletion);
		bHasPendingDialogue = false;
		PendingDialogue = FWSPreparedDialogue();
		++DialogueGeneration;
		if (Result.ReasonCode == EWSReasonCode::DialogueStateChanged)
		{
			FWSAgentReply RetryReply = Prepared.LocalFallback;
			RetryReply.Utterance = TEXT("情况刚刚有变化。按现在的状态再问一次。");
			RetryReply.SemanticSpine = RetryReply.Utterance;
			RetryReply.PersonaTail.Reset();
			RetryReply.ReferencedFactIds.Reset();
			RetryReply.PlannedDisclosureFacts.Reset();
			RetryReply.DisclosedFactIds.Reset();
			RetryReply.AnswerSource = TEXT("stale_retry");
			RetryReply.Provider = TEXT("preset");
			RetryReply.ValidationReason = TEXT("state_revision_changed");
			LatestDialogue = RetryReply;
			BroadcastDialogueLine(LatestDialogue);
		}
		CompleteDialogueSubmission(Result, MoveTemp(Completion));
		return;
	}
	if (!AppendDialogueAudit(Prepared, Outcome))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Failed to append committed dialogue audit for %s"),
			*Prepared.TransactionId.ToString(EGuidFormats::DigitsWithHyphens));
	}

	TFunction<void(const FWSActionResult&)> Completion =
		MoveTemp(PendingDialogueCompletion);
	bHasPendingDialogue = false;
	PendingDialogue = FWSPreparedDialogue();
	++DialogueGeneration;
	++StateRevision;
	const FWSAgentReply CommittedReply = Outcome.FinalReply;
	LatestDialogue = CommittedReply;
	RecordCommittedDialogueSession(Prepared.OriginalRequest);
	if (AgentGateway)
	{
		AgentGateway->RecordCommittedDialogueTurn(
			Prepared.OriginalRequest,
			Outcome.FinalReply);
	}
	SaveSnapshot();
	const int64 BroadcastGeneration = DialogueGeneration;
	const int64 CommittedRevision = StateRevision;
	{
		TGuardValue<bool> DispatchGuard(bCommitDispatchActive, true);
		OnActionCommitted.Broadcast(Result);
#if WITH_DEV_AUTOMATION_TESTS
		if (DialogueCommitDispatchTestHook)
		{
			DialogueCommitDispatchTestHook();
		}
#endif
		BroadcastDialogueLine(CommittedReply);
		if (DialogueGeneration == BroadcastGeneration
			&& StateRevision == CommittedRevision)
		{
			BroadcastState();
		}
	}
	CompleteDialogueSubmission(Result, MoveTemp(Completion));
}

bool UWindStationStateSubsystem::CommitDialogueOutcome(
	const FWSPreparedDialogue& Prepared,
	const FWSDialogueOutcome& Outcome,
	FWSActionResult& OutResult)
{
	if (!bHasPendingDialogue
		|| !CanCommitPreparedDialogue(
			Prepared,
			PendingDialogue,
			StateRevision,
			DialogueGeneration,
			RulesEngine.GetState().CommittedTransactions))
	{
		OutResult.ActionId = Prepared.OriginalRequest.ActionId;
		OutResult.TransactionId = Prepared.TransactionId;
		OutResult.DialogueAct = Prepared.OriginalRequest.DialogueAct;
		OutResult.PromiseCondition = Prepared.OriginalRequest.PromiseCondition;
		OutResult.APBefore = RulesEngine.IsV11()
			? RulesEngine.GetState().PhaseActionPoints
			: RulesEngine.GetState().ActionPoints;
		OutResult.APAfter = OutResult.APBefore;
		OutResult.ReasonCode = RulesEngine.GetState().CommittedTransactions.Contains(
			Prepared.TransactionId)
			? EWSReasonCode::DuplicateTransaction
			: EWSReasonCode::DialogueStateChanged;
		return false;
	}
	OutResult = RulesEngine.CommitDialogueOutcome(Prepared, Outcome);
	return OutResult.bCommitted;
}

FString UWindStationStateSubsystem::GetDialogueAuditPath() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (!DialogueAuditPathForTest.IsEmpty())
	{
		return DialogueAuditPathForTest;
	}
	if (GIsAutomationTesting)
	{
		return FPaths::ProjectSavedDir()
			/ TEXT("Automation/WhiteoutStation_DialogueAudit.jsonl");
	}
#endif
	return FPaths::ProjectSavedDir()
		/ TEXT("Logs/WhiteoutStation_DialogueAudit.jsonl");
}

bool UWindStationStateSubsystem::AppendDialogueAudit(
	const FWSPreparedDialogue& Prepared,
	const FWSDialogueOutcome& Outcome) const
{
	TArray<FName> RequiredAtomIds;
	RequiredAtomIds.Reserve(Prepared.Contract.MustRealize.Num());
	for (const FWSDialogueSemanticAtom& Atom : Prepared.Contract.MustRealize)
	{
		RequiredAtomIds.Add(Atom.AtomId);
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("kind"), TEXT("dialogue_expression"));
	Root->SetStringField(
		TEXT("transaction_id"),
		Prepared.TransactionId.ToString(EGuidFormats::DigitsWithHyphens).ToLower());
	Root->SetStringField(
		TEXT("speaker"),
		DialogueSpeakerId(Outcome.FinalReply.Speaker));
	Root->SetStringField(
		TEXT("query_type"),
		StaticEnum<EWSDialogueQueryType>()->GetNameStringByValue(
			static_cast<int64>(Prepared.OriginalRequest.SemanticFrame.QueryType)).ToLower());
	const FName TargetActionId =
		Prepared.OriginalRequest.SemanticFrame.TargetActionId.IsNone()
			? Prepared.OriginalRequest.ActionId
			: Prepared.OriginalRequest.SemanticFrame.TargetActionId;
	Root->SetStringField(TEXT("target_action_id"), TargetActionId.ToString());
	Root->SetArrayField(
		TEXT("planned_disclosure_fact_ids"),
		NameIdArray(Prepared.PlannedDisclosureFacts));
	Root->SetArrayField(
		TEXT("final_disclosed_fact_ids"),
		NameIdArray(Outcome.DisclosedFactIds));
	Root->SetArrayField(
		TEXT("required_atom_ids"),
		NameIdArray(RequiredAtomIds));
	Root->SetArrayField(
		TEXT("realized_atom_ids"),
		NameIdArray(Outcome.RealizedAtomIds));
	Root->SetArrayField(
		TEXT("referenced_knowledge_ids"),
		NameIdArray(Outcome.FinalReply.ReferencedKnowledgeIds));
	Root->SetStringField(
		TEXT("speech_function"),
		StaticEnum<EWSRoleplaySpeechFunction>()->GetNameStringByValue(
			static_cast<int64>(Outcome.FinalReply.SpeechFunction)).ToLower());
	Root->SetNumberField(
		TEXT("turn_index"),
		Prepared.OriginalRequest.DialogueTurnIndex);
	Root->SetStringField(
		TEXT("proposal_type"),
		StaticEnum<EWSRoleplayProposalType>()->GetNameStringByValue(
			static_cast<int64>(
				Outcome.FinalReply.ProposedAction.Type)).ToLower());
	const FString AnswerSource = !Outcome.FinalReply.bFallback
		? Outcome.AnswerSource
		: TEXT("local_natural_fallback");
	Root->SetStringField(TEXT("answer_source"), AnswerSource);
	Root->SetStringField(
		TEXT("validation_outcome"),
		NormalizeValidationOutcome(
			Outcome.ValidationOutcome,
			Outcome.FinalReply.bFallback,
			Outcome.FinalReply.ValidationReason));
	Root->SetNumberField(TEXT("prompt_tokens"), Outcome.PromptTokens);
	Root->SetNumberField(TEXT("completion_tokens"), Outcome.CompletionTokens);

	FString Json;
	const TSharedRef<
		TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<
			TCHAR,
			TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}
	const FString AuditPath = GetDialogueAuditPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(AuditPath), true);
	return FFileHelper::SaveStringToFile(
		Json + LINE_TERMINATOR,
		*AuditPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(),
		FILEWRITE_Append);
}

void UWindStationStateSubsystem::AbortPendingDialogue(
	const EWSReasonCode Reason,
	const bool bNotifyCompletion,
	const bool bResetGateway)
{
	TFunction<void(const FWSActionResult&)> Completion;
	FWSActionResult Result;
	if (bHasPendingDialogue)
	{
		Result.ActionId = PendingDialogue.OriginalRequest.ActionId;
		Result.TransactionId = PendingDialogue.TransactionId;
		Result.DialogueAct = PendingDialogue.OriginalRequest.DialogueAct;
		Result.PromiseCondition = PendingDialogue.OriginalRequest.PromiseCondition;
		Result.APBefore = RulesEngine.GetState().ActionPoints;
		Result.APAfter = Result.APBefore;
		Result.ReasonCode = Reason;
		Completion = MoveTemp(PendingDialogueCompletion);
	}
	bHasPendingDialogue = false;
	PendingDialogue = FWSPreparedDialogue();
	PendingDialogueCompletion = {};
	++DialogueGeneration;
	if (bResetGateway && AgentGateway)
	{
		AgentGateway->ResetSession();
	}
	if (bNotifyCompletion && Completion)
	{
		CompleteDialogueSubmission(Result, MoveTemp(Completion));
	}
}

void UWindStationStateSubsystem::CompleteDialogueSubmission(
	const FWSActionResult& Result,
	TFunction<void(const FWSActionResult&)> Completion)
{
	if (Completion)
	{
		Completion(Result);
	}
}

void UWindStationStateSubsystem::BroadcastDialogueLine(
	const FWSAgentReply& Reply)
{
#if WITH_DEV_AUTOMATION_TESTS
	++DialogueLineBroadcastCountForTest;
#endif
	OnDialogueLine.Broadcast(Reply);
}

bool UWindStationStateSubsystem::BeginDayPhase(
	const EWSHeatingZone HeatingZone,
	EWSReasonCode& OutReason,
	TArray<FString>& OutChanges)
{
	if (bLifecycleTransitionActive || bCommitDispatchActive)
	{
		OutReason = EWSReasonCode::DialogueCancelled;
		OutChanges.Reset();
		return false;
	}
	const bool bStarted =
		RulesEngine.BeginDayPhase(HeatingZone, OutReason, OutChanges);
	if (bStarted)
	{
		DialogueSessions.Reset();
		++StateRevision;
		SaveSnapshot();
		BroadcastState();
	}
	return bStarted;
}

bool UWindStationStateSubsystem::SettleCurrentDayPhase(
	EWSReasonCode& OutReason,
	FWSPhaseSummary& OutSummary)
{
	if (bLifecycleTransitionActive || bCommitDispatchActive)
	{
		OutReason = EWSReasonCode::DialogueCancelled;
		OutSummary = FWSPhaseSummary();
		return false;
	}
	TSet<FName> ActiveOfferIdsBeforeSettlement;
	for (const FWSNegotiationOffer& Offer : RulesEngine.GetState().NegotiationOffers)
	{
		if (Offer.bAccepted && !Offer.bFulfilled && !Offer.bBroken)
		{
			ActiveOfferIdsBeforeSettlement.Add(Offer.OfferId);
		}
	}
	const bool bSettled =
		RulesEngine.SettleDayPhase(OutReason, OutSummary);
	if (bSettled)
	{
		DialogueSessions.Reset();
		for (const FWSNegotiationOffer& Offer : RulesEngine.GetState().NegotiationOffers)
		{
			if (ActiveOfferIdsBeforeSettlement.Contains(Offer.OfferId) && Offer.bBroken)
			{
				AppendOfferAudit(
					TEXT("broken"),
					Offer.TargetActionId,
					&Offer,
					RulesEngine.GetState());
			}
		}
		++StateRevision;
		SaveSnapshot();
		BroadcastState();
	}
	return bSettled;
}

FWSGameState UWindStationStateSubsystem::EndGame()
{
	if (bLifecycleTransitionActive || bCommitDispatchActive)
	{
		return RulesEngine.GetState();
	}
	TGuardValue<bool> LifecycleGuard(bLifecycleTransitionActive, true);
	AbortPendingDialogue(
		EWSReasonCode::DialogueCancelled,
		true,
		true);
	RulesEngine.EndGame();
	++StateRevision;
	SaveSnapshot();
	BroadcastState();
	return RulesEngine.GetState();
}

bool UWindStationStateSubsystem::SaveSnapshot()
{
	UWindStationSaveGame* Save = Cast<UWindStationSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UWindStationSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}
	Save->State = RulesEngine.GetState();
	return UGameplayStatics::SaveGameToSlot(Save, GetActiveSaveSlot(), 0);
}

FWSGameState UWindStationStateSubsystem::MigrateSaveStateForV13(
	const FWSGameState& SourceState,
	const FString& SourceSaveVersion,
	const int32 TargetRulesSchemaVersion,
	const FString& TargetRulesVersion)
{
	FWSGameState MigratedState = SourceState;
	if (SourceSaveVersion != TEXT("1.4.0")
		&& SourceSaveVersion != TEXT("1.3.0"))
	{
		if (MigratedState.Flags.bHeatPackRevealed)
		{
			MigratedState.PlayerKnowledge.FindOrAdd(
				TEXT("FACT_HEAT_PACK")) = EWSKnowledgeLevel::Confirmed;
		}
		if (MigratedState.Flags.bGuHengDiagnosed)
		{
			MigratedState.PlayerKnowledge.FindOrAdd(
				TEXT("FACT_HAND_INJURY")) = EWSKnowledgeLevel::Confirmed;
			MigratedState.PlayerKnowledge.FindOrAdd(
				TEXT("FACT_MEDICAL_DIAGNOSIS")) = EWSKnowledgeLevel::Confirmed;
		}
		if (MigratedState.Flags.bRelayCompatibilityKnown)
		{
			MigratedState.PlayerKnowledge.FindOrAdd(
				TEXT("FACT_RELAY_COMPATIBILITY")) = EWSKnowledgeLevel::Confirmed;
		}
	}
	MigratedState.RulesSchemaVersion = TargetRulesSchemaVersion;
	MigratedState.RulesVersion = TargetRulesVersion;
	return MigratedState;
}

bool UWindStationStateSubsystem::LoadSnapshot()
{
	if (bLifecycleTransitionActive || bCommitDispatchActive)
	{
		return false;
	}
	const FString& ActiveSaveSlot = GetActiveSaveSlot();
	FString SlotToLoad = ActiveSaveSlot;
	bool bAllowLegacyFallback = true;
#if WITH_DEV_AUTOMATION_TESTS
	bAllowLegacyFallback = AutomationSaveSlot.IsEmpty();
#endif
	if (bAllowLegacyFallback
		&& !UGameplayStatics::DoesSaveGameExist(SlotToLoad, 0))
	{
		SlotToLoad = UGameplayStatics::DoesSaveGameExist(LegacySaveSlotV13, 0)
			? LegacySaveSlotV13
			: UGameplayStatics::DoesSaveGameExist(LegacySaveSlotV12, 0)
				? LegacySaveSlotV12
				: LegacySaveSlotV11;
	}
	const bool bLoadLegacySlot = SlotToLoad != ActiveSaveSlot;
	if (!UGameplayStatics::DoesSaveGameExist(SlotToLoad, 0))
	{
		return false;
	}
	UWindStationSaveGame* Save = Cast<UWindStationSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotToLoad, 0));
	if (!Save
		|| (Save->SaveVersion != TEXT("1.4.0")
			&& Save->SaveVersion != TEXT("1.3.0")
			&& Save->SaveVersion != TEXT("1.2.0")
			&& Save->SaveVersion != TEXT("1.1.0")))
	{
		return false;
	}
	TGuardValue<bool> LifecycleGuard(bLifecycleTransitionActive, true);
	AbortPendingDialogue(
		EWSReasonCode::DialogueCancelled,
		true,
		true);
	const FWSGameState MigratedState = MigrateSaveStateForV13(
		Save->State,
		Save->SaveVersion,
		RulesEngine.GetConfig().SchemaVersion,
		RulesEngine.GetConfig().RulesVersion);
	RulesEngine.SetState(MigratedState);
	DialogueSessions.Reset();
	++StateRevision;
	LatestDialogue = FWSAgentReply();
	if (bLoadLegacySlot || Save->SaveVersion != TEXT("1.4.0"))
	{
		SaveSnapshot();
	}
	BroadcastState();
	return true;
}

bool UWindStationStateSubsystem::HasSnapshot() const
{
	const FString& ActiveSaveSlot = GetActiveSaveSlot();
#if WITH_DEV_AUTOMATION_TESTS
	if (!AutomationSaveSlot.IsEmpty())
	{
		return UGameplayStatics::DoesSaveGameExist(ActiveSaveSlot, 0);
	}
#endif
	return UGameplayStatics::DoesSaveGameExist(ActiveSaveSlot, 0)
		|| UGameplayStatics::DoesSaveGameExist(LegacySaveSlotV13, 0)
		|| UGameplayStatics::DoesSaveGameExist(LegacySaveSlotV12, 0)
		|| UGameplayStatics::DoesSaveGameExist(LegacySaveSlotV11, 0);
}

const FString& UWindStationStateSubsystem::GetActiveSaveSlot() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (!AutomationSaveSlot.IsEmpty())
	{
		return AutomationSaveSlot;
	}
#endif
	return SaveSlot;
}

bool UWindStationStateSubsystem::ExportEventLog(FString& OutFilePath) const
{
	TArray<TSharedPtr<FJsonValue>> Events;
	for (const FWSEventRecord& Event : RulesEngine.GetState().EventLog)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("index"), Event.Index);
		Object->SetStringField(TEXT("action_id"), Event.ActionId.ToString());
		Object->SetStringField(TEXT("transaction_id"), Event.TransactionId.ToString());
		Object->SetNumberField(TEXT("ap_before"), Event.APBefore);
		Object->SetNumberField(TEXT("ap_after"), Event.APAfter);
		Object->SetStringField(TEXT("reason_code"), StaticEnum<EWSReasonCode>()->GetNameStringByValue(static_cast<int64>(Event.ReasonCode)));
		Object->SetStringField(
			TEXT("dialogue_act"),
			StaticEnum<EWSDialogueAct>()->GetNameStringByValue(static_cast<int64>(Event.DialogueAct)));
		Object->SetStringField(
			TEXT("promise_condition"),
			Event.PromiseCondition.IsNone() ? TEXT("none") : Event.PromiseCondition.ToString());
		Object->SetBoolField(TEXT("promise_recorded"), Event.bPromiseRecorded);
		Object->SetBoolField(TEXT("crisis_triggered"), Event.bCrisisTriggered);
		Object->SetStringField(
			TEXT("speaker"),
			DialogueSpeakerId(Event.DialogueSpeaker));
		Object->SetArrayField(
			TEXT("planned_disclosure_fact_ids"),
			NameIdArray(Event.PlannedDisclosureFacts));
		Object->SetArrayField(
			TEXT("final_disclosed_fact_ids"),
			NameIdArray(Event.DisclosedFactIds));
		Object->SetArrayField(
			TEXT("realized_atom_ids"),
			NameIdArray(Event.RealizedAtomIds));
		Object->SetStringField(
			TEXT("answer_source"),
			Event.DialogueAnswerSource);
		TArray<TSharedPtr<FJsonValue>> Changes;
		for (const FString& Change : Event.Changes)
		{
			Changes.Add(MakeShared<FJsonValueString>(Change));
		}
		Object->SetArrayField(TEXT("changes"), Changes);
		Events.Add(MakeShared<FJsonValueObject>(Object));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	const FWSGameState& Snapshot = RulesEngine.GetState();
	Root->SetStringField(TEXT("rules_version"), Snapshot.RulesVersion);
	Root->SetArrayField(TEXT("events"), Events);
	Root->SetNumberField(
		TEXT("remaining_ap"),
		RulesEngine.IsV11()
			? Snapshot.PhaseActionPoints
			: Snapshot.ActionPoints);
	Root->SetNumberField(
		TEXT("phase_action_points"),
		Snapshot.PhaseActionPoints);
	Root->SetStringField(
		TEXT("phase"),
		StaticEnum<EWSGamePhase>()->GetNameStringByValue(
			static_cast<int64>(Snapshot.Phase)));
	Root->SetStringField(
		TEXT("day_phase"),
		StaticEnum<EWSDayPhase>()->GetNameStringByValue(
			static_cast<int64>(Snapshot.DayPhase)));
	Root->SetBoolField(
		TEXT("day_phase_started"),
		Snapshot.bDayPhaseStarted);
	Root->SetBoolField(TEXT("day_window_closed"), Snapshot.bDayWindowClosed);
	Root->SetBoolField(
		TEXT("mid_crisis_triggered"),
		Snapshot.bMidCrisisTriggered);
	TSharedRef<FJsonObject> Heating = MakeShared<FJsonObject>();
	Heating->SetStringField(
		TEXT("current_zone"),
		StaticEnum<EWSHeatingZone>()->GetNameStringByValue(
			static_cast<int64>(Snapshot.Heating.CurrentZone)));
	Heating->SetBoolField(TEXT("locked"), Snapshot.Heating.bLocked);
	TArray<TSharedPtr<FJsonValue>> HeatingHistory;
	for (const FWSHeatingSelectionRecord& Selection : Snapshot.Heating.History)
	{
		TSharedRef<FJsonObject> SelectionObject = MakeShared<FJsonObject>();
		SelectionObject->SetStringField(
			TEXT("phase"),
			StaticEnum<EWSDayPhase>()->GetNameStringByValue(
				static_cast<int64>(Selection.Phase)));
		SelectionObject->SetStringField(
			TEXT("zone"),
			StaticEnum<EWSHeatingZone>()->GetNameStringByValue(
				static_cast<int64>(Selection.Zone)));
		HeatingHistory.Add(MakeShared<FJsonValueObject>(SelectionObject));
	}
	Heating->SetArrayField(TEXT("history"), HeatingHistory);
	Root->SetObjectField(TEXT("heating"), Heating);
	Root->SetBoolField(TEXT("signal_sent"), Snapshot.Tasks.bSignalSent);
	Root->SetStringField(TEXT("ending"), StaticEnum<EWSEndingType>()->GetNameStringByValue(static_cast<int64>(Snapshot.Ending)));
	Root->SetNumberField(TEXT("score"), Snapshot.Score.Total);
	Root->SetStringField(TEXT("rating"), Snapshot.Score.Rating);
	Root->SetNumberField(TEXT("model_calls"), Snapshot.ModelCalls);

	TSharedRef<FJsonObject> ScoreBreakdown = MakeShared<FJsonObject>();
	ScoreBreakdown->SetNumberField(TEXT("task_quality"), Snapshot.Score.TaskQuality);
	ScoreBreakdown->SetNumberField(TEXT("people"), Snapshot.Score.People);
	ScoreBreakdown->SetNumberField(
		TEXT("effective_reserves"),
		Snapshot.Score.EffectiveReserves);
	ScoreBreakdown->SetNumberField(
		TEXT("social_stability"),
		Snapshot.Score.SocialStability);
	ScoreBreakdown->SetNumberField(
		TEXT("information_responsibility"),
		Snapshot.Score.InformationResponsibility);
	ScoreBreakdown->SetNumberField(TEXT("total"), Snapshot.Score.Total);
	ScoreBreakdown->SetStringField(TEXT("rating"), Snapshot.Score.Rating);
	Root->SetObjectField(TEXT("score_breakdown"), ScoreBreakdown);

	TSharedRef<FJsonObject> PlayerKnowledge = MakeShared<FJsonObject>();
	TArray<FName> KnowledgeFactIds;
	Snapshot.PlayerKnowledge.GenerateKeyArray(KnowledgeFactIds);
	KnowledgeFactIds.Sort([](const FName Left, const FName Right)
	{
		return Left.ToString().Compare(
			Right.ToString(),
			ESearchCase::CaseSensitive) < 0;
	});
	for (const FName FactId : KnowledgeFactIds)
	{
		const EWSKnowledgeLevel Level = Snapshot.PlayerKnowledge.FindChecked(FactId);
		PlayerKnowledge->SetStringField(
			FactId.ToString(),
			StaticEnum<EWSKnowledgeLevel>()->GetNameStringByValue(
				static_cast<int64>(Level)));
	}
	Root->SetObjectField(TEXT("player_knowledge"), PlayerKnowledge);
	TArray<FName> DisclosedFactIds;
	for (const FWSEventRecord& Event : Snapshot.EventLog)
	{
		for (const FName FactId : Event.DisclosedFactIds)
		{
			DisclosedFactIds.AddUnique(FactId);
		}
	}
	Root->SetArrayField(
		TEXT("disclosed_fact_ids"),
		NameIdArray(DisclosedFactIds));

	TSharedRef<FJsonObject> Resources = MakeShared<FJsonObject>();
	Resources->SetNumberField(TEXT("fuel"), Snapshot.Resources.Fuel);
	Resources->SetNumberField(TEXT("food"), Snapshot.Resources.Food);
	Resources->SetNumberField(TEXT("medicine"), Snapshot.Resources.Medicine);
	Resources->SetNumberField(TEXT("heat_pack"), Snapshot.Resources.HeatPack);
	Resources->SetNumberField(
		TEXT("replacement_relay"),
		Snapshot.Resources.ReplacementRelay);
	Root->SetObjectField(TEXT("resources"), Resources);

	TSharedRef<FJsonObject> RelatedFlags = MakeShared<FJsonObject>();
	RelatedFlags->SetBoolField(
		TEXT("kitchen_heater_intact"),
		Snapshot.Flags.bKitchenHeaterIntact);
	RelatedFlags->SetBoolField(
		TEXT("heat_pack_revealed"),
		Snapshot.Flags.bHeatPackRevealed);
	RelatedFlags->SetBoolField(
		TEXT("repair_room_heated"),
		Snapshot.Flags.bRepairRoomHeated);
	RelatedFlags->SetBoolField(
		TEXT("medical_room_heated"),
		Snapshot.Flags.bMedicalRoomHeated);
	RelatedFlags->SetBoolField(
		TEXT("gu_heng_diagnosed"),
		Snapshot.Flags.bGuHengDiagnosed);
	RelatedFlags->SetBoolField(
		TEXT("gu_heng_treated"),
		Snapshot.Flags.bGuHengTreated);
	RelatedFlags->SetBoolField(TEXT("gu_heng_fed"), Snapshot.Flags.bGuHengFed);
	RelatedFlags->SetBoolField(
		TEXT("gu_heng_cooperative"),
		Snapshot.Flags.bGuHengCooperative);
	RelatedFlags->SetBoolField(
		TEXT("relay_compatibility_known"),
		Snapshot.Flags.bRelayCompatibilityKnown);
	RelatedFlags->SetBoolField(
		TEXT("relay_installed"),
		Snapshot.Flags.bRelayInstalled);
	RelatedFlags->SetBoolField(
		TEXT("self_repair_used"),
		Snapshot.Flags.bSelfRepairUsed);
	RelatedFlags->SetBoolField(
		TEXT("records_preserved"),
		Snapshot.Flags.bRecordsPreserved);
	RelatedFlags->SetBoolField(TEXT("player_fed"), Snapshot.Flags.bPlayerFed);
	RelatedFlags->SetBoolField(
		TEXT("ye_cheng_fed"),
		Snapshot.Flags.bYeChengFed);
	RelatedFlags->SetBoolField(
		TEXT("cabinet_inspected"),
		Snapshot.Flags.bCabinetInspected);
	RelatedFlags->SetBoolField(
		TEXT("log_penalty_active"),
		Snapshot.Flags.bLogPenaltyActive);
	RelatedFlags->SetNumberField(
		TEXT("forced_action_count"),
		Snapshot.Flags.ForcedActionCount);
	RelatedFlags->SetNumberField(
		TEXT("risky_repair_count"),
		Snapshot.Flags.RiskyRepairCount);
	Root->SetObjectField(TEXT("related_flags"), RelatedFlags);

	TSharedRef<FJsonObject> Tasks = MakeShared<FJsonObject>();
	Tasks->SetNumberField(
		TEXT("generator_progress"),
		Snapshot.Tasks.GeneratorProgress);
	Tasks->SetNumberField(
		TEXT("antenna_calibration"),
		Snapshot.Tasks.AntennaCalibration);
	Tasks->SetBoolField(TEXT("signal_sent"), Snapshot.Tasks.bSignalSent);
	Tasks->SetBoolField(
		TEXT("generator_stable"),
		Snapshot.Tasks.bGeneratorStable);
	Root->SetObjectField(TEXT("tasks"), Tasks);

	TSharedRef<FJsonObject> Characters = MakeShared<FJsonObject>();
	const TArray<EWSCharacterId> CharacterIds = {
		EWSCharacterId::Player,
		EWSCharacterId::GuHeng,
		EWSCharacterId::YeCheng};
	for (const EWSCharacterId CharacterId : CharacterIds)
	{
		const FWSCharacterState Character =
			Snapshot.Characters.FindRef(CharacterId);
		TSharedRef<FJsonObject> CharacterObject = MakeShared<FJsonObject>();
		CharacterObject->SetNumberField(TEXT("health"), Character.Health);
		CharacterObject->SetNumberField(
			TEXT("temperature"),
			Character.Temperature);
		CharacterObject->SetNumberField(TEXT("hunger"), Character.Hunger);
		CharacterObject->SetNumberField(TEXT("fatigue"), Character.Fatigue);
		CharacterObject->SetNumberField(TEXT("pressure"), Character.Pressure);
		CharacterObject->SetNumberField(TEXT("trust"), Character.Trust);
		CharacterObject->SetNumberField(TEXT("stamina"), Character.Stamina);
		CharacterObject->SetStringField(
			TEXT("injury_severity"),
			StaticEnum<EWSInjurySeverity>()->GetNameStringByValue(
				static_cast<int64>(Character.InjurySeverity)));
		CharacterObject->SetStringField(
			TEXT("injury_id"),
			Character.InjuryId.IsNone()
				? TEXT("none")
				: Character.InjuryId.ToString());
		CharacterObject->SetNumberField(
			TEXT("injury_worsening_marks"),
			Character.InjuryWorseningMarks);
		CharacterObject->SetNumberField(
			TEXT("bandage_protection"),
			Character.BandageProtection);
		CharacterObject->SetNumberField(
			TEXT("temporary_support_uses"),
			Character.TemporarySupportUses);
		CharacterObject->SetStringField(
			TEXT("temporary_support_phase"),
			StaticEnum<EWSDayPhase>()->GetNameStringByValue(
				static_cast<int64>(Character.TemporarySupportPhase)));
		CharacterObject->SetStringField(
			TEXT("location"),
			StaticEnum<EWSCharacterLocation>()->GetNameStringByValue(
				static_cast<int64>(Character.Location)));
		Characters->SetObjectField(
			DialogueSpeakerId(CharacterId),
			CharacterObject);
	}
	Root->SetObjectField(TEXT("characters"), Characters);

	struct FRequirementCardExport
	{
		FString ActionId;
		FString RequirementId;
		bool bMet = false;
		FString PlayerFacingDetail;
	};
	TArray<FRequirementCardExport> RequirementCardRecords;
	const TArray<FName> RequirementActions = {TEXT("repair_generator")};
	for (const FName ActionId : RequirementActions)
	{
		const FWSActionRequirementReport Report =
			EvaluateActionRequirements(ActionId);
		const auto AddRequirement =
			[&RequirementCardRecords, ActionId](const FWSRequirementItem& Item)
			{
				FRequirementCardExport Card;
				Card.ActionId = ActionId.ToString();
				Card.RequirementId = Item.RequirementId.ToString();
				Card.bMet = Item.bSatisfied;
				Card.PlayerFacingDetail = Item.PlayerFacingDetail.ToString();
				RequirementCardRecords.Add(MoveTemp(Card));
			};
		for (const FWSRequirementItem& Item : Report.UniversalRequirements)
		{
			AddRequirement(Item);
		}
		for (const FWSRequirementPlan& Plan : Report.AlternativePlans)
		{
			for (const FWSRequirementItem& Item : Plan.Requirements)
			{
				AddRequirement(Item);
			}
		}
		for (const FWSRequirementItem& Item : Report.Risks)
		{
			AddRequirement(Item);
		}
	}
	RequirementCardRecords.Sort(
		[](const FRequirementCardExport& Left, const FRequirementCardExport& Right)
		{
			const int32 ActionComparison = Left.ActionId.Compare(
				Right.ActionId,
				ESearchCase::CaseSensitive);
			return ActionComparison == 0
				? Left.RequirementId.Compare(
					Right.RequirementId,
					ESearchCase::CaseSensitive) < 0
				: ActionComparison < 0;
		});
	TArray<TSharedPtr<FJsonValue>> RequirementCards;
	for (const FRequirementCardExport& Card : RequirementCardRecords)
	{
		TSharedRef<FJsonObject> CardObject = MakeShared<FJsonObject>();
		CardObject->SetStringField(TEXT("action_id"), Card.ActionId);
		CardObject->SetStringField(
			TEXT("requirement_id"),
			Card.RequirementId);
		CardObject->SetBoolField(TEXT("met"), Card.bMet);
		CardObject->SetStringField(
			TEXT("player_facing_detail"),
			Card.PlayerFacingDetail);
		RequirementCards.Add(MakeShared<FJsonValueObject>(CardObject));
	}
	Root->SetArrayField(TEXT("requirement_cards"), RequirementCards);
	TArray<TSharedPtr<FJsonValue>> PhaseSummaries;
	for (const FWSPhaseSummary& Summary : Snapshot.PhaseSummaries)
	{
		TSharedRef<FJsonObject> SummaryObject =
			MakeShared<FJsonObject>();
		SummaryObject->SetStringField(
			TEXT("phase"),
			StaticEnum<EWSDayPhase>()->GetNameStringByValue(
				static_cast<int64>(Summary.Phase)));
		SummaryObject->SetStringField(
			TEXT("heating_zone"),
			StaticEnum<EWSHeatingZone>()->GetNameStringByValue(
				static_cast<int64>(Summary.HeatingZone)));
		SummaryObject->SetNumberField(
			TEXT("unused_ap_discarded"),
			Summary.UnusedAPDiscarded);
		TArray<TSharedPtr<FJsonValue>> SummaryChanges;
		for (const FString& Change : Summary.Changes)
		{
			SummaryChanges.Add(MakeShared<FJsonValueString>(Change));
		}
		SummaryObject->SetArrayField(TEXT("changes"), SummaryChanges);
		SummaryObject->SetStringField(
			TEXT("phase_event"),
			Summary.PhaseEvent.IsNone()
				? TEXT("none")
				: Summary.PhaseEvent.ToString());
		SummaryObject->SetStringField(
			TEXT("npc_reaction"),
			Summary.NPCReaction.IsNone()
				? TEXT("none")
				: Summary.NPCReaction.ToString());
		PhaseSummaries.Add(MakeShared<FJsonValueObject>(SummaryObject));
	}
	Root->SetArrayField(TEXT("phase_summaries"), PhaseSummaries);
	TArray<TSharedPtr<FJsonValue>> Promises;
	for (const FWSPromiseRecord& Promise : Snapshot.Promises)
	{
		TSharedRef<FJsonObject> PromiseObject = MakeShared<FJsonObject>();
		PromiseObject->SetStringField(TEXT("promise_id"), Promise.PromiseId.ToString());
		PromiseObject->SetStringField(TEXT("condition_id"), Promise.ConditionId.ToString());
		PromiseObject->SetBoolField(TEXT("settled"), Promise.bSettled);
		PromiseObject->SetBoolField(TEXT("fulfilled"), Promise.bFulfilled);
		Promises.Add(MakeShared<FJsonValueObject>(PromiseObject));
	}
	Root->SetArrayField(TEXT("promises"), Promises);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}

	OutFilePath = FPaths::ProjectSavedDir()
		/ TEXT("Logs/WhiteoutStation_EventLog.json");
#if WITH_DEV_AUTOMATION_TESTS
	if (!EventLogExportPathForTest.IsEmpty())
	{
		OutFilePath = EventLogExportPathForTest;
	}
#endif
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutFilePath), true);
	return FFileHelper::SaveStringToFile(Json, *OutFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UWindStationStateSubsystem::BroadcastState()
{
	OnStateChanged.Broadcast(RulesEngine.GetState());
}

void UWindStationStateSubsystem::RequestActionExpression(const FWSActionRequest& ActionRequest)
{
	if (!AgentGateway || !UWSNPCDecisionService::RequiresExpression(ActionRequest.ActionId))
	{
		return;
	}

	FWSActionRequirementReport RequirementReport;
	if (ActionRequest.SemanticFrame.TargetActionId == TEXT("repair_generator"))
	{
		FWSActionRequest TargetRequest;
		TargetRequest.ActionId = TEXT("repair_generator");
		RequirementReport = RulesEngine.EvaluateActionRequirements(TargetRequest);
	}
	const FWSAgentReply Decision = UWSNPCDecisionService::BuildDeterministicReply(
		ActionRequest,
		RulesEngine.GetState(),
		RequirementReport);
	AgentGateway->RecordCommittedDialogueTurn(ActionRequest, Decision);
	HandleAgentReply(Decision);
}

void UWindStationStateSubsystem::HandleAgentReply(const FWSAgentReply& Reply)
{
	LatestDialogue = Reply;
	BroadcastDialogueLine(LatestDialogue);
}
