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
	TEXT("WhiteoutStation_Autosave_v1_2"));
const FString UWindStationStateSubsystem::LegacySaveSlot(
	TEXT("WhiteoutStation_Autosave_v1_1"));

void UWindStationStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
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
	if (AgentGateway)
	{
		AgentGateway->ResetSession();
	}
	ActionResolver = nullptr;
	AgentGateway = nullptr;
	Super::Deinitialize();
}

bool UWindStationStateSubsystem::ApplyLLMRuntimeConfiguration(FString& OutError)
{
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
	if (!RulesEngine.SetRequirementPinned(ActionId, bPinned))
	{
		return false;
	}
	AppendOfferAudit(
		bPinned ? TEXT("pinned") : TEXT("unpinned"),
		ActionId,
		nullptr,
		RulesEngine.GetState());
	SaveSnapshot();
	BroadcastState();
	return true;
}

bool UWindStationStateSubsystem::AcceptLatestNegotiationOffer(FString& OutMessage)
{
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
	if (AgentGateway)
	{
		AgentGateway->ResetSession();
	}
	RulesEngine.Reset();
	LatestDialogue = FWSAgentReply();
	BroadcastState();
}

FWSGameState UWindStationStateSubsystem::GetStateSnapshot() const
{
	return RulesEngine.GetState();
}

void UWindStationStateSubsystem::CancelPendingDialogue()
{
	if (AgentGateway)
	{
		AgentGateway->ResetSession();
	}
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
	return RulesEngine.EvaluateActionRequirements(Request);
}

FWSActionResult UWindStationStateSubsystem::CommitAction(const FWSActionRequest& Request)
{
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
		SaveSnapshot();
		OnActionCommitted.Broadcast(Result);
		BroadcastState();
		FWSActionRequest CommittedRequest = Request;
		CommittedRequest.TransactionId = Result.TransactionId;
		RequestActionExpression(CommittedRequest);
	}
	return Result;
}

bool UWindStationStateSubsystem::BeginDayPhase(
	const EWSHeatingZone HeatingZone,
	EWSReasonCode& OutReason,
	TArray<FString>& OutChanges)
{
	const bool bStarted =
		RulesEngine.BeginDayPhase(HeatingZone, OutReason, OutChanges);
	if (bStarted)
	{
		SaveSnapshot();
		BroadcastState();
	}
	return bStarted;
}

bool UWindStationStateSubsystem::SettleCurrentDayPhase(
	EWSReasonCode& OutReason,
	FWSPhaseSummary& OutSummary)
{
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
		SaveSnapshot();
		BroadcastState();
	}
	return bSettled;
}

FWSGameState UWindStationStateSubsystem::EndGame()
{
	RulesEngine.EndGame();
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
	return UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0);
}

bool UWindStationStateSubsystem::LoadSnapshot()
{
	const bool bLoadLegacySlot = !UGameplayStatics::DoesSaveGameExist(SaveSlot, 0)
		&& UGameplayStatics::DoesSaveGameExist(LegacySaveSlot, 0);
	const FString& SlotToLoad = bLoadLegacySlot ? LegacySaveSlot : SaveSlot;
	UWindStationSaveGame* Save = Cast<UWindStationSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotToLoad, 0));
	if (!Save
		|| (Save->SaveVersion != TEXT("1.2.0")
			&& Save->SaveVersion != TEXT("1.1.0")))
	{
		return false;
	}
	if (AgentGateway)
	{
		AgentGateway->ResetSession();
	}
	RulesEngine.SetState(Save->State);
	LatestDialogue = FWSAgentReply();
	if (bLoadLegacySlot || Save->SaveVersion == TEXT("1.1.0"))
	{
		SaveSnapshot();
	}
	BroadcastState();
	return true;
}

bool UWindStationStateSubsystem::HasSnapshot() const
{
	return UGameplayStatics::DoesSaveGameExist(SaveSlot, 0)
		|| UGameplayStatics::DoesSaveGameExist(LegacySaveSlot, 0);
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
		ActionRequest.ActionId,
		Decision.Speaker,
		RulesEngine.GetState());
	const bool bKnowledgeBoundaryOpen =
		UWSAgentGateway::IsExpressionKnowledgeBoundaryOpen(
			Decision.Speaker,
			AllowedFacts);
	const bool bUseLiveProvider =
		LLMConfigurationError.IsEmpty()
		&& AgentGateway->HasLiveProvider()
		&& bKnowledgeBoundaryOpen
		&& RulesEngine.TryRecordModelCall();
	if (bUseLiveProvider)
	{
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
			[WeakThis](const FWSAgentReply& Reply)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleAgentReply(Reply);
				}
			}));
}

void UWindStationStateSubsystem::HandleAgentReply(const FWSAgentReply& Reply)
{
	LatestDialogue = Reply;
	OnDialogueLine.Broadcast(LatestDialogue);
}
