#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "Agents/WSAgentGateway.h"
#include "Agents/WSNPCDecisionService.h"
#include "State/WhiteoutRulesEngine.h"

namespace WhiteoutDialogueV3Tests
{
	FWSGameState MakeDialogueState()
	{
		FWSGameState State;
		State.RulesSchemaVersion = 6;
		FWSCharacterState GuHeng;
		GuHeng.Stamina = 1;
		GuHeng.Trust = 3.5f;
		GuHeng.Pressure = 6.0f;
		GuHeng.InjurySeverity = EWSInjurySeverity::Restricted;
		State.Characters.Add(EWSCharacterId::GuHeng, GuHeng);
		FWSCharacterState YeCheng;
		YeCheng.Trust = 6.0f;
		YeCheng.Pressure = 4.0f;
		State.Characters.Add(EWSCharacterId::YeCheng, YeCheng);
		return State;
	}

	FWSActionRequest MakeGuHengRequirementsRequest()
	{
		FWSActionRequest Request;
		Request.ActionId = TEXT("talk_gu_heng");
		Request.TransactionId = FGuid::NewGuid();
		Request.DialogueSessionId = FGuid::NewGuid();
		Request.DialogueAct = EWSDialogueAct::Ask;
		Request.PlayerSaid = TEXT("要怎么样你才能帮我修发电机？");
		Request.SemanticFrame.SpeechAct = EWSDialogueAct::Ask;
		Request.SemanticFrame.QueryType = EWSDialogueQueryType::Requirements;
		Request.SemanticFrame.TargetActionId = TEXT("repair_generator");
		Request.SemanticFrame.TargetCharacter = EWSCharacterId::GuHeng;
		Request.SemanticFrame.Source = TEXT("automation_test");
		return Request;
	}

	FWSActionRequest MakeDiagnosisRequest()
	{
		FWSActionRequest Request;
		Request.ActionId = TEXT("talk_ye_cheng");
		Request.TransactionId = FGuid::NewGuid();
		Request.DialogueSessionId = FGuid::NewGuid();
		Request.DialogueAct = EWSDialogueAct::Ask;
		Request.PlayerSaid = TEXT("顾衡还能不能做精细维修？");
		Request.SemanticFrame.SpeechAct = EWSDialogueAct::Ask;
		Request.SemanticFrame.QueryType = EWSDialogueQueryType::Status;
		Request.SemanticFrame.TargetFactId = TEXT("FACT_HAND_INJURY");
		Request.SemanticFrame.TargetCharacter = EWSCharacterId::GuHeng;
		Request.SemanticFrame.Source = TEXT("automation_test");
		return Request;
	}

	FWSPreparedDialogue Prepare(
		const FWSActionRequest& Request,
		const FWSGameState& State,
		const FWSActionRequirementReport& RequirementReport = {})
	{
		FWSPreparedDialogue Prepared;
		Prepared.TransactionId = Request.TransactionId;
		Prepared.OriginalRequest = Request;
		Prepared.ReadSnapshot = State;
		Prepared.Contract = UWSNPCDecisionService::BuildDialogueContract(
			Request,
			State,
			RequirementReport,
			Prepared.LocalFallback);
		Prepared.AllowedFactIds = UWSNPCDecisionService::BuildAllowedFacts(
			Request,
			Prepared.LocalFallback.Speaker,
			State);
		Prepared.PlannedDisclosureFacts =
			Prepared.LocalFallback.PlannedDisclosureFacts;
		Prepared.PlannedKnowledgeUpgrades =
			Prepared.LocalFallback.DisclosedFactIds;
		return Prepared;
	}

	FWSDialogueOutcome FallbackOutcome(const FWSPreparedDialogue& Prepared)
	{
		FWSDialogueOutcome Outcome;
		Outcome.FinalReply = Prepared.LocalFallback;
		Outcome.DisclosedFactIds = Prepared.PlannedKnowledgeUpgrades;
		Outcome.RealizedAtomIds = Prepared.LocalFallback.RealizedAtomIds;
		Outcome.AnswerSource = Prepared.LocalFallback.AnswerSource;
		return Outcome;
	}

	bool KnowledgeMatches(
		const TMap<FName, EWSKnowledgeLevel>& Left,
		const TMap<FName, EWSKnowledgeLevel>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (const TPair<FName, EWSKnowledgeLevel>& Pair : Left)
		{
			const EWSKnowledgeLevel* Other = Right.Find(Pair.Key);
			if (!Other || *Other != Pair.Value)
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueV3FullLineValidationTest,
	"WhiteoutStation.Dialogue.V13.FullLineValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueV3FullLineValidationTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueV3Tests;
	const FWSPreparedDialogue Prepared = Prepare(
		MakeGuHengRequirementsRequest(),
		MakeDialogueState());
	FString Reason;
	TestTrue(
		TEXT("The natural fallback passes the same v3 validator"),
		UWSAgentGateway::ValidateDialogueOutcome(
			Prepared,
			FallbackOutcome(Prepared),
			Reason));

	const FString ValidPayload =
		TEXT("{\"npc_line\":\"你留下来搭把手，把维修间弄暖。让我先缓口气，我就动手。\","
			"\"realized_atom_ids\":[\"PLAYER_ASSISTANCE_NEEDED\",\"GU_HENG_NEEDS_RECOVERY\",\"REPAIR_ROOM_SHOULD_BE_WARM\"],"
			"\"disclosed_fact_ids\":[],\"emotion\":\"measured\",\"movement_intent\":\"stay\",\"reaction_action\":\"consider\"}");
	FWSDialogueOutcome Outcome;
	TestTrue(
		TEXT("A valid full-line payload is accepted"),
		UWSAgentGateway::ValidateDialogueOutcomePayload(
			ValidPayload,
			Prepared,
			Outcome,
			Reason));
	TestEqual(
		TEXT("The gateway returns the model's true realized set"),
		Outcome.RealizedAtomIds.Num(),
		3);
	TestFalse(TEXT("The accepted full line is not a fallback"), Outcome.FinalReply.bFallback);

	const TArray<TPair<FString, FString>> InvalidPayloads = {
		{TEXT("missing_atom"),
			TEXT("{\"npc_line\":\"你留下来搭把手，把维修间弄暖。\",\"realized_atom_ids\":[\"PLAYER_ASSISTANCE_NEEDED\",\"REPAIR_ROOM_SHOULD_BE_WARM\"],\"disclosed_fact_ids\":[],\"emotion\":\"measured\",\"movement_intent\":\"stay\",\"reaction_action\":\"consider\"}")},
		{TEXT("forbidden_fact"),
			TEXT("{\"npc_line\":\"你留下来搭把手，把维修间弄暖。让我缓口气，右手受伤也能动手。\",\"realized_atom_ids\":[\"PLAYER_ASSISTANCE_NEEDED\",\"GU_HENG_NEEDS_RECOVERY\",\"REPAIR_ROOM_SHOULD_BE_WARM\"],\"disclosed_fact_ids\":[],\"emotion\":\"measured\",\"movement_intent\":\"stay\",\"reaction_action\":\"consider\"}")},
		{TEXT("added_condition"),
			TEXT("{\"npc_line\":\"你留下来搭把手，把维修间弄暖。让我缓口气，还得修天线。\",\"realized_atom_ids\":[\"PLAYER_ASSISTANCE_NEEDED\",\"GU_HENG_NEEDS_RECOVERY\",\"REPAIR_ROOM_SHOULD_BE_WARM\"],\"disclosed_fact_ids\":[],\"emotion\":\"measured\",\"movement_intent\":\"stay\",\"reaction_action\":\"consider\"}")},
		{TEXT("system_jargon"),
			TEXT("{\"npc_line\":\"你留下来搭把手，把维修间弄暖。让我恢复到至少两点体力。\",\"realized_atom_ids\":[\"PLAYER_ASSISTANCE_NEEDED\",\"GU_HENG_NEEDS_RECOVERY\",\"REPAIR_ROOM_SHOULD_BE_WARM\"],\"disclosed_fact_ids\":[],\"emotion\":\"measured\",\"movement_intent\":\"stay\",\"reaction_action\":\"consider\"}")},
		{TEXT("duplicate_atom"),
			TEXT("{\"npc_line\":\"你留下来搭把手，把维修间弄暖。让我缓口气。\",\"realized_atom_ids\":[\"PLAYER_ASSISTANCE_NEEDED\",\"GU_HENG_NEEDS_RECOVERY\",\"REPAIR_ROOM_SHOULD_BE_WARM\",\"PLAYER_ASSISTANCE_NEEDED\"],\"disclosed_fact_ids\":[],\"emotion\":\"measured\",\"movement_intent\":\"stay\",\"reaction_action\":\"consider\"}")},
		{TEXT("case_changed_atom"),
			TEXT("{\"npc_line\":\"你留下来搭把手，把维修间弄暖。让我缓口气。\",\"realized_atom_ids\":[\"player_assistance_needed\",\"GU_HENG_NEEDS_RECOVERY\",\"REPAIR_ROOM_SHOULD_BE_WARM\"],\"disclosed_fact_ids\":[],\"emotion\":\"measured\",\"movement_intent\":\"stay\",\"reaction_action\":\"consider\"}")},
		{TEXT("invalid_performance"),
			TEXT("{\"npc_line\":\"你留下来搭把手，把维修间弄暖。让我缓口气。\",\"realized_atom_ids\":[\"PLAYER_ASSISTANCE_NEEDED\",\"GU_HENG_NEEDS_RECOVERY\",\"REPAIR_ROOM_SHOULD_BE_WARM\"],\"disclosed_fact_ids\":[],\"emotion\":\"omniscient\",\"movement_intent\":\"teleport\",\"reaction_action\":\"consider\"}")},
		{TEXT("extra_field"),
			TEXT("{\"npc_line\":\"你留下来搭把手，把维修间弄暖。让我缓口气。\",\"realized_atom_ids\":[\"PLAYER_ASSISTANCE_NEEDED\",\"GU_HENG_NEEDS_RECOVERY\",\"REPAIR_ROOM_SHOULD_BE_WARM\"],\"disclosed_fact_ids\":[],\"emotion\":\"measured\",\"movement_intent\":\"stay\",\"reaction_action\":\"consider\",\"debug\":true}")}};
	for (const TPair<FString, FString>& Case : InvalidPayloads)
	{
		FWSDialogueOutcome Rejected;
		TestFalse(
			*FString::Printf(TEXT("%s is rejected"), *Case.Key),
			UWSAgentGateway::ValidateDialogueOutcomePayload(
				Case.Value,
				Prepared,
				Rejected,
				Reason));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueV3ContractDefenseTest,
	"WhiteoutStation.Dialogue.V13.ContractDefense",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueV3ContractDefenseTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueV3Tests;
	const FWSPreparedDialogue Prepared = Prepare(
		MakeDiagnosisRequest(),
		MakeDialogueState());
	TestTrue(
		TEXT("The diagnosis plan freezes both knowledge upgrades"),
		Prepared.PlannedKnowledgeUpgrades.Contains(TEXT("FACT_HAND_INJURY"))
			&& Prepared.PlannedKnowledgeUpgrades.Contains(TEXT("FACT_MEDICAL_DIAGNOSIS")));
	const FString ValidPayload =
		TEXT("{\"npc_line\":\"他的右手已经影响精细操作。先让医务室暖起来，再处理。\","
			"\"realized_atom_ids\":[\"HAND_INJURY_AFFECTS_FINE_WORK\",\"MEDICAL_ROOM_SHOULD_BE_WARM\"],"
			"\"disclosed_fact_ids\":[\"FACT_HAND_INJURY\",\"FACT_MEDICAL_DIAGNOSIS\"],"
			"\"emotion\":\"focused\",\"movement_intent\":\"stay\",\"reaction_action\":\"consider\"}");
	FString Reason;
	FWSDialogueOutcome OnlineOutcome;
	TestTrue(
		TEXT("The exact diagnosis outcome is accepted"),
		UWSAgentGateway::ValidateDialogueOutcomePayload(
			ValidPayload,
			Prepared,
			OnlineOutcome,
			Reason));
	TestTrue(
		TEXT("The diagnosis fallback passes the full validator"),
		UWSAgentGateway::ValidateDialogueOutcome(
			Prepared,
			FallbackOutcome(Prepared),
			Reason));

	FWSActionRequest GeneralYeRequest;
	GeneralYeRequest.ActionId = TEXT("talk_ye_cheng");
	GeneralYeRequest.TransactionId = FGuid::NewGuid();
	GeneralYeRequest.DialogueSessionId = FGuid::NewGuid();
	GeneralYeRequest.DialogueAct = EWSDialogueAct::Ask;
	GeneralYeRequest.PlayerSaid = TEXT("现在是什么情况？");
	GeneralYeRequest.SemanticFrame.SpeechAct = EWSDialogueAct::Ask;
	GeneralYeRequest.SemanticFrame.QueryType = EWSDialogueQueryType::Unknown;
	GeneralYeRequest.SemanticFrame.TargetCharacter = EWSCharacterId::YeCheng;
	const FWSPreparedDialogue GeneralYe = Prepare(
		GeneralYeRequest,
		MakeDialogueState());
	TestTrue(
		TEXT("The general-status fallback validates all three stable atoms"),
		UWSAgentGateway::ValidateDialogueOutcome(
			GeneralYe,
			FallbackOutcome(GeneralYe),
			Reason));

	FWSGameState EvidenceState = MakeDialogueState();
	EvidenceState.PlayerKnowledge.Add(
		TEXT("FACT_FORCED_RESTART_SUSPICION"),
		EWSKnowledgeLevel::Suspected);
	EvidenceState.PlayerKnowledge.Add(
		TEXT("FACT_BURNT_RELAY"),
		EWSKnowledgeLevel::Confirmed);
	FWSActionRequest ChallengeRequest;
	ChallengeRequest.ActionId = TEXT("talk_gu_heng");
	ChallengeRequest.TransactionId = FGuid::NewGuid();
	ChallengeRequest.DialogueSessionId = FGuid::NewGuid();
	ChallengeRequest.DialogueAct = EWSDialogueAct::Challenge;
	ChallengeRequest.SemanticFrame.SpeechAct = EWSDialogueAct::Challenge;
	ChallengeRequest.SemanticFrame.QueryType = EWSDialogueQueryType::Evidence;
	ChallengeRequest.SemanticFrame.TargetActionId = TEXT("repair_generator");
	ChallengeRequest.SemanticFrame.TargetCharacter = EWSCharacterId::GuHeng;
	const FWSPreparedDialogue EvidenceChallenge = Prepare(
		ChallengeRequest,
		EvidenceState);
	TestTrue(
		TEXT("The evidence-backed Gu Heng fallback validates fact-specific atoms"),
		UWSAgentGateway::ValidateDialogueOutcome(
			EvidenceChallenge,
			FallbackOutcome(EvidenceChallenge),
			Reason));

	FWSGameState ReassureState = MakeDialogueState();
	ReassureState.Flags.bGuHengDiagnosed = true;
	ReassureState.PlayerKnowledge.Add(
		TEXT("FACT_HAND_INJURY"),
		EWSKnowledgeLevel::Confirmed);
	FWSActionRequest ReassureRequest;
	ReassureRequest.ActionId = TEXT("talk_gu_heng");
	ReassureRequest.TransactionId = FGuid::NewGuid();
	ReassureRequest.DialogueSessionId = FGuid::NewGuid();
	ReassureRequest.DialogueAct = EWSDialogueAct::Reassure;
	ReassureRequest.SemanticFrame.SpeechAct = EWSDialogueAct::Reassure;
	ReassureRequest.SemanticFrame.QueryType = EWSDialogueQueryType::Status;
	ReassureRequest.SemanticFrame.TargetCharacter = EWSCharacterId::GuHeng;
	const FWSPreparedDialogue Reassure = Prepare(
		ReassureRequest,
		ReassureState);
	TestTrue(
		TEXT("The injury-aware reassurance fallback validates fact and stance atoms"),
		UWSAgentGateway::ValidateDialogueOutcome(
			Reassure,
			FallbackOutcome(Reassure),
			Reason));

	FWSDialogueOutcome Partial = OnlineOutcome;
	Partial.DisclosedFactIds = {TEXT("FACT_HAND_INJURY")};
	Partial.FinalReply.ReferencedFactIds = Partial.DisclosedFactIds;
	Partial.FinalReply.DisclosedFactIds = Partial.DisclosedFactIds;
	TestFalse(
		TEXT("Rules reject an online disclosure subset"),
		FWhiteoutRulesEngine::ValidateDialogueOutcomeContract(
			Prepared,
			Partial,
			Reason));

	FWSPreparedDialogue InvalidMay = Prepared;
	FWSDialogueSemanticAtom MayAtom;
	MayAtom.AtomId = TEXT("OPTIONAL_SECRET");
	MayAtom.RequiredConceptTokens = {TEXT("保温包")};
	MayAtom.RelatedFactIds = {TEXT("FACT_HEAT_PACK")};
	MayAtom.bRequired = false;
	InvalidMay.Contract.MayRealize.Add(MayAtom);
	TestFalse(
		TEXT("Rules reject state-changing facts on may atoms"),
		FWhiteoutRulesEngine::ValidateDialogueOutcomeContract(
			InvalidMay,
			OnlineOutcome,
			Reason));

	FWSPreparedDialogue UnsupportedFact = Prepared;
	for (FWSDialogueSemanticAtom& Atom : UnsupportedFact.Contract.MustRealize)
	{
		Atom.RelatedFactIds.Reset();
	}
	TestFalse(
		TEXT("Every planned fact requires a realized must atom"),
		FWhiteoutRulesEngine::ValidateDialogueOutcomeContract(
			UnsupportedFact,
			OnlineOutcome,
			Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueV3AIStateEquivalenceTest,
	"WhiteoutStation.Dialogue.V13.AIStateEquivalence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueV3AIStateEquivalenceTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueV3Tests;
	FWhiteoutRulesEngine OfflineEngine;
	FWhiteoutRulesEngine OnlineEngine;
	FString Error;
	const FString RulesPath = FPaths::ProjectContentDir()
		/ TEXT("Rules/WhiteoutStationRules.v1.1.json");
	if (!TestTrue(TEXT("Offline rules load"), OfflineEngine.LoadConfig(RulesPath, Error))
		|| !TestTrue(TEXT("Online rules load"), OnlineEngine.LoadConfig(RulesPath, Error)))
	{
		return false;
	}
	EWSReasonCode OfflineReason = EWSReasonCode::UnknownAction;
	EWSReasonCode OnlineReason = EWSReasonCode::UnknownAction;
	TArray<FString> OfflineChanges;
	TArray<FString> OnlineChanges;
	if (!TestTrue(
			TEXT("Offline phase starts"),
			OfflineEngine.BeginDayPhase(
				EWSHeatingZone::ControlRoom,
				OfflineReason,
				OfflineChanges))
		|| !TestTrue(
			TEXT("Online phase starts"),
			OnlineEngine.BeginDayPhase(
				EWSHeatingZone::ControlRoom,
				OnlineReason,
				OnlineChanges)))
	{
		return false;
	}

	FWSActionRequest Request = MakeDiagnosisRequest();
	const FWSPreparedDialogue OfflinePrepared = Prepare(
		Request,
		OfflineEngine.GetState());
	const FWSPreparedDialogue OnlinePrepared = Prepare(
		Request,
		OnlineEngine.GetState());
	const FWSDialogueOutcome OfflineOutcome =
		FallbackOutcome(OfflinePrepared);
	const FString OnlinePayload =
		TEXT("{\"npc_line\":\"他的右手已经影响精细操作。先让医务室暖起来，再处理。\","
			"\"realized_atom_ids\":[\"HAND_INJURY_AFFECTS_FINE_WORK\",\"MEDICAL_ROOM_SHOULD_BE_WARM\"],"
			"\"disclosed_fact_ids\":[\"FACT_HAND_INJURY\",\"FACT_MEDICAL_DIAGNOSIS\"],"
			"\"emotion\":\"focused\",\"movement_intent\":\"stay\",\"reaction_action\":\"consider\"}");
	FWSDialogueOutcome OnlineOutcome;
	FString ValidationReason;
	if (!TestTrue(
			TEXT("Online line validates"),
			UWSAgentGateway::ValidateDialogueOutcomePayload(
				OnlinePayload,
				OnlinePrepared,
				OnlineOutcome,
				ValidationReason)))
	{
		AddError(ValidationReason);
		return false;
	}
	const FWSActionResult OfflineCommit =
		OfflineEngine.CommitDialogueOutcome(OfflinePrepared, OfflineOutcome);
	const FWSActionResult OnlineCommit =
		OnlineEngine.CommitDialogueOutcome(OnlinePrepared, OnlineOutcome);
	TestTrue(TEXT("Offline fallback commits"), OfflineCommit.bCommitted);
	TestTrue(TEXT("Online full line commits"), OnlineCommit.bCommitted);

	const FWSGameState& Offline = OfflineEngine.GetState();
	const FWSGameState& Online = OnlineEngine.GetState();
	TestEqual(TEXT("AI mode preserves AP"), Online.ActionPoints, Offline.ActionPoints);
	TestEqual(TEXT("AI mode preserves phase AP"), Online.PhaseActionPoints, Offline.PhaseActionPoints);
	TestTrue(
		TEXT("AI mode preserves player knowledge"),
		KnowledgeMatches(Online.PlayerKnowledge, Offline.PlayerKnowledge));
	TestEqual(
		TEXT("AI mode preserves diagnosis flag"),
		Online.Flags.bGuHengDiagnosed,
		Offline.Flags.bGuHengDiagnosed);
	TestEqual(
		TEXT("AI mode preserves heat-pack flag"),
		Online.Flags.bHeatPackRevealed,
		Offline.Flags.bHeatPackRevealed);
	TestEqual(
		TEXT("AI mode preserves Ye Cheng trust"),
		Online.Characters.FindChecked(EWSCharacterId::YeCheng).Trust,
		Offline.Characters.FindChecked(EWSCharacterId::YeCheng).Trust);
	TestEqual(
		TEXT("AI mode preserves Ye Cheng pressure"),
		Online.Characters.FindChecked(EWSCharacterId::YeCheng).Pressure,
		Offline.Characters.FindChecked(EWSCharacterId::YeCheng).Pressure);
	return true;
}

#endif
