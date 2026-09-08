#include "Dialogue/WSDialogueV15Types.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FWSDialogueOutcome FWSDialogueOutcome::Combine(const TArray<FWSDialogueOutcome>& InParts, const FString& Notice)
{
	FWSDialogueOutcome Result;
	if (InParts.IsEmpty()) return Result;
	Result = InParts[0]; Result.Parts = InParts;
	Result.FinalReply.Utterance.Reset(); Result.FinalReply.Assertions.Reset();
	Result.FinalReply.ReferencedKnowledgeIds.Reset(); Result.DisclosedFactIds.Reset();
	Result.FinalReply.bFallback = false;
	for (const auto& Part : InParts)
	{
		if (!Result.FinalReply.Utterance.IsEmpty()) Result.FinalReply.Utterance += TEXT("\n");
		Result.FinalReply.Utterance += Part.FinalReply.Utterance;
		for (FName Id : Part.DisclosedFactIds) Result.DisclosedFactIds.AddUnique(Id);
		for (FName Id : Part.FinalReply.ReferencedKnowledgeIds) Result.FinalReply.ReferencedKnowledgeIds.AddUnique(Id);
		Result.FinalReply.Assertions.Append(Part.FinalReply.Assertions);
		Result.FinalReply.bFallback |= Part.FinalReply.bFallback;
	}
	Result.FinalReply.SystemNotice = Notice;
	Result.FinalReply.DisclosedFactIds = Result.DisclosedFactIds;
	Result.FinalReply.ReferencedFactIds = Result.DisclosedFactIds;
	Result.AnswerSource = Result.FinalReply.bFallback ? TEXT("mixed_authored_recovery_v15") : TEXT("controlled_multi_v15");
	if (InParts.Num() == 1) Result.AnswerSource = InParts[0].AnswerSource;
	Result.FinalReply.AnswerSource = Result.AnswerSource;
	Result.FinalReply.MemorySummary = TEXT("完成一条含多个意图的交谈。");
	Result.ValidationOutcome = TEXT("ok");
	return Result;
}

bool FWSCanonicalIntent::Parse(const FString& Json, const FName ExpectedSpeaker,
	const FString& PlayerText, const int32 LatestContextTurn,
	FWSCanonicalIntent& Out, FString& Error)
{
	Out = FWSCanonicalIntent();
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root)
	{
		Error = TEXT("intent_invalid_json");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* IntentArray = nullptr;
	if (Root->TryGetArrayField(TEXT("intents"), IntentArray))
	{
		if (Root->Values.Num() != 1 || IntentArray->IsEmpty() || IntentArray->Num() > 6)
		{ Error = TEXT("intent_invalid_parts"); return false; }
		TArray<FWSCanonicalIntent> Parts;
		for (const auto& Value : *IntentArray)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value->TryGetObject(Object) || (*Object)->HasField(TEXT("intents")))
			{ Error = TEXT("intent_invalid_part"); return false; }
			FString PartJson;
			FJsonSerializer::Serialize((*Object).ToSharedRef(), TJsonWriterFactory<>::Create(&PartJson));
			FWSCanonicalIntent Part;
			if (!Parse(PartJson, ExpectedSpeaker, PlayerText, LatestContextTurn, Part, Error)) return false;
			Parts.Add(MoveTemp(Part));
		}
		Out = Parts[0]; Out.Parts = MoveTemp(Parts);
		Error = TEXT("ok"); return true;
	}
	const TSet<FString> Fields = {TEXT("speaker_id"), TEXT("topic_id"), TEXT("speech_act"),
		TEXT("query_type"), TEXT("target_action_id"), TEXT("target_character"), TEXT("polarity"),
		TEXT("commitment"), TEXT("promise_condition"), TEXT("confidence"),
		TEXT("needs_clarification"), TEXT("evidence_spans"), TEXT("resolved_from_turn"),
		TEXT("clarification"), TEXT("terms"), TEXT("proposal_id"), TEXT("proposal_version"), TEXT("question_purpose"), TEXT("requested_actor_id")};
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
	{
		if (!Fields.Contains(Pair.Key)) { Error = TEXT("intent_unexpected_field"); return false; }
	}
	FString Speaker, Topic, Act, Query, Action, Target, PolarityText, CommitmentText, Promise;
	double Confidence = 0, ContextTurn = 0;
	bool Clarify = false;
	const TArray<TSharedPtr<FJsonValue>>* Spans = nullptr;
	if (!Root->TryGetStringField(TEXT("speaker_id"), Speaker)
		|| !Root->TryGetStringField(TEXT("topic_id"), Topic)
		|| !Root->TryGetStringField(TEXT("speech_act"), Act)
		|| !Root->TryGetStringField(TEXT("query_type"), Query)
		|| !Root->TryGetStringField(TEXT("target_action_id"), Action)
		|| !Root->TryGetStringField(TEXT("target_character"), Target)
		|| !Root->TryGetStringField(TEXT("polarity"), PolarityText)
		|| !Root->TryGetStringField(TEXT("commitment"), CommitmentText)
		|| !Root->TryGetStringField(TEXT("promise_condition"), Promise)
		|| !Root->TryGetNumberField(TEXT("confidence"), Confidence)
		|| !Root->TryGetBoolField(TEXT("needs_clarification"), Clarify)
		|| !Root->TryGetArrayField(TEXT("evidence_spans"), Spans)
		|| !Root->TryGetNumberField(TEXT("resolved_from_turn"), ContextTurn))
	{ Error = TEXT("intent_missing_field"); return false; }
	const TMap<FString, EWSDialogueAct> Acts = {
		{TEXT("ask"), EWSDialogueAct::Ask}, {TEXT("challenge"), EWSDialogueAct::Challenge},
		{TEXT("command"), EWSDialogueAct::Command}, {TEXT("promise"), EWSDialogueAct::Promise},
		{TEXT("trade"), EWSDialogueAct::Trade}, {TEXT("reassure"), EWSDialogueAct::Reassure}};
	const TMap<FString, EWSDialogueQueryType> Queries = {
		{TEXT("unknown"), EWSDialogueQueryType::Unknown}, {TEXT("status"), EWSDialogueQueryType::Status},
		{TEXT("requirements"), EWSDialogueQueryType::Requirements}, {TEXT("cause"), EWSDialogueQueryType::Cause},
		{TEXT("alternative"), EWSDialogueQueryType::Alternative}, {TEXT("evidence"), EWSDialogueQueryType::Evidence},
		{TEXT("consequence"), EWSDialogueQueryType::Consequence}};
	const TMap<FString, EWSCharacterId> Characters = {
		{TEXT("gu_heng"), EWSCharacterId::GuHeng}, {TEXT("ye_cheng"), EWSCharacterId::YeCheng},
		{TEXT("player"), EWSCharacterId::Player}};
	const TMap<FString, EWSIntentPolarity> Polarities = {
		{TEXT("affirmative"), EWSIntentPolarity::Affirmative}, {TEXT("negated"), EWSIntentPolarity::Negated},
		{TEXT("hypothetical"), EWSIntentPolarity::Hypothetical}, {TEXT("quoted"), EWSIntentPolarity::Quoted}};
	const TMap<FString, EWSCommitmentIntent> Commitments = {
		{TEXT("none"), EWSCommitmentIntent::None}, {TEXT("proposed"), EWSCommitmentIntent::Proposed},
		{TEXT("confirm_pending"), EWSCommitmentIntent::ConfirmPending},
		{TEXT("reject_pending"), EWSCommitmentIntent::RejectPending}};
	const TSet<FString> Topics = {TEXT("person"), TEXT("status"), TEXT("generator"),
		TEXT("repair_requirements"), TEXT("medical"), TEXT("medical_alternative"),
		TEXT("restart_evidence"), TEXT("relay_alternative"), TEXT("heating"),
		TEXT("relationship"), TEXT("commitment"), TEXT("rescue"), TEXT("unknown")};
	const TSet<FString> Actions = {TEXT(""), TEXT("repair_generator"), TEXT("treat_gu_heng"),
		TEXT("treat_character"), TEXT("calibrate_antenna"), TEXT("send_signal"), TEXT("salvage_kitchen_relay")};
	const TSet<FString> Promises = {TEXT(""), TEXT("heat_repair_room"), TEXT("heat_zone"), TEXT("unsupported"), TEXT("keep_records"), TEXT("reserve_medicine")};
	if (FName(*Speaker) != ExpectedSpeaker || (ExpectedSpeaker != TEXT("gu_heng") && ExpectedSpeaker != TEXT("ye_cheng"))
		|| !Acts.Contains(Act) || !Queries.Contains(Query) || !Characters.Contains(Target)
		|| !Polarities.Contains(PolarityText) || !Commitments.Contains(CommitmentText)
		|| !Topics.Contains(Topic) || !Actions.Contains(Action) || !Promises.Contains(Promise)
		|| !FMath::IsFinite(Confidence) || Confidence < 0 || Confidence > 1
		|| !FMath::IsFinite(ContextTurn) || ContextTurn < 0 || ContextTurn > LatestContextTurn
		|| ContextTurn != FMath::FloorToDouble(ContextTurn))
	{ Error = TEXT("intent_invalid_value"); return false; }
	FWSCanonicalIntent Candidate;
	Candidate.SpeakerId = ExpectedSpeaker;
	Candidate.TopicId = FName(*Topic);
	Candidate.Frame.SpeechAct = Acts.FindChecked(Act);
	Candidate.Frame.QueryType = Queries.FindChecked(Query);
	Candidate.Frame.TargetCharacter = Characters.FindChecked(Target);
	Candidate.Frame.TargetActionId = FName(*Action);
	Candidate.Frame.Confidence = Confidence;
	Candidate.Frame.Source = TEXT("canonical_v15");
	Root->TryGetStringField(TEXT("question_purpose"), Candidate.Frame.QuestionPurpose);
	FString Actor; Root->TryGetStringField(TEXT("requested_actor_id"), Actor);
	if (!Actor.IsEmpty() && !Characters.Contains(Actor)) { Error = TEXT("intent_invalid_actor"); return false; }
	Candidate.Frame.RequestedActorId = FName(*Actor);
	Candidate.Polarity = Polarities.FindChecked(PolarityText);
	Candidate.Commitment = Commitments.FindChecked(CommitmentText);
	Candidate.PromiseCondition = FName(*Promise);
	Candidate.ResolvedFromTurn = static_cast<int32>(ContextTurn);
	Root->TryGetStringField(TEXT("clarification"), Candidate.Clarification);
	FString ProposalId;
	if (Root->TryGetStringField(TEXT("proposal_id"), ProposalId) && !ProposalId.IsEmpty()
		&& !FGuid::Parse(ProposalId, Candidate.ProposalId))
	{ Error = TEXT("intent_invalid_proposal_id"); return false; }
	Root->TryGetNumberField(TEXT("proposal_version"), Candidate.ProposalVersion);
	const TArray<TSharedPtr<FJsonValue>>* Terms = nullptr;
	if (Root->TryGetArrayField(TEXT("terms"), Terms))
	{
		for (const auto& Value : *Terms)
		{
			const TSharedPtr<FJsonObject>* Term = nullptr;
			FString Kind, Zone; FWSPromiseTerms Item;
			if (!Value->TryGetObject(Term) || (*Term)->Values.Num() != 5
				|| !(*Term)->TryGetStringField(TEXT("kind"), Kind)
				|| !(*Term)->TryGetStringField(TEXT("zone"), Zone)
				|| !(*Term)->TryGetNumberField(TEXT("phase_offset"), Item.PhaseOffset)
				|| !(*Term)->TryGetStringField(TEXT("prerequisite"), Item.Prerequisite)
				|| !(*Term)->TryGetStringField(TEXT("description"), Item.Description))
			{ Error = TEXT("intent_invalid_terms"); return false; }
			const TMap<FString, EWSHeatingZone> Zones = {{TEXT(""), EWSHeatingZone::None},
				{TEXT("repair_room"), EWSHeatingZone::RepairRoom}, {TEXT("medical_room"), EWSHeatingZone::MedicalRoom},
				{TEXT("kitchen"), EWSHeatingZone::Kitchen}, {TEXT("control_room"), EWSHeatingZone::ControlRoom}};
			if (!Zones.Contains(Zone) || Item.PhaseOffset < 0 || Item.PhaseOffset > 3)
			{ Error = TEXT("intent_invalid_terms"); return false; }
			Item.Kind = FName(*Kind); Item.Zone = Zones.FindChecked(Zone);
			Candidate.Terms.Add(MoveTemp(Item));
		}
	}
	Candidate.bNeedsClarification = Clarify || Confidence < 0.75 || Topic == TEXT("unknown")
		|| (Query == TEXT("requirements") && Action.IsEmpty());
	for (const auto& SpanValue : *Spans)
	{
		FString Span;
		if (!SpanValue->TryGetString(Span) || Span.IsEmpty() || !PlayerText.Contains(Span, ESearchCase::CaseSensitive))
		{ Error = TEXT("intent_invalid_evidence_span"); return false; }
		Candidate.EvidenceSpans.Add(Span);
	}
	if (Candidate.EvidenceSpans.IsEmpty()) Candidate.bNeedsClarification = true;
	const bool bPromise = Candidate.Frame.SpeechAct == EWSDialogueAct::Promise;
	if (bPromise && Candidate.PromiseCondition.IsNone() && Candidate.Terms.IsEmpty()
		&& Candidate.Commitment == EWSCommitmentIntent::Proposed)
		Candidate.bNeedsClarification = true;
	if (!bPromise && !Candidate.PromiseCondition.IsNone()) Candidate.bNeedsClarification = true;
	if (Candidate.Polarity != EWSIntentPolarity::Affirmative
		&& !(Candidate.Polarity == EWSIntentPolarity::Negated && Candidate.Commitment == EWSCommitmentIntent::RejectPending))
	{
		// Non-asserted speech cannot register a promise or trigger a behavior penalty.
		Candidate.Frame.SpeechAct = EWSDialogueAct::Ask;
		Candidate.Commitment = EWSCommitmentIntent::None;
		Candidate.PromiseCondition = NAME_None;
		Candidate.Terms.Reset();
		Candidate.bNeedsClarification |= bPromise || Candidate.Frame.QueryType == EWSDialogueQueryType::Unknown;
	}
	Out = MoveTemp(Candidate);
	Error = TEXT("ok");
	return true;
}

bool FWSCanonicalIntent::CanPlan() const
{
	if (!Parts.IsEmpty()) return Parts.ContainsByPredicate([](const FWSCanonicalIntent& Part) { return Part.CanPlan(); });
	const bool SocialPressure = Frame.SpeechAct == EWSDialogueAct::Command
		&& TopicId == TEXT("relationship") && Polarity == EWSIntentPolarity::Affirmative;
	return Frame.Confidence >= 0.75f && (!bNeedsClarification || SocialPressure);
}

void FWSCanonicalIntent::ApplyTo(FWSActionRequest& Request) const
{
	Request.OnlineMessageId = MessageId;
	Request.ConfirmProposalId = ProposalId;
	Request.ConfirmProposalVersion = ProposalVersion;
	Request.PromiseTerms = Terms;
	Request.DialogueNotice = Notice;
	const TCHAR* Polarities[] = {TEXT("affirmative"), TEXT("negated"), TEXT("hypothetical"), TEXT("quoted")};
	Request.DialoguePolarity = Polarities[static_cast<uint8>(Polarity)];
	Request.LocalClarification = Clarification; Request.bLocalClarification = bNeedsClarification;
	Request.bConfirmationClosure = bConfirmationClosure;
	for (const FWSCanonicalIntent& Part : Parts)
	{
		FWSActionRequest Child; Part.ApplyTo(Child);
		Child.PlayerSaid = FString::Join(Part.EvidenceSpans, TEXT(" "));
		Request.DialogueParts.Add(MoveTemp(Child));
	}
	Request.DialogueAct = Frame.SpeechAct;
	Request.SemanticFrame = Frame;
	Request.PromiseCondition = PromiseCondition;
	Request.SemanticFrame.TopicId = TopicId;
	Request.SemanticFrame.DeadlineSeconds = DeadlineSeconds;
	Request.SemanticFrame.bCanonicalIntentValidated = true;
	if (TopicId == TEXT("medical") && Frame.TargetCharacter == EWSCharacterId::GuHeng) Request.SemanticFrame.TargetFactId = TEXT("FACT_HAND_INJURY");
	else if (TopicId == TEXT("medical_alternative")) Request.SemanticFrame.TargetFactId = TEXT("FACT_HEAT_PACK");
	else if (TopicId == TEXT("relay_alternative")) Request.SemanticFrame.TargetFactId = TEXT("FACT_RELAY_COMPATIBILITY");
	else if (TopicId == TEXT("restart_evidence")) Request.SemanticFrame.TargetFactId = TEXT("FACT_FORCED_RESTART_CONFIRMED");
}
