#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
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

#endif
