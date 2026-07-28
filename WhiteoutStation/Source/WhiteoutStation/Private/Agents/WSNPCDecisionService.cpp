#include "Agents/WSNPCDecisionService.h"

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
	return ActionId == TEXT("talk_gu_heng") || ActionId == TEXT("talk_ye_cheng");
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
	FWSAgentReply Reply;
	Reply.ActionId = Request.ActionId;
	Reply.TransactionId = Request.TransactionId;
	Reply.DialogueSessionId = Request.DialogueSessionId;
	Reply.bAccepted = true;
	Reply.bFallback = true;
	Reply.Provider = TEXT("preset");
	Reply.ValidationReason = TEXT("deterministic_decision");

	if (Request.ActionId == TEXT("talk_ye_cheng"))
	{
		Reply.Speaker = EWSCharacterId::YeCheng;
		Reply.ResponseType = EWSResponseType::FullDisclosure;
		Reply.Emotion = TEXT("focused");
		Reply.ReferencedFactIds = {WhiteoutAgentFacts::HandInjury, WhiteoutAgentFacts::MedicalDiagnosis};
		if (Request.DialogueAct == EWSDialogueAct::Challenge)
		{
			Reply.ResponseType = EWSResponseType::PartialDisclosure;
			Reply.Emotion = TEXT("firm");
			Reply.Utterance = TEXT("你可以质疑我的判断，但顾衡的手伤和低温都在恶化。先给医务室升温，我会把诊断依据逐项告诉你。");
		}
		else if (Request.DialogueAct == EWSDialogueAct::Reassure)
		{
			Reply.ResponseType = EWSResponseType::Reassure;
			Reply.Emotion = TEXT("steadier");
			Reply.Utterance = TEXT("好，我会稳住。顾衡需要的是升温、诊断和固定伤手，我们按这个顺序来。");
		}
		else if (Request.DialogueAct == EWSDialogueAct::Promise)
		{
			Reply.ResponseType = EWSResponseType::Refuse;
			Reply.Emotion = TEXT("reserved");
			Reply.Utterance = TEXT("先别把承诺给我。把可执行的方案说清楚，再去和顾衡确认条件。");
		}
		else if (State.Flags.bHeatPackRevealed)
		{
			Reply.ReferencedFactIds.Add(WhiteoutAgentFacts::HeatPack);
			Reply.Utterance = TEXT("顾衡的手伤已经影响精细操作。先把医务室升温；必要时，柜底还有一只保温包可用。");
		}
		else
		{
			Reply.Utterance = TEXT("顾衡的手伤不能再拖。先让医务室恢复温度，我才能完成诊断和处理。");
		}
	}
	else if (Request.ActionId == TEXT("talk_gu_heng"))
	{
		Reply.Speaker = EWSCharacterId::GuHeng;
		const bool bKnowsRestartSuspicion =
			PlayerKnows(State, WhiteoutAgentFacts::ForcedRestartSuspicion);
		const bool bKnowsBurntRelay = PlayerKnows(State, WhiteoutAgentFacts::BurntRelay);
		const bool bKnowsRelayCompatibility = State.Flags.bRelayCompatibilityKnown;
		const bool bHasBothEvidence = bKnowsRestartSuspicion && bKnowsBurntRelay;
		if (Request.DialogueAct == EWSDialogueAct::Promise)
		{
			Reply.ResponseType = EWSResponseType::ConditionalAccept;
			Reply.Emotion = TEXT("measured");
			if (Request.PromiseCondition == TEXT("heat_repair_room"))
			{
				Reply.Utterance = TEXT("把维修间温度拉起来，我就配合完成发电机修复。条件写进记录，别临时改口。");
			}
			else if (Request.PromiseCondition == TEXT("reserve_medicine"))
			{
				Reply.Utterance = TEXT("药品留给真正需要的人，我接受。先把伤情和剩余资源记清楚。");
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
			Reply.ReferencedFactIds = {WhiteoutAgentFacts::HandInjury};
			Reply.Utterance = TEXT("我听到了。先把伤和低温处理掉，再给我一个能执行的维修顺序。");
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
				Reply.Utterance = TEXT("日志和熔毁继电器都对得上，保护回路很可能被旁路过。厨房加热器的规格能替，但拆掉会失去一处热源。");
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
		else if (State.Flags.bGuHengTreated)
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
			Reply.ReferencedFactIds = {WhiteoutAgentFacts::HandInjury};
			Reply.Utterance = TEXT("先别审我。我的手还使不上力，想让我修，就先解决伤和低温。");
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
		Reply.ResponseType = State.Flags.bGuHengFed ? EWSResponseType::Reassure : EWSResponseType::Accuse;
		Reply.Emotion = State.Flags.bGuHengFed ? TEXT("steadier") : TEXT("resentful");
		Reply.Utterance = State.Flags.bGuHengFed
			? TEXT("这份够我撑到修完。接下来按你排的顺序做。")
			: TEXT("你分得很清楚。要我带伤干活，却连一口热量都没有。");
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
		Reply.Emotion = State.Flags.bGuHengTreated ? TEXT("focused") : TEXT("strained");
		Reply.ReferencedFactIds = {WhiteoutAgentFacts::HandInjury};
		Reply.Utterance = State.Tasks.GeneratorProgress >= 2
			? TEXT("转速稳住了，母线电压正在回升。现在去处理天线。")
			: TEXT("这一段恢复了，但还差最后一处故障。给我一点时间，或者找替代件。");
	}
	else if (Request.ActionId == TEXT("forced_self_repair"))
	{
		Reply.Speaker = EWSCharacterId::GuHeng;
		Reply.ResponseType = EWSResponseType::Accuse;
		Reply.Emotion = TEXT("alarmed");
		Reply.ReferencedFactIds = {WhiteoutAgentFacts::GeneratorProtectionStop};
		Reply.Utterance = TEXT("停手！保护停机不是让你硬跨过去的。你已经伤到了自己，别再碰第二次。");
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
		if (ActionId == TEXT("talk_ye_cheng") || ActionId == TEXT("treat_gu_heng"))
		{
			Result.AddUnique(HandInjury);
			Result.AddUnique(MedicalDiagnosis);
		}
		if (State.Flags.bHeatPackRevealed)
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
		if (ActionId == TEXT("talk_gu_heng") || ActionId == TEXT("repair_generator"))
		{
			Result.AddUnique(HandInjury);
		}
		if (ActionId == TEXT("forced_self_repair") && PlayerKnows(State, GeneratorProtectionStop))
		{
			Result.AddUnique(GeneratorProtectionStop);
		}
		if (State.Flags.bRelayCompatibilityKnown)
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
