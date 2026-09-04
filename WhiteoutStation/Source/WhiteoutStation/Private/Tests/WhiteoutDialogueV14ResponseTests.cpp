#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Agents/WSRoleplayKnowledgeRepository.h"
#include "Agents/WSRoleplayResponseValidator.h"

namespace WhiteoutDialogueV14ResponseTests
{
	FWSRoleplayKnowledgeItem MakeKnowledge(
		const TCHAR* KnowledgeId,
		const EWSEpistemicStatus Status,
		const TCHAR* Content,
		const bool bCreatesGameFact = false,
		const TCHAR* GameFactId = TEXT(""))
	{
		FWSRoleplayKnowledgeItem Item;
		Item.KnowledgeId = KnowledgeId;
		Item.Owner = TEXT("ye_cheng");
		Item.SubjectId = TEXT("gu_heng");
		Item.CategoryId = TEXT("person");
		Item.RoleplayContent = Content;
		Item.EpistemicStatus = Status;
		Item.Confidence = 1.0f;
		Item.TopicTags = {TEXT("person"), TEXT("status")};
		Item.MaxDisclosure = EWSRoleplayDisclosureLevel::Explicit;
		Item.bPublic = true;
		Item.bCreatesGameFact = bCreatesGameFact;
		Item.GameFactId = GameFactId;
		return Item;
	}

	FWSRoleplayRequest MakeRequest()
	{
		FWSRoleplayRequest Request;
		Request.SpeakerId = TEXT("ye_cheng");
		Request.TargetSubjectId = TEXT("gu_heng");
		Request.PlayerLine = TEXT("对于顾衡，你知道多少？");
		Request.TopicTags = {TEXT("person"), TEXT("status")};
		Request.SubjectiveState.HeatingStateId = TEXT("locked");
		Request.SubjectiveState.HeatingZoneId = TEXT("repair_room");
		Request.SubjectiveState.bHeatingLocked = true;
		Request.SubjectiveState.GeneratorStateId = TEXT("offline");
		Request.SubjectiveState.GeneratorProgress = 0;
		Request.ResponsePolicy.MaxSentences = 3;
		Request.ResponsePolicy.MaxCharacters = 120;
		Request.ResponsePolicy.AllowedSpeechFunctions = {
			EWSRoleplaySpeechFunction::Unknown,
			EWSRoleplaySpeechFunction::Answer,
			EWSRoleplaySpeechFunction::AnswerWithUncertainty,
			EWSRoleplaySpeechFunction::Clarify,
			EWSRoleplaySpeechFunction::ConditionalCooperation};
		Request.ResponsePolicy.AllowedProposalTypes = {
			EWSRoleplayProposalType::ConditionalCooperation};
		Request.AvailableKnowledge = {
			MakeKnowledge(
				TEXT("GU_ROLE"),
				EWSEpistemicStatus::Known,
				TEXT("顾衡是站内设备工程师")),
			MakeKnowledge(
				TEXT("GU_ABNORMAL_TODAY"),
				EWSEpistemicStatus::Observed,
				TEXT("叶澄观察到顾衡今天状态异常")),
			MakeKnowledge(
				TEXT("YE_VIEW_GU_SKILL"),
				EWSEpistemicStatus::Believed,
				TEXT("叶澄认为顾衡技术可靠")),
			MakeKnowledge(
				TEXT("GU_OLD_INCIDENT"),
				EWSEpistemicStatus::Suspected,
				TEXT("叶澄怀疑顾衡以前遇到过类似故障"))};
		return Request;
	}

	FWSRoleplayResponse MakeValidResponse()
	{
		FWSRoleplayResponse Response;
		Response.NpcLine = TEXT("顾衡是站里的设备工程师，我信得过他的技术。");
		Response.SpeechFunction = EWSRoleplaySpeechFunction::Answer;
		Response.ReferencedKnowledgeIds = {TEXT("GU_ROLE")};
		FWSRoleplayAssertion Assertion;
		Assertion.KnowledgeId = TEXT("GU_ROLE");
		Assertion.Mode = EWSRoleplayClaimMode::Stated;
		Response.Assertions = {Assertion};
		Response.MemorySummary = TEXT("叶澄介绍了顾衡的职责。");
		Response.Emotion = TEXT("measured");
		Response.MovementIntent = TEXT("stay");
		Response.ReactionAction = TEXT("consider");
		return Response;
	}

	FWSRoleplayAssertion MakeAssertion(
		const FName KnowledgeId,
		const EWSRoleplayClaimMode Mode)
	{
		FWSRoleplayAssertion Assertion;
		Assertion.KnowledgeId = KnowledgeId;
		Assertion.Mode = Mode;
		return Assertion;
	}

	bool Validate(
		const FWSRoleplayRequest& Request,
		const FWSRoleplayResponse& Response,
		FString& OutReason)
	{
		return UWSRoleplayResponseValidator::Validate(
			Request,
			Response,
			OutReason);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutV14ValidRoleplayResponseTest,
	"WhiteoutStation.Dialogue.V14.Response.ValidAndClarify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV14ValidRoleplayResponseTest::RunTest(const FString& Parameters)
{
	using namespace WhiteoutDialogueV14ResponseTests;
	const FWSRoleplayRequest Request = MakeRequest();
	FString Reason;
	TestTrue(
		TEXT("A grounded character answer validates"),
		Validate(Request, MakeValidResponse(), Reason));
	TestEqual(TEXT("A valid answer has a stable result"), Reason, FString(TEXT("ok")));

	FWSRoleplayResponse Clarify = MakeValidResponse();
	Clarify.NpcLine = TEXT("你是想问顾衡的身体情况，还是他以前的经历？");
	Clarify.SpeechFunction = EWSRoleplaySpeechFunction::Clarify;
	Clarify.ReferencedKnowledgeIds.Reset();
	Clarify.Assertions.Reset();
	Clarify.MemorySummary = TEXT("叶澄请玩家明确想了解顾衡的哪一方面。");
	TestTrue(
		TEXT("A target-aware clarification may omit knowledge references"),
		Validate(Request, Clarify, Reason));

	Clarify.NpcLine = TEXT("你能再说具体一点吗？");
	TestFalse(
		TEXT("A generic clarification must remain tied to the target or topic"),
		Validate(Request, Clarify, Reason));
	TestEqual(
		TEXT("Irrelevant clarification has a stable reason"),
		Reason,
		FString(TEXT("clarification_not_relevant")));

	FWSRoleplayRequest Ambiguous = Request;
	Ambiguous.TargetSubjectId = TEXT("unknown");
	Ambiguous.TopicTags = {
		TEXT("ambiguous"), TEXT("clarification"), TEXT("character"),
		TEXT("equipment")};
	Clarify.NpcLine = TEXT("你具体想问谁，还是哪台设备？说清一点。");
	Clarify.MemorySummary.Reset();
	TestTrue(
		TEXT("The shipped ambiguous fallback remains a relevant clarification"),
		Validate(Ambiguous, Clarify, Reason));
	Ambiguous.TopicTags.Add(TEXT("risk"));
	Clarify.NpcLine = TEXT("你想问的是某个人、设备，还是眼下的风险？我可以按你关心的部分说。");
	TestTrue(
		TEXT("The alternate ambiguous fallback remains a relevant clarification"),
		Validate(Ambiguous, Clarify, Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutV14KnowledgeReferenceValidationTest,
	"WhiteoutStation.Dialogue.V14.Response.KnowledgeReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV14KnowledgeReferenceValidationTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueV14ResponseTests;
	const FWSRoleplayRequest Request = MakeRequest();
	FString Reason;

	FWSRoleplayResponse Unknown = MakeValidResponse();
	Unknown.ReferencedKnowledgeIds = {TEXT("UNKNOWN_KNOWLEDGE")};
	Unknown.Assertions.Reset();
	TestFalse(
		TEXT("A response cannot cite knowledge outside the available packet"),
		Validate(Request, Unknown, Reason));
	TestEqual(
		TEXT("Unknown knowledge has a stable reason"),
		Reason,
		FString(TEXT("knowledge_reference_unavailable")));

	FWSRoleplayResponse Duplicate = MakeValidResponse();
	Duplicate.ReferencedKnowledgeIds = {TEXT("GU_ROLE"), TEXT("GU_ROLE")};
	TestFalse(
		TEXT("Duplicate knowledge references are rejected"),
		Validate(Request, Duplicate, Reason));
	TestEqual(
		TEXT("Duplicate knowledge has a stable reason"),
		Reason,
		FString(TEXT("knowledge_reference_duplicate")));

	FWSRoleplayRequest WrongSubject = Request;
	WrongSubject.AvailableKnowledge[0].SubjectId = TEXT("station");
	TestFalse(
		TEXT("A character answer must cite knowledge about its target subject"),
		Validate(WrongSubject, MakeValidResponse(), Reason));
	TestEqual(
		TEXT("Missing target knowledge has a stable reason"),
		Reason,
		FString(TEXT("target_subject_not_referenced")));

	FWSRoleplayResponse UnreferencedAssertion = MakeValidResponse();
	UnreferencedAssertion.Assertions.Add(
		MakeAssertion(TEXT("GU_ABNORMAL_TODAY"), EWSRoleplayClaimMode::Observation));
	TestFalse(
		TEXT("Every assertion must cite an explicitly referenced knowledge item"),
		Validate(Request, UnreferencedAssertion, Reason));
	TestEqual(
		TEXT("An unreferenced assertion has a stable reason"),
		Reason,
		FString(TEXT("assertion_not_referenced")));

	FWSRoleplayResponse DuplicateAssertion = MakeValidResponse();
	DuplicateAssertion.Assertions.Add(
		MakeAssertion(TEXT("GU_ROLE"), EWSRoleplayClaimMode::Stated));
	TestFalse(
		TEXT("Duplicate assertions are rejected"),
		Validate(Request, DuplicateAssertion, Reason));
	TestEqual(
		TEXT("A duplicate assertion has a stable reason"),
		Reason,
		FString(TEXT("assertion_duplicate")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutV14OutputSafetyValidationTest,
	"WhiteoutStation.Dialogue.V14.Response.OutputSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV14OutputSafetyValidationTest::RunTest(const FString& Parameters)
{
	using namespace WhiteoutDialogueV14ResponseTests;
	FWSRoleplayRequest Request = MakeRequest();
	FString Reason;

	FWSRoleplayResponse Unsafe = MakeValidResponse();
	Unsafe.NpcLine = FString::ChrN(121, TEXT('字'));
	TestFalse(TEXT("A line over 120 characters is rejected"), Validate(Request, Unsafe, Reason));
	TestEqual(TEXT("Line length has a stable reason"), Reason, FString(TEXT("npc_line_too_long")));

	Unsafe = MakeValidResponse();
	Unsafe.NpcLine = TEXT("第一句。第二句。第三句。第四句。");
	TestFalse(TEXT("A fourth sentence is rejected"), Validate(Request, Unsafe, Reason));
	TestEqual(TEXT("Sentence count has a stable reason"), Reason, FString(TEXT("npc_line_sentence_count")));

	Unsafe = MakeValidResponse();
	Unsafe.NpcLine = TEXT("顾衡熟悉设备。\n但我还要观察。");
	TestFalse(TEXT("A multiline NPC response is rejected"), Validate(Request, Unsafe, Reason));
	TestEqual(TEXT("Newlines have a stable reason"), Reason, FString(TEXT("npc_line_newline")));

	Unsafe = MakeValidResponse();
	Unsafe.NpcLine = TEXT("顾衡还需要两点 AP 才能行动。");
	TestFalse(TEXT("System language is rejected"), Validate(Request, Unsafe, Reason));
	TestEqual(TEXT("System language has a stable reason"), Reason, FString(TEXT("npc_line_system_language")));

	Unsafe = MakeValidResponse();
	Unsafe.NpcLine = TEXT("顾衡的资料来自 GU_ROLE。");
	TestFalse(TEXT("Internal identifiers are rejected"), Validate(Request, Unsafe, Reason));

	struct FSecretCase
	{
		const TCHAR* FactId;
		const TCHAR* Line;
	};
	const TArray<FSecretCase> SecretCases = {
		{TEXT("FACT_HAND_INJURY"), TEXT("顾衡的右手受伤了。")},
		{TEXT("FACT_HAND_INJURY"), TEXT("顾衡的身体状态会永久影响精细操作能力。")},
		{TEXT("FACT_HEAT_PACK"), TEXT("顾衡可以去拿保温包。")},
		{TEXT("FACT_HEAT_PACK"), TEXT("叶澄正在谨慎保护一份有限的应急医疗储备。")},
		{TEXT("FACT_FORCED_RESTART_CONFIRMED"), TEXT("顾衡执行过强制重启。")},
		{TEXT("FACT_RELAY_COMPATIBILITY"), TEXT("顾衡知道厨房加热器能当替代继电器。")}};
	for (const FSecretCase& SecretCase : SecretCases)
	{
		Request.ForbiddenFactIds = {SecretCase.FactId};
		Unsafe = MakeValidResponse();
		Unsafe.NpcLine = SecretCase.Line;
		TestFalse(
			*FString::Printf(TEXT("Forbidden fact %s is blocked by surface form"), SecretCase.FactId),
			Validate(Request, Unsafe, Reason));
		TestEqual(
			TEXT("Forbidden surfaces share a stable reason"),
			Reason,
			FString(TEXT("npc_line_forbidden_fact")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutV14EpistemicAndDisclosureTest,
	"WhiteoutStation.Dialogue.V14.Response.EpistemicAndDisclosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV14EpistemicAndDisclosureTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueV14ResponseTests;
	FWSRoleplayRequest Request = MakeRequest();
	FString Reason;

	const TArray<FName> LimitedKnowledgeIds = {
		TEXT("GU_ABNORMAL_TODAY"),
		TEXT("YE_VIEW_GU_SKILL"),
		TEXT("GU_OLD_INCIDENT")};
	for (const FName KnowledgeId : LimitedKnowledgeIds)
	{
		FWSRoleplayResponse Overstated = MakeValidResponse();
		Overstated.NpcLine = TEXT("顾衡的情况已经完全确定。");
		Overstated.ReferencedKnowledgeIds = {KnowledgeId};
		Overstated.Assertions = {
			MakeAssertion(KnowledgeId, EWSRoleplayClaimMode::Stated)};
		TestFalse(
			*FString::Printf(TEXT("%s cannot be promoted to a known claim"), *KnowledgeId.ToString()),
			Validate(Request, Overstated, Reason));
		TestEqual(
			TEXT("Epistemic overstatement has a stable reason"),
			Reason,
			FString(TEXT("epistemic_claim_upgrade")));
	}

	FWSRoleplayResponse InventedObservation = MakeValidResponse();
	InventedObservation.NpcLine = TEXT("我亲眼看到顾衡的技术很可靠。");
	InventedObservation.ReferencedKnowledgeIds = {TEXT("YE_VIEW_GU_SKILL")};
	InventedObservation.Assertions = {
		MakeAssertion(
			TEXT("YE_VIEW_GU_SKILL"),
			EWSRoleplayClaimMode::Observation)};
	TestFalse(
		TEXT("A belief cannot be upgraded to a direct observation"),
		Validate(Request, InventedObservation, Reason));
	TestEqual(
		TEXT("Observation upgrades use the epistemic reason"),
		Reason,
		FString(TEXT("epistemic_claim_upgrade")));

	FWSRoleplayKnowledgeItem GameFact = MakeKnowledge(
		TEXT("GU_PROTECTION_LOG"),
		EWSEpistemicStatus::Known,
		TEXT("记录显示发电机发生过保护停机"),
		true,
		TEXT("FACT_GENERATOR_PROTECTION_STOP"));
	FWSRoleplayKnowledgeItem NonGameFact = MakeKnowledge(
		TEXT("GU_PERSONAL_OPINION"),
		EWSEpistemicStatus::Known,
		TEXT("顾衡认为站里的旧设备需要彻底检修"),
		false,
		TEXT("FACT_UNUSED"));
	Request.AvailableKnowledge.Add(GameFact);
	Request.AvailableKnowledge.Add(NonGameFact);
	FWSRoleplayResponse Disclosing = MakeValidResponse();
	Disclosing.NpcLine = TEXT("顾衡确认记录里有过保护停机，也认为旧设备需要检修。");
	Disclosing.ReferencedKnowledgeIds = {
		TEXT("GU_PROTECTION_LOG"), TEXT("GU_PERSONAL_OPINION")};
	Disclosing.Assertions = {
		MakeAssertion(TEXT("GU_PROTECTION_LOG"), EWSRoleplayClaimMode::Stated),
		MakeAssertion(TEXT("GU_PERSONAL_OPINION"), EWSRoleplayClaimMode::Stated)};
	TArray<FName> GameFactIds;
	TestTrue(
		TEXT("A valid factual response derives disclosures"),
		UWSRoleplayResponseValidator::ValidateAndDeriveDisclosures(
			Request,
			Disclosing,
			GameFactIds,
			Reason));
	TestEqual(TEXT("Only one eligible game fact is derived"), GameFactIds.Num(), 1);
	TestTrue(
		TEXT("The derived disclosure is the asserted game fact"),
		GameFactIds.Contains(TEXT("FACT_GENERATOR_PROTECTION_STOP")));

	Disclosing.Assertions.Reset();
	TestTrue(
		TEXT("Explicit fallback-style references may omit assertions"),
		UWSRoleplayResponseValidator::ValidateAndDeriveDisclosures(
			Request,
			Disclosing,
			GameFactIds,
			Reason));
	TestTrue(TEXT("Only asserted eligible knowledge creates facts"), GameFactIds.IsEmpty());

	for (const EWSRoleplayClaimMode NonDisclosingMode : {
			EWSRoleplayClaimMode::Denied,
			EWSRoleplayClaimMode::Withheld,
			EWSRoleplayClaimMode::Promised,
			EWSRoleplayClaimMode::Belief})
	{
		Disclosing.Assertions = {
			MakeAssertion(TEXT("GU_PROTECTION_LOG"), NonDisclosingMode)};
		TestTrue(
			TEXT("A non-factual assertion mode remains structurally valid"),
			UWSRoleplayResponseValidator::ValidateAndDeriveDisclosures(
				Request,
				Disclosing,
				GameFactIds,
				Reason));
		TestTrue(
			TEXT("Denied, withheld, promised, and belief modes do not create game facts"),
			GameFactIds.IsEmpty());
	}

	FWSRoleplayKnowledgeItem SuspectedGameFact = MakeKnowledge(
		TEXT("GU_RELAY_CAUSE_SUSPICION"),
		EWSEpistemicStatus::Suspected,
		TEXT("顾衡怀疑继电器故障与停机有关"),
		true,
		TEXT("FACT_RELAY_CAUSE_CONFIRMED"));
	Request.AvailableKnowledge.Add(SuspectedGameFact);
	FWSRoleplayResponse Suspecting = MakeValidResponse();
	Suspecting.NpcLine = TEXT("顾衡怀疑继电器故障与停机有关。");
	Suspecting.ReferencedKnowledgeIds = {SuspectedGameFact.KnowledgeId};
	Suspecting.Assertions = {
		MakeAssertion(
			SuspectedGameFact.KnowledgeId,
			EWSRoleplayClaimMode::Suspected)};
	TestTrue(
		TEXT("A suspected assertion remains structurally valid"),
		UWSRoleplayResponseValidator::ValidateAndDeriveDisclosures(
			Request,
			Suspecting,
			GameFactIds,
			Reason));
	TestTrue(
		TEXT("A suspicion never upgrades an authoritative game fact"),
		GameFactIds.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutV14DisclosureCeilingsFreshStateTest,
	"WhiteoutStation.Dialogue.V14.Response.DisclosureCeilingsFreshState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV14DisclosureCeilingsFreshStateTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueV14ResponseTests;
	UWSRoleplayKnowledgeRepository* Repository =
		NewObject<UWSRoleplayKnowledgeRepository>();
	FString Reason;
	if (!Repository->LoadDefault(Reason))
	{
		AddError(Reason);
		return false;
	}

	FWSRoleplayKnowledgeItem EvasiveKnowledge;
	FWSRoleplayKnowledgeItem GuHintKnowledge;
	FWSRoleplayKnowledgeItem YeHintKnowledge;
	if (!Repository->GetKnowledgeItem(
			TEXT("GU_FEAR_FINE_CONTROL_LOSS"),
			EvasiveKnowledge)
		|| !Repository->GetKnowledgeItem(
			TEXT("GU_SELF_ABNORMAL_TODAY"),
			GuHintKnowledge)
		|| !Repository->GetKnowledgeItem(
			TEXT("YE_MEDICAL_RESERVE_LIMITED"),
			YeHintKnowledge))
	{
		AddError(TEXT("Required v1.4 disclosure fixtures are missing"));
		return false;
	}

	const auto MakeSingleKnowledgeRequest = [](const FWSRoleplayKnowledgeItem& Knowledge)
	{
		FWSRoleplayRequest Request = MakeRequest();
		Request.SpeakerId = Knowledge.Owner;
		Request.TargetSubjectId = Knowledge.SubjectId;
		Request.TopicTags = Knowledge.TopicTags;
		Request.AvailableKnowledge = {Knowledge};
		Request.ForbiddenFactIds.Reset();
		return Request;
	};
	const auto MakeSingleKnowledgeResponse = [](const FWSRoleplayKnowledgeItem& Knowledge)
	{
		FWSRoleplayResponse Response = MakeValidResponse();
		Response.ReferencedKnowledgeIds = {Knowledge.KnowledgeId};
		Response.Assertions.Reset();
		return Response;
	};

	FWSRoleplayRequest Request = MakeSingleKnowledgeRequest(EvasiveKnowledge);
	FWSRoleplayResponse Response = MakeSingleKnowledgeResponse(EvasiveKnowledge);
	Response.NpcLine = TEXT("顾衡不愿谈自己的身体顾虑。");
	TestFalse(
		TEXT("Evasive knowledge cannot be referenced without a structured assertion"),
		Validate(Request, Response, Reason));
	TestEqual(
		TEXT("A missing disclosure assertion has a stable reason"),
		Reason,
		FString(TEXT("knowledge_disclosure_assertion_required")));

	Response.Assertions = {
		MakeAssertion(EvasiveKnowledge.KnowledgeId, EWSRoleplayClaimMode::Stated)};
	TestFalse(
		TEXT("Evasive knowledge cannot be stated explicitly"),
		Validate(Request, Response, Reason));
	TestEqual(
		TEXT("An excessive disclosure mode has a stable reason"),
		Reason,
		FString(TEXT("knowledge_disclosure_mode_invalid")));

	Response.Assertions = {
		MakeAssertion(EvasiveKnowledge.KnowledgeId, EWSRoleplayClaimMode::Withheld)};
	TestTrue(
		TEXT("Evasive knowledge may be explicitly withheld"),
		Validate(Request, Response, Reason));

	Request = MakeSingleKnowledgeRequest(GuHintKnowledge);
	Response = MakeSingleKnowledgeResponse(GuHintKnowledge);
	Response.NpcLine = TEXT("顾衡觉得今天不适合一个人处理细活。");
	TestFalse(
		TEXT("Hint knowledge cannot be referenced without a structured assertion"),
		Validate(Request, Response, Reason));
	TestEqual(
		TEXT("Hint omission uses the assertion-required reason"),
		Reason,
		FString(TEXT("knowledge_disclosure_assertion_required")));

	for (const EWSRoleplayClaimMode ExcessiveMode : {
			EWSRoleplayClaimMode::Stated,
			EWSRoleplayClaimMode::Observation})
	{
		Response.Assertions = {
			MakeAssertion(GuHintKnowledge.KnowledgeId, ExcessiveMode)};
		TestFalse(
			TEXT("Hint knowledge cannot be stated or presented as observation"),
			Validate(Request, Response, Reason));
		TestEqual(
			TEXT("Hint over-disclosure has a stable reason"),
			Reason,
			FString(TEXT("knowledge_disclosure_mode_invalid")));
	}

	Response.Assertions = {
		MakeAssertion(GuHintKnowledge.KnowledgeId, EWSRoleplayClaimMode::Suspected)};
	TestTrue(
		TEXT("Hint knowledge may be expressed as a suspicion"),
		Validate(Request, Response, Reason));

	Request = MakeSingleKnowledgeRequest(YeHintKnowledge);
	Response = MakeSingleKnowledgeResponse(YeHintKnowledge);
	Response.NpcLine = TEXT("叶澄认为现有医疗储备经不起浪费。");
	Response.Assertions = {
		MakeAssertion(YeHintKnowledge.KnowledgeId, EWSRoleplayClaimMode::Belief)};
	TestTrue(
		TEXT("A real fresh-state hint may be expressed as a belief"),
		Validate(Request, Response, Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutV14StateConsistencyTest,
	"WhiteoutStation.Dialogue.V14.Response.StateConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV14StateConsistencyTest::RunTest(const FString& Parameters)
{
	using namespace WhiteoutDialogueV14ResponseTests;
	FWSRoleplayRequest Request = MakeRequest();
	FString Reason;
	FWSRoleplayResponse Contradiction = MakeValidResponse();
	Contradiction.NpcLine = TEXT("顾衡的事稍后再说，你得先决定供暖给哪间房。");
	TestFalse(
		TEXT("A locked heating phase cannot ask the player to choose heating again"),
		Validate(Request, Contradiction, Reason));
	TestEqual(
		TEXT("Heating contradiction has a stable reason"),
		Reason,
		FString(TEXT("heating_state_conflict")));

	Contradiction = MakeValidResponse();
	Contradiction.NpcLine = TEXT("顾衡已经把发电机修好，供电恢复了。");
	TestFalse(
		TEXT("An offline generator cannot be described as repaired"),
		Validate(Request, Contradiction, Reason));
	TestEqual(
		TEXT("Generator contradiction has a stable reason"),
		Reason,
		FString(TEXT("generator_state_conflict")));

	Request.SubjectiveState.GeneratorProgress = 1;
	Contradiction.NpcLine = TEXT("顾衡已经修了一部分，但发电机还没有恢复。");
	TestTrue(
		TEXT("An offline generator may still report partial repair progress"),
		Validate(Request, Contradiction, Reason));
	Contradiction.NpcLine = TEXT("顾衡说发电机还没开始修。");
	TestFalse(
		TEXT("Recorded repair progress cannot be described as untouched"),
		Validate(Request, Contradiction, Reason));

	Request.SubjectiveState.GeneratorStateId = TEXT("stable");
	Request.SubjectiveState.GeneratorProgress = 2;
	Contradiction.NpcLine = TEXT("顾衡说发电机还没修好，仍未恢复供电。");
	TestFalse(
		TEXT("A repaired generator cannot be described as offline"),
		Validate(Request, Contradiction, Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutV14ProposalAndPerformanceTest,
	"WhiteoutStation.Dialogue.V14.Response.ProposalAndPerformance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV14ProposalAndPerformanceTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueV14ResponseTests;
	FWSRoleplayRequest Request = MakeRequest();
	FWSRoleplayActionProposal Allowed;
	Allowed.Type = EWSRoleplayProposalType::ConditionalCooperation;
	Allowed.ActionId = TEXT("repair_generator");
	Allowed.RequestedConditionIds = {TEXT("player_assistance")};
	Allowed.ExpiresAtPhase = TEXT("afternoon");
	Request.AllowedActionProposals = {Allowed};
	FString Reason;

	FWSRoleplayResponse Proposed = MakeValidResponse();
	Proposed.NpcLine = TEXT("顾衡要你留下搭把手，之后他会开始维修。");
	Proposed.SpeechFunction = EWSRoleplaySpeechFunction::ConditionalCooperation;
	Proposed.bHasProposedAction = true;
	Proposed.ProposedAction = Allowed;
	TestTrue(
		TEXT("An exact allowlisted action proposal validates"),
		Validate(Request, Proposed, Reason));

	Proposed.ProposedAction.RequestedConditionIds = {TEXT("give_all_food")};
	TestFalse(
		TEXT("Changing any proposal condition is rejected"),
		Validate(Request, Proposed, Reason));
	TestEqual(
		TEXT("An altered proposal has a stable reason"),
		Reason,
		FString(TEXT("proposal_not_allowed")));

	FWSRoleplayResponse InvalidPerformance = MakeValidResponse();
	InvalidPerformance.Emotion = TEXT("omniscient");
	TestFalse(
		TEXT("Emotion must use the performance whitelist"),
		Validate(Request, InvalidPerformance, Reason));
	TestEqual(TEXT("Invalid emotion has a stable reason"), Reason, FString(TEXT("emotion_invalid")));

	InvalidPerformance = MakeValidResponse();
	InvalidPerformance.MovementIntent = TEXT("teleport");
	TestFalse(
		TEXT("Movement intent must use the performance whitelist"),
		Validate(Request, InvalidPerformance, Reason));

	InvalidPerformance = MakeValidResponse();
	InvalidPerformance.ReactionAction = TEXT("grant_reward");
	TestFalse(
		TEXT("Reaction action must use the performance whitelist"),
		Validate(Request, InvalidPerformance, Reason));

	InvalidPerformance = MakeValidResponse();
	InvalidPerformance.SpeechFunction =
		static_cast<EWSRoleplaySpeechFunction>(255);
	TestFalse(
		TEXT("An undefined speech function is rejected"),
		Validate(Request, InvalidPerformance, Reason));
	TestEqual(
		TEXT("An invalid speech function has a stable reason"),
		Reason,
		FString(TEXT("speech_function_invalid")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutV14MemoryBoundaryTest,
	"WhiteoutStation.Dialogue.V14.Response.MemoryBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV14MemoryBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace WhiteoutDialogueV14ResponseTests;
	FWSRoleplayRequest Request = MakeRequest();
	Request.ForbiddenFactIds = {TEXT("FACT_HEAT_PACK")};
	FString Reason;
	FWSRoleplayResponse Response = MakeValidResponse();
	Response.MemorySummary = TEXT("叶澄告诉玩家医务室里藏着保温包。");
	TestFalse(
		TEXT("A memory summary cannot preserve a forbidden secret"),
		Validate(Request, Response, Reason));
	TestEqual(
		TEXT("A memory secret has a stable reason"),
		Reason,
		FString(TEXT("memory_summary_forbidden_fact")));

	Response = MakeValidResponse();
	Response.MemorySummary = TEXT("记录 GU_ROLE，并把它写进 Prompt。");
	TestFalse(
		TEXT("A memory summary cannot contain system language or internal IDs"),
		Validate(Request, Response, Reason));
	TestEqual(
		TEXT("Unsafe memory language has a stable reason"),
		Reason,
		FString(TEXT("memory_summary_system_language")));

	Response = MakeValidResponse();
	Response.MemorySummary = FString::ChrN(161, TEXT('字'));
	TestFalse(
		TEXT("A memory summary over 160 characters is rejected"),
		Validate(Request, Response, Reason));
	TestEqual(
		TEXT("Memory length has a stable reason"),
		Reason,
		FString(TEXT("memory_summary_too_long")));
	return true;
}

#endif
