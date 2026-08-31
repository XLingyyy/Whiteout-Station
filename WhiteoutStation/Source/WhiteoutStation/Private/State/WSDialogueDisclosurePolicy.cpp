#include "State/WSDialogueDisclosurePolicy.h"

namespace
{
	const FName TalkYeCheng(TEXT("talk_ye_cheng"));
	const FName RepairGenerator(TEXT("repair_generator"));
	const FName TreatGuHeng(TEXT("treat_gu_heng"));
	const FName TreatCharacter(TEXT("treat_character"));
	const FName FactHandInjury(TEXT("FACT_HAND_INJURY"));
	const FName FactMedicalDiagnosis(TEXT("FACT_MEDICAL_DIAGNOSIS"));
	const FName FactHeatPack(TEXT("FACT_HEAT_PACK"));

	bool IsAsk(const FWSActionRequest& Request)
	{
		return Request.ActionId == TalkYeCheng
			&& Request.DialogueAct == EWSDialogueAct::Ask
			&& Request.SemanticFrame.SpeechAct == EWSDialogueAct::Ask;
	}

	bool ContainsAny(const FString& Text, const TArray<FString>& Terms)
	{
		for (const FString& Term : Terms)
		{
			if (Text.Contains(Term))
			{
				return true;
			}
		}
		return false;
	}
}

bool WSDialogueDisclosurePolicy::IsTargetedGuHengDiagnosisQuestion(
	const FWSActionRequest& Request)
{
	if (!IsAsk(Request))
	{
		return false;
	}

	const FWSDialogueSemanticFrame& Frame = Request.SemanticFrame;
	const bool bValidTarget = Frame.TargetCharacter == EWSCharacterId::GuHeng;
	const bool bValidQuery = Frame.QueryType == EWSDialogueQueryType::Status
		|| Frame.QueryType == EWSDialogueQueryType::Evidence;
	const bool bValidAction = Frame.TargetActionId.IsNone()
		|| Frame.TargetActionId == RepairGenerator;
	const bool bTargetsDiagnosisFact = Frame.TargetFactId == FactHandInjury
		|| Frame.TargetFactId == FactMedicalDiagnosis;
	const bool bExplicitConditionQuestion = ContainsAny(Request.PlayerSaid, {
		TEXT("能不能修"), TEXT("顾衡的手怎么"), TEXT("顾衡受伤"),
		TEXT("顾衡的伤势"), TEXT("顾衡的右手"), TEXT("顾衡身体")});
	const bool bFineWorkAbilityQuestion = ContainsAny(Request.PlayerSaid, {
		TEXT("精细维修"), TEXT("精细操作")})
		&& ContainsAny(Request.PlayerSaid, {
			TEXT("能不能"), TEXT("还能不能"), TEXT("是否能"), TEXT("影响"),
			TEXT("做不了"), TEXT("无法"), TEXT("撑得住"), TEXT("完成不了")});
	const bool bSpecificConditionQuestion = bExplicitConditionQuestion
		|| bFineWorkAbilityQuestion;
	const bool bValidSemanticQuestion = bValidTarget
		&& bValidQuery
		&& bValidAction
		&& bSpecificConditionQuestion
		&& (bTargetsDiagnosisFact || Frame.TargetFactId.IsNone());

	return bValidSemanticQuestion;
}

bool WSDialogueDisclosurePolicy::IsGuHengConditionObservationQuestion(
	const FWSActionRequest& Request)
{
	if (!IsAsk(Request)
		|| IsTargetedGuHengDiagnosisQuestion(Request))
	{
		return false;
	}
	const FWSDialogueSemanticFrame& Frame = Request.SemanticFrame;
	return Frame.TargetCharacter == EWSCharacterId::GuHeng
		&& Frame.QueryType == EWSDialogueQueryType::Status
		&& Frame.TargetActionId.IsNone()
		&& ContainsAny(Request.PlayerSaid, {
			TEXT("顾衡现在怎么样"), TEXT("顾衡怎么样"), TEXT("顾衡的情况"),
			TEXT("顾衡的状态"), TEXT("顾衡还好吗"), TEXT("他现在怎么样"),
			TEXT("他怎么样"), TEXT("他的情况"), TEXT("他的状态"), TEXT("他还好吗")});
}

bool WSDialogueDisclosurePolicy::IsHeatPackDisclosureQuestion(
	const FWSActionRequest& Request)
{
	if (!IsAsk(Request))
	{
		return false;
	}

	const FWSDialogueSemanticFrame& Frame = Request.SemanticFrame;
	const bool bExplicitSupportQuestion = ContainsAny(Request.PlayerSaid, {
		TEXT("保温包"), TEXT("医疗物资")});
	const bool bMedicalContext = ContainsAny(Request.PlayerSaid, {
		TEXT("处理办法"), TEXT("治疗"), TEXT("医疗"), TEXT("药品"),
		TEXT("药物"), TEXT("伤势"), TEXT("受伤"), TEXT("失温"), TEXT("保暖")});
	const bool bMedicalAlternative =
		Frame.QueryType == EWSDialogueQueryType::Alternative
		&& Frame.TargetCharacter == EWSCharacterId::GuHeng
		&& (Frame.TargetActionId == TreatGuHeng
			|| Frame.TargetActionId == TreatCharacter)
		&& bMedicalContext;
	const bool bWorkSupportAlternative =
		Frame.QueryType == EWSDialogueQueryType::Alternative
		&& Frame.TargetCharacter == EWSCharacterId::GuHeng
		&& Frame.TargetActionId == RepairGenerator
		&& ContainsAny(Request.PlayerSaid, {
			TEXT("撑过一次维修"), TEXT("撑过维修"), TEXT("支撑一次维修"),
			TEXT("坚持一次维修"), TEXT("完成一次维修")});
	const bool bExplicitFactQuestion = Frame.TargetFactId == FactHeatPack
		&& (Frame.QueryType == EWSDialogueQueryType::Alternative
			|| Frame.QueryType == EWSDialogueQueryType::Unknown)
		&& (bExplicitSupportQuestion || bMedicalContext);
	return bExplicitSupportQuestion
		|| bMedicalAlternative
		|| bWorkSupportAlternative
		|| bExplicitFactQuestion;
}
