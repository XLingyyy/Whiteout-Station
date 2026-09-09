#include "Dialogue/WSConversationValidator.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "State/WSKnowledgePolicy.h"

namespace
{
	bool UniqueSubset(const TArray<FString>& Values, const TArray<FString>& Allowed)
	{
		TSet<FString> Seen;
		for (const auto& Value : Values)
		{
			if (!Allowed.Contains(Value) || Seen.Contains(Value)) return false;
			Seen.Add(Value);
		}
		return true;
	}
}

bool FWSConversationValidator::ParseReply(const FString& Json, const FWSPreparedDialogue& Prepared,
	FWSDialogueOutcome& Outcome, FString& Error)
{
	TSharedPtr<FJsonObject> Root;
	FString Line, Emotion, Reaction;
	TArray<FString> Goals, References, Proposals;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root || Root->Values.Num() != 6
		|| !Root->TryGetStringField(TEXT("npc_line"), Line)
		|| !Root->TryGetStringArrayField(TEXT("addressed_goal_ids"), Goals)
		|| !Root->TryGetStringArrayField(TEXT("referenced_fact_ids"), References)
		|| !Root->TryGetStringArrayField(TEXT("action_proposal_ids"), Proposals)
		|| !Root->TryGetStringField(TEXT("emotion"), Emotion)
		|| !Root->TryGetStringField(TEXT("reaction_action"), Reaction))
	{ Error = TEXT("natural_schema_invalid"); return false; }
	Line.TrimStartAndEndInline();
	const TArray<FString> Emotions = {TEXT("clinical"), TEXT("guarded"), TEXT("calm"), TEXT("concerned"), TEXT("firm"), TEXT("relieved")};
	const TArray<FString> Reactions = {TEXT("consider"), TEXT("acknowledge"), TEXT("reject"), TEXT("reassure")};
	if (Line.IsEmpty() || Line.Len() > 320 || !Emotions.Contains(Emotion) || !Reactions.Contains(Reaction)
		|| !UniqueSubset(Goals, Prepared.AnswerGoalIds) || Goals.Num() != Prepared.AnswerGoalIds.Num())
	{ Error = TEXT("natural_line_or_goals_invalid"); return false; }
	TArray<FString> AllowedFacts;
	for (const auto& Pair : Prepared.NaturalFacts) AllowedFacts.Add(Pair.Key.ToString());
	if (!UniqueSubset(References, AllowedFacts)) { Error = TEXT("natural_unauthorized_fact"); return false; }
	TArray<FString> AllowedProposals;
	for (int32 I = 0; I < Prepared.RoleplayRequest.AllowedActionProposals.Num(); ++I)
		AllowedProposals.Add(FString::Printf(TEXT("proposal_%d"), I));
	if (!UniqueSubset(Proposals, AllowedProposals) || Proposals.Num() > 1)
	{ Error = TEXT("natural_unauthorized_proposal"); return false; }
	Outcome = {};
	auto& Reply = Outcome.FinalReply;
	Reply = Prepared.LocalFallback;
	Reply.Utterance = Line; Reply.Emotion = Emotion;
	Reply.Reaction = Reaction == TEXT("reject") ? EWSNPCReaction::Reject : Reaction == TEXT("reassure")
		? EWSNPCReaction::Reassure : Reaction == TEXT("acknowledge") ? EWSNPCReaction::Acknowledge : EWSNPCReaction::Consider;
	Reply.MovementIntent = EWSNPCMovementIntent::Stay;
	Reply.SpeechFunction = EWSRoleplaySpeechFunction::Answer;
	Reply.Assertions.Reset(); Reply.ReferencedKnowledgeIds.Reset(); Reply.ReferencedFactIds.Reset(); Reply.DisclosedFactIds.Reset();
	Reply.RealizedAtomIds.Reset(); Reply.SemanticSpine.Reset(); Reply.PersonaTail.Reset(); Reply.PlannedDisclosureFacts.Reset();
	for (const auto& Id : References) Reply.ReferencedKnowledgeIds.Add(FName(*Id));
	Reply.bHasProposedAction = !Proposals.IsEmpty();
	if (Reply.bHasProposedAction) Reply.ProposedAction = Prepared.RoleplayRequest.AllowedActionProposals[AllowedProposals.IndexOfByKey(Proposals[0])];
	Reply.bFallback = false; Reply.AuthoredLineId = NAME_None; Reply.MemorySummary = TEXT("完成一次交谈；事实以当前状态和事件为准。");
	Reply.AnswerSource = TEXT("natural_roleplay_v16"); Reply.ValidationReason = TEXT("structure_valid");
	Outcome.AnswerSource = Reply.AnswerSource; Outcome.AddressedGoalIds = Goals;
	Outcome.ValidationOutcome = TEXT("awaiting_verification");
	Error = TEXT("ok"); return true;
}

bool FWSConversationValidator::NeedsSemanticCheck(const FWSPreparedDialogue& Prepared, const FWSDialogueOutcome& Outcome)
{
	if (Outcome.FinalReply.bHasProposedAction || !Prepared.OriginalRequest.DialogueNotice.IsEmpty()
		|| Prepared.OriginalRequest.ConfirmProposalId.IsValid()) return true;
	const auto CriticalPart = [](const FWSPreparedDialogue& Part)
	{
		const auto Topic = Part.OriginalRequest.SemanticFrame.TopicId;
		return Topic != TEXT("person") && Topic != TEXT("relationship");
	};
	if (Prepared.Parts.IsEmpty() ? CriticalPart(Prepared) : Prepared.Parts.ContainsByPredicate(CriticalPart)) return true;
	for (const FName Id : Outcome.FinalReply.ReferencedKnowledgeIds)
	{
		const auto* Fact = Prepared.NaturalFacts.Find(Id);
		if (Fact && (Fact->bCreatesGameFact || Fact->CategoryId == TEXT("current_state"))) return true;
	}
	// These terms route unexpected factual claims to a semantic check; they are not a truth test.
	for (const TCHAR* Term : {TEXT("伤"), TEXT("治疗"), TEXT("处理"), TEXT("修"), TEXT("药"), TEXT("包扎"), TEXT("继电"), TEXT("已经"), TEXT("完成"), TEXT("供暖"), TEXT("重启"), TEXT("诊断"), TEXT("失温"), TEXT("承诺"), TEXT("保证")})
		if (Outcome.FinalReply.Utterance.Contains(Term)) return true;
	return false;
}

bool FWSConversationValidator::ApplyVerdict(const FString& Json, const FWSPreparedDialogue& Prepared,
	FWSDialogueOutcome& Outcome, FString& Error)
{
	TSharedPtr<FJsonObject> Root; bool Safe = false;
	TArray<FString> Issues, Expressed, Goals; FString Correction;
	const TArray<TSharedPtr<FJsonValue>>* EventClaims = nullptr;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root || Root->Values.Num() != 6
		|| !Root->TryGetBoolField(TEXT("safe"), Safe) || !Root->TryGetStringArrayField(TEXT("issues"), Issues)
		|| !Root->TryGetStringArrayField(TEXT("expressed_fact_ids"), Expressed)
		|| !Root->TryGetStringArrayField(TEXT("addressed_goal_ids"), Goals)
		|| !Root->TryGetStringField(TEXT("corrects_entry_id"), Correction)
		|| !Root->TryGetArrayField(TEXT("event_claims"), EventClaims))
	{ Error = TEXT("verification_schema_invalid"); return false; }
	if (!Safe || !Issues.IsEmpty() || !UniqueSubset(Goals, Prepared.AnswerGoalIds) || Goals.Num() != Prepared.AnswerGoalIds.Num())
	{ Error = TEXT("verification_rejected"); return false; }
	// A semantic verdict does not authorize an invented event. Check extracted assertions against rules.
	for (const auto& Value : *EventClaims)
	{
		const TSharedPtr<FJsonObject>* Claim = nullptr; FString Action, Target, Method, Status;
		if (!Value->TryGetObject(Claim) || (*Claim)->Values.Num() != 4
			|| !(*Claim)->TryGetStringField(TEXT("action"), Action) || !(*Claim)->TryGetStringField(TEXT("target"), Target)
			|| !(*Claim)->TryGetStringField(TEXT("method"), Method) || !(*Claim)->TryGetStringField(TEXT("status"), Status)
			|| (Status != TEXT("completed") && Status != TEXT("not_completed") && Status != TEXT("in_progress")))
		{ Error = TEXT("verification_event_schema"); return false; }
		bool Completed = false;
		if (Action == TEXT("treatment"))
		{
			if ((Target != TEXT("gu_heng") && Target != TEXT("ye_cheng") && Target != TEXT("player"))
				|| (Method != TEXT("full") && Method != TEXT("bandage") && Method != TEXT("temporary_support")
					&& Method != TEXT("initial") && Method != TEXT("unspecified")))
			{ Error = TEXT("verification_event_target"); return false; }
			const auto Id = Target == TEXT("gu_heng") ? EWSCharacterId::GuHeng : Target == TEXT("ye_cheng") ? EWSCharacterId::YeCheng : EWSCharacterId::Player;
			const auto View = FWSKnowledgePolicy::CharacterState(Id, Prepared.ReadSnapshot, true);
			Completed = Method == TEXT("full") ? View.TreatmentStatus == TEXT("completed")
				: Method == TEXT("bandage") ? View.bBandaged : Method == TEXT("temporary_support") ? View.bTemporarySupport
				: Method == TEXT("unspecified") && View.TreatmentStatus != TEXT("not_started") && View.TreatmentStatus != TEXT("unknown");
			for (const auto& Event : Prepared.ReadSnapshot.EventLog)
			{
				if (!Event.bHasActionProvenance || Event.TargetCharacter != Id
					|| (Event.ActionId != TEXT("treat_character") && Event.ActionId != TEXT("treat_gu_heng"))) continue;
				Completed |= Method == TEXT("unspecified") || (Method == TEXT("full") && Event.TreatmentMethod == EWSTreatmentMethod::Full)
					|| (Method == TEXT("bandage") && Event.TreatmentMethod == EWSTreatmentMethod::Bandage)
					|| (Method == TEXT("temporary_support") && Event.TreatmentMethod == EWSTreatmentMethod::HeatPack);
			}
		}
		else if (Action == TEXT("inspection") && Method.IsEmpty())
		{
			const bool Cabinet = Prepared.ReadSnapshot.ActionCounts.FindRef(TEXT("inspect_control_cabinet")) > 0;
			const bool Log = Prepared.ReadSnapshot.ActionCounts.FindRef(TEXT("investigate_generator_log")) > 0;
			if (Target != TEXT("control_cabinet") && Target != TEXT("generator_log") && Target != TEXT("unspecified"))
			{ Error = TEXT("verification_event_target"); return false; }
			Completed = Target == TEXT("control_cabinet") ? Cabinet : Target == TEXT("generator_log") ? Log : Cabinet || Log;
		}
		else if (Action == TEXT("repair") && Target == TEXT("generator") && Method.IsEmpty())
			Completed = Prepared.ReadSnapshot.Tasks.GeneratorProgress >= Prepared.RoleplayRequest.SubjectiveState.GeneratorRequired;
		else if (Action == TEXT("rest") && (Target == TEXT("player") || Target == TEXT("gu_heng") || Target == TEXT("ye_cheng"))
			&& (Method.IsEmpty() || Method == TEXT("heated")))
		{
			const auto Id = Target == TEXT("gu_heng") ? EWSCharacterId::GuHeng : Target == TEXT("ye_cheng") ? EWSCharacterId::YeCheng : EWSCharacterId::Player;
			Completed = Prepared.ReadSnapshot.EventLog.ContainsByPredicate([&](const auto& Event)
			{ return Event.ActionRulesSchema >= 8 && Event.ActionId == TEXT("rest") && Event.TargetCharacter == Id && (Method.IsEmpty() || Event.bHeatedRest); });
		}
		else if (Action == TEXT("repair_preparation") && Target == TEXT("gu_heng") && (Method == TEXT("granted") || Method == TEXT("consumed") || Method == TEXT("available")))
		{
			Completed = Method == TEXT("available") ? Prepared.ReadSnapshot.bRepairPreparationAvailable : Prepared.ReadSnapshot.EventLog.ContainsByPredicate([&](const auto& Event)
			{ return Method == TEXT("granted") ? Event.bRepairPreparationGranted : Event.bRepairPreparationConsumed; });
		}
		else { Error = TEXT("verification_event_action"); return false; }
		// Actions in this game commit atomically; there is no ongoing autonomous medical/inspection task.
		if (Status == TEXT("in_progress") || (Status == TEXT("completed")) != Completed)
		{ Error = TEXT("verification_event_state_mismatch"); return false; }
	}
	TArray<FString> Authorized;
	for (const auto& Pair : Prepared.NaturalFacts) Authorized.Add(Pair.Key.ToString());
	if (!UniqueSubset(Expressed, Authorized)) { Error = TEXT("verification_fact_mismatch"); return false; }
	if (!Correction.IsEmpty())
	{
		if (!FGuid::Parse(Correction, Outcome.CorrectsEntryId)
			|| !Prepared.ReadSnapshot.ConversationHistory.ContainsByPredicate([&](const auto& Entry)
				{ return Entry.EntryId == Outcome.CorrectsEntryId && Entry.SpeakerId == Prepared.RoleplayRequest.SpeakerId; }))
		{ Error = TEXT("verification_correction_invalid"); return false; }
	}
	for (const auto& Id : Expressed)
	{
		const auto& Fact = Prepared.NaturalFacts.FindChecked(FName(*Id));
		if (Fact.bCreatesGameFact && !Fact.GameFactId.IsNone()) Outcome.DisclosedFactIds.AddUnique(Fact.GameFactId);
	}
	Outcome.FinalReply.DisclosedFactIds = Outcome.DisclosedFactIds;
	Outcome.FinalReply.ReferencedFactIds = Outcome.DisclosedFactIds;
	Outcome.bFullTextVerified = true; Outcome.ValidationOutcome = TEXT("verified_full_text");
	Outcome.FinalReply.ValidationReason = Outcome.ValidationOutcome;
	Error = TEXT("ok"); return true;
}

bool FWSConversationValidator::ValidateCommitted(const FWSPreparedDialogue& Prepared, const FWSDialogueOutcome& Outcome, FString& Error)
{
	const auto& Reply = Outcome.FinalReply; const auto& Request = Prepared.OriginalRequest;
	if (!Prepared.bNaturalV16 || !Prepared.TransactionId.IsValid() || Prepared.TransactionId != Reply.TransactionId
		|| Prepared.TransactionId != Request.TransactionId || Reply.ActionId != Request.ActionId
		|| Reply.DialogueSessionId != Request.DialogueSessionId
		|| Reply.Speaker != (Request.ActionId == TEXT("talk_ye_cheng") ? EWSCharacterId::YeCheng : EWSCharacterId::GuHeng)
		|| Reply.Utterance.IsEmpty() || Reply.Utterance.Len() > 320
		|| Outcome.AnswerSource != TEXT("natural_roleplay_v16") || Reply.AnswerSource != Outcome.AnswerSource
		|| !UniqueSubset(Outcome.AddressedGoalIds, Prepared.AnswerGoalIds) || Outcome.AddressedGoalIds.Num() != Prepared.AnswerGoalIds.Num()
		|| (NeedsSemanticCheck(Prepared, Outcome) && !Outcome.bFullTextVerified))
	{ Error = TEXT("natural_unverified_or_identity_mismatch"); return false; }
	for (const FName Id : Reply.ReferencedKnowledgeIds)
		if (!Prepared.NaturalFacts.Contains(Id)) { Error = TEXT("natural_unauthorized_reference"); return false; }
	for (const FName Id : Outcome.DisclosedFactIds)
		if (!Prepared.AllowedFactIds.Contains(Id) || !Outcome.bFullTextVerified)
		{ Error = TEXT("natural_unauthorized_disclosure"); return false; }
	if (Reply.DisclosedFactIds != Outcome.DisclosedFactIds || Reply.ReferencedFactIds != Outcome.DisclosedFactIds)
	{ Error = TEXT("natural_disclosure_mismatch"); return false; }
	Error = TEXT("accepted"); return true;
}
