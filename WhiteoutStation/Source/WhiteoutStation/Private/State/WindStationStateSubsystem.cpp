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
	TEXT("WhiteoutStation_Autosave_v1_6"));
const FString UWindStationStateSubsystem::LegacySaveSlotV15(TEXT("WhiteoutStation_Autosave_v1_5"));
const FString UWindStationStateSubsystem::LegacySaveSlotV14(
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
	if (!AuthoredRepository.Load(FPaths::ProjectContentDir() / TEXT("Dialogue/v1.6"), Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Authored dialogue unavailable: %s"), *Error);
	}
	const FString ConfigPath =
		FPaths::ProjectContentDir()
		/ TEXT("Rules/WhiteoutStationRules.v1.6.json");
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

EWSDialogueMode UWindStationStateSubsystem::GetDialogueMode() const
{
	const UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>();
	if (!Settings || !Settings->IsLLMEnabled()) return EWSDialogueMode::Authored;
	return HasLiveLLMProvider() ? EWSDialogueMode::Online : EWSDialogueMode::InvalidConfiguration;
}

TArray<FWSAuthoredChoice> UWindStationStateSubsystem::GetAuthoredDialogueChoices(FName ActionId) const
{
	const FName Speaker = ActionId == TEXT("talk_ye_cheng") ? FName(TEXT("ye_cheng")) : FName(TEXT("gu_heng"));
	return AuthoredRepository.GetChoices(Speaker, RulesEngine.GetState());
}

FWSActionResult UWindStationStateSubsystem::SubmitAuthoredDialogueChoice(FName ActionId,
	FName ChoiceId, FGuid SessionId, TFunction<void(const FWSActionResult&)> Completion)
{
	FWSActionRequest Request;
	Request.ActionId = ActionId;
	Request.AuthoredChoiceId = ChoiceId;
	Request.DialogueSessionId = SessionId;
	Request.TransactionId = FGuid::NewGuid();
	return SubmitDialogueAction(Request, MoveTemp(Completion));
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

TArray<FWSConversationEntry> UWindStationStateSubsystem::GetConversationHistory(FName ActionId) const
{
	const FName Speaker = ActionId == TEXT("talk_ye_cheng") ? FName(TEXT("ye_cheng")) : FName(TEXT("gu_heng"));
	return RulesEngine.GetState().ConversationHistory.FilterByPredicate(
		[Speaker](const FWSConversationEntry& Entry) { return Entry.SpeakerId == Speaker; });
}

TArray<FString> UWindStationStateSubsystem::BuildOnlineConversationHistory(FName ActionId, FGuid SessionId) const
{
	const auto Entries = GetConversationHistory(ActionId);
	TArray<FString> History, Topics;
	for (const auto& Entry : Entries)
		for (FName Topic : Entry.Topics) Topics.AddUnique(Topic.ToString());
	History.Add(TEXT("previously_discussed_topics=") + FString::Join(Topics, TEXT(",")));
	for (int32 I = FMath::Max(0, Entries.Num() - 24); I < Entries.Num(); ++I)
	{
		const auto& Entry = Entries[I];
		History.Add(FString::Printf(TEXT("historical_exchange day_phase=%d current_session=%s committed=%s current_turn=%d\n玩家：%s\nNPC：%s"),
			static_cast<int32>(Entry.DayPhase), Entry.SessionId == SessionId ? TEXT("true") : TEXT("false"),
			Entry.bCommitted ? TEXT("true") : TEXT("false"), Entry.SessionId == SessionId && Entry.bCommitted ? Entry.TurnIndex : 0,
			*Entry.PlayerLine, *Entry.NpcLine));
	}
	return History;
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
	if (bHasPendingDialogue && PendingDialogue.OriginalRequest.DialogueSessionId == DialogueSessionId)
		CancelPendingDialogue();
	if (bHasPendingOnlineIntent) CancelPendingDialogue();
	RulesEngine.SetPendingConversationStatus(DialogueSessionId, TEXT("cancelled"));
	DialogueSessions.Remove(DialogueSessionId);
	SaveSnapshot();
}

bool UWindStationStateSubsystem::CanContinueDialogueSession(
	const FGuid& DialogueSessionId) const
{
	const FWSDialogueSessionRuntimeState* Session =
		DialogueSessions.Find(DialogueSessionId);
	return Session && (GetDialogueTurnsUsed(Session->ActionId) < GetDialogueTurnLimit() || Session->PendingCommitment.IsSet());
}

bool UWindStationStateSubsystem::NormalizeDialogueSessionRequest(
	FWSActionRequest& InOutRequest,
	EWSReasonCode& OutReason) const
{
	if (!InOutRequest.DialogueSessionId.IsValid())
	{
		InOutRequest.DialogueSessionId = FGuid::NewGuid();
	}
	InOutRequest.DialogueSessionMaxTurns = GetDialogueTurnLimit();
	InOutRequest.DialogueTurnIndex = 1;
	InOutRequest.bDialogueSessionFollowUp = false;
	InOutRequest.bDialoguePositiveRewardApplied = false;
	InOutRequest.bDialogueBehaviorEffectApplied = false;
	InOutRequest.bConfirmationClosure = false;
	if (const FWSDialogueSessionRuntimeState* Session =
			DialogueSessions.Find(InOutRequest.DialogueSessionId))
	{
		if (InOutRequest.OnlineMessageId.IsValid())
		{
			if (Session->CommittedMessages.Contains(InOutRequest.OnlineMessageId))
			{ OutReason = EWSReasonCode::DuplicateTransaction; return false; }
			if (!Session->ResolvedMessage.IsSet() || Session->LatestMessageId != InOutRequest.OnlineMessageId)
			{ OutReason = EWSReasonCode::DialogueStateChanged; return false; }
			const FName Action = InOutRequest.ActionId;
			const FGuid Tx = InOutRequest.TransactionId, Id = InOutRequest.DialogueSessionId;
			const FString Text = InOutRequest.PlayerSaid;
			InOutRequest = {}; Session->ResolvedMessage->ApplyTo(InOutRequest);
			InOutRequest.ActionId = Action; InOutRequest.TransactionId = Tx;
			InOutRequest.DialogueSessionId = Id; InOutRequest.PlayerSaid = Text;
			for (const auto& Part : InOutRequest.DialogueParts)
				if (Part.ConfirmProposalId.IsValid() && (!Session->PendingCommitment.IsSet()
					|| Part.ConfirmProposalId != Session->PendingCommitment->ProposalId
					|| Part.ConfirmProposalVersion != Session->PendingCommitment->ProposalVersion))
				{ OutReason = EWSReasonCode::DialogueStateChanged; return false; }
		}
		if (Session->ActionId != InOutRequest.ActionId
			|| Session->DayPhase != RulesEngine.GetState().DayPhase)
		{
			OutReason = EWSReasonCode::DialogueStateChanged;
			return false;
		}
		if (GetDialogueTurnsUsed(InOutRequest.ActionId) >= GetDialogueTurnLimit() && !InOutRequest.bConfirmationClosure)
		{
			OutReason = EWSReasonCode::DialogueSessionComplete;
			return false;
		}
		InOutRequest.DialogueTurnIndex = GetDialogueTurnsUsed(InOutRequest.ActionId) + (InOutRequest.bConfirmationClosure ? 0 : 1);
		InOutRequest.bDialogueSessionFollowUp = GetDialogueTurnsUsed(InOutRequest.ActionId) > 0;
		InOutRequest.bDialoguePositiveRewardApplied = Session->bPositiveRewardApplied;
		const FName EffectKey(*FString::Printf(TEXT("%d:%d"),
			static_cast<int32>(InOutRequest.DialogueAct),
			static_cast<int32>(InOutRequest.SemanticFrame.TargetCharacter)));
		InOutRequest.bDialogueBehaviorEffectApplied = Session->AppliedEffectKeys.Contains(EffectKey);
	}
	const FName Speaker = InOutRequest.ActionId == TEXT("talk_ye_cheng") ? FName(TEXT("ye_cheng")) : FName(TEXT("gu_heng"));
	const auto* Ledger = RulesEngine.GetState().DialogueLedger.Find(Speaker);
	if (Ledger)
	{
		const FName Key(*FString::Printf(TEXT("%d:%d"), static_cast<int32>(InOutRequest.DialogueAct), static_cast<int32>(InOutRequest.SemanticFrame.TargetCharacter)));
		InOutRequest.bDialogueBehaviorEffectApplied = Ledger->SocialEffectKeys.Contains(Key);
		InOutRequest.bDialoguePositiveRewardApplied = Ledger->SocialEffectKeys.Contains(Key);
	}
	InOutRequest.DialogueTurnIndex = GetDialogueTurnsUsed(InOutRequest.ActionId) + (InOutRequest.bConfirmationClosure ? 0 : 1);
	InOutRequest.DialogueSessionMaxTurns = GetDialogueTurnLimit();
	if (InOutRequest.DialogueTurnIndex > GetDialogueTurnLimit() && !InOutRequest.bConfirmationClosure)
	{ OutReason = EWSReasonCode::DialogueSessionComplete; return false; }
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
	if (Request.OnlineMessageId.IsValid())
	{
		Session.CommittedMessages.Add(Request.OnlineMessageId);
		Session.bProposalChangePending = false; Session.ProposalBeforeMessage.Reset();
		for (const auto& Part : Request.DialogueParts)
			if (Session.PendingCommitment.IsSet() && Part.ConfirmProposalId == Session.PendingCommitment->ProposalId
				&& Part.ConfirmProposalVersion == Session.PendingCommitment->ProposalVersion)
			{
				RulesEngine.SetPendingConversationStatus(Request.DialogueSessionId, TEXT("confirmed"));
				Session.PendingCommitment.Reset();
			}
		Session.ResolvedMessage.Reset();
	}
	Session.PaidAP = 0;
	if (Request.DialogueAct == EWSDialogueAct::Command)
	{
		Session.AppliedEffectKeys.Add(FName(*FString::Printf(TEXT("%d:%d"),
			static_cast<int32>(Request.DialogueAct),
			static_cast<int32>(Request.SemanticFrame.TargetCharacter))));
	}
	else
	{
		Session.bPositiveRewardApplied = true;
	}
	Session.CommittedTurns = GetDialogueTurnsUsed(Request.ActionId);
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
		Preview.Costs.Resources.Add(TEXT("fuel"), 1);
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
	if (!Request.AuthoredChoiceId.IsNone())
	{
		const FWSAuthoredChoice* Choice = AuthoredRepository.FindChoice(Request.AuthoredChoiceId);
		const FName Speaker = Request.ActionId == TEXT("talk_ye_cheng") ? FName(TEXT("ye_cheng")) : FName(TEXT("gu_heng"));
		if (!Choice || !AuthoredRepository.CanSelect(*Choice, Speaker, RulesEngine.GetState()))
		{
			FWSActionResult Rejected;
			Rejected.ActionId = Request.ActionId;
			Rejected.TransactionId = Request.TransactionId;
			Rejected.ReasonCode = EWSReasonCode::DialogueOutcomeInvalid;
			CompleteDialogueSubmission(Rejected, MoveTemp(Completion));
			return Rejected;
		}
		Choice->Intent.ApplyTo(NormalizedRequest);
		NormalizedRequest.PlayerSaid = Choice->Text;
	}
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
		if (auto* Session = DialogueSessions.Find(Request.DialogueSessionId)) Session->RollbackProposal();
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
		if (auto* Session = DialogueSessions.Find(Request.DialogueSessionId)) Session->RollbackProposal();
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
	if (!ActionRequest.DialogueParts.IsEmpty() && !bHasPendingDialogue)
	{
		TArray<FWSPreparedDialogue> Parts;
		TArray<FWSDialogueOutcome> Fallbacks;
		FWSActionResult Result;
		for (FWSActionRequest Child : ActionRequest.DialogueParts)
		{
			Child.ActionId = ActionRequest.ActionId; Child.TransactionId = ActionRequest.TransactionId;
			Child.DialogueSessionId = ActionRequest.DialogueSessionId;
			if (Child.PlayerSaid.IsEmpty()) Child.PlayerSaid = ActionRequest.PlayerSaid;
			Child.DialogueTurnIndex = ActionRequest.DialogueTurnIndex;
			Child.DialogueSessionMaxTurns = ActionRequest.DialogueSessionMaxTurns;
			Child.bDialogueSessionFollowUp = ActionRequest.bDialogueSessionFollowUp;
			Child.bDialoguePositiveRewardApplied = ActionRequest.bDialoguePositiveRewardApplied;
			Child.bDialogueBehaviorEffectApplied = ActionRequest.bDialogueBehaviorEffectApplied;
			Child.DialogueParts.Reset();
			Result = PrepareDialogue(Child);
			if (!Result.bPendingDialogue) { bHasPendingDialogue = false; PendingDialogue = {}; return Result; }
			Parts.Add(PendingDialogue);
			FWSDialogueOutcome Fallback; Fallback.FinalReply = PendingDialogue.LocalFallback;
			Fallback.DisclosedFactIds = Fallback.FinalReply.DisclosedFactIds;
			Fallback.AnswerSource = Fallback.FinalReply.AnswerSource;
			Fallbacks.Add(Fallback);
			bHasPendingDialogue = false;
		}
		PendingDialogue = Parts[0]; PendingDialogue.Parts = Parts;
		PendingDialogue.OriginalRequest = ActionRequest; PendingDialogue.Generation = ++DialogueGeneration;
		PendingDialogue.LocalFallback = FWSDialogueOutcome::Combine(Fallbacks, ActionRequest.DialogueNotice).FinalReply;
		if (PendingDialogue.bNaturalV16 && AgentGateway) AgentGateway->BuildNaturalContext(PendingDialogue);
		bHasPendingDialogue = true;
		return Result;
	}
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
	Prepared.RoleplayRequest.SubjectiveState.GeneratorRequired =
		RulesEngine.GetConfig().GeneratorRequired;
	Prepared.bRoleplayV15 = ActionRequest.SemanticFrame.bCanonicalIntentValidated;
	Prepared.bNaturalV16 = RulesEngine.IsV16() && Prepared.bRoleplayV15 && ActionRequest.AuthoredChoiceId.IsNone() && !ActionRequest.bConfirmationClosure;
#if WITH_DEV_AUTOMATION_TESTS
	// Legacy reply hooks inject authored lines. Natural output is tested through its full outcome contract and real provider probes.
	if (DialogueRealizeTestHook) Prepared.bNaturalV16 = false;
#endif
	Prepared.RoleplayRequest.bControlledClaims = Prepared.bRoleplayV15;
	if (Prepared.bRoleplayV15) Prepared.RoleplayRequest.ResponsePolicy.MaxCharacters = 240;
	if (Prepared.bRoleplayV15 && ActionRequest.AuthoredChoiceId.IsNone())
	{
		if (const FWSAuthoredChoice* Equivalent = AuthoredRepository.FindEquivalent(
			Prepared.RoleplayRequest.SpeakerId, ActionRequest.SemanticFrame))
		{
			Prepared.bHasAuthoredFallback = AuthoredRepository.SelectLine(*Equivalent, Prepared.ReadSnapshot,
				Prepared.RoleplayRequest, RoleplayFallback, RoleplayError);
		}
	}
	if (!ActionRequest.AuthoredChoiceId.IsNone())
	{
		Prepared.RoleplayRequest.bAuthoredDialogue = true;
		const FWSAuthoredChoice* Choice = AuthoredRepository.FindChoice(ActionRequest.AuthoredChoiceId);
		if (!Choice || !AuthoredRepository.SelectLine(*Choice, Prepared.ReadSnapshot,
			Prepared.RoleplayRequest, RoleplayFallback, RoleplayError))
		{
			Result.ReasonCode = EWSReasonCode::DialogueOutcomeInvalid;
			return Result;
		}
	}
	Prepared.Contract.PersonaStyleId =
		Prepared.RoleplayRequest.SpeakerId.ToString();
	if (Prepared.bRoleplayV15 && ActionRequest.ConfirmProposalId.IsValid() && !ActionRequest.PromiseTerms.IsEmpty())
	{
		// Registered terms are rendered from the bound proposal, not regenerated by the provider.
		RoleplayFallback.Line = TEXT("我记下了，按你确认的条款登记。");
		RoleplayFallback.FallbackId = TEXT("v15_confirmed_terms");
		RoleplayFallback.ReferencedKnowledgeIds.Reset(); RoleplayFallback.Assertions.Reset();
		Prepared.bHasAuthoredFallback = true;
	}
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
	Fallback.Assertions = RoleplayFallback.Assertions;
	for (const FWSRoleplayAssertion& Assertion : Fallback.Assertions)
	{
		const FWSRoleplayKnowledgeItem* Knowledge =
			Prepared.RoleplayRequest.AvailableKnowledge.FindByPredicate(
				[&Assertion](const FWSRoleplayKnowledgeItem& Item)
				{
					return Item.KnowledgeId == Assertion.KnowledgeId;
				});
		if (Knowledge && Knowledge->bCreatesGameFact)
		{
			Fallback.DisclosedFactIds.AddUnique(Knowledge->GameFactId);
		}
	}
	Fallback.ReferencedFactIds = Fallback.DisclosedFactIds;
	Fallback.MemorySummary = RoleplayFallback.Line.Left(160);
	Fallback.SemanticFrame = ActionRequest.SemanticFrame;
	Fallback.Emotion = Fallback.Speaker == EWSCharacterId::YeCheng
		? TEXT("clinical")
		: TEXT("guarded");
	Fallback.AnswerSource = TEXT("local_natural_fallback");
	Fallback.Provider = TEXT("preset");
	Fallback.ValidationReason = TEXT("local_natural_fallback");
	Fallback.bFallback = true;
	if (Prepared.bRoleplayV15)
	{
		if (ActionRequest.SemanticFrame.TopicId == TEXT("generator")
			&& ActionRequest.SemanticFrame.QueryType == EWSDialogueQueryType::Status)
		{
			FWSRoleplayKnowledgeItem Progress;
			Progress.KnowledgeId = TEXT("DIALOGUE_GENERATOR_PROGRESS");
			Progress.Owner = Prepared.RoleplayRequest.SpeakerId; Progress.SubjectId = TEXT("generator");
			Progress.CategoryId = TEXT("public_status"); Progress.EpistemicStatus = EWSEpistemicStatus::Known;
			Progress.MaxDisclosure = EWSRoleplayDisclosureLevel::Explicit; Progress.Confidence = 1.0f;
			Progress.RoleplayContent = FString::Printf(TEXT("发电机检修进度为 %d/%d，%s。"),
				Prepared.ReadSnapshot.Tasks.GeneratorProgress, RulesEngine.GetConfig().GeneratorRequired,
				Prepared.ReadSnapshot.Tasks.GeneratorProgress >= RulesEngine.GetConfig().GeneratorRequired ? TEXT("检修已完成") : TEXT("检修尚未完成"));
			Prepared.RoleplayRequest.AvailableKnowledge.Add(Progress);
			Prepared.RequiredClaims.Add(Progress.KnowledgeId, Progress.RoleplayContent);
		}
		for (const FName KnowledgeId : RoleplayFallback.ReferencedKnowledgeIds)
		{
			FString ClaimText; FName ClaimKnowledge;
			if (AuthoredRepository.RenderClaim(KnowledgeId, Prepared.RoleplayRequest, ClaimText, ClaimKnowledge))
				Prepared.RequiredClaims.Add(KnowledgeId, ClaimText);
		}
	}
	if (Prepared.bHasAuthoredFallback) Fallback.AuthoredLineId = RoleplayFallback.FallbackId;
	if (!ActionRequest.AuthoredChoiceId.IsNone())
	{
		Fallback.AnswerSource = TEXT("authored_v15");
		Fallback.AuthoredLineId = RoleplayFallback.FallbackId;
		Fallback.bFallback = false;
	}

	if (!Prepared.bNaturalV16)
	{
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

	}
	if (Prepared.bNaturalV16 && AgentGateway) AgentGateway->BuildNaturalContext(Prepared);
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
	if (!PendingDialogue.OriginalRequest.AuthoredChoiceId.IsNone() || PendingDialogue.OriginalRequest.bConfirmationClosure)
	{
		HandlePreparedDialogueReply(PendingDialogue.LocalFallback, TransactionId, Generation);
		return;
	}
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
	if (Prepared.bRoleplayV15)
	{
		if (!bUseLiveProvider)
		{
			AbortPendingDialogue(EWSReasonCode::DialogueOutcomeInvalid, true, false);
			return;
		}
		AgentGateway->RequestNaturalRoleplay(Prepared, FWSDialogueOutcomeCallback::CreateLambda(
			[WeakThis, TransactionId, Generation](const FWSDialogueOutcome& Outcome)
			{
				if (WeakThis.IsValid()) WeakThis->HandlePreparedDialogueOutcome(Outcome, TransactionId, Generation);
			}), [WeakThis]() { return WeakThis.IsValid() && WeakThis->RulesEngine.TryRecordModelCall(); });
		return;
	}
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
	if (!PendingDialogue.Parts.IsEmpty())
	{
		TArray<FWSDialogueOutcome> Parts;
		for (const auto& Prepared : PendingDialogue.Parts)
		{
			FWSDialogueOutcome Part; Part.FinalReply = Prepared.LocalFallback;
			Part.DisclosedFactIds = Part.FinalReply.DisclosedFactIds; Part.AnswerSource = Part.FinalReply.AnswerSource;
			Parts.Add(Part);
		}
		auto Combined = FWSDialogueOutcome::Combine(Parts, PendingDialogue.OriginalRequest.DialogueNotice);
		if (Reply.Utterance != Combined.FinalReply.Utterance) Combined.ValidationOutcome = TEXT("v15_no_response");
		HandlePreparedDialogueOutcome(Combined, TransactionId, Generation); return;
	}
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
	if (Prepared.bRoleplayV15 && (Outcome.ValidationOutcome == TEXT("v15_no_response") || Outcome.ValidationOutcome == TEXT("v16_no_response")))
	{
		AbortPendingDialogue(EWSReasonCode::DialogueOutcomeInvalid, true, false);
		return;
	}
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
		UE_LOG(LogTemp, Display, TEXT("Whiteout prepared outcome rejected: %s"), *ValidationReason);
		if (Prepared.bNaturalV16 || !Prepared.Parts.IsEmpty())
		{ AbortPendingDialogue(EWSReasonCode::DialogueOutcomeInvalid, true, false); return; }
		if (Prepared.bRoleplayV15 && !Prepared.bHasAuthoredFallback && Prepared.OriginalRequest.AuthoredChoiceId.IsNone())
		{
			AbortPendingDialogue(EWSReasonCode::DialogueOutcomeInvalid, true, false);
			return;
		}
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
		if (auto* Session = DialogueSessions.Find(Prepared.OriginalRequest.DialogueSessionId)) Session->RollbackProposal();
		TFunction<void(const FWSActionResult&)> Completion =
			MoveTemp(PendingDialogueCompletion);
		bHasPendingDialogue = false;
		PendingDialogue = FWSPreparedDialogue();
		++DialogueGeneration;
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
	FWSAgentReply CommittedReply = Outcome.FinalReply;
	CommittedReply.SystemNotice = Prepared.OriginalRequest.DialogueNotice;
	LatestDialogue = CommittedReply;
	RecordCommittedDialogueSession(Prepared.OriginalRequest);
	if (const auto* Session = DialogueSessions.Find(Prepared.OriginalRequest.DialogueSessionId))
		CommittedReply.bPendingConfirmation = Session->PendingCommitment.IsSet();
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
	Root->SetNumberField(TEXT("message_schema_version"), 2);
	TArray<TSharedPtr<FJsonValue>> ClauseResults;
	for (int32 I = 0; I < Prepared.Parts.Num(); ++I)
	{
		const auto& Part = Prepared.Parts[I]; const auto& Reply = Prepared.bNaturalV16 ? Outcome.FinalReply : Outcome.Parts[I].FinalReply;
		auto Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("topic"), Part.OriginalRequest.SemanticFrame.TopicId.ToString());
		Item->SetNumberField(TEXT("target"), static_cast<int32>(Part.OriginalRequest.SemanticFrame.TargetCharacter));
		Item->SetStringField(TEXT("polarity"), Part.OriginalRequest.DialoguePolarity);
		Item->SetBoolField(TEXT("local_clarification"), Part.OriginalRequest.bLocalClarification);
		Item->SetArrayField(TEXT("allowed_fact_ids"), NameIdArray(Part.AllowedFactIds));
		Item->SetArrayField(TEXT("disclosed_fact_ids"), NameIdArray(Reply.DisclosedFactIds));
		// Dialogue text belongs to saves; default telemetry contains metadata only.
		Item->SetStringField(TEXT("source"), Reply.AnswerSource);
		ClauseResults.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("clause_results"), ClauseResults);
	Root->SetStringField(TEXT("protocol_version"), Prepared.bNaturalV16 ? TEXT("natural_roleplay_v6") : Prepared.bRoleplayV15 ? TEXT("bounded_roleplay_v5") : TEXT("legacy"));
	Root->SetStringField(TEXT("authored_choice_id"), Prepared.OriginalRequest.AuthoredChoiceId.ToString());
	Root->SetStringField(TEXT("authored_line_id"), Outcome.FinalReply.AuthoredLineId.ToString());
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
	const FString AnswerSource = Prepared.bRoleplayV15 || !Outcome.FinalReply.bFallback
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
	bHasPendingOnlineIntent = false;
	TFunction<void(const FWSActionResult&)> Completion;
	FWSActionResult Result;
	if (bHasPendingDialogue)
	{
		if (auto* Session = DialogueSessions.Find(PendingDialogue.OriginalRequest.DialogueSessionId)) Session->RollbackProposal();
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
	if (SourceState.RulesSchemaVersion < 8 && TargetRulesSchemaVersion >= 8)
	{
		MigratedState.bRepairPreparationAvailable = false;
		MigratedState.bScoreRulesMigrated = true;
	}
	for (auto& Entry : MigratedState.ConversationHistory)
		if (Entry.ControlStatus == TEXT("pending")) Entry.ControlStatus = TEXT("cancelled");
	if (SourceSaveVersion != TEXT("1.6.0")
		&& SourceSaveVersion != TEXT("1.5.0")
		&& SourceSaveVersion != TEXT("1.4.0")
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
	if (TargetRulesSchemaVersion >= 7 && MigratedState.DialogueLedgerVersion < 1)
	{
		MigratedState.DialogueLedger.Reset();
		for (auto& Entry : MigratedState.ConversationHistory)
		{
			if (!Entry.bCommitted || !Entry.EntryId.IsValid()) continue;
			auto& Ledger = MigratedState.DialogueLedger.FindOrAdd(Entry.SpeakerId);
			if (Ledger.SuccessfulMessageIds.Contains(Entry.EntryId)) continue;
			Ledger.SuccessfulMessageIds.Add(Entry.EntryId); ++Ledger.UsedTurns; Entry.bCountedTurn = true;
		}
		MigratedState.DialogueLedgerVersion = 1;
	}
	if (!MigratedState.RunId.IsValid()) MigratedState.RunId = FGuid::NewGuid();
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
		SlotToLoad = UGameplayStatics::DoesSaveGameExist(LegacySaveSlotV15, 0)
			? LegacySaveSlotV15
			: UGameplayStatics::DoesSaveGameExist(LegacySaveSlotV14, 0)
			? LegacySaveSlotV14
			: UGameplayStatics::DoesSaveGameExist(LegacySaveSlotV13, 0)
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
		|| (Save->SaveVersion != TEXT("1.6.0")
			&& Save->SaveVersion != TEXT("1.5.0")
			&& Save->SaveVersion != TEXT("1.4.0")
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
	FWSGameState MigratedState = MigrateSaveStateForV13(
		Save->State,
		Save->SaveVersion,
		RulesEngine.GetConfig().SchemaVersion,
		RulesEngine.GetConfig().RulesVersion);
	RulesEngine.SetState(MigratedState);
	DialogueSessions.Reset();
	++StateRevision;
	LatestDialogue = FWSAgentReply();
	if (bLoadLegacySlot || Save->SaveVersion != TEXT("1.6.0"))
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
		|| UGameplayStatics::DoesSaveGameExist(LegacySaveSlotV15, 0)
		|| UGameplayStatics::DoesSaveGameExist(LegacySaveSlotV14, 0)
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
		Object->SetStringField(TEXT("executor"), DialogueSpeakerId(Event.Executor));
		Object->SetStringField(TEXT("collaborator"), Event.bHasCollaborator ? DialogueSpeakerId(Event.Collaborator) : TEXT("none"));
		Object->SetBoolField(TEXT("repair_preparation_granted"), Event.bRepairPreparationGranted);
		Object->SetBoolField(TEXT("repair_preparation_consumed"), Event.bRepairPreparationConsumed);
		Object->SetNumberField(TEXT("actual_ap"), Event.ActualAP);
		const auto ResourceCosts = MakeShared<FJsonObject>();
		for (const auto& Cost : Event.Costs.Resources) ResourceCosts->SetNumberField(Cost.Key.ToString(), Cost.Value);
		Object->SetObjectField(TEXT("resource_costs"), ResourceCosts);
		const auto StaminaCosts = MakeShared<FJsonObject>();
		for (const auto& Cost : Event.Costs.Stamina) StaminaCosts->SetNumberField(DialogueSpeakerId(Cost.Key), Cost.Value);
		Object->SetObjectField(TEXT("stamina_costs"), StaminaCosts);
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
	Root->SetNumberField(TEXT("rules_schema"), Snapshot.RulesSchemaVersion);
	Root->SetBoolField(TEXT("repair_preparation_available"), Snapshot.bRepairPreparationAvailable);
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

int32 UWindStationStateSubsystem::GetDialogueTurnsUsed(FName ActionId) const
{
	const FName Speaker = ActionId == TEXT("talk_ye_cheng") ? FName(TEXT("ye_cheng")) : FName(TEXT("gu_heng"));
	const auto* Ledger = RulesEngine.GetState().DialogueLedger.Find(Speaker);
	return Ledger ? Ledger->UsedTurns : 0;
}
