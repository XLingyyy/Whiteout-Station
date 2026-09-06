#include "Dialogue/WSAuthoredDialogueRepository.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace WSAuthoredPrivate
{
	bool Read(const FString& Path, TArray<TSharedPtr<FJsonValue>>& Out)
	{
		FString Text;
		return FFileHelper::LoadFileToString(Text, *Path)
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out);
	}
	bool Strings(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<FString>& Out)
	{
		return Object->TryGetStringArrayField(Field, Out);
	}
}

bool FWSAuthoredDialogueRepository::Load(const FString& Directory, FString& Error)
{
	bAvailable = false;
	Choices.Reset(); Lines.Reset(); Claims.Reset();
	TArray<TSharedPtr<FJsonValue>> ChoiceJson, LineJson, ClaimJson;
	if (!WSAuthoredPrivate::Read(Directory / TEXT("AuthoredChoices.json"), ChoiceJson)
		|| !WSAuthoredPrivate::Read(Directory / TEXT("AuthoredLines.json"), LineJson)
		|| !WSAuthoredPrivate::Read(Directory / TEXT("DialogueClaims.json"), ClaimJson))
	{ Error = TEXT("authored_files_invalid"); return false; }
	for (const auto& Value : ClaimJson)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FString Id, Knowledge, Text;
		if (!Value->TryGetObject(Object) || !(*Object)->TryGetStringField(TEXT("claim_id"), Id)
			|| !(*Object)->TryGetStringField(TEXT("knowledge_id"), Knowledge)
			|| !(*Object)->TryGetStringField(TEXT("text"), Text)
			|| Id.IsEmpty() || Knowledge.IsEmpty() || Text.IsEmpty() || Claims.Contains(FName(*Id)))
		{ Error = TEXT("authored_claim_invalid"); return false; }
		Claims.Add(FName(*Id), {FName(*Id), FName(*Knowledge), Text});
	}
	TSet<FName> ChoiceIds, LineIds;
	for (const auto& Value : ChoiceJson)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value->TryGetObject(Object)) { Error = TEXT("authored_choice_invalid"); return false; }
		const auto& O = *Object;
		FString Id, Speaker, Group, Text, Repeat;
		const TSharedPtr<FJsonObject>* IntentObject = nullptr;
		FWSAuthoredChoice Choice;
		if (!O->TryGetStringField(TEXT("choice_id"), Id) || Id.IsEmpty()
			|| !O->TryGetStringField(TEXT("speaker_id"), Speaker)
			|| !O->TryGetStringField(TEXT("text"), Text) || Text.IsEmpty()
			|| !O->TryGetStringField(TEXT("line_group_id"), Group) || Group.IsEmpty()
			|| !O->TryGetStringField(TEXT("repeat_policy"), Repeat) || Repeat != TEXT("recap_no_bonus")
			|| !O->TryGetObjectField(TEXT("intent"), IntentObject)
			|| !WSAuthoredPrivate::Strings(O, TEXT("visible_if"), Choice.VisibleIf)
			|| !WSAuthoredPrivate::Strings(O, TEXT("enabled_if"), Choice.EnabledIf)
			|| ChoiceIds.Contains(FName(*Id)))
		{ Error = TEXT("authored_choice_invalid"); return false; }
		FString IntentJson;
		FJsonSerializer::Serialize((*IntentObject).ToSharedRef(), TJsonWriterFactory<>::Create(&IntentJson));
		if (!FWSCanonicalIntent::Parse(IntentJson, FName(*Speaker), Text, 0, Choice.Intent, Error)
			|| !Choice.Intent.CanPlan()) return false;
		Choice.ChoiceId = FName(*Id); Choice.SpeakerId = FName(*Speaker);
		Choice.Text = Text; Choice.LineGroupId = FName(*Group);
		for (const auto& Predicate : Choice.VisibleIf)
			if (!IsPredicateRegistered(Predicate)) { Error = TEXT("authored_predicate_unknown"); return false; }
		for (const auto& Predicate : Choice.EnabledIf)
			if (!IsPredicateRegistered(Predicate)) { Error = TEXT("authored_predicate_unknown"); return false; }
		ChoiceIds.Add(Choice.ChoiceId); Choices.Add(MoveTemp(Choice));
	}
	TSet<FString> Priorities;
	for (const auto& Value : LineJson)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value->TryGetObject(Object)) { Error = TEXT("authored_line_invalid"); return false; }
		const auto& O = *Object;
		FWSAuthoredLine Line;
		FString Id, Group;
		TArray<FString> ClaimIds;
		double Priority = 0;
		if (!O->TryGetStringField(TEXT("line_id"), Id) || Id.IsEmpty()
			|| !O->TryGetStringField(TEXT("line_group_id"), Group) || Group.IsEmpty()
			|| !O->TryGetNumberField(TEXT("priority"), Priority) || !FMath::IsFinite(Priority)
			|| Priority < 0 || Priority > 1000 || Priority != FMath::FloorToDouble(Priority)
			|| !O->TryGetStringField(TEXT("npc_line"), Line.Text) || Line.Text.IsEmpty()
			|| !O->TryGetStringField(TEXT("reaction"), Line.Reaction)
			|| !WSAuthoredPrivate::Strings(O, TEXT("when"), Line.When)
			|| !WSAuthoredPrivate::Strings(O, TEXT("claim_ids"), ClaimIds)
			|| LineIds.Contains(FName(*Id)))
		{ Error = TEXT("authored_line_invalid"); return false; }
		if (Line.Reaction != TEXT("consider") && Line.Reaction != TEXT("acknowledge")
			&& Line.Reaction != TEXT("reassure") && Line.Reaction != TEXT("reject"))
		{ Error = TEXT("authored_reaction_invalid"); return false; }
		Line.LineId = FName(*Id); Line.GroupId = FName(*Group); Line.Priority = static_cast<int32>(Priority);
		const FString PriorityKey = FString::Printf(TEXT("%s:%d"), *Group, Line.Priority);
		if (Priorities.Contains(PriorityKey)) { Error = TEXT("authored_priority_conflict"); return false; }
		Priorities.Add(PriorityKey);
		for (const auto& Predicate : Line.When)
			if (!IsPredicateRegistered(Predicate)) { Error = TEXT("authored_predicate_unknown"); return false; }
		for (const FString& Claim : ClaimIds)
		{
			if (!Claims.Contains(FName(*Claim)) || Line.ClaimIds.Contains(FName(*Claim))
				|| !Line.Text.Contains(TEXT("{claim:") + Claim + TEXT("}")))
			{ Error = TEXT("authored_claim_reference_invalid"); return false; }
			Line.ClaimIds.Add(FName(*Claim));
		}
		LineIds.Add(Line.LineId); Lines.Add(MoveTemp(Line));
	}
	for (const FWSAuthoredChoice& Choice : Choices)
	{
		if (!Lines.ContainsByPredicate([&](const FWSAuthoredLine& Line)
			{ return Line.GroupId == Choice.LineGroupId && Line.When.IsEmpty() && Line.ClaimIds.IsEmpty(); }))
		{ Error = TEXT("authored_group_missing_fallback"); return false; }
	}
	Lines.Sort([](const FWSAuthoredLine& A, const FWSAuthoredLine& B) { return A.Priority > B.Priority; });
	bAvailable = !Choices.IsEmpty(); Error = bAvailable ? TEXT("ok") : TEXT("authored_choices_empty");
	return bAvailable;
}

bool FWSAuthoredDialogueRepository::IsPredicateRegistered(const FString& Predicate)
{
	static const TSet<FString> Predicates = {TEXT("always"), TEXT("actual_evidence"), TEXT("future_phase"),
		TEXT("generator_unstarted"), TEXT("generator_partial"), TEXT("generator_complete"),
		TEXT("gu_diagnosed"), TEXT("gu_treated"), TEXT("heating_locked"), TEXT("dialogue_turn_available")};
	return Predicates.Contains(Predicate);
}

bool FWSAuthoredDialogueRepository::Matches(const TArray<FString>& Predicates,
	const FWSGameState& State, const FWSRoleplayRequest* Context) const
{
	for (const FString& P : Predicates)
	{
		if (P == TEXT("always")) continue;
		if (P == TEXT("actual_evidence") && State.Evidence.Contains(TEXT("EVIDENCE_DEEP_GENERATOR_LOG"))
			&& State.Evidence.Contains(TEXT("EVIDENCE_BURNT_RELAY"))) continue;
		if (P == TEXT("future_phase") && State.DayPhase < EWSDayPhase::Dusk) continue;
		const int32 Required = Context ? Context->SubjectiveState.GeneratorRequired : 2;
		if (P == TEXT("generator_unstarted") && State.Tasks.GeneratorProgress == 0) continue;
		if (P == TEXT("generator_partial") && State.Tasks.GeneratorProgress > 0 && State.Tasks.GeneratorProgress < Required) continue;
		if (P == TEXT("generator_complete") && State.Tasks.GeneratorProgress >= Required) continue;
		if (P == TEXT("gu_diagnosed") && State.Flags.bGuHengDiagnosed) continue;
		if (P == TEXT("gu_treated") && State.Flags.bGuHengTreated) continue;
		if (P == TEXT("heating_locked") && State.Heating.bLocked) continue;
		if (P == TEXT("dialogue_turn_available") && (!Context || Context->TurnIndex <= 3)) continue;
		return false;
	}
	return true;
}

const FWSAuthoredChoice* FWSAuthoredDialogueRepository::FindChoice(FName Id) const
{
	return bAvailable ? Choices.FindByPredicate([Id](const FWSAuthoredChoice& C) { return C.ChoiceId == Id; }) : nullptr;
}

const FWSAuthoredChoice* FWSAuthoredDialogueRepository::FindEquivalent(FName Speaker, const FWSDialogueSemanticFrame& Frame) const
{
	return Choices.FindByPredicate([&](const FWSAuthoredChoice& C)
	{
		return C.SpeakerId == Speaker && C.Intent.TopicId == Frame.TopicId
			&& C.Intent.Frame.SpeechAct == Frame.SpeechAct
			&& C.Intent.Frame.QueryType == Frame.QueryType
			&& C.Intent.Frame.TargetCharacter == Frame.TargetCharacter
			&& C.Intent.Frame.TargetActionId == Frame.TargetActionId;
	});
}

TArray<FWSAuthoredChoice> FWSAuthoredDialogueRepository::GetChoices(FName Speaker, const FWSGameState& State) const
{
	return Choices.FilterByPredicate([&](const FWSAuthoredChoice& C)
		{ return bAvailable && C.SpeakerId == Speaker && Matches(C.VisibleIf, State); });
}

bool FWSAuthoredDialogueRepository::CanSelect(const FWSAuthoredChoice& Choice, FName Speaker, const FWSGameState& State) const
{
	return bAvailable && Choice.SpeakerId == Speaker && Matches(Choice.VisibleIf, State) && Matches(Choice.EnabledIf, State);
}

bool FWSAuthoredDialogueRepository::RenderClaim(FName Id, const FWSRoleplayRequest& Context,
	FString& Text, FName& KnowledgeId) const
{
	const FWSDialogueClaim* Claim = Claims.Find(Id);
	if (!Claim) return false;
	const FWSRoleplayKnowledgeItem* Knowledge = Context.AvailableKnowledge.FindByPredicate(
		[&](const FWSRoleplayKnowledgeItem& K) { return K.KnowledgeId == Claim->KnowledgeId; });
	if (!Knowledge || Knowledge->MaxDisclosure != EWSRoleplayDisclosureLevel::Explicit
		|| Knowledge->EpistemicStatus != EWSEpistemicStatus::Known) return false;
	Text = Claim->Text; KnowledgeId = Claim->KnowledgeId;
	return true;
}

bool FWSAuthoredDialogueRepository::SelectLine(const FWSAuthoredChoice& Choice, const FWSGameState& State,
	const FWSRoleplayRequest& Context, FWSRoleplayFallback& Out, FString& Error) const
{
	for (const FWSAuthoredLine& Line : Lines)
	{
		if (Line.GroupId != Choice.LineGroupId || !Matches(Line.When, State, &Context)) continue;
		FWSRoleplayFallback Candidate;
		Candidate.FallbackId = Line.LineId; Candidate.SpeakerId = Choice.SpeakerId;
		Candidate.TargetSubjectId = Context.TargetSubjectId;
		Candidate.SpeechFunction = EWSRoleplaySpeechFunction::Answer;
		Candidate.Line = Line.Text;
		bool Valid = true;
		for (const FName Claim : Line.ClaimIds)
		{
			FString Text; FName Knowledge;
			if (!RenderClaim(Claim, Context, Text, Knowledge)) { Valid = false; break; }
			Candidate.Line.ReplaceInline(*(TEXT("{claim:") + Claim.ToString() + TEXT("}")), *Text);
			Candidate.ReferencedKnowledgeIds.AddUnique(Knowledge);
			FWSRoleplayAssertion Assertion; Assertion.KnowledgeId = Knowledge; Assertion.Mode = EWSRoleplayClaimMode::Stated;
			Candidate.Assertions.Add(Assertion);
		}
		if (!Valid) continue;
		if (Candidate.Line.Contains(TEXT("{claim:"))) { Error = TEXT("authored_unexpanded_claim"); return false; }
		Out = MoveTemp(Candidate); Error = TEXT("ok"); return true;
	}
	Error = TEXT("authored_no_matching_line"); return false;
}
