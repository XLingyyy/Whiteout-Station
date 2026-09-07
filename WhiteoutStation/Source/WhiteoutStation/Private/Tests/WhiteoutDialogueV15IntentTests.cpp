#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Dialogue/WSDialogueV15Types.h"
#include "State/WSDialogueDisclosurePolicy.h"
#include "Agents/WSAgentGateway.h"
#include "Presentation/WSStatusPresenter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutV15LocalClarificationRegression,
	"WhiteoutStation.Dialogue.V15.Revision.LocalClarification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWhiteoutV15LocalClarificationRegression::RunTest(const FString& Parameters)
{
	FWSCanonicalIntent Intent;
	Intent.SpeakerId = TEXT("gu_heng");
	Intent.TopicId = TEXT("relationship");
	Intent.Frame.SpeechAct = EWSDialogueAct::Command;
	Intent.Frame.TargetCharacter = EWSCharacterId::GuHeng;
	Intent.Frame.Confidence = 1.0f;
	Intent.bNeedsClarification = true;
	TestTrue(TEXT("Clear social pressure survives missing physical task"), Intent.CanPlan());
	FWSActionRequest Request;
	Intent.ApplyTo(Request);
	TestTrue(TEXT("No physical task is invented"), Request.SemanticFrame.TargetActionId.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutV15CanonicalIntentTest,
	"WhiteoutStation.Dialogue.V15.Intent.ValidationAndDisclosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV15CanonicalIntentTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"({"speaker_id":"ye_cheng","topic_id":"medical","speech_act":"ask","query_type":"status","target_action_id":"repair_generator","target_character":"gu_heng","polarity":"affirmative","commitment":"none","promise_condition":"","confidence":0.98,"needs_clarification":false,"evidence_spans":["手部还能操作工具吗"],"resolved_from_turn":1})");
	FWSCanonicalIntent Intent;
	FString Error;
	TestTrue(TEXT("A medical paraphrase parses with a valid context reference"),
		FWSCanonicalIntent::Parse(Json, TEXT("ye_cheng"), TEXT("那他的手部还能操作工具吗？"), 1, Intent, Error));
	FWSActionRequest Request;
	Request.ActionId = TEXT("talk_ye_cheng");
	Request.PlayerSaid = TEXT("另一种表达，不含旧分类器短语");
	Intent.ApplyTo(Request);
	TestTrue(TEXT("Validated meaning controls diagnosis routing independently of spelling"),
		WSDialogueDisclosurePolicy::IsTargetedGuHengDiagnosisQuestion(Request));
	TestEqual(TEXT("Request and frame speech acts agree"), Request.DialogueAct, Request.SemanticFrame.SpeechAct);
	TestFalse(TEXT("Unknown context turn is rejected"),
		FWSCanonicalIntent::Parse(Json, TEXT("ye_cheng"), TEXT("手部还能操作工具吗"), 0, Intent, Error));
	TestFalse(TEXT("Speaker spoof is rejected"),
		FWSCanonicalIntent::Parse(Json, TEXT("gu_heng"), TEXT("手部还能操作工具吗"), 1, Intent, Error));
	TestFalse(TEXT("Model cannot invent input evidence"),
		FWSCanonicalIntent::Parse(Json, TEXT("ye_cheng"), TEXT("你好"), 1, Intent, Error));
	for (const FString Polarity : {FString(TEXT("negated")), FString(TEXT("quoted")), FString(TEXT("hypothetical"))})
	{
		FString NonAsserted = Json.Replace(TEXT("affirmative"), *Polarity)
			.Replace(TEXT("\"ask\""), TEXT("\"promise\""))
			.Replace(TEXT("\"commitment\":\"none\""), TEXT("\"commitment\":\"proposed\""))
			.Replace(TEXT("\"promise_condition\":\"\""), TEXT("\"promise_condition\":\"heat_repair_room\""));
		TestTrue(TEXT("Non-asserted speech is represented explicitly"),
			FWSCanonicalIntent::Parse(NonAsserted, TEXT("ye_cheng"), TEXT("手部还能操作工具吗"), 1, Intent, Error));
		TestTrue(TEXT("Non-asserted speech cannot register a promise"), Intent.PromiseCondition.IsNone());
		TestEqual(TEXT("Non-asserted promise has no commitment"), Intent.Commitment, EWSCommitmentIntent::None);
	}
	const FString Cancel = Json.Replace(TEXT("affirmative"), TEXT("negated"))
		.Replace(TEXT("\"commitment\":\"none\""), TEXT("\"commitment\":\"reject_pending\""));
	TestTrue(TEXT("Negated cancellation parses"), FWSCanonicalIntent::Parse(Cancel, TEXT("ye_cheng"), TEXT("手部还能操作工具吗"), 1, Intent, Error));
	TestEqual(TEXT("Negation does not erase explicit cancellation"), Intent.Commitment, EWSCommitmentIntent::RejectPending);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutV15ControlledClaimsTest,
	"WhiteoutStation.Dialogue.V15.Expression.RequiredClaims",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWhiteoutV15ControlledClaimsTest::RunTest(const FString& Parameters)
{
	FWSPreparedDialogue Prepared; FWSAgentReply Reply; FString Error;
	Prepared.RequiredClaims.Add(TEXT("diagnosis"), TEXT("手部操作受限。"));
	const FString Envelope = TEXT(R"({"segments":%SEGMENTS%,"referenced_knowledge_ids":[],"proposal_id":"","memory_summary":"","emotion":"neutral","reaction_action":"consider"})");
	TestFalse(TEXT("Missing mandatory claim is rejected"), UWSAgentGateway::ParseControlledRoleplay(
		Envelope.Replace(TEXT("%SEGMENTS%"), TEXT(R"([{"kind":"text","text":"说点别的。"}])")), Prepared, Reply, Error));
	TestEqual(TEXT("Failure identifies missing claim"), Error, FString(TEXT("roleplay_required_claim_missing")));
	TestFalse(TEXT("Unlisted claim cannot introduce a fact"), UWSAgentGateway::ParseControlledRoleplay(
		Envelope.Replace(TEXT("%SEGMENTS%"), TEXT(R"([{"kind":"claim","claim_id":"secret"}])")), Prepared, Reply, Error));
	TestEqual(TEXT("Unknown claim is blocked before text rendering"), Error, FString(TEXT("roleplay_claim_unknown_or_duplicate")));
	TestFalse(TEXT("Critical facts cannot be paraphrased in free text"), UWSAgentGateway::ParseControlledRoleplay(
		Envelope.Replace(TEXT("%SEGMENTS%"), TEXT(R"([{"kind":"text","text":"右手撕裂加失温。"},{"kind":"claim","claim_id":"diagnosis"}])")), Prepared, Reply, Error));
	TestEqual(TEXT("Duplicate free diagnosis is rejected before committing"), Error, FString(TEXT("roleplay_fact_outside_claim")));
	TestFalse(TEXT("Claim cannot repeat"), UWSAgentGateway::ParseControlledRoleplay(
		Envelope.Replace(TEXT("%SEGMENTS%"), TEXT(R"([{"kind":"claim","claim_id":"diagnosis"},{"kind":"claim","claim_id":"diagnosis"}])")), Prepared, Reply, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutV15StatusPermissionsTest,
	"WhiteoutStation.UI.V15.Status.Permissions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWhiteoutV15StatusPermissionsTest::RunTest(const FString& Parameters)
{
	FWSGameState State; FWhiteoutRuleConfig Config;
	FWSCharacterState Gu; Gu.InjurySeverity = EWSInjurySeverity::Critical; Gu.Pressure = 9; Gu.Trust = 2;
	State.Characters.Add(EWSCharacterId::GuHeng, Gu);
	State.Characters.Add(EWSCharacterId::Player, Gu);
	const auto NPC = WSStatusPresenter::Build(EWSCharacterId::GuHeng, State, Config);
	TestEqual(TEXT("NPC contains four public fields"), NPC.Fields.Num(), 4);
	TestEqual(TEXT("Undiagnosed injury is unknown regardless of hidden severity"), NPC.Fields[2].Visibility, EWSStatusVisibility::Unknown);
	TestEqual(TEXT("Hidden injury severity is not rendered"), NPC.Fields[2].DisplayValue, FString(TEXT("尚未确认")));
	for (const auto& Field : NPC.Fields) TestFalse(TEXT("NPC has no exact ratio"), Field.DisplayRatio.IsSet());
	const auto Self = WSStatusPresenter::Build(EWSCharacterId::Player, State, Config);
	TestEqual(TEXT("Player sees own critical injury"), Self.Fields[2].Severity, EWSStatusSeverity::Critical);
	TestEqual(TEXT("Player fourth field is pressure"), Self.Fields[3].Label, FString(TEXT("压力")));
	return true;
}
#endif
