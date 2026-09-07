#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "State/WindStationStateSubsystem.h"
#include "Agents/WSAgentGateway.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutV15MessageStateTest,
	"WhiteoutStation.Dialogue.V15.Revision.MessageState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV15MessageStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* Game = NewObject<UGameInstance>(); Game->AddToRoot(); Game->Init();
	auto* State = Game->GetSubsystem<UWindStationStateSubsystem>();
	if (!TestNotNull(TEXT("Subsystem"), State)) { Game->Shutdown(); Game->RemoveFromRoot(); return false; }
	const FString Slot = TEXT("Whiteout_V15_Message_") + FGuid::NewGuid().ToString();
	State->SetAutomationSaveSlot(Slot);
	const auto Reset = [&]()
	{
		State->NewGame(); EWSReasonCode Reason; TArray<FString> Changes;
		State->BeginDayPhase(EWSHeatingZone::MedicalRoom, Reason, Changes);
		State->SetDialogueRealizeTestHook([](const FWSPreparedDialogue& P, FWSDialogueRealizeTestCallback Callback) { Callback(P.LocalFallback); });
	};
	Reset();
	const auto Ask = [](FName Topic)
	{
		FWSCanonicalIntent P; P.SpeakerId = TEXT("gu_heng"); P.TopicId = Topic;
		P.Frame.SpeechAct = EWSDialogueAct::Ask; P.Frame.TargetCharacter = EWSCharacterId::GuHeng;
		P.Frame.Confidence = 1.0f;
		if (Topic == TEXT("generator")) { P.Frame.QueryType = EWSDialogueQueryType::Status; P.Frame.TargetActionId = TEXT("repair_generator"); }
		return P;
	};
	const auto Propose = [&](EWSHeatingZone Zone, int32 Offset)
	{
		auto P = Ask(TEXT("commitment")); P.Frame.SpeechAct = EWSDialogueAct::Promise;
		P.Commitment = EWSCommitmentIntent::Proposed; P.PromiseCondition = TEXT("heat_zone");
		FWSPromiseTerms T; T.Kind = TEXT("heat_zone"); T.Zone = Zone; T.PhaseOffset = Offset; P.Terms.Add(T);
		return P;
	};
	FGuid Session = FGuid::NewGuid(); FWSCanonicalIntent Resolved; FString Status;
	const auto Resolve = [&](const FWSCanonicalIntent& P)
	{ return State->ResolveParsedOnlineMessage(TEXT("talk_gu_heng"), TEXT("构造解析结果状态机测试"), Session, P, Resolved, Status); };
	const auto Request = [&]()
	{
		FWSActionRequest R; Resolved.ApplyTo(R); R.ActionId = TEXT("talk_gu_heng");
		R.DialogueSessionId = Session; R.TransactionId = FGuid::NewGuid(); R.PlayerSaid = TEXT("构造解析结果状态机测试"); return R;
	};
	const auto Submit = [&]() { return State->SubmitDialogueAction(Request()); };
	const auto Confirm = [&]()
	{
		auto P = Ask(TEXT("commitment")); P.Frame.SpeechAct = EWSDialogueAct::Promise;
		P.Commitment = EWSCommitmentIntent::ConfirmPending;
		if (State->DialogueSessions[Session].PendingCommitment.IsSet())
		{
			P.ProposalId = State->DialogueSessions[Session].PendingCommitment->ProposalId;
			P.ProposalVersion = State->DialogueSessions[Session].PendingCommitment->ProposalVersion;
		}
		return P;
	};
	const int32 InitialAP = State->GetStateSnapshot().PhaseActionPoints;
	auto Pressure = Ask(TEXT("relationship")); Pressure.Frame.SpeechAct = EWSDialogueAct::Command;
	Pressure.bNeedsClarification = true; Pressure.Clarification = TEXT("具体要我做什么？");
	TestTrue(TEXT("Pressure remains actionable while task is missing"), Resolve(Pressure));
	TestTrue(TEXT("Local question retained"), Resolved.Notice.Contains(TEXT("具体")));
	const auto FirstRequest = Request(); const auto First = State->SubmitDialogueAction(FirstRequest);
	TestTrue(TEXT("Pressure commits"), First.bCommitted);
	TestEqual(TEXT("One AP for first actual response"), State->GetStateSnapshot().PhaseActionPoints, InitialAP - 1);
	TestEqual(TEXT("No invented repair"), State->GetStateSnapshot().Tasks.GeneratorProgress, 0);
	const int32 Forced = State->GetStateSnapshot().Flags.ForcedActionCount;
	TestEqual(TEXT("Pressure has one rule effect"), Forced, 1);
	auto Duplicate = FirstRequest; Duplicate.TransactionId = FGuid::NewGuid();
	TestFalse(TEXT("Same message cannot be replayed with a new transaction"), State->SubmitDialogueAction(Duplicate).bCommitted);
	TestTrue(TEXT("Second pressure resolves"), Resolve(Pressure));
	TestTrue(TEXT("Second pressure commits"), Submit().bCommitted);
	TestEqual(TEXT("Pressure effect deduplicates across session"), State->GetStateSnapshot().Flags.ForcedActionCount, Forced);
	auto Unclear = Ask(TEXT("unknown")); Unclear.bNeedsClarification = true;
	TestFalse(TEXT("Pure clarification never submits"), Resolve(Unclear));
	TestEqual(TEXT("Clarification preserves two committed turns"), State->DialogueSessions[Session].CommittedTurns, 2);

	FWSCanonicalIntent Mixed; Mixed.Parts = {Ask(TEXT("person")), Ask(TEXT("generator")), Propose(EWSHeatingZone::Kitchen, 1)};
	TestTrue(TEXT("Questions accompanying a proposal survive"), Resolve(Mixed));
	TestEqual(TEXT("Two response clauses retained"), Resolved.Parts.Num(), 2);
	TestTrue(TEXT("Third mixed message commits"), Submit().bCommitted);
	TestEqual(TEXT("Two questions consume one turn"), State->DialogueSessions[Session].CommittedTurns, 3);
	TestEqual(TEXT("No extra AP for sub-intents"), State->GetStateSnapshot().PhaseActionPoints, InitialAP - 1);
	TestTrue(TEXT("Third response exposes pending confirmation"), State->GetLatestDialogue().bPendingConfirmation);
	TestTrue(TEXT("Pending terms remain kitchen"), State->DialogueSessions[Session].PendingCommitment->Terms[0].Zone == EWSHeatingZone::Kitchen);
	TestFalse(TEXT("Fourth free-chat message rejected"), Resolve(Ask(TEXT("person"))));
	FWSActionRequest ForgedClosure; Ask(TEXT("person")).ApplyTo(ForgedClosure);
	ForgedClosure.ActionId = TEXT("talk_gu_heng"); ForgedClosure.DialogueSessionId = Session;
	ForgedClosure.bConfirmationClosure = true;
	TestFalse(TEXT("Caller cannot grant itself a fourth turn"), State->SubmitDialogueAction(ForgedClosure).bCommitted);
	TestTrue(TEXT("Closure confirmation resolves"), Resolve(Confirm()));
	TestTrue(TEXT("Closure flag is authoritative"), Resolved.bConfirmationClosure);
	const auto ClosingRequest = Request();
	TestTrue(TEXT("Closure commits"), State->SubmitDialogueAction(ClosingRequest).bCommitted);
	TestEqual(TEXT("Closure does not count fourth turn"), State->DialogueSessions[Session].CommittedTurns, 3);
	TestEqual(TEXT("Closure does not charge AP"), State->GetStateSnapshot().PhaseActionPoints, InitialAP - 1);
	TestEqual(TEXT("One promise registered"), State->GetStateSnapshot().Promises.Num(), 1);
	TestTrue(TEXT("Correct heating zone persisted"), State->GetStateSnapshot().Promises[0].Terms.Zone == EWSHeatingZone::Kitchen);
	TestFalse(TEXT("Committed confirmation consumed pending item"), State->DialogueSessions[Session].PendingCommitment.IsSet());
	TestFalse(TEXT("Duplicate closure cannot register twice"), State->SubmitDialogueAction(ClosingRequest).bCommitted);
	TestTrue(TEXT("Parameterized promise saves"), State->SaveSnapshot());
	TestTrue(TEXT("Parameterized promise loads"), State->LoadSnapshot());
	TestEqual(TEXT("Deadline survives save/load"), State->GetStateSnapshot().Promises[0].Terms.DuePhase, 1);
	TestTrue(TEXT("Zone survives save/load"), State->GetStateSnapshot().Promises[0].Terms.Zone == EWSHeatingZone::Kitchen);
	for (const bool CorrectPhase : {false, true})
	{
		FWhiteoutRulesEngine Simulation = State->GetRulesEngine();
		auto Snapshot = Simulation.GetState();
		FWSHeatingSelectionRecord Heating; Heating.Zone = EWSHeatingZone::Kitchen;
		Heating.Phase = CorrectPhase ? EWSDayPhase::Afternoon : EWSDayPhase::Dusk;
		Snapshot.Heating.History.Add(Heating); Simulation.SetState(Snapshot); Simulation.EndGame();
		TestEqual(TEXT("Heating fulfills only the bound zone and phase"), Simulation.GetState().Promises[0].bFulfilled, CorrectPhase);
	}

	Reset(); Session = FGuid::NewGuid();
	TestFalse(TEXT("First proposal is free"), Resolve(Propose(EWSHeatingZone::RepairRoom, 1)));
	const auto Old = Confirm();
	auto Changed = Old; Changed.Terms = Propose(EWSHeatingZone::Kitchen, 2).Terms;
	TestFalse(TEXT("Changed confirmation becomes new proposal"), Resolve(Changed));
	TestEqual(TEXT("Proposal identity retained on amendment"), State->DialogueSessions[Session].PendingCommitment->ProposalId, Old.ProposalId);
	TestEqual(TEXT("Version incremented"), State->DialogueSessions[Session].PendingCommitment->ProposalVersion, Old.ProposalVersion + 1);
	TestEqual(TEXT("Absolute dusk deadline retained"), State->DialogueSessions[Session].PendingCommitment->Terms[0].DuePhase, 2);
	TestFalse(TEXT("Old version cannot confirm changed terms"), Resolve(Old));
	TestTrue(TEXT("New version confirms"), Resolve(Confirm()));
	FWSDialogueRealizeTestCallback Delayed;
	FWSAgentReply DelayedReply;
	State->SetDialogueRealizeTestHook([&](const auto& P, auto Callback) { DelayedReply = P.LocalFallback; Delayed = MoveTemp(Callback); });
	const auto Pending = Submit();
	TestTrue(TEXT("Confirmation pending realization"), Pending.bPendingDialogue);
	const auto ActiveGeneration = State->PendingDialogue.Generation;
	TestFalse(TEXT("Concurrent resubmission does not commit"), Submit().bCommitted);
	TestTrue(TEXT("Concurrent resubmission preserves active transaction"), State->bHasPendingDialogue);
	TestEqual(TEXT("Concurrent resubmission preserves callback generation"), State->PendingDialogue.Generation, ActiveGeneration);
	TestTrue(TEXT("Proposal not consumed before commit"), State->DialogueSessions[Session].PendingCommitment.IsSet());
	State->CancelPendingDialogue();
	TestTrue(TEXT("Cancel preserves recoverable proposal"), State->DialogueSessions[Session].PendingCommitment.IsSet());
	Delayed(DelayedReply);
	TestEqual(TEXT("Late callback creates no promise"), State->GetStateSnapshot().Promises.Num(), 0);
	TestEqual(TEXT("Late callback costs no AP"), State->GetStateSnapshot().PhaseActionPoints, InitialAP);
	State->SetDialogueRealizeTestHook([](const auto& P, auto Callback) { Callback(P.LocalFallback); });
	TestTrue(TEXT("Retry binds same proposal"), Resolve(Confirm()));
	TestTrue(TEXT("Retry commits"), Submit().bCommitted);
	TestEqual(TEXT("Retry registers once"), State->GetStateSnapshot().Promises.Num(), 1);

	Reset(); Session = FGuid::NewGuid();
	TestFalse(TEXT("Proposal before response failure"), Resolve(Propose(EWSHeatingZone::Kitchen, 1)));
	TestTrue(TEXT("Confirmation before response failure"), Resolve(Confirm()));
	State->SetDialogueRealizeTestHook([](const auto& P, auto Callback)
		{ auto Bad = P.LocalFallback; Bad.Utterance = TEXT("未获授权的不同结果"); Callback(Bad); });
	TestFalse(TEXT("Failed realization does not commit"), Submit().bCommitted);
	TestTrue(TEXT("Failed realization preserves proposal"), State->DialogueSessions[Session].PendingCommitment.IsSet());
	TestEqual(TEXT("Failed realization creates no promise"), State->GetStateSnapshot().Promises.Num(), 0);
	TestEqual(TEXT("Failed realization costs no AP"), State->GetStateSnapshot().PhaseActionPoints, InitialAP);
	State->SetDialogueRealizeTestHook([](const auto& P, auto Callback) { Callback(P.LocalFallback); });
	auto Rejected = Confirm(); Rejected.Commitment = EWSCommitmentIntent::RejectPending;
	TestFalse(TEXT("Explicit rejection is control-only"), Resolve(Rejected));
	TestFalse(TEXT("Explicit rejection removes pending proposal"), State->DialogueSessions[Session].PendingCommitment.IsSet());
	TestFalse(TEXT("Cancelled proposal cannot be confirmed"), Resolve(Confirm()));

	Reset(); Session = FGuid::NewGuid();
	FWSCanonicalIntent TwoQuestions; TwoQuestions.Parts = {Ask(TEXT("person")), Ask(TEXT("generator"))};
	TestTrue(TEXT("Two clauses for independent validation"), Resolve(TwoQuestions));
	State->SetDialogueRealizeTestHook([&](const auto& P, auto Callback)
	{
		TArray<FWSDialogueOutcome> Outcomes;
		for (const auto& Child : P.Parts)
		{
			FWSDialogueOutcome O; O.FinalReply = Child.LocalFallback;
			O.DisclosedFactIds = O.FinalReply.DisclosedFactIds; O.AnswerSource = O.FinalReply.AnswerSource; Outcomes.Add(O);
		}
		FString Error;
		auto Missing = FWSDialogueOutcome::Combine(Outcomes, P.OriginalRequest.DialogueNotice); Missing.Parts.Pop();
		TestFalse(TEXT("Missing response clause cannot pass"), UWSAgentGateway::ValidateDialogueOutcome(P, Missing, Error));
		Outcomes[0].DisclosedFactIds.Add(TEXT("FACT_FORCED_RESTART_CONFIRMED"));
		auto Leaked = FWSDialogueOutcome::Combine(Outcomes, P.OriginalRequest.DialogueNotice);
		TestFalse(TEXT("Another clause cannot grant unauthorized disclosure"), UWSAgentGateway::ValidateDialogueOutcome(P, Leaked, Error));
		Callback(P.LocalFallback);
	});
	TestTrue(TEXT("Complete authorized multi-response commits"), Submit().bCommitted);
	TestEqual(TEXT("Multi-response has one transaction"), State->GetStateSnapshot().CommittedTransactions.Num(), 1);

	Reset(); Session = FGuid::NewGuid();
	TestFalse(TEXT("Missing deadline requires clarification"), Resolve(Propose(EWSHeatingZone::Kitchen, 0)));
	TestFalse(TEXT("Missing deadline does not create pending item"), State->DialogueSessions[Session].PendingCommitment.IsSet());
	auto Unsupported = Propose(EWSHeatingZone::Kitchen, 1); Unsupported.Terms[0].Prerequisite = TEXT("你认为安全");
	TestFalse(TEXT("Conditional promise not registered"), Resolve(Unsupported));
	TestTrue(TEXT("Unsupported terms explained"), Status.Contains(TEXT("不支持")));
	TestFalse(TEXT("Unsupported not substituted"), State->DialogueSessions[Session].PendingCommitment.IsSet());
	auto MultiTerms = Propose(EWSHeatingZone::Kitchen, 1);
	MultiTerms.Terms.Append(Propose(EWSHeatingZone::RepairRoom, 2).Terms);
	TestFalse(TEXT("Two heating phases form one pending proposal"), Resolve(MultiTerms));
	TestEqual(TEXT("Both terms retained"), State->DialogueSessions[Session].PendingCommitment->Terms.Num(), 2);
	TestTrue(TEXT("Multi-term confirmation resolves"), Resolve(Confirm()));
	TestTrue(TEXT("Multi-term confirmation commits once"), Submit().bCommitted);
	TestEqual(TEXT("Two obligations registered"), State->GetStateSnapshot().Promises.Num(), 2);
	TestEqual(TEXT("Multi-term registration consumes one turn"), State->DialogueSessions[Session].CommittedTurns, 1);
	Reset(); Session = FGuid::NewGuid();
	auto Conflict = Propose(EWSHeatingZone::Kitchen, 1);
	Conflict.Terms.Append(Propose(EWSHeatingZone::RepairRoom, 1).Terms);
	TestFalse(TEXT("One phase cannot promise two heating zones"), Resolve(Conflict));
	TestFalse(TEXT("Conflicting zones are not registered"), State->DialogueSessions[Session].PendingCommitment.IsSet());
	TestTrue(TEXT("First actual turn"), Resolve(Ask(TEXT("person"))) && Submit().bCommitted);
	TestTrue(TEXT("Second actual turn"), Resolve(Ask(TEXT("generator"))) && Submit().bCommitted);
	TestFalse(TEXT("Third-slot pure proposal is not a committed turn"), Resolve(Propose(EWSHeatingZone::RepairRoom, 1)));
	TestEqual(TEXT("Still two turns after proposal"), State->DialogueSessions[Session].CommittedTurns, 2);
	TestTrue(TEXT("Third-slot confirmation resolves"), Resolve(Confirm()));
	TestTrue(TEXT("Third-slot confirmation commits"), Submit().bCommitted);
	TestEqual(TEXT("Exactly three turns after confirmation"), State->DialogueSessions[Session].CommittedTurns, 3);
	State->SetDialogueRealizeTestHook({}); Game->Shutdown(); Game->RemoveFromRoot(); UGameplayStatics::DeleteGameInSlot(Slot, 0);
	return true;
}
#endif
