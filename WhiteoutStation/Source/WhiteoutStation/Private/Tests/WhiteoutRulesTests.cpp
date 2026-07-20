#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Agents/WSAgentGateway.h"
#include "Agents/WSNPCDecisionService.h"
#include "State/WhiteoutRulesEngine.h"

namespace WhiteoutRuleTests
{
	FWSActionRequest MakeRequest(const TCHAR* ActionId)
	{
		FWSActionRequest Request;
		Request.ActionId = FName(ActionId);
		Request.TransactionId = FGuid::NewGuid();
		return Request;
	}

	bool Commit(FAutomationTestBase& Test, FWhiteoutRulesEngine& Engine, FWSActionRequest Request)
	{
		const FWSActionResult Result = Engine.Commit(MoveTemp(Request));
		Test.TestTrue(FString::Printf(TEXT("%s commits"), *Result.ActionId.ToString()), Result.bCommitted);
		if (!Result.bCommitted)
		{
			Test.AddError(FString::Printf(
				TEXT("Action %s rejected with %s"),
				*Result.ActionId.ToString(),
				*StaticEnum<EWSReasonCode>()->GetNameStringByValue(static_cast<int64>(Result.ReasonCode))));
		}
		return Result.bCommitted;
	}

	FWhiteoutRulesEngine LoadedEngine(FAutomationTestBase& Test)
	{
		FWhiteoutRulesEngine Engine;
		FString Error;
		Test.TestTrue(
			TEXT("Rules JSON loads"),
			Engine.LoadConfig(
				FPaths::ProjectContentDir() / TEXT("Rules/WhiteoutStationRules.v0.1.json"), Error));
		if (!Error.IsEmpty())
		{
			Test.AddError(Error);
		}
		return Engine;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutAPFlowTest,
	"WhiteoutStation.Rules.APFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutAPFlowTest::RunTest(const FString& Parameters)
{
	FWhiteoutRulesEngine Engine = WhiteoutRuleTests::LoadedEngine(*this);
	FWSGameState& State = Engine.GetMutableStateForTesting();
	State.ActionPoints = 5;
	State.Tasks.GeneratorProgress = 2;
	const FWSActionResult Calibrate = Engine.Commit(WhiteoutRuleTests::MakeRequest(TEXT("calibrate_antenna")));
	TestTrue(TEXT("2 AP calibration commits"), Calibrate.bCommitted);
	TestEqual(TEXT("5 AP crosses to 3"), Engine.GetState().ActionPoints, 3);
	TestTrue(TEXT("Crisis triggers once"), Calibrate.bCrisisTriggered && Engine.GetState().bMidCrisisTriggered);

	Engine.Reset();
	FWSGameState& ZeroWindow = Engine.GetMutableStateForTesting();
	ZeroWindow.ActionPoints = 2;
	ZeroWindow.Tasks.GeneratorProgress = 2;
	TestTrue(TEXT("Final paid action commits"), Engine.Commit(WhiteoutRuleTests::MakeRequest(TEXT("calibrate_antenna"))).bCommitted);
	TestEqual(TEXT("AP reaches zero"), Engine.GetState().ActionPoints, 0);
	TestTrue(TEXT("0 AP response window remains"), Engine.GetState().Phase == EWSGamePhase::PostActionWindow);
	TestTrue(TEXT("0 AP signal commits"), Engine.Commit(WhiteoutRuleTests::MakeRequest(TEXT("send_signal"))).bCommitted);
	TestTrue(TEXT("Signal is sent"), Engine.GetState().Tasks.bSignalSent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutTransactionTest,
	"WhiteoutStation.Rules.Transactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutTransactionTest::RunTest(const FString& Parameters)
{
	FWhiteoutRulesEngine Engine = WhiteoutRuleTests::LoadedEngine(*this);
	const FWSGameState Before = Engine.GetState();
	const FWSActionResult Invalid = Engine.Commit(WhiteoutRuleTests::MakeRequest(TEXT("calibrate_antenna")));
	TestFalse(TEXT("Invalid action is rejected"), Invalid.bCommitted);
	TestEqual(TEXT("Invalid action does not spend AP"), Engine.GetState().ActionPoints, Before.ActionPoints);
	TestEqual(TEXT("Invalid action does not log"), Engine.GetState().EventLog.Num(), 0);

	FWSActionRequest Request = WhiteoutRuleTests::MakeRequest(TEXT("investigate_generator_log"));
	const FGuid TransactionId = Request.TransactionId;
	TestTrue(TEXT("First transaction commits"), Engine.Commit(Request).bCommitted);
	Request.TransactionId = TransactionId;
	const FWSActionResult Duplicate = Engine.Commit(Request);
	TestFalse(TEXT("Duplicate transaction is ignored"), Duplicate.bCommitted);
	TestTrue(TEXT("Duplicate has explicit reason"), Duplicate.ReasonCode == EWSReasonCode::DuplicateTransaction);
	TestEqual(TEXT("Transaction spends AP once"), Engine.GetState().ActionPoints, 7);
	TestEqual(TEXT("Transaction logs once"), Engine.GetState().EventLog.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutKnowledgeTest,
	"WhiteoutStation.Rules.KnowledgeAndAgentValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutKnowledgeTest::RunTest(const FString& Parameters)
{
	FWhiteoutRulesEngine Engine = WhiteoutRuleTests::LoadedEngine(*this);
	const TArray<FName> GuFacts = Engine.BuildAllowedFactIds(EWSCharacterId::GuHeng);
	const TArray<FName> YeFacts = Engine.BuildAllowedFactIds(EWSCharacterId::YeCheng);
	TestFalse(TEXT("Gu Heng does not know heat pack"), GuFacts.Contains(TEXT("FACT_HEAT_PACK")));
	TestFalse(TEXT("Ye Cheng does not know relay compatibility"), YeFacts.Contains(TEXT("FACT_RELAY_COMPATIBILITY")));

	FString Reason;
	TestFalse(
		TEXT("Unauthorized fact is rejected"),
		FWhiteoutRulesEngine::ValidateAgentResponse(
			TEXT("我知道那个保温包。"),
			{TEXT("FACT_HEAT_PACK")},
			{TEXT("FACT_HAND_INJURY")},
			false,
			Reason));
	TestEqual(TEXT("Fact rejection reason"), Reason, FString(TEXT("fact_permission_violation")));
	TestFalse(
		TEXT("Model rule mutation is rejected"),
		FWhiteoutRulesEngine::ValidateAgentResponse(TEXT("修好了。"), {}, {}, true, Reason));
	TestEqual(TEXT("Rule mutation reason"), Reason, FString(TEXT("model_attempted_rule_change")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutRouteTest,
	"WhiteoutStation.Rules.Routes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutRouteTest::RunTest(const FString& Parameters)
{
	{
		FWhiteoutRulesEngine Engine = WhiteoutRuleTests::LoadedEngine(*this);
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("talk_ye_cheng")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("heat_medical_room")))) return false;
		FWSActionRequest Treat = WhiteoutRuleTests::MakeRequest(TEXT("treat_gu_heng"));
		Treat.TreatmentResource = EWSResourceType::Medicine;
		if (!WhiteoutRuleTests::Commit(*this, Engine, Treat)) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("talk_gu_heng")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("heat_repair_room")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("repair_generator")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("calibrate_antenna")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("send_signal")))) return false;
		Engine.EndGame();
		TestTrue(TEXT("Medical route succeeds"), Engine.GetState().Ending == EWSEndingType::TaskSuccess);
		TestTrue(TEXT("Medical score is in range"), Engine.GetState().Score.Total >= 70.0f && Engine.GetState().Score.Total <= 89.0f);
	}

	{
		FWhiteoutRulesEngine Engine = WhiteoutRuleTests::LoadedEngine(*this);
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("investigate_generator_log")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("inspect_control_cabinet")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("talk_gu_heng")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("dismantle_kitchen_heater")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("heat_repair_room")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("repair_generator")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("calibrate_antenna")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("send_signal")))) return false;
		Engine.EndGame();
		TestTrue(TEXT("Technical route succeeds"), Engine.GetState().Ending == EWSEndingType::TaskSuccess);
		TestTrue(TEXT("Technical score is in range"), Engine.GetState().Score.Total >= 65.0f && Engine.GetState().Score.Total <= 84.0f);
	}

	{
		FWhiteoutRulesEngine Engine = WhiteoutRuleTests::LoadedEngine(*this);
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("heat_repair_room")))) return false;
		FWSActionRequest Food = WhiteoutRuleTests::MakeRequest(TEXT("distribute_food"));
		Food.FoodForPlayer = 1;
		Food.FoodForGuHeng = 1;
		if (!WhiteoutRuleTests::Commit(*this, Engine, Food)) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("repair_generator")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("repair_generator")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("calibrate_antenna")))) return false;
		if (!WhiteoutRuleTests::Commit(*this, Engine, WhiteoutRuleTests::MakeRequest(TEXT("send_signal")))) return false;
		Engine.EndGame();
		TestTrue(TEXT("Quick route succeeds"), Engine.GetState().Ending == EWSEndingType::TaskSuccess);
		TestTrue(TEXT("Quick score is in range"), Engine.GetState().Score.Total >= 55.0f && Engine.GetState().Score.Total <= 79.0f);
		TestEqual(TEXT("Quick route retains two AP"), Engine.GetState().ActionPoints, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueBoundaryTest,
	"WhiteoutStation.Agents.DialogueBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueBoundaryTest::RunTest(const FString& Parameters)
{
	FWhiteoutRulesEngine Engine = WhiteoutRuleTests::LoadedEngine(*this);
	const FWSGameState State = Engine.GetState();
	const FWSAgentReply Decision = UWSNPCDecisionService::BuildDeterministicReply(TEXT("talk_gu_heng"), State);
	const TArray<FName> AllowedFacts = UWSNPCDecisionService::BuildAllowedFacts(TEXT("talk_gu_heng"), Decision.Speaker, State);
	TestTrue(TEXT("Preset fallback is always available"), Decision.bAccepted && Decision.bFallback && !Decision.Utterance.IsEmpty());
	TestFalse(TEXT("Early Gu context excludes Ye's heat pack"), AllowedFacts.Contains(TEXT("FACT_HEAT_PACK")));
	TestFalse(TEXT("Early Gu context excludes restart confession"), AllowedFacts.Contains(TEXT("FACT_FORCED_RESTART_CONFIRMED")));

	FWSAgentReply ModelReply;
	FString Reason;
	const FString ValidPayload = TEXT("{\"utterance\":\"手伤还在，先处理低温。\",\"emotion\":\"guarded\",\"response_type\":\"Deflect\",\"referenced_fact_ids\":[\"FACT_HAND_INJURY\"]}");
	TestTrue(
		TEXT("Schema-valid expression is accepted"),
		UWSAgentGateway::ValidateModelPayload(ValidPayload, Decision, AllowedFacts, ModelReply, Reason));
	TestFalse(TEXT("Accepted model line is not marked fallback"), ModelReply.bFallback);

	const FString MutationPayload = TEXT("{\"utterance\":\"修好了。\",\"emotion\":\"calm\",\"response_type\":\"Deflect\",\"referenced_fact_ids\":[],\"ap_delta\":2}");
	TestFalse(
		TEXT("State mutation field is rejected"),
		UWSAgentGateway::ValidateModelPayload(MutationPayload, Decision, AllowedFacts, ModelReply, Reason));
	TestEqual(TEXT("Mutation rejection is explicit"), Reason, FString(TEXT("model_attempted_rule_change")));

	const FString LeakPayload = TEXT("{\"utterance\":\"保温包在柜底。\",\"emotion\":\"calm\",\"response_type\":\"Deflect\",\"referenced_fact_ids\":[\"FACT_HEAT_PACK\"]}");
	TestFalse(
		TEXT("Unauthorized fact citation is rejected"),
		UWSAgentGateway::ValidateModelPayload(LeakPayload, Decision, AllowedFacts, ModelReply, Reason));
	TestEqual(TEXT("Leak rejection is explicit"), Reason, FString(TEXT("fact_permission_violation")));

	const FString UntaggedLeakPayload = TEXT("{\"utterance\":\"柜底还有保温包。\",\"emotion\":\"calm\",\"response_type\":\"Deflect\",\"referenced_fact_ids\":[]}");
	TestFalse(
		TEXT("Untagged protected claim is rejected"),
		UWSAgentGateway::ValidateModelPayload(UntaggedLeakPayload, Decision, AllowedFacts, ModelReply, Reason));
	TestTrue(TEXT("Semantic leak identifies protected fact"), Reason.StartsWith(TEXT("semantic_fact_permission_violation:FACT_HEAT_PACK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutModelBudgetTest,
	"WhiteoutStation.Agents.ModelBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutModelBudgetTest::RunTest(const FString& Parameters)
{
	FWhiteoutRulesEngine Engine = WhiteoutRuleTests::LoadedEngine(*this);
	for (int32 Index = 0; Index < 10; ++Index)
	{
		TestTrue(FString::Printf(TEXT("Model call %d is admitted"), Index + 1), Engine.TryRecordModelCall());
	}
	TestEqual(TEXT("Budget reaches hard limit"), Engine.GetState().ModelCalls, 10);
	TestFalse(TEXT("Eleventh model call is blocked"), Engine.TryRecordModelCall());
	TestEqual(TEXT("Rejected call does not exceed budget"), Engine.GetState().ModelCalls, 10);
	return true;
}

#endif
