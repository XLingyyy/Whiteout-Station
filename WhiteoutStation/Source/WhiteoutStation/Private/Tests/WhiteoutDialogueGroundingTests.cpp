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
	return true;
}

#endif
