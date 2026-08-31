#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "HAL/IConsoleManager.h"
#include "HUD/WhiteoutHUDWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutHUDConditionDisclosureTest,
	"WhiteoutStation.HUD.V13.ConditionCardDisclosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutHUDConditionDisclosureTest::RunTest(const FString& Parameters)
{
	FWSActionRequirementReport Report;
	Report.ActionId = TEXT("repair_generator");

	FWSRequirementItem KnownRequirement;
	KnownRequirement.RequirementId = TEXT("player_assists");
	KnownRequirement.bSatisfied = false;
	KnownRequirement.MechanicalVisibility = EWSRequirementMechanicalVisibility::Visible;
	KnownRequirement.DisclosureLevel = EWSDisclosureLevel::Explicit;
	KnownRequirement.PlayerFacingDetail = FText::FromString(TEXT("玩家协助维修"));
	Report.UniversalRequirements.Add(KnownRequirement);

	FWSRequirementPlan UnknownPlan;
	UnknownPlan.PlanId = TEXT("private_relay_route");
	FWSRequirementItem HiddenRoute;
	HiddenRoute.RequirementId = TEXT("private_relay_requirement");
	HiddenRoute.bSatisfied = true;
	HiddenRoute.MechanicalVisibility = EWSRequirementMechanicalVisibility::Visible;
	HiddenRoute.DisclosureLevel = EWSDisclosureLevel::Hidden;
	HiddenRoute.PlayerFacingDetail = FText::FromString(
		TEXT("需调查：替代继电器已经可用"));
	UnknownPlan.Requirements.Add(HiddenRoute);
	Report.AlternativePlans.Add(UnknownPlan);

	FWSRequirementItem EvasiveRisk;
	EvasiveRisk.RequirementId = TEXT("private_injury_risk");
	EvasiveRisk.bSatisfied = true;
	EvasiveRisk.MechanicalVisibility = EWSRequirementMechanicalVisibility::Visible;
	EvasiveRisk.DisclosureLevel = EWSDisclosureLevel::Evasive;
	EvasiveRisk.PlayerFacingDetail = FText::FromString(TEXT("顾衡右手受伤"));
	Report.Risks.Add(EvasiveRisk);

	const FString Summary = UWhiteoutHUDWidget::BuildDialogueConditionSummary(Report);
	TestTrue(TEXT("Explicit condition remains visible"), Summary.Contains(TEXT("玩家协助维修")));
	TestTrue(TEXT("Hidden route remains an investigation lead"), Summary.Contains(TEXT("路线 A：需调查")));
	TestTrue(TEXT("Evasive risk remains an investigation lead"), Summary.Contains(TEXT("风险：需调查")));
	TestFalse(TEXT("Hidden route detail never reaches the card"), Summary.Contains(TEXT("替代继电器")));
	TestFalse(TEXT("Hidden injury detail never reaches the card"), Summary.Contains(TEXT("右手")));
	TestFalse(TEXT("A hidden satisfied route is not revealed as satisfied"), Summary.Contains(TEXT("路线 A：当前已满足")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutHUDEvidenceSourceTest,
	"WhiteoutStation.HUD.V13.EvidenceSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutHUDEvidenceSourceTest::RunTest(const FString& Parameters)
{
	FWSGameState State;
	FWSEventRecord GuDialogue;
	GuDialogue.ActionId = TEXT("talk_gu_heng");
	GuDialogue.DialogueSpeaker = EWSCharacterId::GuHeng;
	GuDialogue.DisclosedFactIds.Add(TEXT("FACT_HAND_INJURY"));
	State.EventLog.Add(GuDialogue);
	TestEqual(
		TEXT("Gu Heng disclosure is attributed to his admission"),
		UWhiteoutHUDWidget::BuildKnowledgeSourceLabel(TEXT("FACT_HAND_INJURY"), State),
		FString(TEXT("顾衡承认")));

	FWSEventRecord YeDialogue;
	YeDialogue.ActionId = TEXT("talk_ye_cheng");
	YeDialogue.DialogueSpeaker = EWSCharacterId::YeCheng;
	YeDialogue.DisclosedFactIds.Add(TEXT("FACT_MEDICAL_DIAGNOSIS"));
	State.EventLog.Add(YeDialogue);

	TestEqual(
		TEXT("Ye Cheng disclosure is attributed to her diagnosis"),
		UWhiteoutHUDWidget::BuildKnowledgeSourceLabel(TEXT("FACT_MEDICAL_DIAGNOSIS"), State),
		FString(TEXT("叶澄诊断")));
	TestEqual(
		TEXT("Ye Cheng diagnosis is also the source of the confirmed hand injury"),
		UWhiteoutHUDWidget::BuildKnowledgeSourceLabel(TEXT("FACT_HAND_INJURY"), State),
		FString(TEXT("叶澄诊断")));
	State.Evidence.Add(TEXT("EVIDENCE_BURNT_RELAY"));
	TestEqual(
		TEXT("Physical evidence remains a field observation"),
		UWhiteoutHUDWidget::BuildKnowledgeSourceLabel(TEXT("FACT_BURNT_RELAY"), State),
		FString(TEXT("现场观察")));

	FWSGameState MigratedState;
	MigratedState.Flags.bGuHengDiagnosed = true;
	MigratedState.PlayerKnowledge.Add(
		TEXT("FACT_MEDICAL_DIAGNOSIS"),
		EWSKnowledgeLevel::Confirmed);
	TestEqual(
		TEXT("A migrated diagnosis retains the reliable Ye Cheng source"),
		UWhiteoutHUDWidget::BuildKnowledgeSourceLabel(
			TEXT("FACT_MEDICAL_DIAGNOSIS"),
			MigratedState),
		FString(TEXT("叶澄诊断")));

	FWSGameState UnknownLegacyState;
	UnknownLegacyState.PlayerKnowledge.Add(
		TEXT("FACT_FORCED_RESTART_CONFIRMED"),
		EWSKnowledgeLevel::Confirmed);
	TestEqual(
		TEXT("An ambiguous migrated fact is not falsely called an observation"),
		UWhiteoutHUDWidget::BuildKnowledgeSourceLabel(
			TEXT("FACT_FORCED_RESTART_CONFIRMED"),
			UnknownLegacyState),
		FString(TEXT("来源未记录")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutHUDDialogueDebugTest,
	"WhiteoutStation.HUD.V13.DialogueDebug",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutHUDDialogueDebugTest::RunTest(const FString& Parameters)
{
	FWSAgentReply Reply;
	Reply.Provider = TEXT("preset");
	Reply.AnswerSource = TEXT("local_fallback");
	Reply.ValidationReason = TEXT("invalid_json");

	TestTrue(
		TEXT("Formal dialogue status contains no implementation label"),
		UWhiteoutHUDWidget::BuildDialogueStatusSummary(Reply, false).IsEmpty());
	TestTrue(
		TEXT("Development dialogue status retains diagnostics"),
		UWhiteoutHUDWidget::BuildDialogueStatusSummary(Reply, true).Contains(TEXT("调试")));
	TestNotNull(
		TEXT("v1.3 dialogue debug cvar uses the documented name"),
		IConsoleManager::Get().FindConsoleVariable(TEXT("ws.DialogueDebug")));
	return true;
}

#endif
