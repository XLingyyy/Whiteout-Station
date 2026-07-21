#include "Presentation/WSPresentationText.h"

namespace
{
	const FName UITextTable(TEXT("/Game/WindStation/UI/ST_WhiteoutStation_zh.ST_WhiteoutStation_zh"));

	FText Text(const TCHAR* Value)
	{
		return FText::FromString(Value);
	}

	FText TableText(const TCHAR* Key, const TCHAR* Fallback)
	{
		const FText Value = FText::FromStringTable(UITextTable, FTextKey(Key));
		return Value.IsEmpty() ? Text(Fallback) : Value;
	}
}

FText FWSPresentationText::ActionLabel(const FName ActionId)
{
	if (ActionId == TEXT("investigate_generator_log")) return TableText(TEXT("action_investigate_generator_log"), TEXT("调查发电机运行记录"));
	if (ActionId == TEXT("send_signal")) return TableText(TEXT("action_send_signal"), TEXT("发送求救信号"));
	if (ActionId == TEXT("inspect_control_cabinet")) return TableText(TEXT("action_inspect_control_cabinet"), TEXT("检查烧毁的控制柜"));
	if (ActionId == TEXT("heat_repair_room")) return TableText(TEXT("action_heat_repair_room"), TEXT("为维修间供暖"));
	if (ActionId == TEXT("repair_generator")) return TableText(TEXT("action_repair_generator"), TEXT("修复柴油发电机"));
	if (ActionId == TEXT("forced_self_repair")) return TableText(TEXT("action_forced_self_repair"), TEXT("强行独自维修"));
	if (ActionId == TEXT("talk_gu_heng")) return TableText(TEXT("action_talk_gu_heng"), TEXT("与顾衡交谈"));
	if (ActionId == TEXT("heat_medical_room")) return TableText(TEXT("action_heat_medical_room"), TEXT("为医务室供暖"));
	if (ActionId == TEXT("treat_gu_heng")) return TableText(TEXT("action_treat_gu_heng"), TEXT("治疗顾衡"));
	if (ActionId == TEXT("talk_ye_cheng")) return TableText(TEXT("action_talk_ye_cheng"), TEXT("与叶澄交谈"));
	if (ActionId == TEXT("distribute_food")) return TableText(TEXT("action_distribute_food"), TEXT("分配口粮"));
	if (ActionId == TEXT("dismantle_kitchen_heater")) return TableText(TEXT("action_dismantle_kitchen_heater"), TEXT("拆解厨房加热器"));
	if (ActionId == TEXT("calibrate_antenna")) return TableText(TEXT("action_calibrate_antenna"), TEXT("校准结冰的天线"));
	return Text(TEXT("未知行动"));
}

FText FWSPresentationText::ActionImpact(const FName ActionId)
{
	if (ActionId == TEXT("investigate_generator_log")) return Text(TEXT("确认故障记录并获得与维修有关的证据。"));
	if (ActionId == TEXT("send_signal")) return Text(TEXT("在发电机与天线就绪后发出求救信号；该行动不消耗行动力。"));
	if (ActionId == TEXT("inspect_control_cabinet")) return Text(TEXT("检查继电器损坏状况，推进替代部件线索。"));
	if (ActionId == TEXT("heat_repair_room")) return Text(TEXT("消耗燃料改善维修环境，也可能兑现对顾衡的承诺。"));
	if (ActionId == TEXT("repair_generator")) return Text(TEXT("推进发电机修复；需要线索、部件或顾衡配合。"));
	if (ActionId == TEXT("forced_self_repair")) return Text(TEXT("无需顾衡配合尝试维修，但会让你承担更高的人身代价。"));
	if (ActionId == TEXT("talk_gu_heng")) return Text(TEXT("根据当前对话意图获取信息、争取配合或记录承诺。"));
	if (ActionId == TEXT("heat_medical_room")) return Text(TEXT("消耗燃料恢复医务室治疗条件。"));
	if (ActionId == TEXT("treat_gu_heng")) return Text(TEXT("消耗医疗资源稳定顾衡，为后续维修创造条件。"));
	if (ActionId == TEXT("talk_ye_cheng")) return Text(TEXT("获取诊断、物资或责任相关信息。"));
	if (ActionId == TEXT("distribute_food")) return Text(TEXT("把有限口粮分给你和顾衡，改善体力并影响关系。"));
	if (ActionId == TEXT("dismantle_kitchen_heater")) return Text(TEXT("牺牲厨房供暖换取可用于发电机的替代继电器。"));
	if (ActionId == TEXT("calibrate_antenna")) return Text(TEXT("在室外完成天线校准，为发送信号做准备。"));
	return Text(TEXT("该行动会写入一次确定性状态事务。"));
}

FText FWSPresentationText::ReasonCause(const EWSReasonCode Reason)
{
	switch (Reason)
	{
	case EWSReasonCode::Ok: return Text(TEXT("条件满足。"));
	case EWSReasonCode::Committed: return Text(TEXT("行动已经完成。"));
	case EWSReasonCode::UnknownAction: return Text(TEXT("这个交互点暂时不可用。"));
	case EWSReasonCode::PhaseLocked: return Text(TEXT("当前阶段不能执行这项行动。"));
	case EWSReasonCode::InsufficientAP: return Text(TEXT("剩余行动力不足。"));
	case EWSReasonCode::AlreadyCompleted: return Text(TEXT("这项工作已经完成。"));
	case EWSReasonCode::UseLimitReached: return Text(TEXT("这项行动的使用次数已经耗尽。"));
	case EWSReasonCode::DuplicateTransaction: return Text(TEXT("这项行动已被系统记录，无需重复提交。"));
	case EWSReasonCode::AlreadyHeated: return Text(TEXT("该区域的供暖已经恢复。"));
	case EWSReasonCode::NeedsFuel: return Text(TEXT("当前没有足够燃料供暖。"));
	case EWSReasonCode::InvalidFoodAllocation: return Text(TEXT("口粮分配超出了现有储备。"));
	case EWSReasonCode::EmptyFoodAllocation: return Text(TEXT("尚未选择要分配的口粮。"));
	case EWSReasonCode::InsufficientFood: return Text(TEXT("食品储备不足。"));
	case EWSReasonCode::NeedsMedicalHeat: return Text(TEXT("医务室过冷，无法安全治疗。"));
	case EWSReasonCode::NeedsDiagnosis: return Text(TEXT("还缺少叶澄的明确诊断。"));
	case EWSReasonCode::HeatPackHidden: return Text(TEXT("保温包的位置尚未被发现。"));
	case EWSReasonCode::InvalidTreatmentResource: return Text(TEXT("选择的治疗资源当前不可用。"));
	case EWSReasonCode::NeedsMedicine: return Text(TEXT("药品已经耗尽。"));
	case EWSReasonCode::NeedsHeatPack: return Text(TEXT("需要先取得隐藏的保温包。"));
	case EWSReasonCode::NeedsRelayEvidence: return Text(TEXT("还不能确认替代继电器是否兼容。"));
	case EWSReasonCode::HeaterAlreadyDismantled: return Text(TEXT("厨房加热器已经被拆解。"));
	case EWSReasonCode::GeneratorAlreadyRepaired: return Text(TEXT("发电机已经修复。"));
	case EWSReasonCode::GuHengCritical: return Text(TEXT("顾衡的身体状况太差，无法参与维修。"));
	case EWSReasonCode::NeedsCooperation: return Text(TEXT("顾衡还没有同意配合维修。"));
	case EWSReasonCode::NeedsGeneratorRecords: return Text(TEXT("缺少发电机运行记录，无法判断故障。"));
	case EWSReasonCode::SelfRepairAlreadyUsed: return Text(TEXT("你已经承担过一次强行维修，不能再次冒险。"));
	case EWSReasonCode::NeedsGenerator: return Text(TEXT("发电机尚未修复。"));
	case EWSReasonCode::AntennaAlreadyCalibrated: return Text(TEXT("天线已经校准。"));
	case EWSReasonCode::PlayerTooCold: return Text(TEXT("你已经严重失温，无法继续室外作业。"));
	case EWSReasonCode::NeedsAntenna: return Text(TEXT("天线尚未校准。"));
	default: return Text(TEXT("当前条件不满足。"));
	}
}

FText FWSPresentationText::ReasonNextStep(const EWSReasonCode Reason)
{
	switch (Reason)
	{
	case EWSReasonCode::InsufficientAP: return Text(TEXT("改选低成本行动，或在行动力归零后使用免费的发信窗口。"));
	case EWSReasonCode::NeedsFuel: return Text(TEXT("保留或寻找燃料，再回到供暖控制器。"));
	case EWSReasonCode::NeedsMedicalHeat: return Text(TEXT("先使用医务室供暖控制器。"));
	case EWSReasonCode::NeedsDiagnosis: return Text(TEXT("先与叶澄交谈并询问顾衡的伤情。"));
	case EWSReasonCode::HeatPackHidden: return Text(TEXT("从叶澄处取得物资线索。"));
	case EWSReasonCode::NeedsMedicine: return Text(TEXT("改用已发现的保温包，或选择不消耗药品的路线。"));
	case EWSReasonCode::NeedsHeatPack: return Text(TEXT("与叶澄交谈，确认隐藏保温包的位置。"));
	case EWSReasonCode::NeedsRelayEvidence: return Text(TEXT("检查控制柜，或取得能证明继电器兼容的记录。"));
	case EWSReasonCode::GuHengCritical: return Text(TEXT("先恢复医务室供暖并治疗顾衡。"));
	case EWSReasonCode::NeedsCooperation: return Text(TEXT("与顾衡交谈，询问、安抚或作出可兑现的承诺。"));
	case EWSReasonCode::NeedsGeneratorRecords: return Text(TEXT("返回控制室调查发电机运行记录。"));
	case EWSReasonCode::NeedsGenerator: return Text(TEXT("先完成发电机两阶段维修。"));
	case EWSReasonCode::PlayerTooCold: return Text(TEXT("留在室内恢复状态，避免继续消耗体温。"));
	case EWSReasonCode::NeedsAntenna: return Text(TEXT("前往室外天线区完成校准。"));
	case EWSReasonCode::PhaseLocked: return Text(TEXT("关闭当前界面或进入下一阶段后再试。"));
	default: return Text(TEXT("检查目标、资源、证据和队员状态后选择下一步。"));
	}
}

FText FWSPresentationText::EvidenceLabel(const FName EvidenceId)
{
	if (EvidenceId == TEXT("generator_log")) return Text(TEXT("发电机运行记录：电压波动始于继电器损坏之后。"));
	if (EvidenceId == TEXT("relay_burn_pattern")) return Text(TEXT("控制柜烧蚀痕迹：旧继电器已经失效。"));
	if (EvidenceId == TEXT("relay_compatibility")) return Text(TEXT("替代继电器标牌：规格可以匹配发电机。"));
	if (EvidenceId == TEXT("gu_heng_diagnosis")) return Text(TEXT("顾衡诊断：失温与外伤正在影响其工作能力。"));
	if (EvidenceId == TEXT("hidden_heat_pack")) return Text(TEXT("隐藏物资：医务柜后方留有一只保温包。"));
	if (EvidenceId == TEXT("antenna_ice")) return Text(TEXT("天线结冰：方位机构需要现场重新校准。"));
	if (EvidenceId == TEXT("records_preserved")) return Text(TEXT("维修记录已保存，可追溯故障与责任。"));
	return Text(TEXT("尚未整理的现场证据。"));
}

FText FWSPresentationText::FactLabel(const FName FactId)
{
	if (FactId == TEXT("generator_fault")) return Text(TEXT("发电机故障与继电器损坏有关"));
	if (FactId == TEXT("relay_compatible")) return Text(TEXT("厨房加热器中的继电器可以替代"));
	if (FactId == TEXT("gu_heng_condition")) return Text(TEXT("顾衡需要治疗后才能稳定工作"));
	if (FactId == TEXT("heat_pack_location")) return Text(TEXT("医务室藏有应急保温包"));
	if (FactId == TEXT("antenna_requires_calibration")) return Text(TEXT("天线必须在室外重新校准"));
	if (FactId == TEXT("records_accountability")) return Text(TEXT("保留维修记录会影响信息责任"));
	return Text(TEXT("未命名事实"));
}

FText FWSPresentationText::PromiseLabel(const FName ConditionId)
{
	if (ConditionId == TEXT("heat_repair_room")) return Text(TEXT("恢复维修间供暖"));
	if (ConditionId == TEXT("reserve_medicine")) return Text(TEXT("为顾衡保留药品"));
	if (ConditionId == TEXT("keep_records")) return Text(TEXT("保存完整维修记录"));
	return Text(TEXT("未说明的承诺"));
}

FText FWSPresentationText::PhaseLabel(const EWSGamePhase Phase)
{
	switch (Phase)
	{
	case EWSGamePhase::Opening: return Text(TEXT("事发"));
	case EWSGamePhase::ActionPhase: return Text(TEXT("抢修"));
	case EWSGamePhase::ResolvingAction: return Text(TEXT("执行中"));
	case EWSGamePhase::DialogueFeedback: return Text(TEXT("交涉"));
	case EWSGamePhase::MidCrisis: return Text(TEXT("备用电池故障"));
	case EWSGamePhase::PostActionWindow: return Text(TEXT("最后窗口"));
	case EWSGamePhase::EndingChoice: return Text(TEXT("最后决定"));
	case EWSGamePhase::Ending: return Text(TEXT("结局"));
	case EWSGamePhase::Results: return Text(TEXT("复盘"));
	default: return Text(TEXT("抢修"));
	}
}

FText FWSPresentationText::EndingTitle(const EWSEndingType Ending)
{
	if (Ending == EWSEndingType::TaskSuccess) return TableText(TEXT("ending_task_success"), TEXT("信号穿过风雪"));
	if (Ending == EWSEndingType::SurvivalWait) return TableText(TEXT("ending_survival_wait"), TEXT("等待天明"));
	if (Ending == EWSEndingType::CostUncontrolled) return TableText(TEXT("ending_cost_uncontrolled"), TEXT("求救成功，代价失控"));
	return TableText(TEXT("ending_total_collapse"), TEXT("气象站失守"));
}

FText FWSPresentationText::EndingSummary(const EWSEndingType Ending)
{
	if (Ending == EWSEndingType::TaskSuccess) return Text(TEXT("求救信号已经发出，人员与储备仍在可控范围内。"));
	if (Ending == EWSEndingType::SurvivalWait) return Text(TEXT("信号未能发出，你们只能依靠现有储备等待风暴减弱。"));
	if (Ending == EWSEndingType::CostUncontrolled) return Text(TEXT("外界收到了信号，但人员、物资或关系付出了沉重代价。"));
	return Text(TEXT("电力、任务与人员状态同时越过了安全边界。"));
}

FText FWSPresentationText::CharacterName(const EWSCharacterId CharacterId)
{
	if (CharacterId == EWSCharacterId::GuHeng) return Text(TEXT("顾衡｜工程师｜41 岁"));
	if (CharacterId == EWSCharacterId::YeCheng) return Text(TEXT("叶澄｜医生｜31 岁"));
	return Text(TEXT("你｜值班负责人"));
}

FText FWSPresentationText::ConditionLevel(const float Value)
{
	if (Value >= 70.0f) return Text(TEXT("稳定"));
	if (Value >= 45.0f) return Text(TEXT("吃紧"));
	if (Value >= 30.0f) return Text(TEXT("危险"));
	return Text(TEXT("危急"));
}

FText FWSPresentationText::TrustLevel(const float Value)
{
	if (Value >= 35.0f) return Text(TEXT("信任"));
	if (Value >= 0.0f) return Text(TEXT("中立"));
	if (Value >= -35.0f) return Text(TEXT("戒备"));
	return Text(TEXT("敌对"));
}

FText FWSPresentationText::KnowledgeLevel(const EWSKnowledgeLevel Level)
{
	if (Level == EWSKnowledgeLevel::Confirmed) return Text(TEXT("已确认"));
	if (Level == EWSKnowledgeLevel::Suspected) return Text(TEXT("有根据的推测"));
	if (Level == EWSKnowledgeLevel::Claimed) return Text(TEXT("单方说法"));
	return Text(TEXT("未知"));
}
