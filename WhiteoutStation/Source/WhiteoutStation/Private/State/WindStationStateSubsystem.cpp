#include "State/WindStationStateSubsystem.h"

#include "Actions/WSActionResolver.h"
#include "Agents/WSAgentGateway.h"
#include "Agents/WSNPCDecisionService.h"
#include "Dom/JsonObject.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Save/WindStationSaveGame.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Settings/WhiteoutSettingsSubsystem.h"

namespace
{
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
	ActionResolver = nullptr;
	AgentGateway = nullptr;
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

FWSActionPreview UWindStationStateSubsystem::PreviewAction(const FWSActionRequest& Request) const
{
	EWSHeatingZone HeatingZone = EWSHeatingZone::None;
	if (RulesEngine.IsV11()
		&& HeatingZoneForAction(Request.ActionId, HeatingZone))
	{
		const FWSGameState& State = RulesEngine.GetState();
		FWSActionPreview Preview;
		Preview.ActionId = Request.ActionId;
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
	return RulesEngine.Preview(Request);
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

	FWSActionRequirementReport RequirementReport;
	if (ActionRequest.SemanticFrame.TargetActionId == TEXT("repair_generator"))
	{
		FWSActionRequest TargetRequest;
		TargetRequest.ActionId = TEXT("repair_generator");
		RequirementReport = RulesEngine.EvaluateActionRequirements(TargetRequest);
	}
	Prepared.LocalFallback = UWSNPCDecisionService::BuildDeterministicReply(
		ActionRequest,
		Prepared.ReadSnapshot,
		RequirementReport);
	Prepared.AllowedFactIds = UWSNPCDecisionService::BuildAllowedFacts(
		ActionRequest,
		Prepared.LocalFallback.Speaker,
		Prepared.ReadSnapshot);
	Prepared.PlannedDisclosureFacts =
		Prepared.LocalFallback.PlannedDisclosureFacts;
	Prepared.PlannedKnowledgeUpgrades =
		Prepared.LocalFallback.DisclosedFactIds;

	FWSDialogueSemanticAtom FallbackAtom;
	FallbackAtom.AtomId = FName(*FString::Printf(
		TEXT("prepared_%s"),
		*ActionRequest.ActionId.ToString()));
	FallbackAtom.NaturalFallback = FText::FromString(
		Prepared.LocalFallback.Utterance);
	FallbackAtom.RelatedFactIds = Prepared.PlannedDisclosureFacts;
	Prepared.Contract.MustRealize.Add(FallbackAtom);
	Prepared.Contract.PersonaStyleId =
		Prepared.LocalFallback.Speaker == EWSCharacterId::YeCheng
			? TEXT("ye_cheng_clinical")
			: TEXT("gu_heng_guarded");
	for (const FName ProtectedFactId : {
		FName(TEXT("FACT_HAND_INJURY")),
		FName(TEXT("FACT_MEDICAL_DIAGNOSIS")),
		FName(TEXT("FACT_HEAT_PACK")),
		FName(TEXT("FACT_RELAY_COMPATIBILITY")),
		FName(TEXT("FACT_FORCED_RESTART_CONFIRMED"))})
	{
		if (!Prepared.AllowedFactIds.Contains(ProtectedFactId))
		{
			Prepared.Contract.ForbiddenFactIds.Add(ProtectedFactId);
		}
	}

	FWSDialogueOutcome SimulationOutcome;
	SimulationOutcome.FinalReply = Prepared.LocalFallback;
	SimulationOutcome.DisclosedFactIds =
		Prepared.LocalFallback.DisclosedFactIds;
	SimulationOutcome.RealizedAtomIds = {FallbackAtom.AtomId};
	SimulationOutcome.AnswerSource =
		Prepared.LocalFallback.AnswerSource;
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
	FWSActionRequirementReport RequirementReport;
	if (PendingDialogue.OriginalRequest.SemanticFrame.TargetActionId
		== TEXT("repair_generator"))
	{
		FWSActionRequest TargetRequest;
		TargetRequest.ActionId = TEXT("repair_generator");
		RequirementReport = RulesEngine.EvaluateActionRequirements(TargetRequest);
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
	const bool bKnowledgeBoundaryOpen =
		UWSAgentGateway::IsExpressionKnowledgeBoundaryOpen(
			PendingDialogue.LocalFallback.Speaker,
			PendingDialogue.AllowedFactIds);
	const bool bLiveProviderEligible =
		LLMConfigurationError.IsEmpty()
		&& AgentGateway
		&& AgentGateway->HasLiveProvider()
		&& bKnowledgeBoundaryOpen
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
	AgentGateway->RequestExpression(
		Prepared.OriginalRequest,
		Prepared.ReadSnapshot,
		RequirementReport,
		bUseLiveProvider,
		FWSAgentReplyCallback::CreateLambda(
			[WeakThis, TransactionId, Generation](const FWSAgentReply& Reply)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandlePreparedDialogueReply(
						Reply,
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
	if (!bHasPendingDialogue
		|| PendingDialogue.TransactionId != TransactionId
		|| PendingDialogue.Generation != Generation)
	{
		return;
	}

	const FWSPreparedDialogue Prepared = PendingDialogue;
	FWSAgentReply FinalReply = Reply;
	const auto IsSubset = [](const TArray<FName>& Candidate, const TArray<FName>& Allowed)
	{
		return !Candidate.ContainsByPredicate(
			[&Allowed](const FName FactId)
			{
				return !Allowed.Contains(FactId);
			});
	};
	const EWSCharacterId ExpectedSpeaker =
		Prepared.OriginalRequest.ActionId == TEXT("talk_ye_cheng")
			? EWSCharacterId::YeCheng
			: EWSCharacterId::GuHeng;
	const bool bReplyIdentityValid =
		FinalReply.TransactionId == Prepared.TransactionId
		&& FinalReply.ActionId == Prepared.OriginalRequest.ActionId
		&& FinalReply.DialogueSessionId
			== Prepared.OriginalRequest.DialogueSessionId
		&& FinalReply.Speaker == ExpectedSpeaker;
	const bool bReplyDisclosureValid =
		IsSubset(
			FinalReply.ReferencedFactIds,
			Prepared.PlannedDisclosureFacts)
		&& IsSubset(
			FinalReply.DisclosedFactIds,
			Prepared.PlannedDisclosureFacts);
	if (!bReplyIdentityValid || !bReplyDisclosureValid)
	{
		const FString FailureReason = FinalReply.ValidationReason;
		FinalReply = Prepared.LocalFallback;
		FinalReply.Provider = Reply.Provider;
		FinalReply.ValidationReason = FailureReason.IsEmpty()
			? TEXT("prepared_outcome_validation_failed")
			: FailureReason;
		FinalReply.bFallback = true;
	}
	FinalReply.PlannedDisclosureFacts =
		Prepared.PlannedDisclosureFacts;

	FWSDialogueOutcome Outcome;
	Outcome.FinalReply = FinalReply;
	Outcome.DisclosedFactIds = FinalReply.DisclosedFactIds;
	for (const FWSDialogueSemanticAtom& Atom : Prepared.Contract.MustRealize)
	{
		Outcome.RealizedAtomIds.AddUnique(Atom.AtomId);
	}
	Outcome.AnswerSource = FinalReply.AnswerSource;

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

	TFunction<void(const FWSActionResult&)> Completion =
		MoveTemp(PendingDialogueCompletion);
	bHasPendingDialogue = false;
	PendingDialogue = FWSPreparedDialogue();
	++DialogueGeneration;
	++StateRevision;
	const FWSAgentReply CommittedReply = Outcome.FinalReply;
	LatestDialogue = CommittedReply;
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
	if (SourceSaveVersion != TEXT("1.3.0"))
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
		SlotToLoad = UGameplayStatics::DoesSaveGameExist(LegacySaveSlotV12, 0)
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
		|| (Save->SaveVersion != TEXT("1.3.0")
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
	++StateRevision;
	LatestDialogue = FWSAgentReply();
	if (bLoadLegacySlot || Save->SaveVersion != TEXT("1.3.0"))
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
	Root->SetNumberField(TEXT("remaining_ap"), Snapshot.ActionPoints);
	Root->SetStringField(
		TEXT("day_phase"),
		StaticEnum<EWSDayPhase>()->GetNameStringByValue(
			static_cast<int64>(Snapshot.DayPhase)));
	Root->SetBoolField(
		TEXT("day_phase_started"),
		Snapshot.bDayPhaseStarted);
	Root->SetBoolField(TEXT("signal_sent"), Snapshot.Tasks.bSignalSent);
	Root->SetStringField(TEXT("ending"), StaticEnum<EWSEndingType>()->GetNameStringByValue(static_cast<int64>(Snapshot.Ending)));
	Root->SetNumberField(TEXT("score"), Snapshot.Score.Total);
	Root->SetStringField(TEXT("rating"), Snapshot.Score.Rating);
	Root->SetNumberField(TEXT("model_calls"), Snapshot.ModelCalls);
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

	OutFilePath = FPaths::ProjectSavedDir() / TEXT("Logs/WhiteoutStation_EventLog.json");
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
	const TArray<FName> AllowedFacts = UWSNPCDecisionService::BuildAllowedFacts(
		ActionRequest,
		Decision.Speaker,
		RulesEngine.GetState());
	const bool bKnowledgeBoundaryOpen =
		UWSAgentGateway::IsExpressionKnowledgeBoundaryOpen(
			Decision.Speaker,
			AllowedFacts);
	const bool bLiveProviderEligible =
		LLMConfigurationError.IsEmpty()
		&& AgentGateway->HasLiveProvider()
		&& bKnowledgeBoundaryOpen;
	const bool bUseLiveProvider =
		bLiveProviderEligible && RulesEngine.TryRecordModelCall();
	if (bUseLiveProvider)
	{
		++StateRevision;
		SaveSnapshot();
		BroadcastState();
	}
	TWeakObjectPtr<UWindStationStateSubsystem> WeakThis(this);
	AgentGateway->RequestExpression(
		ActionRequest,
		RulesEngine.GetState(),
		RequirementReport,
		bUseLiveProvider,
		FWSAgentReplyCallback::CreateLambda(
			[WeakThis, ActionRequest](const FWSAgentReply& Reply)
			{
				if (WeakThis.IsValid())
				{
					if (WeakThis->AgentGateway)
					{
						WeakThis->AgentGateway->RecordCommittedDialogueTurn(
							ActionRequest,
							Reply);
					}
					WeakThis->HandleAgentReply(Reply);
				}
			}));
}

void UWindStationStateSubsystem::HandleAgentReply(const FWSAgentReply& Reply)
{
	LatestDialogue = Reply;
	BroadcastDialogueLine(LatestDialogue);
}
