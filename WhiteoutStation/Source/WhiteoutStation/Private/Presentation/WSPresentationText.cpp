#include "Presentation/WSPresentationText.h"

#include "State/WSKnowledgePolicy.h"

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
		const FString Resolved = Value.ToString();
		return Value.IsEmpty() || Resolved.Contains(TEXT("MISSING STRING TABLE ENTRY")) ? Text(Fallback) : Value;
	}
}

FText FWSPresentationText::UI(const FName Key, const TCHAR* Fallback)
{
	const FString KeyString = Key.ToString();
	const FText Value = FText::FromStringTable(UITextTable, FTextKey(KeyString));
	const FString Resolved = Value.ToString();
	return Value.IsEmpty() || Resolved.Contains(TEXT("MISSING STRING TABLE ENTRY")) ? Text(Fallback) : Value;
}

FText FWSPresentationText::ActionLabel(const FName ActionId)
{
	if (ActionId == TEXT("investigate_generator_log")) return TableText(TEXT("action_investigate_generator_log"), TEXT("调查发电机运行记录"));
	if (ActionId == TEXT("send_signal")) return TableText(TEXT("action_send_signal"), TEXT("发送求救信号"));
	if (ActionId == TEXT("heat_control_room")) return Text(TEXT("本阶段为控制室供暖"));
	if (ActionId == TEXT("inspect_control_cabinet")) return TableText(TEXT("action_inspect_control_cabinet"), TEXT("检查烧毁的控制柜"));
	if (ActionId == TEXT("heat_repair_room")) return TableText(TEXT("action_heat_repair_room"), TEXT("为维修间供暖"));
	if (ActionId == TEXT("repair_generator")) return TableText(TEXT("action_repair_generator"), TEXT("修复柴油发电机"));
	if (ActionId == TEXT("forced_self_repair")) return TableText(TEXT("action_forced_self_repair"), TEXT("强行独自维修"));
	if (ActionId == TEXT("talk_gu_heng")) return TableText(TEXT("action_talk_gu_heng"), TEXT("与顾衡交谈"));
	if (ActionId == TEXT("heat_medical_room")) return TableText(TEXT("action_heat_medical_room"), TEXT("为医务室供暖"));
	if (ActionId == TEXT("treat_gu_heng") || ActionId == TEXT("treat_character")) return TableText(TEXT("action_treat_gu_heng"), TEXT("诊断与治疗"));
	if (ActionId == TEXT("talk_ye_cheng")) return TableText(TEXT("action_talk_ye_cheng"), TEXT("与叶澄交谈"));
	if (ActionId == TEXT("distribute_food")) return TableText(TEXT("action_distribute_food"), TEXT("分配口粮"));
	if (ActionId == TEXT("heat_kitchen")) return Text(TEXT("本阶段为厨房供暖"));
	if (ActionId == TEXT("rest")) return Text(TEXT("休整或等待"));
	if (ActionId == TEXT("dismantle_kitchen_heater")) return TableText(TEXT("action_dismantle_kitchen_heater"), TEXT("拆解厨房加热器"));
	if (ActionId == TEXT("calibrate_antenna")) return TableText(TEXT("action_calibrate_antenna"), TEXT("校准结冰的天线"));
	return TableText(TEXT("action_unknown"), TEXT("未知行动"));
}

FText FWSPresentationText::ActionImpact(const FName ActionId)
{
	if (ActionId == TEXT("investigate_generator_log")) return TableText(TEXT("impact_investigate_generator_log"), TEXT("确认故障记录并获得与维修有关的证据。"));
	if (ActionId == TEXT("send_signal")) return TableText(TEXT("impact_send_signal"), TEXT("在发电机与天线就绪后发出求救信号；该行动不消耗行动力。"));
	if (ActionId == TEXT("heat_control_room")) return Text(TEXT("锁定控制室供暖，保护记录并降低备用节电惩罚。"));
	if (ActionId == TEXT("inspect_control_cabinet")) return Text(TEXT("检查控制柜故障状况并记录现场线索。"));
	if (ActionId == TEXT("heat_repair_room")) return TableText(TEXT("impact_heat_repair_room"), TEXT("锁定本阶段维修间供暖，降低精细维修的低温代价。"));
	if (ActionId == TEXT("repair_generator")) return Text(TEXT("推进发电机修复；具体条件随调查、环境与人员状态更新。"));
	if (ActionId == TEXT("forced_self_repair")) return TableText(TEXT("impact_forced_self_repair"), TEXT("无需顾衡配合尝试维修，但会让你承担更高的人身代价。"));
	if (ActionId == TEXT("talk_gu_heng")) return TableText(TEXT("impact_talk_gu_heng"), TEXT("根据当前对话意图获取信息、争取配合或记录承诺。"));
	if (ActionId == TEXT("heat_medical_room")) return TableText(TEXT("impact_heat_medical_room"), TEXT("锁定本阶段医务室供暖，开放完整治疗。"));
	if (ActionId == TEXT("treat_gu_heng") || ActionId == TEXT("treat_character")) return Text(TEXT("根据当前已开放方案处理指定角色的状态。"));
	if (ActionId == TEXT("talk_ye_cheng")) return Text(TEXT("询问人员状态、风险与可执行建议。"));
	if (ActionId == TEXT("distribute_food")) return TableText(TEXT("impact_distribute_food"), TEXT("在三人间分配有限食物，恢复体能并改变压力与信任。"));
	if (ActionId == TEXT("heat_kitchen")) return Text(TEXT("锁定本阶段厨房供暖，允许准备热餐。"));
	if (ActionId == TEXT("rest")) return Text(TEXT("供暖区休整恢复体能；未供暖区等待只降低压力。"));
	if (ActionId == TEXT("dismantle_kitchen_heater")) return Text(TEXT("永久拆除厨房加热器，并失去该处供暖。"));
	if (ActionId == TEXT("calibrate_antenna")) return TableText(TEXT("impact_calibrate_antenna"), TEXT("在室外完成天线校准，为发送信号做准备。"));
	return Text(TEXT("行动结果会立即记录。"));
}

FText FWSPresentationText::ActionExecutor(const FName ActionId)
{
	if (ActionId == TEXT("talk_gu_heng")) return UI(TEXT("executor_gu_dialogue"), TEXT("你与顾衡"));
	if (ActionId == TEXT("talk_ye_cheng")) return UI(TEXT("executor_ye_dialogue"), TEXT("你与叶澄"));
	if (ActionId == TEXT("treat_gu_heng") || ActionId == TEXT("treat_character")) return UI(TEXT("executor_treatment"), TEXT("叶澄主治，可选择协作者"));
	if (ActionId == TEXT("repair_generator")) return UI(TEXT("executor_repair"), TEXT("顾衡主修，可由玩家协助"));
	if (ActionId == TEXT("dismantle_kitchen_heater")) return Text(TEXT("顾衡主拆，可由玩家协助"));
	return UI(TEXT("executor_player"), TEXT("你｜值班负责人"));
}

FText FWSPresentationText::ActionResourceCost(const FName ActionId)
{
	if (ActionId == TEXT("heat_control_room") || ActionId == TEXT("heat_repair_room") || ActionId == TEXT("heat_medical_room") || ActionId == TEXT("heat_kitchen")) return UI(TEXT("resource_fuel_one"), TEXT("燃料 ×1"));
	if (ActionId == TEXT("distribute_food")) return UI(TEXT("resource_food_allocation"), TEXT("食品 1–2 份（按当前分配）"));
	if (ActionId == TEXT("treat_gu_heng") || ActionId == TEXT("treat_character")) return Text(TEXT("按当前选择消耗界面已显示的治疗资源"));
	if (ActionId == TEXT("dismantle_kitchen_heater")) return UI(TEXT("resource_kitchen_heater"), TEXT("厨房加热器完整性（不可逆）"));
	return UI(TEXT("resource_none"), TEXT("无直接物资消耗"));
}

FText FWSPresentationText::ReasonCause(const EWSReasonCode Reason)
{
	switch (Reason)
	{
	case EWSReasonCode::Ok: return TableText(TEXT("reason_ok_cause"), TEXT("条件满足。"));
	case EWSReasonCode::Committed: return TableText(TEXT("reason_committed_cause"), TEXT("行动已经完成。"));
	case EWSReasonCode::UnknownAction: return TableText(TEXT("reason_unknown_action_cause"), TEXT("这个交互点暂时不可用。"));
	case EWSReasonCode::PhaseLocked: return TableText(TEXT("reason_phase_locked_cause"), TEXT("当前阶段不能执行这项行动。"));
	case EWSReasonCode::InsufficientAP: return TableText(TEXT("reason_insufficient_ap_cause"), TEXT("剩余行动力不足。"));
	case EWSReasonCode::AlreadyCompleted: return TableText(TEXT("reason_already_completed_cause"), TEXT("这项工作已经完成。"));
	case EWSReasonCode::UseLimitReached: return TableText(TEXT("reason_use_limit_cause"), TEXT("这项行动的使用次数已经耗尽。"));
	case EWSReasonCode::DuplicateTransaction: return TableText(TEXT("reason_duplicate_cause"), TEXT("这项行动已被系统记录，无需重复提交。"));
	case EWSReasonCode::AlreadyHeated: return TableText(TEXT("reason_already_heated_cause"), TEXT("该区域的供暖已经恢复。"));
	case EWSReasonCode::NeedsFuel: return TableText(TEXT("reason_needs_fuel_cause"), TEXT("当前没有足够燃料供暖。"));
	case EWSReasonCode::InvalidFoodAllocation: return TableText(TEXT("reason_invalid_food_cause"), TEXT("口粮分配超出了现有储备。"));
	case EWSReasonCode::EmptyFoodAllocation: return TableText(TEXT("reason_empty_food_cause"), TEXT("尚未选择要分配的口粮。"));
	case EWSReasonCode::InsufficientFood: return TableText(TEXT("reason_insufficient_food_cause"), TEXT("食品储备不足。"));
	case EWSReasonCode::NeedsMedicalHeat: return TableText(TEXT("reason_needs_medical_heat_cause"), TEXT("医务室过冷，无法安全治疗。"));
	case EWSReasonCode::NeedsDiagnosis: return Text(TEXT("尚未确认目标当前是否需要这项处理。"));
	case EWSReasonCode::HeatPackHidden: return Text(TEXT("当前选择的治疗资源尚未公开或不可用。"));
	case EWSReasonCode::InvalidTreatmentResource: return TableText(TEXT("reason_invalid_treatment_cause"), TEXT("选择的治疗资源当前不可用。"));
	case EWSReasonCode::NeedsMedicine: return TableText(TEXT("reason_needs_medicine_cause"), TEXT("药品已经耗尽。"));
	case EWSReasonCode::NeedsHeatPack: return Text(TEXT("当前选择的治疗资源储备不足。"));
	case EWSReasonCode::NeedsRelayEvidence: return Text(TEXT("现有线索还不足以确认这条维修路线。"));
	case EWSReasonCode::HeaterAlreadyDismantled: return TableText(TEXT("reason_heater_dismantled_cause"), TEXT("厨房加热器已经被拆解。"));
	case EWSReasonCode::GeneratorAlreadyRepaired: return TableText(TEXT("reason_generator_repaired_cause"), TEXT("发电机已经修复。"));
	case EWSReasonCode::GuHengCritical: return TableText(TEXT("reason_gu_critical_cause"), TEXT("顾衡的身体状况太差，无法参与维修。"));
	case EWSReasonCode::NeedsCooperation: return TableText(TEXT("reason_needs_cooperation_cause"), TEXT("顾衡还没有同意配合维修。"));
	case EWSReasonCode::NeedsGeneratorRecords: return TableText(TEXT("reason_needs_records_cause"), TEXT("缺少发电机运行记录，无法判断故障。"));
	case EWSReasonCode::SelfRepairAlreadyUsed: return TableText(TEXT("reason_self_repair_used_cause"), TEXT("你已经承担过一次强行维修，不能再次冒险。"));
	case EWSReasonCode::NeedsGenerator: return TableText(TEXT("reason_needs_generator_cause"), TEXT("发电机尚未修复。"));
	case EWSReasonCode::AntennaAlreadyCalibrated: return TableText(TEXT("reason_antenna_done_cause"), TEXT("天线已经校准。"));
	case EWSReasonCode::PlayerTooCold: return TableText(TEXT("reason_player_cold_cause"), TEXT("你已经严重失温，无法继续室外作业。"));
	case EWSReasonCode::NeedsAntenna: return TableText(TEXT("reason_needs_antenna_cause"), TEXT("天线尚未校准。"));
	case EWSReasonCode::DialogueActUnavailable: return TableText(TEXT("reason_dialogue_act_unavailable_cause"), TEXT("当前交谈对象不接受这种交涉方式。"));
	case EWSReasonCode::InvalidPromiseCondition: return TableText(TEXT("reason_invalid_promise_cause"), TEXT("这项承诺没有可核验的条件。"));
	case EWSReasonCode::DuplicatePromise: return TableText(TEXT("reason_duplicate_promise_cause"), TEXT("同一项承诺已经记录。"));
	case EWSReasonCode::PhaseNotStarted: return Text(TEXT("本阶段尚未选择供暖区。"));
	case EWSReasonCode::HeatingLocked: return Text(TEXT("本阶段的供暖区已经锁定。"));
	case EWSReasonCode::UnknownHeatingZone: return Text(TEXT("无效的供暖区选择。"));
	case EWSReasonCode::WindowClosed: return Text(TEXT("当天的行动窗口已经关闭。"));
	case EWSReasonCode::ExecutorExhausted: return Text(TEXT("执行者体能已经耗尽。"));
	case EWSReasonCode::ExecutorHypothermic: return Text(TEXT("执行者已经失温，无法安全执行。"));
	case EWSReasonCode::RelevantInjuryCritical: return Text(TEXT("相关执行者当前无法安全参与。"));
	case EWSReasonCode::InvalidCollaborator: return Text(TEXT("所选协作者不适合这项行动。"));
	case EWSReasonCode::CollaboratorUnavailable: return Text(TEXT("所选协作者当前无法参与。"));
	case EWSReasonCode::HotMealUnavailable: return Text(TEXT("厨房未供暖或加热器已经拆除，无法准备热餐。"));
	case EWSReasonCode::InvalidMealType: return Text(TEXT("餐食方案无效。"));
	case EWSReasonCode::UnknownTarget: return Text(TEXT("目标角色无效。"));
	case EWSReasonCode::InvalidTreatmentMethod: return Text(TEXT("治疗方案无效。"));
	case EWSReasonCode::TreatmentNotNeeded: return Text(TEXT("该角色当前无需这种治疗。"));
	case EWSReasonCode::NeedsHeatedMedicalRoom: return Text(TEXT("完整治疗需要本阶段为医务室供暖。"));
	case EWSReasonCode::YeChengExhausted: return Text(TEXT("叶澄的体能已经耗尽，无法继续治疗。"));
	case EWSReasonCode::NeedsRelayKnowledge: return Text(TEXT("现有信息不足，无法确定这项操作的作用。"));
	case EWSReasonCode::GuHengRefused: return Text(TEXT("顾衡当前无法或拒绝继续维修。"));
	case EWSReasonCode::NeedsGuHengConditions: return Text(TEXT("当前配合与现场条件还不足以安全推进维修。"));
	case EWSReasonCode::NeedsReplacementRelay: return Text(TEXT("当前选择的维修方案缺少必要部件。"));
	default: return TableText(TEXT("reason_default_cause"), TEXT("当前条件不满足。"));
	}
}

FText FWSPresentationText::ReasonNextStep(const EWSReasonCode Reason)
{
	switch (Reason)
	{
	case EWSReasonCode::InsufficientAP: return TableText(TEXT("reason_insufficient_ap_next"), TEXT("改选低成本行动，或在行动力归零后使用免费的发信窗口。"));
	case EWSReasonCode::NeedsFuel: return TableText(TEXT("reason_needs_fuel_next"), TEXT("保留或寻找燃料，再回到供暖控制器。"));
	case EWSReasonCode::NeedsMedicalHeat: return TableText(TEXT("reason_needs_medical_heat_next"), TEXT("先使用医务室供暖控制器。"));
	case EWSReasonCode::NeedsDiagnosis: return Text(TEXT("先向叶澄询问顾衡当前的身体状态。"));
	case EWSReasonCode::HeatPackHidden: return Text(TEXT("与叶澄确认当前可用的治疗方案。"));
	case EWSReasonCode::NeedsMedicine: return Text(TEXT("选择其他已公开且可用的治疗方案，或保留资源。"));
	case EWSReasonCode::NeedsHeatPack: return Text(TEXT("改用其他已公开的治疗方案。"));
	case EWSReasonCode::NeedsRelayEvidence: return Text(TEXT("继续检查控制柜并核对维修所需规格。"));
	case EWSReasonCode::GuHengCritical: return TableText(TEXT("reason_gu_critical_next"), TEXT("先恢复医务室供暖并治疗顾衡。"));
	case EWSReasonCode::NeedsCooperation: return TableText(TEXT("reason_needs_cooperation_next"), TEXT("与顾衡交谈，询问、安抚或作出可兑现的承诺。"));
	case EWSReasonCode::NeedsGeneratorRecords: return TableText(TEXT("reason_needs_records_next"), TEXT("返回控制室调查发电机运行记录。"));
	case EWSReasonCode::NeedsGenerator: return TableText(TEXT("reason_needs_generator_next"), TEXT("先完成发电机两阶段维修。"));
	case EWSReasonCode::PlayerTooCold: return TableText(TEXT("reason_player_cold_next"), TEXT("留在室内恢复状态，避免继续消耗体温。"));
	case EWSReasonCode::NeedsAntenna: return TableText(TEXT("reason_needs_antenna_next"), TEXT("前往室外天线区完成校准。"));
	case EWSReasonCode::PhaseLocked: return TableText(TEXT("reason_phase_locked_next"), TEXT("关闭当前界面或进入下一阶段后再试。"));
	case EWSReasonCode::DialogueActUnavailable: return TableText(TEXT("reason_dialogue_act_unavailable_next"), TEXT("改用询问、质疑或安抚；承诺只向顾衡提出。"));
	case EWSReasonCode::InvalidPromiseCondition: return TableText(TEXT("reason_invalid_promise_next"), TEXT("从三个可追踪条件中重新选择。"));
	case EWSReasonCode::DuplicatePromise: return TableText(TEXT("reason_duplicate_promise_next"), TEXT("继续执行这项承诺，或选择另一种交涉方式。"));
	case EWSReasonCode::PhaseNotStarted: return Text(TEXT("前往四个供暖控制器之一，按 F 锁定本阶段供暖区。"));
	case EWSReasonCode::HeatingLocked: return Text(TEXT("完成本阶段并按 Enter 结算后，可在下一阶段重新选择。"));
	case EWSReasonCode::ExecutorExhausted: return Text(TEXT("分配食物或在供暖区休整，恢复执行者体能。"));
	case EWSReasonCode::ExecutorHypothermic: return Text(TEXT("先进入供暖区休整，或采用已公开的保暖方案。"));
	case EWSReasonCode::RelevantInjuryCritical: return Text(TEXT("先恢复执行者状态，或改选其他执行者与策略。"));
	case EWSReasonCode::InvalidCollaborator: return Text(TEXT("按 Q 切回单独执行，或选择行动支持的协作者。"));
	case EWSReasonCode::CollaboratorUnavailable: return Text(TEXT("先恢复协作者状态，或按 Q 切回单独执行。"));
	case EWSReasonCode::HotMealUnavailable: return Text(TEXT("下一阶段为厨房供暖并保留加热器，或按 Q 切换为冷口粮。"));
	case EWSReasonCode::TreatmentNotNeeded: return Text(TEXT("按 Q 更换目标或治疗方法。"));
	case EWSReasonCode::NeedsHeatedMedicalRoom: return Text(TEXT("下一阶段选择医务室供暖，或按 Q 改用已开放的处理方式。"));
	case EWSReasonCode::YeChengExhausted: return Text(TEXT("先为叶澄分配食物或让她在供暖区休整。"));
	case EWSReasonCode::NeedsRelayKnowledge: return Text(TEXT("继续检查相关设备，并核对已经取得的线索。"));
	case EWSReasonCode::GuHengRefused: return Text(TEXT("恢复顾衡状态、改善配合，或改用其他已知方案。"));
	case EWSReasonCode::NeedsGuHengConditions: return Text(TEXT("与顾衡确认条件，并改善维修环境或人员状态。"));
	case EWSReasonCode::NeedsReplacementRelay: return Text(TEXT("检查已公开方案所需条件，或切换到已知可行的方案。"));
	default: return TableText(TEXT("reason_default_next"), TEXT("检查目标、资源、证据和队员状态后选择下一步。"));
	}
}

FText FWSPresentationText::EvidenceLabel(const FName EvidenceId)
{
	if (EvidenceId == TEXT("EVIDENCE_DEEP_GENERATOR_LOG"))
	{
		return Text(TEXT("发电机深层日志：08:07 保护系统因输出电压异常自动停机；08:11 有人执行手动旁路重启。日志证明异常重启发生在故障之后。"));
	}
	if (EvidenceId == TEXT("EVIDENCE_BURNT_RELAY"))
	{
		return Text(TEXT("烧毁的主继电器：控制柜触点已经熔毁，发电机无法稳定向天线供电；维修前必须恢复继电器功能。"));
	}
	if (EvidenceId == TEXT("EVIDENCE_ARC_MARKS"))
	{
		return Text(TEXT("控制柜电弧痕迹：柜内存在异常电弧和熔损，可用于判断故障发生顺序。"));
	}
	if (EvidenceId == TEXT("EVIDENCE_BLOODY_BANDAGE"))
	{
		return Text(TEXT("带血绷带：纤维和血迹来自新近处理的手部撕裂伤，说明伤者在事故后接受过临时包扎。"));
	}
	if (EvidenceId == TEXT("EVIDENCE_HAND_OBSERVATION"))
	{
		return Text(TEXT("控制柜边缘的手部异常痕迹：带血指印和绷带纤维表明，事故时有人带着新近包扎操作控制柜。"));
	}
	if (EvidenceId == TEXT("EVIDENCE_MEDICAL_DIAGNOSIS"))
	{
		return Text(TEXT("叶澄的诊断记录：顾衡右手撕裂并伴随失温，继续带伤维修会恶化；医务室升温后再安排处理。"));
	}
	if (EvidenceId == TEXT("EVIDENCE_HEAT_PACK"))
	{
		return Text(TEXT("应急保温包位置：医务柜后方存有一只保温包，可替代药品稳定顾衡一次。"));
	}
	if (EvidenceId == TEXT("EVIDENCE_HEATER_SERVICE_LABEL"))
	{
		return Text(TEXT("厨房加热器维护标牌：内部继电器规格与发电机控制柜一致，可拆作替代件；拆除后厨房会永久失去供暖。"));
	}
	if (EvidenceId == TEXT("generator_log")) return TableText(TEXT("evidence_generator_log"), TEXT("发电机运行记录：电压波动始于继电器损坏之后。"));
	if (EvidenceId == TEXT("relay_burn_pattern")) return TableText(TEXT("evidence_relay_burn_pattern"), TEXT("控制柜烧蚀痕迹：旧继电器已经失效。"));
	if (EvidenceId == TEXT("relay_compatibility")) return TableText(TEXT("evidence_relay_compatibility"), TEXT("替代继电器标牌：规格可以匹配发电机。"));
	if (EvidenceId == TEXT("gu_heng_diagnosis")) return TableText(TEXT("evidence_gu_diagnosis"), TEXT("顾衡诊断：失温与外伤正在影响其工作能力。"));
	if (EvidenceId == TEXT("hidden_heat_pack")) return TableText(TEXT("evidence_heat_pack"), TEXT("隐藏物资：医务柜后方留有一只保温包。"));
	if (EvidenceId == TEXT("antenna_ice")) return TableText(TEXT("evidence_antenna_ice"), TEXT("天线结冰：方位机构需要现场重新校准。"));
	if (EvidenceId == TEXT("records_preserved")) return TableText(TEXT("evidence_records_preserved"), TEXT("维修记录已保存，可追溯故障与责任。"));
	return FText::FromString(FString::Printf(
		TEXT("未配置证据 %s：该记录尚未写入内容说明。"),
		*EvidenceId.ToString()));
}

FText FWSPresentationText::FactLabel(const FName FactId)
{
	if (FactId == TEXT("FACT_GENERATOR_PROTECTION_STOP")) return Text(TEXT("发电机因电压异常触发保护停机"));
	if (FactId == TEXT("FACT_FORCED_RESTART_SUSPICION")) return Text(TEXT("保护停机后发生过手动旁路重启"));
	if (FactId == TEXT("FACT_BURNT_RELAY")) return Text(TEXT("控制柜主继电器已经烧毁"));
	if (FactId == TEXT("FACT_HAND_INJURY")) return Text(TEXT("顾衡的手部异常仍待医疗确认"));
	if (FactId == TEXT("FACT_MEDICAL_DIAGNOSIS")) return Text(TEXT("顾衡需要治疗后再参与稳定维修"));
	if (FactId == TEXT("FACT_HEAT_PACK")) return Text(TEXT("医务室藏有可用于治疗的应急保温包"));
	if (FactId == TEXT("FACT_RELAY_COMPATIBILITY")) return Text(TEXT("厨房加热器继电器可替代发电机损坏件"));
	if (FactId == TEXT("FACT_FORCED_RESTART_CONFIRMED")) return Text(TEXT("日志与电弧痕迹共同确认了强制重启"));
	if (FactId == TEXT("generator_fault")) return TableText(TEXT("fact_generator_fault"), TEXT("发电机故障与继电器损坏有关"));
	if (FactId == TEXT("relay_compatible")) return TableText(TEXT("fact_relay_compatible"), TEXT("厨房加热器中的继电器可以替代"));
	if (FactId == TEXT("gu_heng_condition")) return TableText(TEXT("fact_gu_condition"), TEXT("顾衡需要治疗后才能稳定工作"));
	if (FactId == TEXT("heat_pack_location")) return TableText(TEXT("fact_heat_pack_location"), TEXT("医务室藏有应急保温包"));
	if (FactId == TEXT("antenna_requires_calibration")) return TableText(TEXT("fact_antenna_calibration"), TEXT("天线必须在室外重新校准"));
	if (FactId == TEXT("records_accountability")) return TableText(TEXT("fact_records_accountability"), TEXT("保留维修记录会影响信息责任"));
	return FText::FromString(FString::Printf(
		TEXT("未配置事实：%s"),
		*FactId.ToString()));
}

FText FWSPresentationText::FactDescription(const FName FactId)
{
	if (FactId == TEXT("FACT_GENERATOR_PROTECTION_STOP"))
	{
		return Text(TEXT("深层日志确认保护系统先于人工操作停机；读取记录后可判断故障顺序并保留事故资料。"));
	}
	if (FactId == TEXT("FACT_FORCED_RESTART_SUSPICION"))
	{
		return Text(TEXT("08:11 的手动旁路记录异常，但仍需控制柜痕迹或顾衡说明才能确认责任链。"));
	}
	if (FactId == TEXT("FACT_BURNT_RELAY"))
	{
		return Text(TEXT("现场检查确认主继电器已经失效；替代路线仍需继续查明规格与来源。"));
	}
	if (FactId == TEXT("FACT_HAND_INJURY"))
	{
		return Text(TEXT("现场血迹与绷带纤维说明顾衡右手可能受伤；严重程度仍待叶澄确认。"));
	}
	if (FactId == TEXT("FACT_MEDICAL_DIAGNOSIS"))
	{
		return Text(TEXT("叶澄确认顾衡右手状态会影响精细操作；先恢复医务室供暖，再安排当前可用的治疗。"));
	}
	if (FactId == TEXT("FACT_HEAT_PACK"))
	{
		return Text(TEXT("医务柜后的保温包可替代药品完成一次治疗，适合保留药品或资源不足的路线。"));
	}
	if (FactId == TEXT("FACT_RELAY_COMPATIBILITY"))
	{
		return Text(TEXT("维护标牌确认两处继电器规格一致；拆取替代件会永久牺牲厨房供暖。"));
	}
	if (FactId == TEXT("FACT_FORCED_RESTART_CONFIRMED"))
	{
		return Text(TEXT("发电机日志与控制柜电弧方向形成完整证据链，可确认有人在保护停机后强制重启。"));
	}
	return FText::FromString(FString::Printf(
		TEXT("事实 %s 尚未关联到具体行动，请检查规则配置。"),
		*FactId.ToString()));
}

FText FWSPresentationText::PromiseLabel(const FName ConditionId)
{
	if (ConditionId == TEXT("heat_repair_room")) return TableText(TEXT("promise_heat_repair_room"), TEXT("恢复维修间供暖"));
	if (ConditionId == TEXT("reserve_medicine")) return TableText(TEXT("promise_reserve_medicine"), TEXT("为顾衡保留药品"));
	if (ConditionId == TEXT("keep_records")) return TableText(TEXT("promise_keep_records"), TEXT("保存完整维修记录"));
	return TableText(TEXT("promise_default"), TEXT("未说明的承诺"));
}

FText FWSPresentationText::PhaseLabel(const EWSGamePhase Phase)
{
	switch (Phase)
	{
	case EWSGamePhase::Opening: return TableText(TEXT("phase_opening"), TEXT("事发"));
	case EWSGamePhase::ActionPhase: return TableText(TEXT("phase_action"), TEXT("抢修"));
	case EWSGamePhase::ResolvingAction: return TableText(TEXT("phase_resolving"), TEXT("执行中"));
	case EWSGamePhase::DialogueFeedback: return TableText(TEXT("phase_dialogue"), TEXT("交涉"));
	case EWSGamePhase::MidCrisis: return TableText(TEXT("phase_crisis"), TEXT("备用电池故障"));
	case EWSGamePhase::PostActionWindow: return TableText(TEXT("phase_post_action"), TEXT("最后窗口"));
	case EWSGamePhase::EndingChoice: return TableText(TEXT("phase_ending_choice"), TEXT("最后决定"));
	case EWSGamePhase::Ending: return TableText(TEXT("phase_ending"), TEXT("结局"));
	case EWSGamePhase::Results: return TableText(TEXT("phase_results"), TEXT("复盘"));
	default: return TableText(TEXT("phase_default"), TEXT("抢修"));
	}
}

FText FWSPresentationText::EndingTitle(const EWSEndingType Ending)
{
	if (Ending == EWSEndingType::TaskSuccess) return TableText(TEXT("ending_task_success"), TEXT("信号穿过风雪"));
	if (Ending == EWSEndingType::SurvivalWait) return TableText(TEXT("ending_survival_wait"), TEXT("等待天明"));
	if (Ending == EWSEndingType::CostUncontrolled) return TableText(TEXT("ending_cost_uncontrolled_v06"), TEXT("代价越过边界"));
	return TableText(TEXT("ending_total_collapse"), TEXT("气象站失守"));
}

FText FWSPresentationText::EndingSummary(const EWSEndingType Ending)
{
	if (Ending == EWSEndingType::TaskSuccess) return TableText(TEXT("summary_task_success"), TEXT("求救信号已经发出，人员与储备仍在可控范围内。"));
	if (Ending == EWSEndingType::SurvivalWait) return TableText(TEXT("summary_survival_wait"), TEXT("信号未能发出，你们只能依靠现有储备等待风暴减弱。"));
	if (Ending == EWSEndingType::CostUncontrolled) return TableText(TEXT("summary_cost_uncontrolled_v06"), TEXT("任务仍在推进，但人员、物资或关系已经越过安全边界。"));
	return TableText(TEXT("summary_total_collapse"), TEXT("电力、任务与人员状态同时越过了安全边界。"));
}

FText FWSPresentationText::EndingAdvice(const EWSEndingType Ending)
{
	if (Ending == EWSEndingType::TaskSuccess) return UI(TEXT("advice_task_success"), TEXT("尝试用更少行动力完成同样目标，并保留更多燃料与医疗物资。"));
	if (Ending == EWSEndingType::SurvivalWait) return UI(TEXT("advice_survival_wait"), TEXT("优先确认故障证据，再把发电机和天线串成一条可执行路线。"));
	if (Ending == EWSEndingType::CostUncontrolled) return UI(TEXT("advice_cost_uncontrolled_v06"), TEXT("推进关键任务前先处理失温、伤势与承诺，并给生存留出余量。"));
	return UI(TEXT("advice_total_collapse"), TEXT("先恢复一处关键供暖并取得维修记录，避免在条件不足时连续冒险。"));
}

FText FWSPresentationText::ScoreAttribution(const FName ScoreId)
{
	if (ScoreId == TEXT("task")) return UI(TEXT("score_attr_task"), TEXT("发电机、天线、信号与剩余行动力"));
	if (ScoreId == TEXT("people")) return UI(TEXT("score_attr_people"), TEXT("三人的体温、体能、伤势与压力"));
	if (ScoreId == TEXT("reserves")) return UI(TEXT("score_attr_reserves"), TEXT("燃料、食品、医疗物资与厨房供暖"));
	if (ScoreId == TEXT("social")) return UI(TEXT("score_attr_social"), TEXT("信任变化与承诺兑现情况"));
	return UI(TEXT("score_attr_information"), TEXT("证据核验、记录保存与责任选择"));
}

FText FWSPresentationText::CharacterName(const EWSCharacterId CharacterId)
{
	if (CharacterId == EWSCharacterId::GuHeng) return TableText(TEXT("character_gu_heng"), TEXT("顾衡｜工程师｜41 岁"));
	if (CharacterId == EWSCharacterId::YeCheng) return TableText(TEXT("character_ye_cheng"), TEXT("叶澄｜医生｜31 岁"));
	return TableText(TEXT("character_player"), TEXT("你｜值班负责人"));
}

FText FWSPresentationText::DialogueOpening(const EWSCharacterId CharacterId, const FWSGameState& State)
{
	if (CharacterId == EWSCharacterId::GuHeng)
	{
		if (State.Flags.bGuHengCooperative)
		{
			return Text(TEXT("工具和备件都点过了，听你安排。"));
		}
		if (State.Flags.bGuHengTreated)
		{
			return Text(TEXT("手能用了。维修间有温度，我就上发电机。"));
		}
		if (State.Flags.bGuHengDiagnosed)
		{
			return Text(TEXT("手使不上力，别问太多。要帮忙，先把医务室弄暖。"));
		}
		return Text(TEXT("……说事。发电机房还等着我。"));
	}
	if (CharacterId == EWSCharacterId::YeCheng)
	{
		if (State.Flags.bGuHengTreated)
		{
			return Text(TEXT("他的手在恢复。别再让那只手受冻。"));
		}
		if (State.Flags.bMedicalRoomHeated)
		{
			return FWSKnowledgePolicy::IsGuHengTreatmentOptionVisible(State)
				? Text(TEXT("医务室能用了。顾衡可以开始治疗，其他突发状况也能处理。"))
				: Text(TEXT("医务室能用了。我可以先做检查，也能处理突发状况。"));
		}
		return FWSKnowledgePolicy::IsGuHengTreatmentOptionVisible(State)
			? Text(TEXT("我在。先说顾衡的处理，还是先说设备？"))
			: Text(TEXT("我在。先说人员状态，还是先说设备？"));
	}
	return FText::GetEmpty();
}

FText FWSPresentationText::ConditionLevel(const float Value)
{
	if (Value >= 7.0f) return TableText(TEXT("level_stable"), TEXT("稳定"));
	if (Value >= 4.5f) return TableText(TEXT("level_strained"), TEXT("吃紧"));
	if (Value >= 3.0f) return TableText(TEXT("level_danger"), TEXT("危险"));
	return TableText(TEXT("level_critical"), TEXT("危急"));
}

FText FWSPresentationText::TrustLevel(const float Value)
{
	if (Value >= 7.5f) return TableText(TEXT("trust_trusted"), TEXT("信任"));
	if (Value >= 5.0f) return TableText(TEXT("trust_neutral"), TEXT("中立"));
	if (Value >= 2.5f) return TableText(TEXT("trust_guarded"), TEXT("戒备"));
	return TableText(TEXT("trust_hostile"), TEXT("敌对"));
}

FText FWSPresentationText::KnowledgeLevel(const EWSKnowledgeLevel Level)
{
	if (Level == EWSKnowledgeLevel::Confirmed) return TableText(TEXT("knowledge_confirmed"), TEXT("已确认"));
	if (Level == EWSKnowledgeLevel::Suspected) return TableText(TEXT("knowledge_suspected"), TEXT("有根据的推测"));
	if (Level == EWSKnowledgeLevel::Claimed) return TableText(TEXT("knowledge_claimed"), TEXT("单方说法"));
	return TableText(TEXT("knowledge_unknown"), TEXT("未知"));
}
