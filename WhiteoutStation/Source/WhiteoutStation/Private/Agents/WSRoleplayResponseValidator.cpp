#include "Agents/WSRoleplayResponseValidator.h"

#include <initializer_list>

namespace WSRoleplayResponseValidatorPrivate
{
	constexpr int32 MaxRoleplayLineCharacters = 120;
	constexpr int32 MaxRoleplaySentences = 3;
	constexpr int32 MaxMemorySummaryCharacters = 160;

	bool Reject(const TCHAR* Reason, TArray<FName>& OutGameFactIds, FString& OutReason)
	{
		OutGameFactIds.Reset();
		OutReason = Reason;
		return false;
	}

	bool ContainsAny(
		const FString& Text,
		const std::initializer_list<const TCHAR*>& Tokens)
	{
		for (const TCHAR* Token : Tokens)
		{
			if (Text.Contains(Token, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	bool IsAsciiIdentifierCharacter(const TCHAR Character)
	{
		return (Character >= TEXT('a') && Character <= TEXT('z'))
			|| (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('0') && Character <= TEXT('9'))
			|| Character == TEXT('_');
	}

	bool ContainsAsciiToken(const FString& Text, const FString& Token)
	{
		int32 SearchFrom = 0;
		while (SearchFrom < Text.Len())
		{
			const int32 Found = Text.Find(
				Token,
				ESearchCase::IgnoreCase,
				ESearchDir::FromStart,
				SearchFrom);
			if (Found == INDEX_NONE)
			{
				return false;
			}
			const int32 End = Found + Token.Len();
			const bool bStartsAtBoundary =
				Found == 0 || !IsAsciiIdentifierCharacter(Text[Found - 1]);
			const bool bEndsAtBoundary =
				End == Text.Len() || !IsAsciiIdentifierCharacter(Text[End]);
			if (bStartsAtBoundary && bEndsAtBoundary)
			{
				return true;
			}
			SearchFrom = Found + 1;
		}
		return false;
	}

	bool ContainsInternalIdentifier(const FString& Text)
	{
		int32 TokenStart = INDEX_NONE;
		bool bHasUnderscore = false;
		for (int32 Index = 0; Index <= Text.Len(); ++Index)
		{
			const bool bAtEnd = Index == Text.Len();
			const TCHAR Character = bAtEnd ? TEXT(' ') : Text[Index];
			if (!bAtEnd && IsAsciiIdentifierCharacter(Character))
			{
				if (TokenStart == INDEX_NONE)
				{
					TokenStart = Index;
				}
				bHasUnderscore |= Character == TEXT('_');
				continue;
			}

			if (TokenStart != INDEX_NONE)
			{
				if (bHasUnderscore && Index - TokenStart >= 3)
				{
					return true;
				}
				TokenStart = INDEX_NONE;
				bHasUnderscore = false;
			}
		}
		return false;
	}

	bool ContainsSystemLanguage(const FString& Text)
	{
		return ContainsAsciiToken(Text, TEXT("AP"))
			|| ContainsAsciiToken(Text, TEXT("ID"))
			|| ContainsAsciiToken(Text, TEXT("JSON"))
			|| ContainsAsciiToken(Text, TEXT("NPC"))
			|| ContainsAsciiToken(Text, TEXT("Prompt"))
			|| ContainsAsciiToken(Text, TEXT("Stamina"))
			|| ContainsAsciiToken(Text, TEXT("Token"))
			|| ContainsAny(
				Text,
				{TEXT("作为 AI"), TEXT("作为AI"), TEXT("语言模型"),
					TEXT("规则引擎"), TEXT("模型调用"), TEXT("行动点"),
					TEXT("系统提示"), TEXT("内部 ID"),
					TEXT("内部ID"), TEXT("条件 ID"), TEXT("条件ID"),
					TEXT("知识 ID"), TEXT("知识ID"), TEXT("阈值"),
					TEXT("修正值"), TEXT("至少两点"), TEXT("至少2点")})
			|| ContainsInternalIdentifier(Text);
	}

	int32 CountSentences(const FString& Text)
	{
		int32 Count = 0;
		bool bHasContent = false;
		for (const TCHAR Character : Text)
		{
			const bool bTerminator = Character == TEXT('。')
				|| Character == TEXT('！')
				|| Character == TEXT('？')
				|| Character == TEXT('.')
				|| Character == TEXT('!')
				|| Character == TEXT('?');
			if (bTerminator)
			{
				if (bHasContent)
				{
					++Count;
					bHasContent = false;
				}
			}
			else if (!FChar::IsWhitespace(Character))
			{
				bHasContent = true;
			}
		}
		return Count + (bHasContent ? 1 : 0);
	}

	bool IsSpeechFunctionValid(const EWSRoleplaySpeechFunction Function)
	{
		switch (Function)
		{
		case EWSRoleplaySpeechFunction::Unknown:
		case EWSRoleplaySpeechFunction::Answer:
		case EWSRoleplaySpeechFunction::AnswerWithUncertainty:
		case EWSRoleplaySpeechFunction::Clarify:
		case EWSRoleplaySpeechFunction::Deflect:
		case EWSRoleplaySpeechFunction::Refuse:
		case EWSRoleplaySpeechFunction::Reassure:
		case EWSRoleplaySpeechFunction::Challenge:
		case EWSRoleplaySpeechFunction::Acknowledge:
		case EWSRoleplaySpeechFunction::ConditionalCooperation:
		case EWSRoleplaySpeechFunction::SuggestAction:
		case EWSRoleplaySpeechFunction::Evade:
		case EWSRoleplaySpeechFunction::Suggest:
		case EWSRoleplaySpeechFunction::ConditionalOffer:
		case EWSRoleplaySpeechFunction::CrisisResponse:
			return true;
		default:
			return false;
		}
	}

	bool IsClaimModeValid(const EWSRoleplayClaimMode Mode)
	{
		switch (Mode)
		{
		case EWSRoleplayClaimMode::Stated:
		case EWSRoleplayClaimMode::Observation:
		case EWSRoleplayClaimMode::Belief:
		case EWSRoleplayClaimMode::Suspected:
		case EWSRoleplayClaimMode::Denied:
		case EWSRoleplayClaimMode::Promised:
		case EWSRoleplayClaimMode::Withheld:
			return true;
		default:
			return false;
		}
	}

	bool IsClaimAllowedByDisclosure(
		const EWSRoleplayDisclosureLevel Disclosure,
		const EWSRoleplayClaimMode Mode)
	{
		switch (Disclosure)
		{
		case EWSRoleplayDisclosureLevel::Evasive:
			return Mode == EWSRoleplayClaimMode::Withheld
				|| Mode == EWSRoleplayClaimMode::Denied;
		case EWSRoleplayDisclosureLevel::Hint:
			return Mode == EWSRoleplayClaimMode::Belief
				|| Mode == EWSRoleplayClaimMode::Suspected
				|| Mode == EWSRoleplayClaimMode::Withheld;
		case EWSRoleplayDisclosureLevel::Partial:
		case EWSRoleplayDisclosureLevel::Explicit:
			return true;
		default:
			return false;
		}
	}

	bool IsClaimAllowedByEpistemicStatus(
		const EWSEpistemicStatus Status,
		const EWSRoleplayClaimMode Mode)
	{
		if (Mode == EWSRoleplayClaimMode::Denied
			|| Mode == EWSRoleplayClaimMode::Promised
			|| Mode == EWSRoleplayClaimMode::Withheld)
		{
			return true;
		}
		switch (Status)
		{
		case EWSEpistemicStatus::Known:
			return Mode == EWSRoleplayClaimMode::Stated
				|| Mode == EWSRoleplayClaimMode::Observation
				|| Mode == EWSRoleplayClaimMode::Belief
				|| Mode == EWSRoleplayClaimMode::Suspected;
		case EWSEpistemicStatus::Observed:
			return Mode == EWSRoleplayClaimMode::Observation
				|| Mode == EWSRoleplayClaimMode::Belief
				|| Mode == EWSRoleplayClaimMode::Suspected;
		case EWSEpistemicStatus::Believed:
		case EWSEpistemicStatus::FalseBelief:
			return Mode == EWSRoleplayClaimMode::Belief
				|| Mode == EWSRoleplayClaimMode::Suspected;
		case EWSEpistemicStatus::Suspected:
			return Mode == EWSRoleplayClaimMode::Suspected;
		default:
			return false;
		}
	}

	const FWSRoleplayKnowledgeItem* FindKnowledge(
		const FWSRoleplayRequest& Request,
		const FName KnowledgeId)
	{
		return Request.AvailableKnowledge.FindByPredicate(
			[KnowledgeId](const FWSRoleplayKnowledgeItem& Item)
			{
				return Item.KnowledgeId == KnowledgeId;
			});
	}

	bool HasForbiddenFact(const FWSRoleplayRequest& Request, const FName FactId)
	{
		return !FactId.IsNone() && Request.ForbiddenFactIds.Contains(FactId);
	}

	bool ContainsForbiddenFactSurface(
		const FString& Text,
		const FWSRoleplayRequest& Request)
	{
		for (const FName FactId : Request.ForbiddenFactIds)
		{
			FString StableId = FactId.ToString();
			StableId.ToUpperInline();
			if (StableId == TEXT("FACT_HAND_INJURY")
				&& (ContainsAny(
						Text,
						{TEXT("手伤"), TEXT("伤手"), TEXT("右手受伤"),
							TEXT("手部受伤"), TEXT("右手使不上力"),
							TEXT("手还使不上力"), TEXT("手上的伤口"),
							TEXT("手上伤口"), TEXT("伤口又裂"), TEXT("hand injury"),
							TEXT("injured hand")})
					|| (Text.Contains(TEXT("精细操作"))
						&& ContainsAny(
							Text,
							{TEXT("身体"), TEXT("状态"), TEXT("不适合"),
								TEXT("影响"), TEXT("能力")}))))
			{
				return true;
			}
			if (StableId == TEXT("FACT_HEAT_PACK")
				&& ContainsAny(
					Text,
					{TEXT("保温包"), TEXT("暖袋"), TEXT("热敷袋"),
						TEXT("应急医疗储备"), TEXT("heat pack")}))
			{
				return true;
			}
			if (StableId.Contains(TEXT("FORCED_RESTART"))
				&& ContainsAny(
					Text,
					{TEXT("强制重启"), TEXT("手动旁路"), TEXT("越过保护"),
						TEXT("绕过保护"), TEXT("forced restart"),
						TEXT("manual bypass"), TEXT("bypassed protection")}))
			{
				return true;
			}
			if (StableId == TEXT("FACT_RELAY_COMPATIBILITY")
				&& ContainsAny(
					Text,
					{TEXT("继电器兼容"), TEXT("替代继电器"),
						TEXT("可靠替代件"), TEXT("继电器能替"),
						TEXT("厨房加热器"), TEXT("规格能对上"),
						TEXT("正好能装上"), TEXT("零件能装上"),
						TEXT("compatible relay")}))
			{
				return true;
			}
		}
		return false;
	}

	bool MentionsSubjectOrTopic(const FString& Text, const FWSRoleplayRequest& Request)
	{
		FString Subject = Request.TargetSubjectId.ToString();
		Subject.ToLowerInline();
		if (!Subject.IsEmpty())
		{
			if ((Subject == TEXT("gu_heng") || Subject == TEXT("guheng"))
				&& ContainsAny(Text, {TEXT("顾衡"), TEXT("顾工")}))
			{
				return true;
			}
			if ((Subject == TEXT("ye_cheng") || Subject == TEXT("yecheng"))
				&& ContainsAny(Text, {TEXT("叶澄"), TEXT("叶医生")}))
			{
				return true;
			}
			if (Subject.Contains(TEXT("generator"))
				&& ContainsAny(Text, {TEXT("发电机"), TEXT("供电"), TEXT("停电")}))
			{
				return true;
			}
			if (Subject.Contains(TEXT("heating"))
				&& ContainsAny(Text, {TEXT("供暖"), TEXT("取暖"), TEXT("暖气")}))
			{
				return true;
			}
			if (Subject == TEXT("relay") && Text.Contains(TEXT("继电器")))
			{
				return true;
			}
			if (Subject == TEXT("repair_room") && Text.Contains(TEXT("维修间")))
			{
				return true;
			}
			if (Subject == TEXT("medical_room") && Text.Contains(TEXT("医务室")))
			{
				return true;
			}
			if (Subject == TEXT("kitchen") && Text.Contains(TEXT("厨房")))
			{
				return true;
			}
			if (Subject == TEXT("control_room") && Text.Contains(TEXT("控制室")))
			{
				return true;
			}
			if (Subject == TEXT("rescue")
				&& ContainsAny(Text, {TEXT("救援"), TEXT("求救")}))
			{
				return true;
			}
			if (Subject == TEXT("weather")
				&& ContainsAny(Text, {TEXT("天气"), TEXT("暴雪"), TEXT("风雪")}))
			{
				return true;
			}
			if (Subject == TEXT("player")
				&& ContainsAny(Text, {TEXT("你自己"), TEXT("你的")}))
			{
				return true;
			}
			if (Text.Contains(Request.TargetSubjectId.ToString(), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		for (const FName TopicTag : Request.TopicTags)
		{
			FString Topic = TopicTag.ToString();
			Topic.ToLowerInline();
			if (Topic.Contains(TEXT("relationship"))
				&& ContainsAny(Text, {TEXT("关系"), TEXT("相处"), TEXT("共事"), TEXT("信任")}))
			{
				return true;
			}
			if ((Topic.Contains(TEXT("status")) || Topic.Contains(TEXT("current")))
				&& ContainsAny(Text, {TEXT("情况"), TEXT("状态"), TEXT("现在"), TEXT("身体")}))
			{
				return true;
			}
			if ((Topic.Contains(TEXT("history")) || Topic.Contains(TEXT("experience")))
				&& ContainsAny(Text, {TEXT("以前"), TEXT("经历"), TEXT("共事"), TEXT("过去")}))
			{
				return true;
			}
			if (Topic.Contains(TEXT("trust"))
				&& ContainsAny(Text, {TEXT("相信"), TEXT("信任"), TEXT("可靠")}))
			{
				return true;
			}
			if ((Topic.Contains(TEXT("generator")) || Topic.Contains(TEXT("repair")))
				&& ContainsAny(Text, {TEXT("发电机"), TEXT("维修"), TEXT("修理"), TEXT("设备")}))
			{
				return true;
			}
			if (Topic.Contains(TEXT("heating"))
				&& ContainsAny(Text, {TEXT("供暖"), TEXT("取暖"), TEXT("暖气")}))
			{
				return true;
			}
			if (Topic.Contains(TEXT("weather"))
				&& ContainsAny(Text, {TEXT("天气"), TEXT("暴雪"), TEXT("风雪")}))
			{
				return true;
			}
			if ((Topic.Contains(TEXT("medical")) || Topic.Contains(TEXT("injury")))
				&& ContainsAny(Text, {TEXT("医疗"), TEXT("检查"), TEXT("伤"), TEXT("身体")}))
			{
				return true;
			}
			if ((Topic.Contains(TEXT("resource")) || Topic.Contains(TEXT("supplies")))
				&& ContainsAny(Text, {TEXT("物资"), TEXT("药"), TEXT("食物"), TEXT("燃料")}))
			{
				return true;
			}
			if (Topic.Contains(TEXT("character"))
				&& ContainsAny(Text, {TEXT("人物"), TEXT("个人"), TEXT("谁"), TEXT("某个人")}))
			{
				return true;
			}
			if (Topic.Contains(TEXT("equipment"))
				&& ContainsAny(Text, {TEXT("设备"), TEXT("哪台"), TEXT("机器"), TEXT("发电机")}))
			{
				return true;
			}
			if (Topic.Contains(TEXT("risk"))
				&& ContainsAny(Text, {TEXT("风险"), TEXT("危险"), TEXT("伤势"), TEXT("暴雪")}))
			{
				return true;
			}
			if ((Topic.Contains(TEXT("ambiguous"))
					|| Topic.Contains(TEXT("clarification")))
				&& ContainsAny(Text, {TEXT("具体"), TEXT("想问"), TEXT("说清"), TEXT("哪一")}))
			{
				return true;
			}
		}
		return (Request.TargetSubjectId.IsNone()
				|| Request.TargetSubjectId == TEXT("unknown"))
			&& Request.TopicTags.IsEmpty();
	}

	bool HasHeatingSelectionContradiction(
		const FString& Text,
		const FWSRoleplaySubjectiveState& State)
	{
		if (!State.bHeatingLocked || !Text.Contains(TEXT("供暖")))
		{
			return false;
		}
		return ContainsAny(
			Text,
			{TEXT("先选择"), TEXT("先选"), TEXT("先决定"), TEXT("先确定"),
				TEXT("供暖给哪"), TEXT("哪间房供暖"), TEXT("哪个区域供暖")});
	}

	bool HasGeneratorContradiction(
		const FString& Text,
		const FWSRoleplaySubjectiveState& State)
	{
		FString GeneratorState = State.GeneratorStateId.ToString();
		GeneratorState.ToLowerInline();
		const bool bComplete = State.GeneratorProgress >= 2
			|| GeneratorState.Contains(TEXT("complete"))
			|| GeneratorState.Contains(TEXT("repaired"))
			|| GeneratorState.Contains(TEXT("online"))
			|| GeneratorState.Contains(TEXT("stable"))
			|| GeneratorState.Contains(TEXT("restored"));
		const bool bHasProgress = State.GeneratorProgress > 0;

		if (!bComplete
			&& ContainsAny(
				Text,
				{TEXT("已经修好"), TEXT("已修好"), TEXT("发电机修好了"),
					TEXT("把发电机修好"), TEXT("已经恢复供电"),
					TEXT("已恢复供电"), TEXT("供电恢复了"), TEXT("修复完成"),
					TEXT("恢复正常"), TEXT("发电机正常"), TEXT("恢复运行")}))
		{
			return true;
		}
		if (bComplete
			&& ContainsAny(
				Text,
				{TEXT("还没修好"), TEXT("尚未修好"), TEXT("仍未修好"),
					TEXT("还未恢复"), TEXT("尚未恢复"), TEXT("仍未恢复"),
					TEXT("还坏着"), TEXT("尚未修复"), TEXT("还没修复")}))
		{
			return true;
		}
		if (!bHasProgress
			&& ContainsAny(Text, {TEXT("修了一部分"), TEXT("完成一部分维修")}))
		{
			return true;
		}
		return bHasProgress
			&& ContainsAny(Text, {TEXT("还没开始修"), TEXT("一点没修"), TEXT("完全没修")});
	}

	bool ProposalMatches(
		const FWSRoleplayActionProposal& Left,
		const FWSRoleplayActionProposal& Right)
	{
		return Left.Type == Right.Type
			&& Left.ActionId == Right.ActionId
			&& Left.RequestedConditionIds == Right.RequestedConditionIds
			&& Left.ExpiresAtPhase == Right.ExpiresAtPhase;
	}

	bool IsPerformanceValueAllowed(
		const FString& Value,
		const std::initializer_list<const TCHAR*>& Allowed)
	{
		FString Clean = Value;
		Clean.TrimStartAndEndInline();
		for (const TCHAR* Candidate : Allowed)
		{
			if (Clean.Equals(Candidate, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	bool ValidateInternal(
		const FWSRoleplayRequest& Request,
		const FWSRoleplayResponse& Response,
		TArray<FName>& OutGameFactIds,
		FString& OutReason)
	{
		OutGameFactIds.Reset();
		OutReason.Reset();

		FString Line = Response.NpcLine;
		Line.TrimStartAndEndInline();
		if (Line.IsEmpty())
		{
			return Reject(TEXT("npc_line_empty"), OutGameFactIds, OutReason);
		}
		if (Response.NpcLine.Contains(TEXT("\n"))
			|| Response.NpcLine.Contains(TEXT("\r")))
		{
			return Reject(TEXT("npc_line_newline"), OutGameFactIds, OutReason);
		}
		const int32 PolicyCharacters = Request.ResponsePolicy.MaxCharacters > 0
			? FMath::Min(Request.ResponsePolicy.MaxCharacters, MaxRoleplayLineCharacters)
			: MaxRoleplayLineCharacters;
		if (Line.Len() > PolicyCharacters)
		{
			return Reject(TEXT("npc_line_too_long"), OutGameFactIds, OutReason);
		}
		const int32 PolicySentences = Request.ResponsePolicy.MaxSentences > 0
			? FMath::Min(Request.ResponsePolicy.MaxSentences, MaxRoleplaySentences)
			: MaxRoleplaySentences;
		const int32 SentenceCount = CountSentences(Line);
		if (SentenceCount < 1 || SentenceCount > PolicySentences)
		{
			return Reject(TEXT("npc_line_sentence_count"), OutGameFactIds, OutReason);
		}
		if (ContainsSystemLanguage(Line))
		{
			return Reject(TEXT("npc_line_system_language"), OutGameFactIds, OutReason);
		}
		if (ContainsForbiddenFactSurface(Line, Request))
		{
			return Reject(TEXT("npc_line_forbidden_fact"), OutGameFactIds, OutReason);
		}

		if (!IsSpeechFunctionValid(Response.SpeechFunction))
		{
			return Reject(TEXT("speech_function_invalid"), OutGameFactIds, OutReason);
		}
		if (!Request.ResponsePolicy.AllowedSpeechFunctions.IsEmpty()
			&& !Request.ResponsePolicy.AllowedSpeechFunctions.Contains(
				Response.SpeechFunction))
		{
			return Reject(TEXT("speech_function_not_allowed"), OutGameFactIds, OutReason);
		}

		TSet<FName> ReferencedIds;
		bool bReferencesTargetSubject = false;
		for (const FName KnowledgeId : Response.ReferencedKnowledgeIds)
		{
			if (KnowledgeId.IsNone() || ReferencedIds.Contains(KnowledgeId))
			{
				return Reject(TEXT("knowledge_reference_duplicate"), OutGameFactIds, OutReason);
			}
			ReferencedIds.Add(KnowledgeId);
			const FWSRoleplayKnowledgeItem* Knowledge = FindKnowledge(Request, KnowledgeId);
			if (!Knowledge)
			{
				return Reject(TEXT("knowledge_reference_unavailable"), OutGameFactIds, OutReason);
			}
			if (HasForbiddenFact(Request, Knowledge->GameFactId))
			{
				return Reject(TEXT("knowledge_reference_forbidden"), OutGameFactIds, OutReason);
			}
			const bool bMatchesTopic = Knowledge->TopicTags.ContainsByPredicate(
				[&Request](const FName TopicTag)
				{
					return Request.TopicTags.Contains(TopicTag);
				});
			const bool bCharacterTarget =
				Request.TargetSubjectId == TEXT("gu_heng")
				|| Request.TargetSubjectId == TEXT("ye_cheng")
				|| Request.TargetSubjectId == TEXT("player");
			const bool bDirectTargetMatch =
				Knowledge->SubjectId == Request.TargetSubjectId
				|| Knowledge->TopicTags.Contains(Request.TargetSubjectId);
			bReferencesTargetSubject |= !Request.TargetSubjectId.IsNone()
				&& (bDirectTargetMatch
					|| (!bCharacterTarget
						&& (bMatchesTopic
							|| Request.TopicTags.Contains(
								Knowledge->CategoryId))));
		}

		const bool bMayOmitReferences =
			Response.SpeechFunction == EWSRoleplaySpeechFunction::Clarify
			|| Response.SpeechFunction == EWSRoleplaySpeechFunction::Unknown;
		if (!Request.TargetSubjectId.IsNone()
			&& !bMayOmitReferences
			&& !bReferencesTargetSubject)
		{
			return Reject(TEXT("target_subject_not_referenced"), OutGameFactIds, OutReason);
		}
		if (bMayOmitReferences && !MentionsSubjectOrTopic(Line, Request))
		{
			return Reject(TEXT("clarification_not_relevant"), OutGameFactIds, OutReason);
		}

		TSet<FName> AssertionIds;
		for (const FWSRoleplayAssertion& Assertion : Response.Assertions)
		{
			if (Assertion.KnowledgeId.IsNone()
				|| AssertionIds.Contains(Assertion.KnowledgeId))
			{
				return Reject(TEXT("assertion_duplicate"), OutGameFactIds, OutReason);
			}
			AssertionIds.Add(Assertion.KnowledgeId);
			if (!ReferencedIds.Contains(Assertion.KnowledgeId))
			{
				return Reject(TEXT("assertion_not_referenced"), OutGameFactIds, OutReason);
			}
			if (!IsClaimModeValid(Assertion.Mode))
			{
				return Reject(TEXT("assertion_mode_invalid"), OutGameFactIds, OutReason);
			}
			const FWSRoleplayKnowledgeItem* Knowledge =
				FindKnowledge(Request, Assertion.KnowledgeId);
			if (!Knowledge)
			{
				return Reject(TEXT("assertion_knowledge_unavailable"), OutGameFactIds, OutReason);
			}
			if (!IsClaimAllowedByDisclosure(
					Knowledge->MaxDisclosure,
					Assertion.Mode))
			{
				return Reject(TEXT("knowledge_disclosure_mode_invalid"), OutGameFactIds, OutReason);
			}
			if (!IsClaimAllowedByEpistemicStatus(
					Knowledge->EpistemicStatus,
					Assertion.Mode))
			{
				return Reject(TEXT("epistemic_claim_upgrade"), OutGameFactIds, OutReason);
			}
			const bool bTruthBearingMode =
				Assertion.Mode == EWSRoleplayClaimMode::Stated
				|| Assertion.Mode == EWSRoleplayClaimMode::Observation;
			const bool bTruthBearingKnowledge =
				Knowledge->EpistemicStatus == EWSEpistemicStatus::Known
				|| Knowledge->EpistemicStatus == EWSEpistemicStatus::Observed;
			if (Knowledge->bCreatesGameFact
				&& !Knowledge->GameFactId.IsNone()
				&& bTruthBearingMode
				&& bTruthBearingKnowledge)
			{
				OutGameFactIds.AddUnique(Knowledge->GameFactId);
			}
		}
		for (const FName KnowledgeId : ReferencedIds)
		{
			const FWSRoleplayKnowledgeItem* Knowledge =
				FindKnowledge(Request, KnowledgeId);
			const bool bRequiresDisclosureAssertion = Knowledge
				&& (Knowledge->MaxDisclosure == EWSRoleplayDisclosureLevel::Hidden
					|| Knowledge->MaxDisclosure == EWSRoleplayDisclosureLevel::Evasive
					|| Knowledge->MaxDisclosure == EWSRoleplayDisclosureLevel::Hint);
			if (bRequiresDisclosureAssertion && !AssertionIds.Contains(KnowledgeId))
			{
				return Reject(
					TEXT("knowledge_disclosure_assertion_required"),
					OutGameFactIds,
					OutReason);
			}
		}

		if (HasHeatingSelectionContradiction(Line, Request.SubjectiveState))
		{
			return Reject(TEXT("heating_state_conflict"), OutGameFactIds, OutReason);
		}
		if (HasGeneratorContradiction(Line, Request.SubjectiveState))
		{
			return Reject(TEXT("generator_state_conflict"), OutGameFactIds, OutReason);
		}

		if (!Response.bHasProposedAction)
		{
			if (Response.ProposedAction.Type != EWSRoleplayProposalType::None
				|| !Response.ProposedAction.ActionId.IsNone()
				|| !Response.ProposedAction.RequestedConditionIds.IsEmpty()
				|| !Response.ProposedAction.ExpiresAtPhase.IsNone())
			{
				return Reject(TEXT("proposal_null_mismatch"), OutGameFactIds, OutReason);
			}
		}
		else
		{
			if (Response.ProposedAction.Type == EWSRoleplayProposalType::None
				|| Response.ProposedAction.ActionId.IsNone())
			{
				return Reject(TEXT("proposal_invalid"), OutGameFactIds, OutReason);
			}
			if (!Request.ResponsePolicy.AllowedProposalTypes.IsEmpty()
				&& !Request.ResponsePolicy.AllowedProposalTypes.Contains(
					Response.ProposedAction.Type))
			{
				return Reject(TEXT("proposal_type_not_allowed"), OutGameFactIds, OutReason);
			}
			const bool bMatched = Request.AllowedActionProposals.ContainsByPredicate(
				[&Response](const FWSRoleplayActionProposal& Allowed)
				{
					return ProposalMatches(Allowed, Response.ProposedAction);
				});
			if (!bMatched)
			{
				return Reject(TEXT("proposal_not_allowed"), OutGameFactIds, OutReason);
			}
		}

		FString Memory = Response.MemorySummary;
		Memory.TrimStartAndEndInline();
		if (Memory.Len() > MaxMemorySummaryCharacters)
		{
			return Reject(TEXT("memory_summary_too_long"), OutGameFactIds, OutReason);
		}
		if (Response.MemorySummary.Contains(TEXT("\n"))
			|| Response.MemorySummary.Contains(TEXT("\r")))
		{
			return Reject(TEXT("memory_summary_newline"), OutGameFactIds, OutReason);
		}
		if (ContainsSystemLanguage(Memory))
		{
			return Reject(TEXT("memory_summary_system_language"), OutGameFactIds, OutReason);
		}
		if (ContainsForbiddenFactSurface(Memory, Request))
		{
			return Reject(TEXT("memory_summary_forbidden_fact"), OutGameFactIds, OutReason);
		}

		if (!IsPerformanceValueAllowed(
				Response.Emotion,
				{TEXT("neutral"), TEXT("focused"), TEXT("firm"),
					TEXT("strained"), TEXT("steadier"), TEXT("reserved"),
					TEXT("measured"), TEXT("defiant"), TEXT("withdrawn"),
					TEXT("wary"), TEXT("cornered"), TEXT("defensive"),
					TEXT("controlled"), TEXT("guarded"), TEXT("uneasy"),
					TEXT("clinical"), TEXT("grim"), TEXT("alarmed"),
					TEXT("urgent"), TEXT("relieved")}))
		{
			return Reject(TEXT("emotion_invalid"), OutGameFactIds, OutReason);
		}
		if (!IsPerformanceValueAllowed(
				Response.MovementIntent,
				{TEXT("stay"), TEXT("step_closer"), TEXT("step_back"),
					TEXT("return_to_post")}))
		{
			return Reject(TEXT("movement_intent_invalid"), OutGameFactIds, OutReason);
		}
		if (!IsPerformanceValueAllowed(
				Response.ReactionAction,
				{TEXT("neutral"), TEXT("acknowledge"), TEXT("consider"),
					TEXT("reassure"), TEXT("reject"), TEXT("alarmed")}))
		{
			return Reject(TEXT("reaction_action_invalid"), OutGameFactIds, OutReason);
		}

		OutReason = TEXT("ok");
		return true;
	}
}

bool UWSRoleplayResponseValidator::Validate(
	const FWSRoleplayRequest& Request,
	const FWSRoleplayResponse& Response,
	FString& OutReason)
{
	TArray<FName> IgnoredGameFactIds;
	return WSRoleplayResponseValidatorPrivate::ValidateInternal(
		Request,
		Response,
		IgnoredGameFactIds,
		OutReason);
}

bool UWSRoleplayResponseValidator::ValidateAndDeriveDisclosures(
	const FWSRoleplayRequest& Request,
	const FWSRoleplayResponse& Response,
	TArray<FName>& OutGameFactIds,
	FString& OutReason)
{
	return WSRoleplayResponseValidatorPrivate::ValidateInternal(
		Request,
		Response,
		OutGameFactIds,
		OutReason);
}
