#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Agents/WSNPCContextBuilder.h"
#include "Agents/WSRoleplayKnowledgeRepository.h"
#include "Agents/WSRoleplayResponseValidator.h"

namespace
{
	FWSActionRequest MakeDialogueRequest(
		const TCHAR* ActionId,
		const TCHAR* PlayerLine)
	{
		FWSActionRequest Request;
		Request.ActionId = ActionId;
		Request.PlayerSaid = PlayerLine;
		Request.DialogueAct = EWSDialogueAct::Ask;
		Request.SemanticFrame = UWSNPCContextBuilder::BuildSemanticFrame(
			Request.PlayerSaid,
			Request.ActionId);
		return Request;
	}

	TArray<FName> KnowledgeIds(const FWSRoleplayRequest& Request)
	{
		TArray<FName> Result;
		for (const FWSRoleplayKnowledgeItem& Item : Request.AvailableKnowledge)
		{
			Result.Add(Item.KnowledgeId);
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueV14KnowledgeRepositoryTest,
	"WhiteoutStation.Dialogue.V14.Knowledge.Repository",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueV14KnowledgeRepositoryTest::RunTest(
	const FString& Parameters)
{
	UWSRoleplayKnowledgeRepository* Repository =
		NewObject<UWSRoleplayKnowledgeRepository>();
	FString Error;
	TestTrue(TEXT("Six v1.4 knowledge assets load"), Repository->LoadDefault(Error));
	if (!Repository->IsAvailable())
	{
		AddError(Error);
		return false;
	}

	TestEqual(TEXT("Knowledge entry count"), Repository->GetKnowledgeCount(), 95);
	FWSRoleplayProfile GuProfile;
	FWSRoleplayProfile YeProfile;
	TestTrue(TEXT("Gu Heng profile is indexed"), Repository->GetProfile(TEXT("gu_heng"), GuProfile));
	TestTrue(TEXT("Ye Cheng profile is indexed"), Repository->GetProfile(TEXT("ye_cheng"), YeProfile));
	TestEqual(TEXT("Gu Heng stable id"), GuProfile.Id, FName(TEXT("gu_heng")));
	TestEqual(TEXT("Ye Cheng stable id"), YeProfile.Id, FName(TEXT("ye_cheng")));

	for (const FName Id : {
		FName(TEXT("WORLD_GENERATOR_STOP_EVENT")),
		FName(TEXT("GU_FORCED_RESTART_KNOWLEDGE")),
		FName(TEXT("YE_HEAT_PACK_KNOWLEDGE")),
		FName(TEXT("REL_GU_SHARED_TENURE"))})
	{
		FWSRoleplayKnowledgeItem Item;
		TestTrue(
			*FString::Printf(TEXT("Knowledge id %s resolves"), *Id.ToString()),
			Repository->GetKnowledgeItem(Id, Item));
	}

	TestFalse(
		TEXT("A failed reload reports failure"),
		Repository->LoadFromDirectory(TEXT("Z:/missing/v1.4"), Error));
	TestFalse(
		TEXT("A failed reload leaves no published repository"),
		Repository->IsAvailable());
	TestEqual(
		TEXT("A failed reload does not retain prior entries"),
		Repository->GetKnowledgeCount(),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueV14ContextSelectionTest,
	"WhiteoutStation.Dialogue.V14.Knowledge.ContextSelection",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueV14ContextSelectionTest::RunTest(
	const FString& Parameters)
{
	UWSRoleplayKnowledgeRepository* Repository =
		NewObject<UWSRoleplayKnowledgeRepository>();
	FString Error;
	if (!Repository->LoadDefault(Error))
	{
		AddError(Error);
		return false;
	}

	FWSGameState State;
	FWSRoleplayRequest First;
	FWSRoleplayRequest Second;
	FWSRoleplayFallback FirstFallback;
	FWSRoleplayFallback SecondFallback;
	const FWSActionRequest GeneratorQuestion = MakeDialogueRequest(
		TEXT("talk_gu_heng"),
		TEXT("发电机为什么停了，现在维修到哪一步？"));
	TestTrue(
		TEXT("Generator context builds"),
		UWSNPCContextBuilder::BuildRequest(
			GeneratorQuestion,
			State,
			*Repository,
			{},
			1,
			First,
			FirstFallback,
			Error));
	TestTrue(
		TEXT("Repeated generator context builds"),
		UWSNPCContextBuilder::BuildRequest(
			GeneratorQuestion,
			State,
			*Repository,
			{},
			1,
			Second,
			SecondFallback,
			Error));
	TestEqual(TEXT("Policy Top-K is applied"), First.AvailableKnowledge.Num(), 10);
	TestEqual(
		TEXT("Target subject is generator"),
		First.TargetSubjectId,
		FName(TEXT("generator")));
	TestEqual(TEXT("Remaining turn budget is derived"), First.RemainingTurns, 2);
	TestTrue(
		TEXT("Generator knowledge is ranked into context"),
		First.AvailableKnowledge.ContainsByPredicate(
			[](const FWSRoleplayKnowledgeItem& Item)
			{
				return Item.SubjectId == TEXT("generator");
			}));
	TestTrue(
		TEXT("Knowledge order is deterministic"),
		KnowledgeIds(First) == KnowledgeIds(Second));
	TestEqual(
		TEXT("Fallback selection is deterministic"),
		FirstFallback.FallbackId,
		SecondFallback.FallbackId);

	FWSRoleplayRequest YeContext;
	FWSRoleplayFallback YeFallback;
	const FWSActionRequest YeGeneratorQuestion = MakeDialogueRequest(
		TEXT("talk_ye_cheng"),
		TEXT("你知道发电机和继电器哪里坏了吗？"));
	TestTrue(
		TEXT("Uncertain Ye Cheng context builds"),
		UWSNPCContextBuilder::BuildRequest(
			YeGeneratorQuestion,
			State,
			*Repository,
			{},
			1,
			YeContext,
			YeFallback,
			Error));
	TestTrue(
		TEXT("Unknown epistemic knowledge remains answerable"),
		KnowledgeIds(YeContext).Contains(TEXT("YE_RELAY_DETAILS_UNKNOWN")));

	FWSRoleplayRequest AmbiguousContext;
	FWSRoleplayFallback AmbiguousFallback;
	const FWSActionRequest AmbiguousQuestion = MakeDialogueRequest(
		TEXT("talk_gu_heng"),
		TEXT("你好，说说看。"));
	TestTrue(
		TEXT("Ambiguous context builds"),
		UWSNPCContextBuilder::BuildRequest(
			AmbiguousQuestion,
			State,
			*Repository,
			{},
			1,
			AmbiguousContext,
			AmbiguousFallback,
			Error));
	TestEqual(
		TEXT("Ambiguous input keeps an unknown target"),
		AmbiguousContext.TargetSubjectId,
		FName(TEXT("unknown")));
	TestEqual(
		TEXT("Ambiguous input selects the ambiguous fallback"),
		AmbiguousFallback.FallbackId,
		FName(TEXT("FB_GU_AMBIGUOUS")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueV14CharacterQuestionTest,
	"WhiteoutStation.Dialogue.V14.Knowledge.CharacterQuestion",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueV14CharacterQuestionTest::RunTest(
	const FString& Parameters)
{
	UWSRoleplayKnowledgeRepository* Repository =
		NewObject<UWSRoleplayKnowledgeRepository>();
	FString Error;
	if (!Repository->LoadDefault(Error))
	{
		AddError(Error);
		return false;
	}

	FWSRoleplayRequest Context;
	FWSRoleplayFallback Fallback;
	const FWSActionRequest Request = MakeDialogueRequest(
		TEXT("talk_ye_cheng"),
		TEXT("对于顾衡，你知道多少？你们过去关系怎么样？"));
	TestTrue(
		TEXT("Character context builds"),
		UWSNPCContextBuilder::BuildRequest(
			Request,
			FWSGameState(),
			*Repository,
			{},
			2,
			Context,
			Fallback,
			Error));
	TestEqual(
		TEXT("Named character becomes target subject"),
		Context.TargetSubjectId,
		FName(TEXT("gu_heng")));
	TestTrue(
		TEXT("Character or relationship knowledge is selected"),
		Context.AvailableKnowledge.ContainsByPredicate(
			[](const FWSRoleplayKnowledgeItem& Item)
			{
				return Item.SubjectId == TEXT("gu_heng")
					|| Item.SubjectId == TEXT("relationship_gu_heng_ye_cheng");
			}));
	TestFalse(
		TEXT("Fallback placeholders are resolved"),
		Fallback.Line.Contains(TEXT("{heating_state}"))
			|| Fallback.Line.Contains(TEXT("{generator_state}")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueV14HeatingStateTest,
	"WhiteoutStation.Dialogue.V14.Knowledge.HeatingState",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueV14HeatingStateTest::RunTest(const FString& Parameters)
{
	FWSGameState State;
	State.Heating.bLocked = false;
	TestEqual(
		TEXT("Unlocked heating remains a pending choice"),
		UWSNPCContextBuilder::BuildSubjectiveState(TEXT("gu_heng"), State).HeatingStateId,
		FName(TEXT("pending_selection")));

	struct FHeatingExpectation
	{
		EWSHeatingZone Zone;
		FName ZoneId;
		FName StateId;
	};
	const FHeatingExpectation Expectations[] = {
		{EWSHeatingZone::RepairRoom, TEXT("repair_room"), TEXT("locked_repair_room")},
		{EWSHeatingZone::MedicalRoom, TEXT("medical_room"), TEXT("locked_medical_room")},
		{EWSHeatingZone::Kitchen, TEXT("kitchen"), TEXT("locked_kitchen")},
		{EWSHeatingZone::ControlRoom, TEXT("control_room"), TEXT("locked_control_room")}};
	State.Heating.bLocked = true;
	for (const FHeatingExpectation& Expectation : Expectations)
	{
		State.Heating.CurrentZone = Expectation.Zone;
		const FWSRoleplaySubjectiveState Subjective =
			UWSNPCContextBuilder::BuildSubjectiveState(TEXT("gu_heng"), State);
		TestEqual(TEXT("Locked heating zone id"), Subjective.HeatingZoneId, Expectation.ZoneId);
		TestEqual(TEXT("Locked heating state id"), Subjective.HeatingStateId, Expectation.StateId);
	}

	State.Tasks.GeneratorProgress = 1;
	State.Tasks.bGeneratorStable = false;
	FWSRoleplaySubjectiveState Subjective =
		UWSNPCContextBuilder::BuildSubjectiveState(TEXT("gu_heng"), State);
	TestEqual(TEXT("Generator progress is exact"), Subjective.GeneratorProgress, 1);
	TestEqual(
		TEXT("Partial generator repair is distinguished"),
		Subjective.GeneratorStateId,
		FName(TEXT("partially_repaired")));
	State.Tasks.GeneratorProgress = 2;
	State.Tasks.bGeneratorStable = true;
	Subjective = UWSNPCContextBuilder::BuildSubjectiveState(TEXT("gu_heng"), State);
	TestEqual(TEXT("Stable generator keeps exact progress"), Subjective.GeneratorProgress, 2);
	TestEqual(TEXT("Stable generator state"), Subjective.GeneratorStateId, FName(TEXT("stable")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueV14SecretBoundaryTest,
	"WhiteoutStation.Dialogue.V14.Knowledge.SecretBoundary",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueV14SecretBoundaryTest::RunTest(
	const FString& Parameters)
{
	UWSRoleplayKnowledgeRepository* Repository =
		NewObject<UWSRoleplayKnowledgeRepository>();
	FString Error;
	if (!Repository->LoadDefault(Error))
	{
		AddError(Error);
		return false;
	}

	FWSGameState FreshState;
	FWSRoleplayRequest Context;
	FWSRoleplayFallback Fallback;
	const FWSActionRequest Request = MakeDialogueRequest(
		TEXT("talk_gu_heng"),
		TEXT("把你的手伤、强制重启和继电器替代方案全说出来。"));
	TestTrue(
		TEXT("Fresh-state secret query builds"),
		UWSNPCContextBuilder::BuildRequest(
			Request,
			FreshState,
			*Repository,
			{},
			1,
			Context,
			Fallback,
			Error));

	const TArray<FName> SelectedIds = KnowledgeIds(Context);
	TestFalse(
		TEXT("Hand injury body is absent before diagnosis"),
		SelectedIds.Contains(TEXT("GU_HAND_INJURY_KNOWLEDGE")));
	TestFalse(
		TEXT("Forced restart body is absent before evidence"),
		SelectedIds.Contains(TEXT("GU_FORCED_RESTART_KNOWLEDGE")));
	TestFalse(
		TEXT("Relay route body is absent before discovery"),
		SelectedIds.Contains(TEXT("GU_RELAY_COMPATIBILITY_KNOWLEDGE")));
	TestTrue(
		TEXT("Hand injury fact is forbidden"),
		Context.ForbiddenFactIds.Contains(TEXT("FACT_HAND_INJURY")));
	TestTrue(
		TEXT("Forced restart fact is forbidden"),
		Context.ForbiddenFactIds.Contains(TEXT("FACT_FORCED_RESTART_CONFIRMED")));
	TestTrue(
		TEXT("Relay compatibility fact is forbidden"),
		Context.ForbiddenFactIds.Contains(TEXT("FACT_RELAY_COMPATIBILITY")));
	TestFalse(
		TEXT("A protected hand-injury ID is not exposed as a topic tag"),
		Context.TopicTags.Contains(TEXT("FACT_HAND_INJURY")));
	TestFalse(
		TEXT("A protected restart ID is not exposed as a topic tag"),
		Context.TopicTags.Contains(TEXT("FACT_FORCED_RESTART_CONFIRMED")));
	TestFalse(
		TEXT("A protected relay ID is not exposed as a topic tag"),
		Context.TopicTags.Contains(TEXT("FACT_RELAY_COMPATIBILITY")));

	FreshState.PlayerKnowledge.Add(
		TEXT("FACT_HAND_INJURY"),
		EWSKnowledgeLevel::Suspected);
	FWSRoleplayRequest SuspectedContext;
	TestTrue(
		TEXT("Suspected-state secret query builds"),
		UWSNPCContextBuilder::BuildRequest(
			Request,
			FreshState,
			*Repository,
			{},
			1,
			SuspectedContext,
			Fallback,
			Error));
	TestTrue(
		TEXT("Suspicion does not remove the hand-injury boundary"),
		SuspectedContext.ForbiddenFactIds.Contains(TEXT("FACT_HAND_INJURY")));

	FWSGameState TrustedDiagnosisState;
	TrustedDiagnosisState.Flags.bGuHengDiagnosed = true;
	FWSCharacterState& YeState = TrustedDiagnosisState.Characters.FindOrAdd(
		EWSCharacterId::YeCheng);
	YeState.Trust = 6.0f;
	YeState.Pressure = 4.0f;
	const FWSActionRequest HeatPackQuestion = MakeDialogueRequest(
		TEXT("talk_ye_cheng"),
		TEXT("医务室的保温包还能用吗？"));
	FWSRoleplayRequest HeatPackContext;
	TestTrue(
		TEXT("Trusted post-diagnosis heat-pack query builds"),
		UWSNPCContextBuilder::BuildRequest(
			HeatPackQuestion,
			TrustedDiagnosisState,
			*Repository,
			{},
			1,
			HeatPackContext,
			Fallback,
			Error));
	TestTrue(
		TEXT("A natural heat-pack query retrieves the unlocked knowledge"),
		KnowledgeIds(HeatPackContext).Contains(TEXT("YE_HEAT_PACK_KNOWLEDGE")));
	TestFalse(
		TEXT("The unlocked heat-pack fact is no longer forbidden"),
		HeatPackContext.ForbiddenFactIds.Contains(TEXT("FACT_HEAT_PACK")));
	FWSRoleplayResponse HeatPackFallbackResponse;
	HeatPackFallbackResponse.NpcLine = Fallback.Line;
	HeatPackFallbackResponse.SpeechFunction = Fallback.SpeechFunction;
	HeatPackFallbackResponse.ReferencedKnowledgeIds =
		Fallback.ReferencedKnowledgeIds;
	HeatPackFallbackResponse.MemorySummary = Fallback.Line;
	HeatPackFallbackResponse.Emotion = TEXT("clinical");
	HeatPackFallbackResponse.MovementIntent = TEXT("stay");
	HeatPackFallbackResponse.ReactionAction = TEXT("neutral");
	FString ValidationReason;
	TestTrue(
		TEXT("The target-aware local fallback remains valid"),
		UWSRoleplayResponseValidator::Validate(
			HeatPackContext,
			HeatPackFallbackResponse,
			ValidationReason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueV14DiagnosisAndMemoryTest,
	"WhiteoutStation.Dialogue.V14.Knowledge.DiagnosisAndMemoryRecency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueV14DiagnosisAndMemoryTest::RunTest(const FString& Parameters)
{
	UWSRoleplayKnowledgeRepository* Repository = NewObject<UWSRoleplayKnowledgeRepository>();
	FString Error;
	if (!Repository->LoadDefault(Error))
	{
		AddError(Error);
		return false;
	}
	FWSGameState State;
	TArray<FWSRoleplayMemoryEntry> Memories;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		FWSRoleplayMemoryEntry Memory;
		Memory.MemoryId = FName(*FString::Printf(TEXT("ye_%d"), Index));
		Memory.Owner = TEXT("ye_cheng");
		Memory.TurnIndex = Index < 6 ? 3 : 1;
		Memories.Add(Memory);
	}
	FWSRoleplayRequest Context;
	FWSRoleplayFallback Fallback;
	TestTrue(TEXT("Explicit medical question builds"), UWSNPCContextBuilder::BuildRequest(
		MakeDialogueRequest(TEXT("talk_ye_cheng"), TEXT("顾衡的手怎么样，会影响精细维修吗？")),
		State, *Repository, Memories, 1, Context, Fallback, Error));
	TestTrue(TEXT("Diagnosis is reachable before treatment"),
		KnowledgeIds(Context).Contains(TEXT("YE_GU_HAND_DIAGNOSIS")));
	TestFalse(TEXT("Permitted diagnosis includes its hand-injury surface"),
		Context.ForbiddenFactIds.Contains(TEXT("FACT_HAND_INJURY")));
	TestEqual(TEXT("Keep six latest memories"), Context.RecentMemory.Num(), 6);
	if (Context.RecentMemory.Num() == 6)
	{
		TestEqual(TEXT("Oldest retained memory"), Context.RecentMemory[0].MemoryId, FName(TEXT("ye_2")));
		TestEqual(TEXT("New session turn one stays newest"), Context.RecentMemory.Last().MemoryId, FName(TEXT("ye_7")));
	}
	FWSRoleplayResponse Response;
	Response.NpcLine = Fallback.Line;
	Response.SpeechFunction = Fallback.SpeechFunction;
	Response.ReferencedKnowledgeIds = Fallback.ReferencedKnowledgeIds;
	Response.Assertions = Fallback.Assertions;
	Response.MemorySummary = Fallback.Line;
	Response.Emotion = TEXT("clinical");
	Response.MovementIntent = TEXT("stay");
	Response.ReactionAction = TEXT("consider");
	TArray<FName> Facts;
	TestTrue(TEXT("Authored factual fallback validates"),
		UWSRoleplayResponseValidator::ValidateAndDeriveDisclosures(Context, Response, Facts, Error));
	TestTrue(TEXT("Fallback asserts diagnosis"), Facts.Contains(TEXT("FACT_MEDICAL_DIAGNOSIS")));
	TestTrue(TEXT("Generic character question builds"), UWSNPCContextBuilder::BuildRequest(
		MakeDialogueRequest(TEXT("talk_ye_cheng"), TEXT("对于顾衡，你知道多少？")),
		State, *Repository, {}, 1, Context, Fallback, Error));
	TestFalse(TEXT("Character chat cannot diagnose"),
		KnowledgeIds(Context).Contains(TEXT("YE_GU_HAND_DIAGNOSIS")));
	TestTrue(TEXT("Character fallback cannot upgrade facts"), Fallback.Assertions.IsEmpty());
	const FWSActionRequest RelayQuestion = MakeDialogueRequest(TEXT("talk_gu_heng"),
		TEXT("控制柜和日志里的证据都看过了，继电器还有替代方案吗？"));
	TestTrue(TEXT("Relay question without evidence builds"), UWSNPCContextBuilder::BuildRequest(
		RelayQuestion, State, *Repository, {}, 1, Context, Fallback, Error));
	TestFalse(TEXT("Claiming evidence cannot unlock the relay"),
		KnowledgeIds(Context).Contains(TEXT("GU_RELAY_COMPATIBILITY_KNOWLEDGE")));
	State.Evidence.Add(TEXT("EVIDENCE_DEEP_GENERATOR_LOG"));
	State.Evidence.Add(TEXT("EVIDENCE_BURNT_RELAY"));
	TestTrue(TEXT("Evidence-backed relay question builds"), UWSNPCContextBuilder::BuildRequest(
		RelayQuestion, State, *Repository, {}, 1, Context, Fallback, Error));
	TestTrue(TEXT("Relay route is reachable before dismantling"),
		KnowledgeIds(Context).Contains(TEXT("GU_RELAY_COMPATIBILITY_KNOWLEDGE")));
	TestEqual(TEXT("Relay fallback carries one explicit assertion"), Fallback.Assertions.Num(), 1);
	return true;
}

#endif
