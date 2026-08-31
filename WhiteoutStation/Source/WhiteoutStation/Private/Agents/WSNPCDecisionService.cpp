#include "Agents/WSNPCDecisionService.h"

#include "State/WSDialogueDisclosurePolicy.h"

namespace WhiteoutAgentFacts
{
	const FName GeneratorProtectionStop(TEXT("FACT_GENERATOR_PROTECTION_STOP"));
	const FName ForcedRestartSuspicion(TEXT("FACT_FORCED_RESTART_SUSPICION"));
	const FName BurntRelay(TEXT("FACT_BURNT_RELAY"));
	const FName HandInjury(TEXT("FACT_HAND_INJURY"));
	const FName MedicalDiagnosis(TEXT("FACT_MEDICAL_DIAGNOSIS"));
	const FName HeatPack(TEXT("FACT_HEAT_PACK"));
	const FName RelayCompatibility(TEXT("FACT_RELAY_COMPATIBILITY"));
	const FName ForcedRestartConfirmed(TEXT("FACT_FORCED_RESTART_CONFIRMED"));
}

bool UWSNPCDecisionService::RequiresExpression(const FName ActionId)
{
	return ActionId == TEXT("talk_gu_heng")
		|| ActionId == TEXT("talk_ye_cheng")
		|| ActionId == TEXT("repair_generator");
}

FWSAgentReply UWSNPCDecisionService::BuildDeterministicReply(const FName ActionId, const FWSGameState& State)
{
	FWSActionRequest Request;
	Request.ActionId = ActionId;
	return BuildDeterministicReply(Request, State);
}

FWSAgentReply UWSNPCDecisionService::BuildDeterministicReply(
	const FWSActionRequest& Request,
	const FWSGameState& State)
{
	FWSActionRequest GroundedRequest = Request;
	if (GroundedRequest.SemanticFrame.QueryType == EWSDialogueQueryType::Unknown
		&& GroundedRequest.ActionId == TEXT("talk_gu_heng")
		&& GroundedRequest.PlayerSaid.Contains(TEXT("发电机"))
		&& (GroundedRequest.PlayerSaid.Contains(TEXT("要怎么样"))
			|| GroundedRequest.PlayerSaid.Contains(TEXT("怎样才"))
			|| GroundedRequest.PlayerSaid.Contains(TEXT("怎么才"))
			|| GroundedRequest.PlayerSaid.Contains(TEXT("什么条件"))
			|| GroundedRequest.PlayerSaid.Contains(TEXT("需要我做什么"))
			|| GroundedRequest.PlayerSaid.Contains(TEXT("要我做什么"))
			|| GroundedRequest.PlayerSaid.Contains(TEXT("我要做什么"))
			|| GroundedRequest.PlayerSaid.Contains(TEXT("才会帮"))
			|| GroundedRequest.PlayerSaid.Contains(TEXT("才肯"))
			|| GroundedRequest.PlayerSaid.Contains(TEXT("才愿意"))))
	{
		GroundedRequest.SemanticFrame.SpeechAct = EWSDialogueAct::Ask;
		GroundedRequest.SemanticFrame.QueryType = EWSDialogueQueryType::Requirements;
		GroundedRequest.SemanticFrame.TargetActionId = TEXT("repair_generator");
		GroundedRequest.SemanticFrame.TargetCharacter = EWSCharacterId::GuHeng;
		GroundedRequest.SemanticFrame.Confidence = 0.99f;
		GroundedRequest.SemanticFrame.Source = TEXT("local_semantic_frame");
	}
	return BuildDeterministicReply(
		GroundedRequest,
		State,
		FWSActionRequirementReport());
}

FWSAgentReply UWSNPCDecisionService::BuildDeterministicReply(
	const FWSActionRequest& Request,
	const FWSGameState& State,
	const FWSActionRequirementReport& RequirementReport)
{
	FWSAgentReply Reply;
	Reply.ActionId = Request.ActionId;
	Reply.TransactionId = Request.TransactionId;
	Reply.DialogueSessionId = Request.DialogueSessionId;
	Reply.SemanticFrame = Request.SemanticFrame;
	Reply.bAccepted = true;
	Reply.bFallback = true;
	Reply.Provider = TEXT("preset");
	Reply.ValidationReason = TEXT("deterministic_decision");

	if (Request.ActionId == TEXT("talk_ye_cheng"))
	{
		Reply.Speaker = EWSCharacterId::YeCheng;
		Reply.ResponseType = EWSResponseType::FullDisclosure;
		Reply.Emotion = TEXT("focused");
		const FWSCharacterState YeCheng =
			State.Characters.FindRef(EWSCharacterId::YeCheng);
		const bool bDiagnosisKnown =
			State.Flags.bGuHengDiagnosed
			|| PlayerKnows(
				State,
				WhiteoutAgentFacts::MedicalDiagnosis,
				EWSKnowledgeLevel::Confirmed);
		const bool bDiagnosisQuestion =
			WSDialogueDisclosurePolicy::IsTargetedGuHengDiagnosisQuestion(Request);
		const bool bConditionObservation =
			WSDialogueDisclosurePolicy::IsGuHengConditionObservationQuestion(Request);
		const bool bHeatPackQuestion =
			WSDialogueDisclosurePolicy::IsHeatPackDisclosureQuestion(Request);
		if (
			Request.DialogueAct == EWSDialogueAct::Command
			&& (
				YeCheng.Trust < 3.0f
				|| YeCheng.Pressure >= 8.5f))
		{
			Reply.ResponseType = EWSResponseType::Refuse;
			Reply.Emotion = TEXT("firm");
			Reply.Utterance = TEXT("我不会在这种状态下照命令冒险。先处理眼前的风险和压力，再谈下一步。");
		}
		else if (
			YeCheng.Pressure >= 9.0f
			&& Request.DialogueAct != EWSDialogueAct::Reassure)
		{
			Reply.ResponseType = EWSResponseType::ConditionalAccept;
			Reply.Emotion = TEXT("strained");
			if (bHeatPackQuestion && bDiagnosisKnown && State.Flags.bHeatPackRevealed)
			{
				Reply.ReferencedFactIds = {
					WhiteoutAgentFacts::HandInjury,
					WhiteoutAgentFacts::MedicalDiagnosis,
					WhiteoutAgentFacts::HeatPack};
				Reply.Utterance = TEXT("我还能配合，但先给医务室升温。顾衡的手伤要尽快处理，柜底那只保温包也可以用。");
			}
			else if (bDiagnosisQuestion)
			{
				Reply.ReferencedFactIds = {
					WhiteoutAgentFacts::HandInjury,
					WhiteoutAgentFacts::MedicalDiagnosis};
				Reply.Utterance = TEXT("我还能配合，但顾衡的手伤不能再拖。先给医务室升温，我再继续处理。");
			}
			else
			{
				Reply.Utterance = TEXT("我还能配合，但先给医务室升温或安排休整；继续硬推只会让情况失控。");
			}
		}
		else if (Request.DialogueAct == EWSDialogueAct::Challenge)
		{
			Reply.ResponseType = EWSResponseType::PartialDisclosure;
			Reply.Emotion = TEXT("firm");
			if (bDiagnosisKnown)
			{
				Reply.ReferencedFactIds = {
					WhiteoutAgentFacts::HandInjury,
					WhiteoutAgentFacts::MedicalDiagnosis};
				Reply.Utterance = TEXT("你可以质疑我的判断，但顾衡的手伤和低温都在恶化。先给医务室升温，我会把诊断依据逐项告诉你。");
			}
			else
			{
				Reply.Utterance = TEXT("你可以质疑我的判断，但现在先把医务室升温。等我检查完，再把依据逐项告诉你。");
			}
		}
		else if (Request.DialogueAct == EWSDialogueAct::Reassure)
		{
			Reply.ResponseType = EWSResponseType::Reassure;
			Reply.Emotion = TEXT("steadier");
			if (bDiagnosisKnown)
			{
				Reply.ReferencedFactIds = {
					WhiteoutAgentFacts::HandInjury,
					WhiteoutAgentFacts::MedicalDiagnosis};
				Reply.Utterance = TEXT("好，我会稳住。顾衡需要升温、复查和固定伤手，我们按这个顺序来。");
			}
			else
			{
				Reply.Utterance = TEXT("好，我会稳住。先把医务室升温，剩下的等检查结果出来再安排。");
			}
		}
		else if (Request.DialogueAct == EWSDialogueAct::Promise)
		{
			Reply.ResponseType = EWSResponseType::Refuse;
			Reply.Emotion = TEXT("reserved");
			Reply.Utterance = TEXT("先别把承诺给我。把可执行的方案说清楚，再去和顾衡确认条件。");
		}
		else if (bHeatPackQuestion && bDiagnosisKnown && State.Flags.bHeatPackRevealed)
		{
			Reply.ReferencedFactIds = {
				WhiteoutAgentFacts::HandInjury,
				WhiteoutAgentFacts::MedicalDiagnosis,
				WhiteoutAgentFacts::HeatPack};
			Reply.Utterance = TEXT("顾衡的手伤已经影响精细操作。先把医务室升温；必要时，柜底还有一只保温包可用。");
		}
		else if (bDiagnosisQuestion)
		{
			Reply.ResponseType = EWSResponseType::PartialDisclosure;
			Reply.ReferencedFactIds = {
				WhiteoutAgentFacts::HandInjury,
				WhiteoutAgentFacts::MedicalDiagnosis};
			Reply.Utterance = TEXT("我已经确认顾衡的右手伤势会影响精细操作。先让医务室恢复温度，才能稳妥处理。");
		}
		else if (bConditionObservation)
		{
			Reply.ResponseType = EWSResponseType::PartialDisclosure;
			if (PlayerKnows(State, WhiteoutAgentFacts::HandInjury))
			{
				Reply.ReferencedFactIds = {WhiteoutAgentFacts::HandInjury};
				Reply.Utterance = TEXT("我只看到他在避开用右手做精细操作。这还是观察，不能代替针对性检查。");
			}
			else
			{
				Reply.Utterance = TEXT("我还没有完成针对性检查。先观察他能否稳定操作，再核对控制柜现场；现在不能下结论。");
			}
		}
		else
		{
			Reply.ResponseType = EWSResponseType::PartialDisclosure;
			Reply.Utterance = TEXT("备用电还在往下掉，暴雪窗口也在缩。先决定这一阶段把供暖给哪间房。");
		}
	}
	else if (Request.ActionId == TEXT("talk_gu_heng"))
	{
		Reply.Speaker = EWSCharacterId::GuHeng;
		const bool bKnowsRestartSuspicion =
			PlayerKnows(State, WhiteoutAgentFacts::ForcedRestartSuspicion);
		const bool bKnowsBurntRelay = PlayerKnows(State, WhiteoutAgentFacts::BurntRelay);
		const bool bKnowsRelayCompatibility = State.Flags.bRelayCompatibilityKnown;
		const bool bKnowsRestartConfirmed =
			PlayerKnows(State, WhiteoutAgentFacts::ForcedRestartConfirmed);
		const bool bHasBothEvidence = bKnowsRestartSuspicion && bKnowsBurntRelay;
		const FWSCharacterState GuHeng =
			State.Characters.FindRef(EWSCharacterId::GuHeng);
		const bool bGuHengTreated =
			State.RulesSchemaVersion >= 4
				? GuHeng.InjurySeverity == EWSInjurySeverity::Normal
				: State.Flags.bGuHengTreated;
		if (
			Request.SemanticFrame.QueryType == EWSDialogueQueryType::Requirements
			&& Request.SemanticFrame.TargetActionId == TEXT("repair_generator"))
		{
			const FWSNPCDialoguePlan Plan = BuildDialoguePlan(Request, State, RequirementReport);
			Reply.Speaker = Plan.Speaker;
			Reply.ResponseType = Plan.Stance;
			Reply.Emotion = TEXT("measured");
			Reply.Utterance = Plan.SemanticSpine;
			Reply.SemanticSpine = Plan.SemanticSpine;
			Reply.AnswerContract = Plan.Contract;
			Reply.RequirementReport = RequirementReport;
			Reply.CoveredConditionIds = Plan.Contract.MustCoverConditionIds;
			if (GuHeng.InjurySeverity != EWSInjurySeverity::Normal)
			{
				Reply.ReferencedFactIds = {WhiteoutAgentFacts::HandInjury};
			}
		}
		else if (
			Request.DialogueAct == EWSDialogueAct::Command
			&& (
				GuHeng.Trust < 4.0f
				|| GuHeng.Pressure >= 8.0f))
		{
			Reply.ResponseType = EWSResponseType::Refuse;
			Reply.Emotion = TEXT("defiant");
			Reply.Utterance = TEXT("条件还没解决，我不会服从这种危险命令。先拿出能执行的方案。");
		}
		else if (
			(
				GuHeng.Trust < 3.0f
				|| GuHeng.Pressure >= 9.0f)
			&& Request.DialogueAct != EWSDialogueAct::Reassure
			&& Request.DialogueAct != EWSDialogueAct::Promise)
		{
			Reply.ResponseType = EWSResponseType::Deflect;
			Reply.Emotion = TEXT("withdrawn");
			Reply.Utterance = TEXT("现在问什么我都不会接。先兑现一个条件，或者让我缓下来。");
		}
		else if (Request.DialogueAct == EWSDialogueAct::Promise)
		{
			Reply.ResponseType = EWSResponseType::ConditionalAccept;
			Reply.Emotion = TEXT("measured");
			if (Request.PromiseCondition == TEXT("heat_repair_room"))
			{
				Reply.Utterance = TEXT("把维修间温度拉起来，我就配合完成发电机修复。条件写进记录，别临时改口。");
			}
			else if (Request.PromiseCondition == TEXT("reserve_medicine"))
			{
				Reply.Utterance = TEXT("药品留作应急，我接受。先把剩余资源和使用顺序记清楚。");
			}
			else
			{
				Reply.Utterance = TEXT("维修记录必须保留。你愿意把责任和过程都写清楚，我就继续谈。");
			}
		}
		else if (Request.DialogueAct == EWSDialogueAct::Reassure)
		{
			Reply.ResponseType = EWSResponseType::Reassure;
			Reply.Emotion = TEXT("wary");
			if (PlayerKnows(State, WhiteoutAgentFacts::HandInjury)
				|| State.Flags.bGuHengDiagnosed)
			{
				Reply.ReferencedFactIds = {WhiteoutAgentFacts::HandInjury};
				Reply.Utterance = TEXT("我听到了。先把伤和低温处理掉，再给我一个能执行的维修顺序。");
			}
			else
			{
				Reply.Utterance = TEXT("我听到了。先把低温和配合问题解决，再给我一个能执行的维修顺序。");
			}
		}
		else if (Request.DialogueAct == EWSDialogueAct::Challenge && bHasBothEvidence)
		{
			Reply.ResponseType = EWSResponseType::PartialDisclosure;
			Reply.Emotion = TEXT("cornered");
			Reply.ReferencedFactIds = {
				WhiteoutAgentFacts::ForcedRestartSuspicion,
				WhiteoutAgentFacts::BurntRelay};
			if (bKnowsRelayCompatibility)
			{
				Reply.ReferencedFactIds.Add(WhiteoutAgentFacts::RelayCompatibility);
				if (bKnowsRestartConfirmed)
				{
					Reply.ReferencedFactIds.Add(WhiteoutAgentFacts::ForcedRestartConfirmed);
					Reply.Utterance = TEXT("日志和熔毁继电器都对得上，可以确认保护回路被旁路过。厨房加热器的规格能替，但拆掉会失去一处热源。");
				}
				else
				{
					Reply.Utterance = TEXT("日志和熔毁继电器都对得上。厨房加热器的规格能替，但拆掉会失去一处热源。");
				}
			}
			else
			{
				Reply.Utterance = TEXT("日志和熔毁继电器都对得上，保护回路很可能被旁路过。先把替代件规格查清楚，再决定怎么修。");
			}
		}
		else if (Request.DialogueAct == EWSDialogueAct::Challenge)
		{
			Reply.ResponseType = EWSResponseType::Deflect;
			Reply.Emotion = TEXT("defensive");
			Reply.Utterance = TEXT("拿证据来再质疑。现在继续逼问，只会浪费修复窗口。");
		}
		else if (bGuHengTreated)
		{
			Reply.ResponseType = EWSResponseType::ConditionalAccept;
			Reply.Emotion = TEXT("controlled");
			Reply.ReferencedFactIds = {WhiteoutAgentFacts::HandInjury};
			Reply.Utterance = TEXT("手现在能稳住。把维修间升温，我就接手发电机，再把已知故障逐项排掉。");
		}
		else if (bKnowsBurntRelay)
		{
			Reply.ResponseType = EWSResponseType::PartialDisclosure;
			Reply.Emotion = TEXT("cornered");
			Reply.ReferencedFactIds = {WhiteoutAgentFacts::BurntRelay};
			if (bKnowsRelayCompatibility)
			{
				Reply.ReferencedFactIds.Add(WhiteoutAgentFacts::RelayCompatibility);
				Reply.Utterance = TEXT("继电器确实烧了。你查到的厨房加热器规格能替，但拆了以后今晚就少一处热源。");
			}
			else
			{
				Reply.Utterance = TEXT("继电器确实烧了。先确认额定规格和接线，再谈替代件。");
			}
		}
		else if (bKnowsRestartSuspicion)
		{
			Reply.ResponseType = EWSResponseType::PartialDisclosure;
			Reply.Emotion = TEXT("guarded");
			Reply.ReferencedFactIds = {WhiteoutAgentFacts::ForcedRestartSuspicion};
			Reply.Utterance = TEXT("日志里的那次强制重启确实可疑，但你还没有找到能确认故障位置的实物证据。");
		}
		else
		{
			Reply.ResponseType = EWSResponseType::Deflect;
			Reply.Emotion = TEXT("guarded");
			Reply.Utterance = TEXT("先别审我。想让我修，就把配合和低温问题解决。");
		}
	}
	else if (Request.ActionId == TEXT("inspect_control_cabinet"))
	{
		Reply.Speaker = EWSCharacterId::GuHeng;
		Reply.ResponseType = EWSResponseType::PartialDisclosure;
		Reply.Emotion = TEXT("uneasy");
		Reply.ReferencedFactIds = {WhiteoutAgentFacts::BurntRelay, WhiteoutAgentFacts::HandInjury};
		Reply.Utterance = TEXT("触点已经熔了，硬接只会再烧一次。血是我的，先把柜门关上。");
	}
	else if (Request.ActionId == TEXT("distribute_food"))
	{
		Reply.Speaker = EWSCharacterId::GuHeng;
		const bool bGuHengReceivedFood = Request.FoodForGuHeng > 0;
		Reply.ResponseType = bGuHengReceivedFood ? EWSResponseType::Reassure : EWSResponseType::Accuse;
		Reply.Emotion = bGuHengReceivedFood ? TEXT("steadier") : TEXT("resentful");
		Reply.Utterance = bGuHengReceivedFood
			? TEXT("这份够我撑到修完。接下来按你排的顺序做。")
			: TEXT("你分得很清楚。要我继续干活，却连一口热量都没有。");
	}
	else if (Request.ActionId == TEXT("treat_gu_heng"))
	{
		Reply.Speaker = EWSCharacterId::YeCheng;
		Reply.ResponseType = EWSResponseType::Accept;
		Reply.Emotion = TEXT("clinical");
		Reply.ReferencedFactIds = {WhiteoutAgentFacts::HandInjury, WhiteoutAgentFacts::MedicalDiagnosis};
		Reply.Utterance = TEXT("伤口已经重新固定，末梢循环在恢复。他能工作，但别让这只手继续受冻。");
	}
	else if (Request.ActionId == TEXT("dismantle_kitchen_heater"))
	{
		Reply.Speaker = EWSCharacterId::GuHeng;
		Reply.ResponseType = EWSResponseType::ConditionalAccept;
		Reply.Emotion = TEXT("grim");
		Reply.ReferencedFactIds = {WhiteoutAgentFacts::RelayCompatibility};
		Reply.Utterance = TEXT("规格能对上。继电器拿走以后厨房会彻底冷下来——这是一次不可逆的交换。");
	}
	else if (Request.ActionId == TEXT("repair_generator"))
	{
		Reply.Speaker = EWSCharacterId::GuHeng;
		Reply.ResponseType = EWSResponseType::Accept;
		const FWSCharacterState GuHeng =
			State.Characters.FindRef(EWSCharacterId::GuHeng);
		const bool bGuHengTreated =
			State.RulesSchemaVersion >= 4
				? GuHeng.InjurySeverity == EWSInjurySeverity::Normal
				: State.Flags.bGuHengTreated;
		Reply.Emotion =
			bGuHengTreated
			? TEXT("focused")
			: TEXT("strained");
		Reply.Utterance = State.Tasks.GeneratorProgress >= 2
			? TEXT("转速稳住了，母线电压正在回升。现在去处理天线。")
			: TEXT("这一段恢复了，但还差最后一处故障。给我一点时间，或者找替代件。");
	}
	else if (Request.ActionId == TEXT("forced_self_repair"))
	{
		Reply.Speaker = EWSCharacterId::GuHeng;
		Reply.ResponseType = EWSResponseType::Accuse;
		Reply.Emotion = TEXT("alarmed");
		if (PlayerKnows(State, WhiteoutAgentFacts::GeneratorProtectionStop))
		{
			Reply.ReferencedFactIds = {WhiteoutAgentFacts::GeneratorProtectionStop};
			Reply.Utterance = TEXT("停手！保护停机不是让你硬跨过去的。你已经伤到了自己，别再碰第二次。");
		}
		else
		{
			Reply.Utterance = TEXT("停手！这种硬接风险太高。你已经伤到了自己，别再碰第二次。");
		}
	}
	else if (Request.ActionId == TEXT("calibrate_antenna"))
	{
		Reply.Speaker = EWSCharacterId::YeCheng;
		Reply.ResponseType = EWSResponseType::Reassure;
		Reply.Emotion = TEXT("urgent");
		Reply.Utterance = TEXT("校准完成，马上回舱。你的体温正在掉，剩下的可以在控制室做。");
	}
	else
	{
		Reply.Speaker = EWSCharacterId::YeCheng;
		Reply.ResponseType = EWSResponseType::Reassure;
		Reply.Emotion = TEXT("relieved");
		Reply.Utterance = TEXT("信号已经送出。现在守住温度和伤员，等救援确认回执。");
	}

	Reply.MovementIntent = EWSNPCMovementIntent::Stay;
	if (Reply.Emotion.Equals(TEXT("alarmed"), ESearchCase::IgnoreCase)
		|| Reply.Emotion.Equals(TEXT("urgent"), ESearchCase::IgnoreCase))
	{
		Reply.Reaction = EWSNPCReaction::Alarmed;
	}
	else
	{
		switch (Reply.ResponseType)
		{
		case EWSResponseType::Accept:
		case EWSResponseType::ConditionalAccept:
			Reply.Reaction = EWSNPCReaction::Acknowledge;
			break;
		case EWSResponseType::PartialDisclosure:
		case EWSResponseType::FullDisclosure:
			Reply.Reaction = EWSNPCReaction::Consider;
			break;
		case EWSResponseType::Reassure:
			Reply.Reaction = EWSNPCReaction::Reassure;
			break;
		case EWSResponseType::Refuse:
		case EWSResponseType::Deflect:
		case EWSResponseType::Accuse:
			Reply.Reaction = EWSNPCReaction::Reject;
			break;
		default:
			Reply.Reaction = EWSNPCReaction::Neutral;
			break;
		}
	}

	const TArray<FName> AllowedFacts = BuildAllowedFacts(Request.ActionId, Reply.Speaker, State);
	Reply.ReferencedFactIds = Reply.ReferencedFactIds.FilterByPredicate(
		[&AllowedFacts](const FName FactId) { return AllowedFacts.Contains(FactId); });
	if (Reply.SemanticSpine.IsEmpty())
	{
		Reply.SemanticSpine = Reply.Utterance;
	}
	Reply.AnswerSource = TEXT("spine_only");
	return Reply;
}

TArray<FName> UWSNPCDecisionService::BuildAllowedFacts(
	const FName ActionId,
	const EWSCharacterId Speaker,
	const FWSGameState& State)
{
	using namespace WhiteoutAgentFacts;
	TArray<FName> Result = State.PublicFacts;
	if (Speaker == EWSCharacterId::YeCheng)
	{
		const bool bDiagnosisKnown =
			State.Flags.bGuHengDiagnosed
			|| PlayerKnows(State, MedicalDiagnosis, EWSKnowledgeLevel::Confirmed);
		if (ActionId == TEXT("treat_gu_heng") || bDiagnosisKnown)
		{
			Result.AddUnique(HandInjury);
			Result.AddUnique(MedicalDiagnosis);
		}
		else if (PlayerKnows(State, HandInjury))
		{
			Result.AddUnique(HandInjury);
		}
		if (PlayerKnows(State, HeatPack)
			|| (ActionId == TEXT("treat_gu_heng") && State.Flags.bHeatPackRevealed))
		{
			Result.AddUnique(HeatPack);
		}
	}
	else if (Speaker == EWSCharacterId::GuHeng)
	{
		if (ActionId == TEXT("inspect_control_cabinet"))
		{
			Result.AddUnique(BurntRelay);
			Result.AddUnique(HandInjury);
		}
		if ((ActionId == TEXT("talk_gu_heng") || ActionId == TEXT("repair_generator"))
			&& (PlayerKnows(State, HandInjury) || State.Flags.bGuHengDiagnosed))
		{
			Result.AddUnique(HandInjury);
		}
		if (ActionId == TEXT("forced_self_repair") && PlayerKnows(State, GeneratorProtectionStop))
		{
			Result.AddUnique(GeneratorProtectionStop);
		}
		if (PlayerKnows(State, RelayCompatibility))
		{
			Result.AddUnique(RelayCompatibility);
		}
		for (const FName FactId : {GeneratorProtectionStop, ForcedRestartSuspicion, BurntRelay, ForcedRestartConfirmed})
		{
			if (PlayerKnows(State, FactId))
			{
				Result.AddUnique(FactId);
			}
		}
	}
	return Result;
}

FWSNPCDialoguePlan UWSNPCDecisionService::BuildDialoguePlan(
	const FWSActionRequest& Request,
	const FWSGameState& State,
	const FWSActionRequirementReport& RequirementReport)
{
	FWSNPCDialoguePlan Plan;
	Plan.Speaker = Request.SemanticFrame.TargetCharacter;
	if (Plan.Speaker == EWSCharacterId::Player)
	{
		Plan.Speaker = EWSCharacterId::GuHeng;
	}
	Plan.Stance = RequirementReport.bCurrentlyExecutable
		? EWSResponseType::Accept
		: EWSResponseType::ConditionalAccept;
	Plan.Contract.QueryType = Request.SemanticFrame.QueryType;
	Plan.Contract.TargetActionId = Request.SemanticFrame.TargetActionId;
	Plan.Contract.MaxSentences = 3;
	Plan.AllowedFactIds = BuildAllowedFacts(Request.ActionId, Plan.Speaker, State);

	if (Request.SemanticFrame.QueryType != EWSDialogueQueryType::Requirements
		|| Request.SemanticFrame.TargetActionId != TEXT("repair_generator"))
	{
		return Plan;
	}

	if (RequirementReport.ActionId != TEXT("repair_generator"))
	{
		Plan.Contract.MustCoverConditionIds = {
			TEXT("player_collaboration"),
			TEXT("repair_room_heated"),
			TEXT("gu_heng_stamina_ready")};
		Plan.SemanticSpine = TEXT("要我接手发电机，你得在旁搭手。先把维修间弄暖，再让我缓口气；设备上的缺口还得继续查清。");
		return Plan;
	}

	bool bAvailable = true;
	bool bCollaborationSatisfied = true;
	for (const FWSRequirementItem& Item : RequirementReport.UniversalRequirements)
	{
		if (!Item.bDisclosable)
		{
			continue;
		}
		if (Item.RequirementId == TEXT("gu_heng_available"))
		{
			bAvailable = Item.bSatisfied;
		}
		else if (Item.RequirementId == TEXT("player_collaboration"))
		{
			bCollaborationSatisfied = Item.bSatisfied;
		}
		if (Item.bSatisfied)
		{
			Plan.Contract.AllowedOptionalConditionIds.AddUnique(Item.RequirementId);
		}
		else
		{
			Plan.Contract.MustCoverConditionIds.AddUnique(Item.RequirementId);
		}
	}

	bool bRepairRoomHeated = false;
	bool bStaminaReady = false;
	bool bRelayReady = false;
	const bool bRelayRouteKnown =
		State.Resources.ReplacementRelay > 0
		|| State.Flags.bRelayCompatibilityKnown
		|| PlayerKnows(State, WhiteoutAgentFacts::RelayCompatibility);
	for (const FWSRequirementPlan& RequirementPlan : RequirementReport.AlternativePlans)
	{
		for (const FWSRequirementItem& Item : RequirementPlan.Requirements)
		{
			if (!Item.bDisclosable)
			{
				continue;
			}
			if (Item.RequirementId == TEXT("replacement_relay_available")
				&& !bRelayRouteKnown)
			{
				continue;
			}
			if (Item.RequirementId == TEXT("repair_room_heated"))
			{
				bRepairRoomHeated = Item.bSatisfied;
			}
			else if (Item.RequirementId == TEXT("gu_heng_stamina_ready"))
			{
				bStaminaReady = Item.bSatisfied;
			}
			else if (Item.RequirementId == TEXT("replacement_relay_available"))
			{
				bRelayReady = Item.bSatisfied;
			}
			if (Item.bSatisfied)
			{
				Plan.Contract.AllowedOptionalConditionIds.AddUnique(Item.RequirementId);
			}
			else
			{
				Plan.Contract.MustCoverConditionIds.AddUnique(Item.RequirementId);
			}
		}
	}
	for (const FWSRequirementItem& Risk : RequirementReport.Risks)
	{
		if (Risk.bDisclosable)
		{
			Plan.Contract.AllowedRiskIds.AddUnique(Risk.RequirementId);
		}
	}

	FString Opening;
	if (!bAvailable)
	{
		Opening = TEXT("先让我缓过来，到了能安全动手的状态再说。维修时你得在旁搭手。");
	}
	else if (!bCollaborationSatisfied)
	{
		Opening = TEXT("要我接手发电机，你得在旁搭手。");
	}
	else
	{
		Opening = TEXT("要我接手发电机，眼下的配合够了。");
	}

	FString SupportedRoute;
	if (bRepairRoomHeated && bStaminaReady)
	{
		SupportedRoute = TEXT("维修间够暖，我也缓过来了，这条路能走");
	}
	else if (!bRepairRoomHeated && !bStaminaReady)
	{
		SupportedRoute = TEXT("先把维修间弄暖，再让我缓口气");
	}
	else if (!bRepairRoomHeated)
	{
		SupportedRoute = TEXT("把维修间升温");
	}
	else
	{
		SupportedRoute = TEXT("先让我缓口气");
	}
	Plan.SemanticSpine = FString::Printf(TEXT("%s%s。"), *Opening, *SupportedRoute);
	if (bRelayRouteKnown)
	{
		Plan.SemanticSpine += bRelayReady
			? TEXT("备用件也已经备好，必要时可以走那条路。")
			: TEXT("或者把已经确认可用的替代件准备好。");
	}
	return Plan;
}

FString UWSNPCDecisionService::SpeakerLabel(const EWSCharacterId Speaker)
{
	return Speaker == EWSCharacterId::GuHeng ? TEXT("顾衡") : Speaker == EWSCharacterId::YeCheng ? TEXT("叶澄") : TEXT("玩家");
}

bool UWSNPCDecisionService::PlayerKnows(
	const FWSGameState& State,
	const FName FactId,
	const EWSKnowledgeLevel Minimum)
{
	const EWSKnowledgeLevel* Level = State.PlayerKnowledge.Find(FactId);
	return Level && static_cast<uint8>(*Level) >= static_cast<uint8>(Minimum);
}
