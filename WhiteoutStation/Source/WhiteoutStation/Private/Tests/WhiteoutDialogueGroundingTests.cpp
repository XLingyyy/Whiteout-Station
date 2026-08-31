#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Agents/WSAgentGateway.h"
#include "Agents/WSNPCDecisionService.h"
#include "State/WindStationTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutRequirementQuestionGroundingRegressionTest,
	"WhiteoutStation.Dialogue.Grounding.RequirementQuestionRegression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutRequirementQuestionGroundingRegressionTest::RunTest(
	const FString& Parameters)
{
	FWSGameState State;
	State.RulesSchemaVersion = 4;
	FWSCharacterState GuHeng;
	GuHeng.Stamina = 1;
	GuHeng.InjurySeverity = EWSInjurySeverity::Restricted;
	GuHeng.Trust = 3.5f;
	GuHeng.Pressure = 7.2f;
	State.Characters.Add(EWSCharacterId::GuHeng, GuHeng);
	State.PlayerKnowledge.Add(
		TEXT("FACT_BURNT_RELAY"),
		EWSKnowledgeLevel::Confirmed);

	FWSActionRequest Request;
	Request.ActionId = TEXT("talk_gu_heng");
	Request.DialogueAct = EWSDialogueAct::Ask;
	Request.PlayerSaid = TEXT("要怎么样你才会帮我修理发电机？");

	const FWSAgentReply Reply =
		UWSNPCDecisionService::BuildDeterministicReply(Request, State);
	TestFalse(
		TEXT("Requirements question never falls through to relay-status preset"),
		Reply.Utterance.StartsWith(TEXT("继电器确实烧了")));
	TestTrue(
		TEXT("Requirements question directly states collaboration conditions"),
		Reply.Utterance.Contains(TEXT("搭手"))
			|| Reply.Utterance.Contains(TEXT("协助")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueSemanticFrameMatrixTest,
	"WhiteoutStation.Dialogue.Grounding.SemanticFrameMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueSemanticFrameMatrixTest::RunTest(const FString& Parameters)
{
	const FWSDialogueIntentResult Requirements = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("要怎么样你才会帮我修理发电机？"),
		TEXT("talk_gu_heng"));
	TestTrue(TEXT("Exact repro maps locally"), Requirements.bMapped);
	TestEqual(TEXT("Exact repro query type"), Requirements.QueryType, EWSDialogueQueryType::Requirements);
	TestEqual(TEXT("Exact repro target"), Requirements.TargetActionId, FName(TEXT("repair_generator")));
	TestTrue(TEXT("Exact repro bypasses online classifier"), Requirements.Confidence >= 0.90f);

	const FWSDialogueIntentResult EllipticalRequirements = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("那我要做什么？"),
		TEXT("talk_gu_heng"),
		TEXT("repair_generator"));
	TestEqual(TEXT("Elliptical follow-up keeps topic"), EllipticalRequirements.QueryType, EWSDialogueQueryType::Requirements);
	TestEqual(TEXT("Elliptical follow-up target"), EllipticalRequirements.TargetActionId, FName(TEXT("repair_generator")));

	const FWSDialogueIntentResult Evidence = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("你怎么知道继电器烧了？"),
		TEXT("talk_gu_heng"),
		TEXT("repair_generator"));
	TestEqual(TEXT("Evidence question remains Ask"), Evidence.DialogueAct, EWSDialogueAct::Ask);
	TestEqual(TEXT("Evidence question is not Challenge"), Evidence.QueryType, EWSDialogueQueryType::Evidence);
	const FWSDialogueIntentResult GuHengEvidence = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("你怎么知道顾衡的右手会影响精细维修？"),
		TEXT("talk_ye_cheng"),
		TEXT("repair_generator"));
	TestEqual(TEXT("Named Gu Heng evidence question targets Gu Heng"), GuHengEvidence.TargetCharacter, EWSCharacterId::GuHeng);
	TestTrue(TEXT("Named Gu Heng evidence question clears a stale repair topic"), GuHengEvidence.TargetActionId.IsNone());

	const FWSDialogueIntentResult Alternative = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("还有别的办法吗？"),
		TEXT("talk_gu_heng"),
		TEXT("repair_generator"));
	TestEqual(TEXT("Alternative follow-up query type"), Alternative.QueryType, EWSDialogueQueryType::Alternative);

	const FWSDialogueIntentResult Challenge = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("你在撒谎。"),
		TEXT("talk_gu_heng"));
	TestEqual(TEXT("Explicit accusation remains Challenge"), Challenge.DialogueAct, EWSDialogueAct::Challenge);

	const FWSDialogueIntentResult TopicSwitch = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("叶澄现在怎么样？"),
		TEXT("talk_gu_heng"),
		TEXT("repair_generator"));
	TestEqual(TEXT("Explicit character switches topic"), TopicSwitch.TargetCharacter, EWSCharacterId::YeCheng);
	TestTrue(TEXT("Character topic switch clears generator action"), TopicSwitch.TargetActionId.IsNone());
	TestEqual(TEXT("Character topic switch asks status"), TopicSwitch.QueryType, EWSDialogueQueryType::Status);

	const FWSDialogueIntentResult GuHengStatus = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("顾衡现在怎么样？"),
		TEXT("talk_ye_cheng"));
	TestTrue(TEXT("Gu Heng status maps locally"), GuHengStatus.bMapped);
	TestEqual(TEXT("Gu Heng status targets Gu Heng"), GuHengStatus.TargetCharacter, EWSCharacterId::GuHeng);
	TestTrue(TEXT("Gu Heng status clears action target"), GuHengStatus.TargetActionId.IsNone());
	TestEqual(TEXT("Gu Heng status query type"), GuHengStatus.QueryType, EWSDialogueQueryType::Status);
	const FWSDialogueIntentResult GuHengFineWork = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("顾衡还能不能做精细维修？"),
		TEXT("talk_ye_cheng"));
	TestEqual(TEXT("Fine-work diagnosis wording targets Gu Heng"), GuHengFineWork.TargetCharacter, EWSCharacterId::GuHeng);
	TestEqual(TEXT("Fine-work diagnosis wording maps to Status"), GuHengFineWork.QueryType, EWSDialogueQueryType::Status);
	const FWSDialogueIntentResult GuHengGlove = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("顾衡的手套放在哪里？"),
		TEXT("talk_ye_cheng"));
	TestFalse(
		TEXT("A question about Gu Heng's gloves is not a condition query"),
		GuHengGlove.TargetCharacter == EWSCharacterId::GuHeng
			&& GuHengGlove.QueryType == EWSDialogueQueryType::Status);

	const FWSDialogueIntentResult MedicalAlternative = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("还有别的处理办法吗？"),
		TEXT("talk_ye_cheng"));
	TestTrue(TEXT("Medical alternative maps locally"), MedicalAlternative.bMapped);
	TestEqual(TEXT("Medical alternative remains Ask"), MedicalAlternative.DialogueAct, EWSDialogueAct::Ask);
	TestEqual(TEXT("Medical alternative query type"), MedicalAlternative.QueryType, EWSDialogueQueryType::Alternative);
	TestEqual(TEXT("Medical alternative target"), MedicalAlternative.TargetActionId, FName(TEXT("treat_gu_heng")));
	TestEqual(TEXT("Medical alternative targets Gu Heng"), MedicalAlternative.TargetCharacter, EWSCharacterId::GuHeng);
	const FWSDialogueIntentResult MedicalAlternativeAfterRepairTopic =
		UWSAgentGateway::ClassifyLocalIntent(
			TEXT("还有别的处理办法吗？"),
			TEXT("talk_ye_cheng"),
			TEXT("repair_generator"));
	TestEqual(
		TEXT("Medical wording overrides a stale repair topic"),
		MedicalAlternativeAfterRepairTopic.TargetActionId,
		FName(TEXT("treat_gu_heng")));

	const FWSDialogueIntentResult MedicalSupplies = UWSAgentGateway::ClassifyLocalIntent(
		TEXT("还有什么医疗物资可用？"),
		TEXT("talk_ye_cheng"));
	TestTrue(TEXT("Medical-supply question maps locally"), MedicalSupplies.bMapped);
	TestEqual(TEXT("Medical-supply query type"), MedicalSupplies.QueryType, EWSDialogueQueryType::Alternative);
	TestEqual(TEXT("Medical-supply target"), MedicalSupplies.TargetActionId, FName(TEXT("treat_gu_heng")));
	return true;
}

#endif
