#include "Dialogue/WSDialogueV15Types.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
	const TSet<FString> Fields = {TEXT("speaker_id"), TEXT("topic_id"), TEXT("speech_act"),
		TEXT("query_type"), TEXT("target_action_id"), TEXT("target_character"), TEXT("polarity"),
		TEXT("commitment"), TEXT("promise_condition"), TEXT("confidence"),
		TEXT("needs_clarification"), TEXT("evidence_spans"), TEXT("resolved_from_turn")};
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
	const TSet<FString> Promises = {TEXT(""), TEXT("heat_repair_room"), TEXT("keep_records"), TEXT("reserve_medicine")};
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
	Candidate.Polarity = Polarities.FindChecked(PolarityText);
	Candidate.Commitment = Commitments.FindChecked(CommitmentText);
	Candidate.PromiseCondition = FName(*Promise);
	Candidate.ResolvedFromTurn = static_cast<int32>(ContextTurn);
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
	if (bPromise && (Candidate.PromiseCondition.IsNone() || Candidate.Commitment == EWSCommitmentIntent::None))
		Candidate.bNeedsClarification = true;
	if (!bPromise && !Candidate.PromiseCondition.IsNone()) Candidate.bNeedsClarification = true;
	if (Candidate.Polarity != EWSIntentPolarity::Affirmative)
	{
		// Non-asserted speech cannot register a promise or trigger a behavior penalty.
		Candidate.Frame.SpeechAct = EWSDialogueAct::Ask;
		Candidate.Commitment = EWSCommitmentIntent::None;
		Candidate.PromiseCondition = NAME_None;
		Candidate.bNeedsClarification = true;
	}
	Out = MoveTemp(Candidate);
	Error = TEXT("ok");
	return true;
}

bool FWSCanonicalIntent::CanPlan() const
{
	return !bNeedsClarification && Frame.Confidence >= 0.75f;
}

void FWSCanonicalIntent::ApplyTo(FWSActionRequest& Request) const
{
	Request.DialogueAct = Frame.SpeechAct;
	Request.SemanticFrame = Frame;
	Request.PromiseCondition = PromiseCondition;
	Request.SemanticFrame.TopicId = TopicId;
	Request.SemanticFrame.DeadlineSeconds = DeadlineSeconds;
	Request.SemanticFrame.bCanonicalIntentValidated = true;
	if (TopicId == TEXT("medical")) Request.SemanticFrame.TargetFactId = TEXT("FACT_HAND_INJURY");
	else if (TopicId == TEXT("medical_alternative")) Request.SemanticFrame.TargetFactId = TEXT("FACT_HEAT_PACK");
	else if (TopicId == TEXT("relay_alternative")) Request.SemanticFrame.TargetFactId = TEXT("FACT_RELAY_COMPATIBILITY");
	else if (TopicId == TEXT("restart_evidence")) Request.SemanticFrame.TargetFactId = TEXT("FACT_FORCED_RESTART_CONFIRMED");
}
