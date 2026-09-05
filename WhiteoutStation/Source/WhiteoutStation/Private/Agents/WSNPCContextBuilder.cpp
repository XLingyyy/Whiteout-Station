#include "Agents/WSNPCContextBuilder.h"

#include "Agents/WSRoleplayKnowledgeRepository.h"

namespace WSNPCContextBuilderPrivate
{
	const FName GuHengId(TEXT("gu_heng"));
	const FName YeChengId(TEXT("ye_cheng"));

	bool ContainsAny(
		const FString& Text,
		std::initializer_list<const TCHAR*> Candidates)
	{
		for (const TCHAR* Candidate : Candidates)
		{
			if (Text.Contains(Candidate, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	FName SpeakerForAction(const FName ActionId)
	{
		return ActionId == TEXT("talk_ye_cheng") ? YeChengId : GuHengId;
	}

	EWSCharacterId CharacterForSpeaker(const FName SpeakerId)
	{
		return SpeakerId == YeChengId
			? EWSCharacterId::YeCheng
			: EWSCharacterId::GuHeng;
	}

	FName PhaseName(const EWSGamePhase Phase)
	{
		switch (Phase)
		{
		case EWSGamePhase::Opening: return TEXT("opening");
		case EWSGamePhase::ActionPhase: return TEXT("action_phase");
		case EWSGamePhase::ResolvingAction: return TEXT("resolving_action");
		case EWSGamePhase::DialogueFeedback: return TEXT("dialogue_feedback");
		case EWSGamePhase::MidCrisis: return TEXT("mid_crisis");
		case EWSGamePhase::PostActionWindow: return TEXT("post_action_window");
		case EWSGamePhase::EndingChoice: return TEXT("ending_choice");
		case EWSGamePhase::Ending: return TEXT("ending");
		case EWSGamePhase::Results: return TEXT("results");
		default: return TEXT("unknown");
		}
	}

	FName DayPhaseName(const EWSDayPhase Phase)
	{
		switch (Phase)
		{
		case EWSDayPhase::Morning: return TEXT("morning");
		case EWSDayPhase::Afternoon: return TEXT("afternoon");
		case EWSDayPhase::Dusk: return TEXT("dusk");
		case EWSDayPhase::Complete: return TEXT("complete");
		default: return TEXT("unknown");
		}
	}

	FName HeatingZoneName(const EWSHeatingZone Zone)
	{
		switch (Zone)
		{
		case EWSHeatingZone::RepairRoom: return TEXT("repair_room");
		case EWSHeatingZone::MedicalRoom: return TEXT("medical_room");
		case EWSHeatingZone::Kitchen: return TEXT("kitchen");
		case EWSHeatingZone::ControlRoom: return TEXT("control_room");
		case EWSHeatingZone::None: return TEXT("none");
		default: return TEXT("none");
		}
	}

	FName LocationName(const EWSCharacterLocation Location)
	{
		switch (Location)
		{
		case EWSCharacterLocation::ControlRoom: return TEXT("control_room");
		case EWSCharacterLocation::RepairRoom: return TEXT("repair_room");
		case EWSCharacterLocation::MedicalRoom: return TEXT("medical_room");
		case EWSCharacterLocation::Kitchen: return TEXT("kitchen");
		case EWSCharacterLocation::OutdoorAntenna: return TEXT("outdoor_antenna");
		default: return TEXT("unknown");
		}
	}

	bool PlayerKnowsFact(const FWSGameState& State, const FName FactId)
	{
		if (const EWSKnowledgeLevel* Level = State.PlayerKnowledge.Find(FactId))
		{
			if (static_cast<uint8>(*Level)
				>= static_cast<uint8>(EWSKnowledgeLevel::Confirmed))
			{
				return true;
			}
		}
		if (State.Evidence.Contains(FactId) || State.PublicFacts.Contains(FactId))
		{
			return true;
		}
		if (FactId == TEXT("FACT_MEDICAL_DIAGNOSIS"))
		{
			return State.Flags.bGuHengDiagnosed;
		}
		if (FactId == TEXT("FACT_HAND_INJURY"))
		{
			return State.Flags.bGuHengDiagnosed;
		}
		if (FactId == TEXT("FACT_HEAT_PACK"))
		{
			return State.Flags.bHeatPackRevealed;
		}
		if (FactId == TEXT("FACT_RELAY_COMPATIBILITY"))
		{
			return State.Flags.bRelayCompatibilityKnown;
		}
		return false;
	}

	bool IsHeatPackQuestion(const FWSDialogueSemanticFrame& Frame)
	{
		return Frame.TargetFactId == TEXT("FACT_HEAT_PACK")
			|| (Frame.QueryType == EWSDialogueQueryType::Alternative
				&& Frame.TargetCharacter == EWSCharacterId::GuHeng
				&& (Frame.TargetActionId == TEXT("repair_generator")
					|| Frame.TargetActionId == TEXT("treat_gu_heng")
					|| Frame.TargetActionId == TEXT("treat_character")));
	}

	bool EvaluateAvailabilityPredicate(
		const FString& Predicate,
		const FName SpeakerId,
		const FWSDialogueSemanticFrame& Frame,
		const FWSGameState& State)
	{
		if (Predicate == TEXT("always")) return true;
		if (Predicate == TEXT("ye_diagnosis_disclosable"))
		{
			return SpeakerId == YeChengId
				&& (State.Flags.bGuHengDiagnosed
					|| (Frame.SpeechAct == EWSDialogueAct::Ask
						&& Frame.TargetCharacter == EWSCharacterId::GuHeng
						&& Frame.TargetFactId == TEXT("FACT_HAND_INJURY")
						&& (Frame.QueryType == EWSDialogueQueryType::Status
							|| Frame.QueryType == EWSDialogueQueryType::Evidence)));
		}
		if (Predicate == TEXT("gu_heng_diagnosed")) return State.Flags.bGuHengDiagnosed;
		if (Predicate == TEXT("gu_heng_treated")) return State.Flags.bGuHengTreated;
		if (Predicate == TEXT("cabinet_inspected")) return State.Flags.bCabinetInspected;
		if (Predicate == TEXT("relay_compatibility_known")) return State.Flags.bRelayCompatibilityKnown;
		if (Predicate == TEXT("heat_pack_revealed")) return State.Flags.bHeatPackRevealed;
		if (Predicate == TEXT("heating_locked")) return State.Heating.bLocked;
		if (Predicate == TEXT("heating_unlocked")) return !State.Heating.bLocked;

		static const FString PlayerKnowsPrefix(TEXT("player_knows:"));
		static const FString PlayerMissingPrefix(TEXT("player_missing:"));
		if (Predicate.StartsWith(PlayerKnowsPrefix))
		{
			return PlayerKnowsFact(
				State,
				FName(*Predicate.RightChop(PlayerKnowsPrefix.Len())));
		}
		if (Predicate.StartsWith(PlayerMissingPrefix))
		{
			return !PlayerKnowsFact(
				State,
				FName(*Predicate.RightChop(PlayerMissingPrefix.Len())));
		}

		const FWSCharacterState Character = State.Characters.FindRef(
			CharacterForSpeaker(SpeakerId));
		if (Predicate == TEXT("ye_heat_pack_disclosable"))
		{
			const bool bDiagnosisKnown = PlayerKnowsFact(
				State,
				TEXT("FACT_MEDICAL_DIAGNOSIS"));
			return SpeakerId == YeChengId
				&& bDiagnosisKnown
				&& IsHeatPackQuestion(Frame)
				&& Character.Trust >= 5.5f
				&& Character.Pressure < 9.0f;
		}
		if (Predicate == TEXT("gu_restart_disclosable"))
		{
			const bool bHasLogEvidence = PlayerKnowsFact(
					State,
					TEXT("FACT_FORCED_RESTART_SUSPICION"))
				|| State.Evidence.Contains(TEXT("EVIDENCE_DEEP_GENERATOR_LOG"));
			const bool bHasRelayEvidence = PlayerKnowsFact(
					State,
					TEXT("FACT_BURNT_RELAY"))
				|| State.Evidence.Contains(TEXT("EVIDENCE_BURNT_RELAY"));
			return SpeakerId == GuHengId
				&& Frame.SpeechAct == EWSDialogueAct::Challenge
				&& bHasLogEvidence
				&& bHasRelayEvidence;
		}
		return false;
	}

	bool IsAvailable(
		const TArray<FString>& Predicates,
		const FName SpeakerId,
		const FWSDialogueSemanticFrame& Frame,
		const FWSGameState& State)
	{
		for (const FString& Predicate : Predicates)
		{
			if (!EvaluateAvailabilityPredicate(Predicate, SpeakerId, Frame, State))
			{
				return false;
			}
		}
		return true;
	}

	void AddTag(TArray<FName>& Tags, const TCHAR* Tag)
	{
		Tags.AddUnique(FName(Tag));
	}

	void ExtractTagsAndTarget(
		const FString& PlayerLine,
		const FName SpeakerId,
		TArray<FName>& OutEntityTags,
		TArray<FName>& OutTopicTags,
		FName& OutTargetSubjectId)
	{
		OutTargetSubjectId = TEXT("unknown");
		if (ContainsAny(PlayerLine, {TEXT("顾衡"), TEXT("gu heng"), TEXT("gu_heng")}))
		{
			AddTag(OutEntityTags, TEXT("gu_heng"));
			AddTag(OutEntityTags, TEXT("person"));
			OutTargetSubjectId = GuHengId;
		}
		if (ContainsAny(PlayerLine, {TEXT("叶澄"), TEXT("ye cheng"), TEXT("ye_cheng")}))
		{
			AddTag(OutEntityTags, TEXT("ye_cheng"));
			AddTag(OutEntityTags, TEXT("person"));
			OutTargetSubjectId = YeChengId;
		}
		if (ContainsAny(PlayerLine, {TEXT("我"), TEXT("玩家"), TEXT("player")}))
		{
			AddTag(OutEntityTags, TEXT("player"));
		}
		if (ContainsAny(PlayerLine, {TEXT("发电机"), TEXT("generator")}))
		{
			AddTag(OutEntityTags, TEXT("generator"));
			AddTag(OutEntityTags, TEXT("device"));
			AddTag(OutTopicTags, TEXT("generator"));
			AddTag(OutTopicTags, TEXT("repair"));
			if (OutTargetSubjectId == TEXT("unknown")) OutTargetSubjectId = TEXT("generator");
		}
		if (ContainsAny(PlayerLine, {TEXT("继电器"), TEXT("relay")}))
		{
			AddTag(OutEntityTags, TEXT("relay"));
			AddTag(OutEntityTags, TEXT("device"));
			AddTag(OutTopicTags, TEXT("relay"));
			AddTag(OutTopicTags, TEXT("repair"));
			if (OutTargetSubjectId == TEXT("unknown")) OutTargetSubjectId = TEXT("relay");
		}
		if (ContainsAny(PlayerLine, {TEXT("供暖"), TEXT("暖气"), TEXT("加热"), TEXT("heating"), TEXT("heater")}))
		{
			AddTag(OutEntityTags, TEXT("heating"));
			AddTag(OutEntityTags, TEXT("device"));
			AddTag(OutTopicTags, TEXT("heating"));
			AddTag(OutTopicTags, TEXT("temperature"));
			if (OutTargetSubjectId == TEXT("unknown")) OutTargetSubjectId = TEXT("heating");
		}
		if (ContainsAny(PlayerLine, {TEXT("维修间"), TEXT("repair room"), TEXT("repair_room")}))
		{
			AddTag(OutEntityTags, TEXT("repair_room"));
			AddTag(OutTopicTags, TEXT("repair"));
			AddTag(OutTopicTags, TEXT("equipment"));
			if (OutTargetSubjectId == TEXT("unknown")) OutTargetSubjectId = TEXT("repair_room");
		}
		if (ContainsAny(PlayerLine, {TEXT("医务室"), TEXT("medical room"), TEXT("medical_room")}))
		{
			AddTag(OutEntityTags, TEXT("medical_room"));
			AddTag(OutTopicTags, TEXT("medical"));
			AddTag(OutTopicTags, TEXT("treatment"));
			if (OutTargetSubjectId == TEXT("unknown")) OutTargetSubjectId = TEXT("medical_room");
		}
		if (ContainsAny(PlayerLine, {TEXT("厨房"), TEXT("kitchen")}))
		{
			AddTag(OutEntityTags, TEXT("kitchen"));
			AddTag(OutTopicTags, TEXT("heating"));
			AddTag(OutTopicTags, TEXT("supplies"));
			if (OutTargetSubjectId == TEXT("unknown")) OutTargetSubjectId = TEXT("kitchen");
		}
		if (ContainsAny(PlayerLine, {TEXT("控制室"), TEXT("control room"), TEXT("control_room")}))
		{
			AddTag(OutEntityTags, TEXT("control_room"));
			AddTag(OutTopicTags, TEXT("equipment"));
			AddTag(OutTopicTags, TEXT("communications"));
			if (OutTargetSubjectId == TEXT("unknown")) OutTargetSubjectId = TEXT("control_room");
		}
		if (ContainsAny(PlayerLine, {TEXT("暴雪"), TEXT("风雪"), TEXT("天气"), TEXT("weather"), TEXT("storm")}))
		{
			AddTag(OutEntityTags, TEXT("weather"));
			AddTag(OutTopicTags, TEXT("weather"));
			AddTag(OutTopicTags, TEXT("storm"));
			if (OutTargetSubjectId == TEXT("unknown")) OutTargetSubjectId = TEXT("weather");
		}
		if (ContainsAny(PlayerLine, {TEXT("救援"), TEXT("求救"), TEXT("信号"), TEXT("天线"), TEXT("rescue"), TEXT("signal"), TEXT("antenna")}))
		{
			AddTag(OutEntityTags, TEXT("rescue"));
			AddTag(OutTopicTags, TEXT("rescue"));
			AddTag(OutTopicTags, TEXT("signal"));
			if (OutTargetSubjectId == TEXT("unknown")) OutTargetSubjectId = TEXT("rescue");
		}
		if (ContainsAny(PlayerLine, {TEXT("关系"), TEXT("信任"), TEXT("过去"), TEXT("以前"), TEXT("relationship"), TEXT("trust")}))
		{
			AddTag(OutTopicTags, TEXT("relationship"));
			AddTag(OutTopicTags, TEXT("trust"));
		}
		if (ContainsAny(PlayerLine, {TEXT("手"), TEXT("伤"), TEXT("治疗"), TEXT("诊断"), TEXT("病"), TEXT("injury"), TEXT("medical"), TEXT("treat")}))
		{
			AddTag(OutTopicTags, TEXT("medical"));
			AddTag(OutTopicTags, TEXT("injury"));
		}
		if (ContainsAny(PlayerLine, {TEXT("食物"), TEXT("吃"), TEXT("补给"), TEXT("food"), TEXT("supplies")}))
		{
			AddTag(OutTopicTags, TEXT("supplies"));
			AddTag(OutTopicTags, TEXT("food"));
		}
		if (OutTargetSubjectId == TEXT("unknown")
			&& ContainsAny(
				PlayerLine,
				{TEXT("你是谁"), TEXT("你怎么样"), TEXT("你现在怎么样"),
					TEXT("你还好吗"), TEXT("你怎么了"), TEXT("你的情况"),
					TEXT("你的目标"), TEXT("你希望"), TEXT("你怎么看"),
					TEXT("about you"), TEXT("your goal"),
					TEXT("how are you")}))
		{
			OutTargetSubjectId = SpeakerId;
			OutEntityTags.AddUnique(SpeakerId);
		}
	}

	bool Intersects(const TArray<FName>& Left, const TArray<FName>& Right)
	{
		for (const FName Value : Left)
		{
			if (Right.Contains(Value))
			{
				return true;
			}
		}
		return false;
	}

	TArray<FName> BuildEventTags(const FWSRoleplaySubjectiveState& State)
	{
		TArray<FName> Tags{
			State.PhaseId,
			State.DayPhaseId,
			State.HeatingStateId,
			State.HeatingZoneId,
			State.GeneratorStateId};
		if (State.bHeatingLocked) Tags.AddUnique(TEXT("heating_locked"));
		else Tags.AddUnique(TEXT("heating_unlocked"));
		Tags.AddUnique(TEXT("heating"));
		if (State.GeneratorStateId == TEXT("offline"))
		{
			Tags.AddUnique(TEXT("generator"));
			Tags.AddUnique(TEXT("outage"));
			Tags.AddUnique(TEXT("power"));
		}
		else if (State.GeneratorStateId == TEXT("partially_repaired"))
		{
			Tags.AddUnique(TEXT("generator"));
			Tags.AddUnique(TEXT("repair"));
		}
		else if (State.GeneratorStateId == TEXT("stable"))
		{
			Tags.AddUnique(TEXT("generator"));
			Tags.AddUnique(TEXT("power"));
		}
		if (State.PhaseId == TEXT("mid_crisis"))
		{
			Tags.AddUnique(TEXT("crisis"));
		}
		return Tags;
	}

	bool MatchesMemory(
		const FWSRoleplayKnowledgeItem& Item,
		const TArray<FWSRoleplayMemoryEntry>& Memory)
	{
		for (const FWSRoleplayMemoryEntry& Entry : Memory)
		{
			if (Entry.KnowledgeIds.Contains(Item.KnowledgeId)
				|| (!Entry.TopicId.IsNone()
					&& (Item.TopicTags.Contains(Entry.TopicId)
						|| Item.CategoryId == Entry.TopicId)))
			{
				return true;
			}
		}
		return false;
	}

	bool IsRelationshipMatch(
		const FWSRoleplayKnowledgeItem& Item,
		const FName SpeakerId,
		const FName TargetSubjectId,
		const TArray<FName>& QueryTopics)
	{
		const bool bRelationshipQuery = QueryTopics.Contains(TEXT("relationship"))
			|| (TargetSubjectId != SpeakerId
				&& (TargetSubjectId == GuHengId || TargetSubjectId == YeChengId));
		return bRelationshipQuery
			&& (Item.CategoryId == TEXT("relationship")
				|| Item.TopicTags.Contains(TEXT("relationship"))
				|| Item.SubjectId == TEXT("relationship_gu_heng_ye_cheng"));
	}

	void AddProposalIfAllowed(
		const FWSRoleplayPolicy& Policy,
		FWSRoleplayActionProposal Proposal,
		TArray<FWSRoleplayActionProposal>& OutProposals)
	{
		if (Policy.AllowedProposalTypes.Contains(Proposal.Type))
		{
			OutProposals.Add(MoveTemp(Proposal));
		}
	}

	void BuildAllowedProposals(
		const FName SpeakerId,
		const FWSGameState& State,
		const FWSRoleplayPolicy& Policy,
		TArray<FWSRoleplayActionProposal>& OutProposals)
	{
		const FWSCharacterState Character = State.Characters.FindRef(
			CharacterForSpeaker(SpeakerId));
		if (SpeakerId == GuHengId && Character.Stamina <= 0)
		{
			FWSRoleplayActionProposal Refusal;
			Refusal.Type = EWSRoleplayProposalType::RefuseAction;
			Refusal.ActionId = TEXT("repair_generator");
			Refusal.RequestedConditionIds.Add(TEXT("gu_heng_needs_rest"));
			Refusal.ExpiresAtPhase = DayPhaseName(State.DayPhase);
			AddProposalIfAllowed(Policy, MoveTemp(Refusal), OutProposals);
		}
		else if (SpeakerId == GuHengId && !State.Flags.bGuHengCooperative)
		{
			FWSRoleplayActionProposal Cooperation;
			Cooperation.Type = EWSRoleplayProposalType::ConditionalCooperation;
			Cooperation.ActionId = TEXT("repair_generator");
			if (!State.Flags.bGuHengTreated)
			{
				Cooperation.RequestedConditionIds.Add(TEXT("gu_heng_treated"));
			}
			if (!State.Flags.bRepairRoomHeated)
			{
				Cooperation.RequestedConditionIds.Add(TEXT("repair_room_heated"));
			}
			if (!Cooperation.RequestedConditionIds.IsEmpty())
			{
				Cooperation.ExpiresAtPhase = TEXT("dusk");
				AddProposalIfAllowed(Policy, MoveTemp(Cooperation), OutProposals);
			}
		}

		FWSRoleplayActionProposal Suggestion;
		Suggestion.Type = EWSRoleplayProposalType::SuggestAction;
		bool bHasSuggestion = true;
		if (!State.Flags.bCabinetInspected)
		{
			Suggestion.ActionId = TEXT("inspect_control_cabinet");
		}
		else if (!State.Tasks.bGeneratorStable
			&& State.Tasks.GeneratorProgress < 2)
		{
			Suggestion.ActionId = TEXT("repair_generator");
		}
		else if (State.Tasks.AntennaCalibration < 1)
		{
			Suggestion.ActionId = TEXT("calibrate_antenna");
		}
		else if (!State.Tasks.bSignalSent)
		{
			Suggestion.ActionId = TEXT("send_signal");
		}
		else
		{
			bHasSuggestion = false;
		}
		if (bHasSuggestion)
		{
			Suggestion.ExpiresAtPhase = TEXT("dusk");
			AddProposalIfAllowed(Policy, MoveTemp(Suggestion), OutProposals);
		}
	}

	FString HeatingDescription(const FWSRoleplaySubjectiveState& State)
	{
		if (!State.bHeatingLocked)
		{
			return TEXT("供暖区尚未锁定");
		}
		if (State.HeatingZoneId == TEXT("repair_room")) return TEXT("供暖已锁定维修间");
		if (State.HeatingZoneId == TEXT("medical_room")) return TEXT("供暖已锁定医务室");
		if (State.HeatingZoneId == TEXT("kitchen")) return TEXT("供暖已锁定厨房");
		if (State.HeatingZoneId == TEXT("control_room")) return TEXT("供暖已锁定控制室");
		return TEXT("供暖已锁定但区域未知");
	}

	FString GeneratorDescription(const FWSRoleplaySubjectiveState& State)
	{
		if (State.GeneratorStateId == TEXT("stable")) return TEXT("发电机已稳定");
		if (State.GeneratorProgress <= 0) return TEXT("发电机维修尚未推进");
		return FString::Printf(
			TEXT("发电机维修进度为%d步"),
			State.GeneratorProgress);
	}

	void SelectFallback(
		const UWSRoleplayKnowledgeRepository& Repository,
		const FName SpeakerId,
		const FName TargetSubjectId,
		const TArray<FName>& TopicTags,
		const FWSDialogueSemanticFrame& Frame,
		const FWSGameState& State,
		const FWSRoleplaySubjectiveState& SubjectiveState,
		const TSet<FName>& AvailableKnowledgeIds,
		FWSRoleplayFallback& OutFallback)
	{
		struct FCandidate
		{
			FWSRoleplayFallback Fallback;
			int32 Score = 0;
		};
		TArray<FCandidate> Candidates;
		for (const FWSRoleplayFallback& Fallback : Repository.GetFallbacks())
		{
			const bool bSpeakerMatch = Fallback.SpeakerId == SpeakerId
				|| Fallback.SpeakerId == TEXT("any");
			const bool bGenericTarget = Fallback.TargetSubjectId == TEXT("any")
				|| (Fallback.TargetSubjectId == TEXT("unknown")
					&& TargetSubjectId == TEXT("unknown"))
				|| (Fallback.TargetSubjectId == TEXT("station")
					&& (TargetSubjectId == TEXT("station")
						|| Fallback.TopicTags.Contains(TargetSubjectId)
						|| Intersects(Fallback.TopicTags, TopicTags)));
			if (!bSpeakerMatch
				|| (Fallback.TargetSubjectId != TargetSubjectId && !bGenericTarget)
				|| !IsAvailable(Fallback.Availability, SpeakerId, Frame, State))
			{
				continue;
			}
			FCandidate Candidate;
			Candidate.Fallback = Fallback;
			Candidate.Score += Fallback.SpeakerId == SpeakerId ? 8 : 0;
			Candidate.Score += Fallback.TargetSubjectId == TargetSubjectId ? 5 : 0;
			for (const FName TopicTag : TopicTags)
			{
				if (Fallback.TopicTags.Contains(TopicTag)) Candidate.Score += 2;
			}
			Candidate.Fallback.ReferencedKnowledgeIds.RemoveAll(
				[&AvailableKnowledgeIds](const FName KnowledgeId)
				{
					return !AvailableKnowledgeIds.Contains(KnowledgeId);
				});
			Candidates.Add(MoveTemp(Candidate));
		}
		Candidates.Sort([](const FCandidate& Left, const FCandidate& Right)
		{
			if (Left.Score != Right.Score) return Left.Score > Right.Score;
			return Left.Fallback.FallbackId.LexicalLess(Right.Fallback.FallbackId);
		});
		if (!Candidates.IsEmpty())
		{
			OutFallback = MoveTemp(Candidates[0].Fallback);
		}
		else
		{
			OutFallback.FallbackId = TEXT("local_safe_fallback");
			OutFallback.SpeakerId = SpeakerId;
			OutFallback.TargetSubjectId = TargetSubjectId;
			OutFallback.SpeechFunction = EWSRoleplaySpeechFunction::Clarify;
			const auto SubjectLabel = [](const FName SubjectId)
			{
				if (SubjectId == TEXT("relay")) return FString(TEXT("继电器"));
				if (SubjectId == TEXT("repair_room")) return FString(TEXT("维修间"));
				if (SubjectId == TEXT("medical_room")) return FString(TEXT("医务室"));
				if (SubjectId == TEXT("kitchen")) return FString(TEXT("厨房"));
				if (SubjectId == TEXT("control_room")) return FString(TEXT("控制室"));
				if (SubjectId == TEXT("rescue")) return FString(TEXT("救援"));
				if (SubjectId == TEXT("weather")) return FString(TEXT("天气"));
				if (SubjectId == TEXT("player")) return FString(TEXT("你自己"));
				return FString(TEXT("这件事"));
			};
			const FString Label = SubjectLabel(TargetSubjectId);
			OutFallback.Line = SpeakerId == YeChengId
				? FString::Printf(
					TEXT("你想确认%s的哪一部分？我只说能确定的内容。"),
					*Label)
				: FString::Printf(
					TEXT("你想问%s的哪一部分？把问题说具体。"),
					*Label);
		}
		OutFallback.Line.ReplaceInline(
			TEXT("{heating_state}"),
			*HeatingDescription(SubjectiveState));
		OutFallback.Line.ReplaceInline(
			TEXT("{generator_state}"),
			*GeneratorDescription(SubjectiveState));
	}
}

bool UWSNPCContextBuilder::BuildRequest(
	const FWSActionRequest& ActionRequest,
	const FWSGameState& FrozenState,
	const UWSRoleplayKnowledgeRepository& Repository,
	const TArray<FWSRoleplayMemoryEntry>& RecentMemory,
	const int32 TurnIndex,
	FWSRoleplayRequest& OutRequest,
	FWSRoleplayFallback& OutFallback,
	FString& OutError)
{
	using WSNPCContextBuilderPrivate::BuildAllowedProposals;
	using WSNPCContextBuilderPrivate::BuildEventTags;
	using WSNPCContextBuilderPrivate::ExtractTagsAndTarget;
	using WSNPCContextBuilderPrivate::GuHengId;
	using WSNPCContextBuilderPrivate::Intersects;
	using WSNPCContextBuilderPrivate::IsAvailable;
	using WSNPCContextBuilderPrivate::IsRelationshipMatch;
	using WSNPCContextBuilderPrivate::MatchesMemory;
	using WSNPCContextBuilderPrivate::PlayerKnowsFact;
	using WSNPCContextBuilderPrivate::SelectFallback;
	using WSNPCContextBuilderPrivate::SpeakerForAction;
	using WSNPCContextBuilderPrivate::YeChengId;

	OutRequest = FWSRoleplayRequest();
	OutFallback = FWSRoleplayFallback();
	OutError.Reset();
	if (!Repository.IsAvailable())
	{
		OutError = TEXT("roleplay knowledge repository is unavailable");
		return false;
	}
	if (ActionRequest.ActionId != TEXT("talk_gu_heng")
		&& ActionRequest.ActionId != TEXT("talk_ye_cheng"))
	{
		OutError = TEXT("roleplay context requires a supported dialogue action");
		return false;
	}

	OutRequest.SpeakerId = SpeakerForAction(ActionRequest.ActionId);
	OutRequest.PlayerLine = ActionRequest.PlayerSaid.TrimStartAndEnd();
	OutRequest.TurnIndex = FMath::Max(1, TurnIndex);
	OutRequest.RemainingTurns = FMath::Max(
		0,
		ActionRequest.DialogueSessionMaxTurns - OutRequest.TurnIndex);
	OutRequest.ResponsePolicy = Repository.GetPolicy();
	if (!Repository.GetProfile(OutRequest.SpeakerId, OutRequest.RoleProfile))
	{
		OutError = FString::Printf(
			TEXT("missing role profile '%s'"),
			*OutRequest.SpeakerId.ToString());
		return false;
	}
	OutRequest.SubjectiveState = BuildSubjectiveState(
		OutRequest.SpeakerId,
		FrozenState);

	FWSDialogueSemanticFrame Frame = ActionRequest.SemanticFrame;
	if (Frame.Confidence <= 0.0f || Frame.Source.IsEmpty())
	{
		Frame = BuildSemanticFrame(
			OutRequest.PlayerLine,
			ActionRequest.ActionId);
	}
	if (ActionRequest.DialogueAct != EWSDialogueAct::Ask)
	{
		Frame.SpeechAct = ActionRequest.DialogueAct;
	}

	TArray<FName> EntityTags;
	ExtractTagsAndTarget(
		OutRequest.PlayerLine,
		OutRequest.SpeakerId,
		EntityTags,
		OutRequest.TopicTags,
		OutRequest.TargetSubjectId);
	if (!Frame.TargetActionId.IsNone())
	{
		OutRequest.TopicTags.AddUnique(Frame.TargetActionId);
	}
	if (!Frame.TargetFactId.IsNone())
	{
		if (PlayerKnowsFact(FrozenState, Frame.TargetFactId))
		{
			OutRequest.TopicTags.AddUnique(Frame.TargetFactId);
		}
	}
	EntityTags.AddUnique(OutRequest.TargetSubjectId);

	OutRequest.RecentMemory = RecentMemory.FilterByPredicate(
		[&OutRequest](const FWSRoleplayMemoryEntry& Entry)
		{
			return Entry.bPublic || Entry.Owner == OutRequest.SpeakerId;
		});
	// Memories are appended in commit order; TurnIndex restarts each session.
	if (OutRequest.RecentMemory.Num() > 6)
	{
		OutRequest.RecentMemory.RemoveAt(0, OutRequest.RecentMemory.Num() - 6);
	}

	TArray<FWSRoleplayKnowledgeItem> Candidates = Repository.GetGlobalKnowledge();
	Candidates.Append(Repository.GetKnowledgeForOwner(OutRequest.SpeakerId));
	TArray<FWSRoleplayKnowledgeItem> AllProtectedCandidates = Candidates;
	if (OutRequest.SpeakerId != GuHengId)
	{
		AllProtectedCandidates.Append(Repository.GetKnowledgeForOwner(GuHengId));
	}
	if (OutRequest.SpeakerId != YeChengId)
	{
		AllProtectedCandidates.Append(Repository.GetKnowledgeForOwner(YeChengId));
	}
	TSet<FName> ProtectedFacts;
	for (const FWSRoleplayKnowledgeItem& Item : AllProtectedCandidates)
	{
		if (!Item.bPublic && !Item.GameFactId.IsNone())
		{
			ProtectedFacts.Add(Item.GameFactId);
		}
	}
	for (auto It = ProtectedFacts.CreateIterator(); It; ++It)
	{
		if (PlayerKnowsFact(FrozenState, *It))
		{
			It.RemoveCurrent();
		}
	}

	const TArray<FName> EventTags = BuildEventTags(OutRequest.SubjectiveState);
	struct FScoredKnowledge
	{
		FWSRoleplayKnowledgeItem Item;
		double Score = 0.0;
	};
	TArray<FScoredKnowledge> Ranked;
	for (const FWSRoleplayKnowledgeItem& Item : Candidates)
	{
		if (Item.MaxDisclosure == EWSRoleplayDisclosureLevel::Hidden
			|| !IsAvailable(
				Item.Availability,
				OutRequest.SpeakerId,
				Frame,
				FrozenState))
		{
			continue;
		}

		FScoredKnowledge Scored;
		Scored.Item = Item;
		Scored.Score = static_cast<double>(Item.Salience);
		if (Item.GameFactId == TEXT("FACT_MEDICAL_DIAGNOSIS")
			&& Frame.TargetFactId == TEXT("FACT_HAND_INJURY"))
		{
			Scored.Score += 8.0;
		}
		if (Item.SubjectId == OutRequest.TargetSubjectId
			|| Item.TopicTags.Contains(OutRequest.TargetSubjectId)
			|| Intersects(Item.TopicTags, EntityTags)
			|| (!Frame.TargetFactId.IsNone()
				&& Item.GameFactId == Frame.TargetFactId))
		{
			Scored.Score += 4.0;
		}
		if (Intersects(Item.TopicTags, OutRequest.TopicTags)
			|| OutRequest.TopicTags.Contains(Item.CategoryId))
		{
			Scored.Score += 3.0;
		}
		if (Intersects(Item.TopicTags, EventTags)
			|| EventTags.Contains(Item.CategoryId))
		{
			Scored.Score += 2.5;
		}
		if (IsRelationshipMatch(
			Item,
			OutRequest.SpeakerId,
			OutRequest.TargetSubjectId,
			OutRequest.TopicTags))
		{
			Scored.Score += 2.0;
		}
		if (MatchesMemory(Item, OutRequest.RecentMemory))
		{
			Scored.Score += 1.5;
		}
		Ranked.Add(MoveTemp(Scored));
	}
	Ranked.Sort([](const FScoredKnowledge& Left, const FScoredKnowledge& Right)
	{
		if (!FMath::IsNearlyEqual(Left.Score, Right.Score))
		{
			return Left.Score > Right.Score;
		}
		return Left.Item.KnowledgeId.LexicalLess(Right.Item.KnowledgeId);
	});

	const int32 TopK = FMath::Clamp(OutRequest.ResponsePolicy.TopK, 8, 12);
	TSet<FName> SelectedSecretFamilies;
	TSet<FName> AvailableKnowledgeIds;
	for (FScoredKnowledge& Scored : Ranked)
	{
		if (OutRequest.AvailableKnowledge.Num() >= TopK)
		{
			break;
		}
		if (!Scored.Item.SecretFamily.IsNone()
			&& SelectedSecretFamilies.Contains(Scored.Item.SecretFamily))
		{
			continue;
		}
		if (!Scored.Item.SecretFamily.IsNone())
		{
			SelectedSecretFamilies.Add(Scored.Item.SecretFamily);
		}
		AvailableKnowledgeIds.Add(Scored.Item.KnowledgeId);
		if (!Scored.Item.GameFactId.IsNone())
		{
			ProtectedFacts.Remove(Scored.Item.GameFactId);
			if (Scored.Item.GameFactId == TEXT("FACT_MEDICAL_DIAGNOSIS"))
			{
				ProtectedFacts.Remove(TEXT("FACT_HAND_INJURY"));
			}
		}
		OutRequest.AvailableKnowledge.Add(MoveTemp(Scored.Item));
	}
	for (const FName ProtectedFact : ProtectedFacts)
	{
		OutRequest.ForbiddenFactIds.Add(ProtectedFact);
	}
	OutRequest.ForbiddenFactIds.Sort([](const FName Left, const FName Right)
	{
		return Left.LexicalLess(Right);
	});

	BuildAllowedProposals(
		OutRequest.SpeakerId,
		FrozenState,
		OutRequest.ResponsePolicy,
		OutRequest.AllowedActionProposals);
	SelectFallback(
		Repository,
		OutRequest.SpeakerId,
		OutRequest.TargetSubjectId,
		OutRequest.TopicTags,
		Frame,
		FrozenState,
		OutRequest.SubjectiveState,
		AvailableKnowledgeIds,
		OutFallback);
	// A local factual fallback states the selected knowledge verbatim, so its
	// assertion has an authored source instead of treating every reference as fact.
	for (const FWSRoleplayKnowledgeItem& Item : OutRequest.AvailableKnowledge)
	{
		const bool bDirectFact = !Frame.TargetFactId.IsNone()
			&& (Item.GameFactId == Frame.TargetFactId
				|| (Item.GameFactId == TEXT("FACT_MEDICAL_DIAGNOSIS")
					&& Frame.TargetFactId == TEXT("FACT_HAND_INJURY")));
		if (bDirectFact && Item.bCreatesGameFact
			&& Item.MaxDisclosure == EWSRoleplayDisclosureLevel::Explicit
			&& Item.EpistemicStatus == EWSEpistemicStatus::Known)
		{
			OutFallback.Line = Item.RoleplayContent + TEXT("。");
			OutFallback.SpeechFunction = EWSRoleplaySpeechFunction::Answer;
			OutFallback.ReferencedKnowledgeIds = {Item.KnowledgeId};
			FWSRoleplayAssertion Assertion;
			Assertion.KnowledgeId = Item.KnowledgeId;
			Assertion.Mode = EWSRoleplayClaimMode::Stated;
			OutFallback.Assertions = {Assertion};
			break;
		}
	}
	return true;
}

FWSDialogueSemanticFrame UWSNPCContextBuilder::BuildSemanticFrame(
	const FString& PlayerLine,
	const FName CurrentDialogueActionId)
{
	using WSNPCContextBuilderPrivate::ContainsAny;

	FWSDialogueSemanticFrame Frame;
	Frame.TargetCharacter = CurrentDialogueActionId == TEXT("talk_ye_cheng")
		? EWSCharacterId::YeCheng
		: EWSCharacterId::GuHeng;
	Frame.Source = TEXT("roleplay_local");
	Frame.Confidence = PlayerLine.TrimStartAndEnd().IsEmpty() ? 0.25f : 0.55f;

	if (ContainsAny(PlayerLine, {TEXT("顾衡"), TEXT("gu heng"), TEXT("gu_heng")}))
	{
		Frame.TargetCharacter = EWSCharacterId::GuHeng;
		Frame.Confidence = 0.9f;
	}
	else if (ContainsAny(PlayerLine, {TEXT("叶澄"), TEXT("ye cheng"), TEXT("ye_cheng")}))
	{
		Frame.TargetCharacter = EWSCharacterId::YeCheng;
		Frame.Confidence = 0.9f;
	}

	if (ContainsAny(PlayerLine, {TEXT("质疑"), TEXT("撒谎"), TEXT("隐瞒"), TEXT("证据"), TEXT("凭什么"), TEXT("challenge")}))
	{
		Frame.SpeechAct = EWSDialogueAct::Challenge;
	}
	else if (ContainsAny(PlayerLine, {TEXT("答应"), TEXT("承诺"), TEXT("保证"), TEXT("promise")}))
	{
		Frame.SpeechAct = EWSDialogueAct::Promise;
	}
	else if (ContainsAny(PlayerLine, {TEXT("交换"), TEXT("条件"), TEXT("交易"), TEXT("trade")}))
	{
		Frame.SpeechAct = EWSDialogueAct::Trade;
	}
	else if (ContainsAny(PlayerLine, {TEXT("放心"), TEXT("别怕"), TEXT("会好起来"), TEXT("reassure")}))
	{
		Frame.SpeechAct = EWSDialogueAct::Reassure;
	}
	else if (ContainsAny(PlayerLine, {TEXT("必须"), TEXT("立刻"), TEXT("命令"), TEXT("command")}))
	{
		Frame.SpeechAct = EWSDialogueAct::Command;
	}

	if (ContainsAny(PlayerLine, {TEXT("证据"), TEXT("凭什么"), TEXT("记录"), TEXT("日志"), TEXT("evidence"), TEXT("log")}))
	{
		Frame.QueryType = EWSDialogueQueryType::Evidence;
	}
	else if (ContainsAny(PlayerLine, {TEXT("为什么"), TEXT("原因"), TEXT("怎么会"), TEXT("why"), TEXT("cause")}))
	{
		Frame.QueryType = EWSDialogueQueryType::Cause;
	}
	else if (ContainsAny(PlayerLine, {TEXT("别的"), TEXT("其他"), TEXT("替代"), TEXT("还有办法"), TEXT("alternative")}))
	{
		Frame.QueryType = EWSDialogueQueryType::Alternative;
	}
	else if (ContainsAny(PlayerLine, {TEXT("需要什么"), TEXT("条件"), TEXT("怎么才能"), TEXT("requirements")}))
	{
		Frame.QueryType = EWSDialogueQueryType::Requirements;
	}
	else if (ContainsAny(PlayerLine, {TEXT("后果"), TEXT("会怎样"), TEXT("会不会"), TEXT("consequence")}))
	{
		Frame.QueryType = EWSDialogueQueryType::Consequence;
	}
	else if (ContainsAny(PlayerLine, {TEXT("情况"), TEXT("状态"), TEXT("怎么样"), TEXT("多少"), TEXT("吗"), TEXT("？"), TEXT("?"), TEXT("status")}))
	{
		Frame.QueryType = EWSDialogueQueryType::Status;
	}

	if (ContainsAny(
			PlayerLine,
			{TEXT("保温包"), TEXT("暖包"), TEXT("热包"),
				TEXT("应急保暖"), TEXT("应急医疗储备"),
				TEXT("heat pack"), TEXT("heat_pack")}))
	{
		Frame.TargetFactId = TEXT("FACT_HEAT_PACK");
		Frame.TargetActionId = TEXT("treat_gu_heng");
		Frame.Confidence = 0.95f;
	}
	else if (ContainsAny(PlayerLine, {TEXT("手伤"), TEXT("手怎么"), TEXT("手的伤"), TEXT("hand injury")}))
	{
		Frame.TargetFactId = TEXT("FACT_HAND_INJURY");
		Frame.TargetActionId = TEXT("treat_gu_heng");
		Frame.Confidence = 0.95f;
	}
	else if (ContainsAny(PlayerLine, {TEXT("继电器"), TEXT("relay")}))
	{
		Frame.TargetFactId = TEXT("FACT_RELAY_COMPATIBILITY");
		Frame.TargetActionId = TEXT("repair_generator");
		Frame.Confidence = 0.9f;
	}
	else if (ContainsAny(PlayerLine, {TEXT("强制重启"), TEXT("forced restart")}))
	{
		Frame.TargetFactId = TEXT("FACT_FORCED_RESTART_CONFIRMED");
		Frame.TargetActionId = TEXT("repair_generator");
		Frame.Confidence = 0.95f;
	}
	else if (ContainsAny(PlayerLine, {TEXT("发电机"), TEXT("generator")}))
	{
		Frame.TargetActionId = TEXT("repair_generator");
		Frame.Confidence = FMath::Max(Frame.Confidence, 0.85f);
	}
	else if (ContainsAny(PlayerLine, {TEXT("治疗"), TEXT("诊断"), TEXT("treat"), TEXT("diagnose")}))
	{
		Frame.TargetActionId = TEXT("treat_gu_heng");
		Frame.Confidence = FMath::Max(Frame.Confidence, 0.8f);
	}
	else if (ContainsAny(PlayerLine, {TEXT("信号"), TEXT("救援"), TEXT("天线"), TEXT("signal"), TEXT("rescue")}))
	{
		Frame.TargetActionId = TEXT("send_signal");
		Frame.Confidence = FMath::Max(Frame.Confidence, 0.8f);
	}
	return Frame;
}

FWSRoleplaySubjectiveState UWSNPCContextBuilder::BuildSubjectiveState(
	const FName SpeakerId,
	const FWSGameState& FrozenState)
{
	using WSNPCContextBuilderPrivate::CharacterForSpeaker;
	using WSNPCContextBuilderPrivate::DayPhaseName;
	using WSNPCContextBuilderPrivate::HeatingZoneName;
	using WSNPCContextBuilderPrivate::LocationName;
	using WSNPCContextBuilderPrivate::PhaseName;

	FWSRoleplaySubjectiveState Result;
	Result.PhaseId = PhaseName(FrozenState.Phase);
	Result.DayPhaseId = DayPhaseName(FrozenState.DayPhase);
	Result.bHeatingLocked = FrozenState.Heating.bLocked;
	Result.HeatingZoneId = HeatingZoneName(FrozenState.Heating.CurrentZone);
	Result.HeatingStateId = !FrozenState.Heating.bLocked
		? FName(TEXT("pending_selection"))
		: FName(*FString::Printf(
			TEXT("locked_%s"),
			*Result.HeatingZoneId.ToString()));
	Result.GeneratorProgress = FrozenState.Tasks.GeneratorProgress;
	Result.GeneratorStateId = FrozenState.Tasks.bGeneratorStable
		|| FrozenState.Tasks.GeneratorProgress >= 2
		? FName(TEXT("stable"))
		: (FrozenState.Tasks.GeneratorProgress <= 0
			? FName(TEXT("offline"))
			: FName(TEXT("partially_repaired")));
	const FWSCharacterState Character = FrozenState.Characters.FindRef(
		CharacterForSpeaker(SpeakerId));
	Result.SpeakerLocationId = LocationName(Character.Location);
	Result.SpeakerTrust = Character.Trust;
	Result.SpeakerPressure = Character.Pressure;
	return Result;
}
