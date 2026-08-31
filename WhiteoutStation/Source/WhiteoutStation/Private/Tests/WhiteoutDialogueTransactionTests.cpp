#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "State/WindStationStateSubsystem.h"

namespace WhiteoutDialogueTransactionTests
{
	class FScopedStateSubsystem
	{
	public:
		FScopedStateSubsystem()
		{
			GameInstance = NewObject<UGameInstance>();
			GameInstance->AddToRoot();
			GameInstance->Init();
			StateSubsystem =
				GameInstance->GetSubsystem<UWindStationStateSubsystem>();
			SaveSlot = FString::Printf(
				TEXT("WhiteoutStation_Automation_%s"),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			if (StateSubsystem)
			{
				StateSubsystem->SetAutomationSaveSlot(SaveSlot);
				UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
				StateSubsystem->NewGame();
				EWSReasonCode Reason = EWSReasonCode::UnknownAction;
				TArray<FString> Changes;
				bReady = StateSubsystem->BeginDayPhase(
					EWSHeatingZone::RepairRoom,
					Reason,
					Changes);
			}
		}

		~FScopedStateSubsystem()
		{
			if (StateSubsystem)
			{
				StateSubsystem->SetDialogueRealizeTestHook({});
				StateSubsystem->SetDialogueCommitDispatchTestHook({});
			}
			UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
			if (GameInstance)
			{
				GameInstance->Shutdown();
				GameInstance->RemoveFromRoot();
			}
		}

		UWindStationStateSubsystem* Get() const
		{
			return StateSubsystem;
		}

		bool IsReady() const
		{
			return bReady;
		}

		const FString& GetSaveSlot() const
		{
			return SaveSlot;
		}

	private:
		UGameInstance* GameInstance = nullptr;
		UWindStationStateSubsystem* StateSubsystem = nullptr;
		FString SaveSlot;
		bool bReady = false;
	};

	FWSActionRequest MakeTalkRequest(const TCHAR* ActionId = TEXT("talk_gu_heng"))
	{
		FWSActionRequest Request;
		Request.ActionId = ActionId;
		Request.TransactionId = FGuid::NewGuid();
		Request.DialogueSessionId = FGuid::NewGuid();
		Request.DialogueAct = EWSDialogueAct::Ask;
		Request.PlayerSaid = TEXT("你现在怎么样？");
		Request.SemanticFrame.SpeechAct = EWSDialogueAct::Ask;
		Request.SemanticFrame.QueryType = EWSDialogueQueryType::Status;
		Request.SemanticFrame.TargetCharacter =
			Request.ActionId == TEXT("talk_ye_cheng")
				? EWSCharacterId::YeCheng
				: EWSCharacterId::GuHeng;
		Request.SemanticFrame.Confidence = 1.0f;
		Request.SemanticFrame.Source = TEXT("automation_test");
		return Request;
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

	bool DialogueVisibleStateMatches(
		const FWSGameState& Left,
		const FWSGameState& Right)
	{
		const FWSCharacterState LeftGu =
			Left.Characters.FindRef(EWSCharacterId::GuHeng);
		const FWSCharacterState RightGu =
			Right.Characters.FindRef(EWSCharacterId::GuHeng);
		const FWSCharacterState LeftYe =
			Left.Characters.FindRef(EWSCharacterId::YeCheng);
		const FWSCharacterState RightYe =
			Right.Characters.FindRef(EWSCharacterId::YeCheng);
		return Left.ActionPoints == Right.ActionPoints
			&& Left.PhaseActionPoints == Right.PhaseActionPoints
			&& Left.ModelCalls == Right.ModelCalls
			&& Left.EventLog.Num() == Right.EventLog.Num()
			&& Left.CommittedTransactions.Num() == Right.CommittedTransactions.Num()
			&& KnowledgeMatches(Left.PlayerKnowledge, Right.PlayerKnowledge)
			&& Left.Resources.Fuel == Right.Resources.Fuel
			&& Left.Resources.Food == Right.Resources.Food
			&& Left.Resources.Medicine == Right.Resources.Medicine
			&& Left.Resources.HeatPack == Right.Resources.HeatPack
			&& Left.Resources.ReplacementRelay == Right.Resources.ReplacementRelay
			&& Left.Flags.bHeatPackRevealed == Right.Flags.bHeatPackRevealed
			&& Left.Flags.bGuHengDiagnosed == Right.Flags.bGuHengDiagnosed
			&& Left.Flags.bRelayCompatibilityKnown == Right.Flags.bRelayCompatibilityKnown
			&& LeftGu.Trust == RightGu.Trust
			&& LeftGu.Pressure == RightGu.Pressure
			&& LeftYe.Trust == RightYe.Trust
			&& LeftYe.Pressure == RightYe.Pressure;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueSynchronousFallbackTransactionTest,
	"WhiteoutStation.Dialogue.Transaction.SynchronousFallbackExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueSynchronousFallbackTransactionTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueTransactionTests;
	FScopedStateSubsystem Fixture;
	UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
	if (!TestNotNull(TEXT("State subsystem initializes"), StateSubsystem))
	{
		return false;
	}
	if (!TestTrue(TEXT("Day phase starts for dialogue"), Fixture.IsReady()))
	{
		return false;
	}

	StateSubsystem->SetDialogueRealizeTestHook(
		[](const FWSPreparedDialogue& Prepared, FWSDialogueRealizeTestCallback Reply)
		{
			Reply(Prepared.LocalFallback);
		});
	const FWSGameState Before = StateSubsystem->GetStateSnapshot();
	const FWSActionRequest Request = MakeTalkRequest();
	int32 CompletionCount = 0;
	FWSActionResult CompletionResult;
	const FWSActionResult Returned = StateSubsystem->SubmitDialogueAction(
		Request,
		[&CompletionCount, &CompletionResult](const FWSActionResult& Result)
		{
			++CompletionCount;
			CompletionResult = Result;
		});
	const FWSGameState After = StateSubsystem->GetStateSnapshot();

	TestTrue(TEXT("Synchronous fallback returns the final commit"), Returned.bCommitted);
	TestFalse(TEXT("Synchronous fallback does not return pending"), Returned.bPendingDialogue);
	TestEqual(TEXT("Returned transaction stays on the submission"), Returned.TransactionId, Request.TransactionId);
	TestEqual(TEXT("Completion runs once"), CompletionCount, 1);
	TestTrue(TEXT("Completion observes the commit"), CompletionResult.bCommitted);
	TestFalse(TEXT("Pending state is cleared"), StateSubsystem->HasPendingDialogue());
	TestEqual(TEXT("Exactly one event is appended"), After.EventLog.Num(), Before.EventLog.Num() + 1);
	TestTrue(TEXT("Transaction is recorded once"), After.CommittedTransactions.Contains(Request.TransactionId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueDelayedAtomicCommitTest,
	"WhiteoutStation.Dialogue.Transaction.DelayedAtomicCommitAndDuplicateReply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueDelayedAtomicCommitTest::RunTest(const FString& Parameters)
{
	using namespace WhiteoutDialogueTransactionTests;
	FScopedStateSubsystem Fixture;
	UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
	if (!TestNotNull(TEXT("State subsystem initializes"), StateSubsystem))
	{
		return false;
	}
	if (!TestTrue(TEXT("Day phase starts for dialogue"), Fixture.IsReady()))
	{
		return false;
	}

	FWSAgentReply DeferredReply;
	FWSDialogueRealizeTestCallback DeferredCallback;
	int32 RealizeHookCount = 0;
	StateSubsystem->SetDialogueRealizeTestHook(
		[&DeferredReply, &DeferredCallback, &RealizeHookCount](
			const FWSPreparedDialogue& Prepared,
			FWSDialogueRealizeTestCallback Reply)
		{
			++RealizeHookCount;
			DeferredReply = Prepared.LocalFallback;
			DeferredCallback = MoveTemp(Reply);
		});
	const FWSGameState Before = StateSubsystem->GetStateSnapshot();
	const FWSActionRequest Request = MakeTalkRequest();
	int32 CompletionCount = 0;
	FWSActionResult CompletionResult;
	const FWSActionResult PendingResult = StateSubsystem->SubmitDialogueAction(
		Request,
		[&CompletionCount, &CompletionResult](const FWSActionResult& Result)
		{
			++CompletionCount;
			CompletionResult = Result;
		});
	const FWSGameState WhilePending = StateSubsystem->GetStateSnapshot();

	TestTrue(TEXT("Delayed realization returns pending"), PendingResult.bPendingDialogue);
	TestFalse(TEXT("Delayed realization has not committed"), PendingResult.bCommitted);
	TestTrue(TEXT("Subsystem owns one pending dialogue"), StateSubsystem->HasPendingDialogue());
	TestEqual(TEXT("Completion waits for realization"), CompletionCount, 0);
	TestTrue(
		TEXT("AP, knowledge, relations, and events remain unchanged while pending"),
		DialogueVisibleStateMatches(Before, WhilePending));
	int32 RejectedCompletionCount = 0;
	const FWSActionResult RejectedWhilePending =
		StateSubsystem->SubmitDialogueAction(
			MakeTalkRequest(TEXT("talk_ye_cheng")),
			[&RejectedCompletionCount](const FWSActionResult&)
			{
				++RejectedCompletionCount;
			});
	TestFalse(TEXT("A second talk cannot commit while pending"), RejectedWhilePending.bCommitted);
	TestFalse(TEXT("A second talk does not replace the pending owner"), RejectedWhilePending.bPendingDialogue);
	TestEqual(TEXT("A second talk reports the pending gate"), RejectedWhilePending.ReasonCode, EWSReasonCode::DialoguePending);
	TestEqual(TEXT("A rejected second talk completes synchronously once"), RejectedCompletionCount, 1);
	TestEqual(TEXT("Only the first talk reaches realization"), RealizeHookCount, 1);
	if (!TestTrue(TEXT("Deferred callback is captured"), static_cast<bool>(DeferredCallback)))
	{
		StateSubsystem->CancelPendingDialogue();
		return false;
	}

	DeferredCallback(DeferredReply);
	const FWSGameState AfterFirstReply = StateSubsystem->GetStateSnapshot();
	TestFalse(TEXT("Successful reply clears pending"), StateSubsystem->HasPendingDialogue());
	TestEqual(TEXT("Successful reply completes once"), CompletionCount, 1);
	TestTrue(TEXT("Successful reply commits"), CompletionResult.bCommitted);
	TestEqual(TEXT("Successful reply appends one event"), AfterFirstReply.EventLog.Num(), Before.EventLog.Num() + 1);
	TestEqual(
		TEXT("Successful reply consumes AP exactly once"),
		AfterFirstReply.PhaseActionPoints,
		Before.PhaseActionPoints - CompletionResult.ActualAP);

	DeferredCallback(DeferredReply);
	const FWSGameState AfterDuplicateReply = StateSubsystem->GetStateSnapshot();
	TestEqual(TEXT("Duplicate callback does not complete twice"), CompletionCount, 1);
	TestEqual(TEXT("Duplicate callback does not append an event"), AfterDuplicateReply.EventLog.Num(), AfterFirstReply.EventLog.Num());
	TestEqual(TEXT("Duplicate callback does not consume AP"), AfterDuplicateReply.PhaseActionPoints, AfterFirstReply.PhaseActionPoints);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueCancellationAndStaleReplyTest,
	"WhiteoutStation.Dialogue.Transaction.CancelAndRevisionRejectLateReply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueCancellationAndStaleReplyTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueTransactionTests;
	{
		FScopedStateSubsystem Fixture;
		UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
		if (!TestNotNull(TEXT("Cancellation subsystem initializes"), StateSubsystem))
		{
			return false;
		}
		if (!TestTrue(TEXT("Cancellation day phase starts"), Fixture.IsReady()))
		{
			return false;
		}

		FWSAgentReply DeferredReply;
		FWSDialogueRealizeTestCallback DeferredCallback;
		StateSubsystem->SetDialogueRealizeTestHook(
			[&DeferredReply, &DeferredCallback](
				const FWSPreparedDialogue& Prepared,
				FWSDialogueRealizeTestCallback Reply)
			{
				DeferredReply = Prepared.LocalFallback;
				DeferredCallback = MoveTemp(Reply);
			});
		const FWSGameState Before = StateSubsystem->GetStateSnapshot();
		int32 CompletionCount = 0;
		FWSActionResult CompletionResult;
		const FWSActionResult PendingResult = StateSubsystem->SubmitDialogueAction(
			MakeTalkRequest(),
			[&CompletionCount, &CompletionResult](const FWSActionResult& Result)
			{
				++CompletionCount;
				CompletionResult = Result;
			});
		TestTrue(TEXT("Cancellation case enters pending"), PendingResult.bPendingDialogue);
		StateSubsystem->CancelPendingDialogue();
		const FWSGameState AfterCancel = StateSubsystem->GetStateSnapshot();
		TestEqual(TEXT("Cancellation completes once"), CompletionCount, 1);
		TestEqual(TEXT("Cancellation reports its reason"), CompletionResult.ReasonCode, EWSReasonCode::DialogueCancelled);
		TestFalse(TEXT("Cancellation clears pending"), StateSubsystem->HasPendingDialogue());
		TestTrue(TEXT("Cancellation leaves dialogue-visible state unchanged"), DialogueVisibleStateMatches(Before, AfterCancel));
		if (DeferredCallback)
		{
			DeferredCallback(DeferredReply);
		}
		const FWSGameState AfterLateReply = StateSubsystem->GetStateSnapshot();
		TestEqual(TEXT("Late cancelled reply cannot complete twice"), CompletionCount, 1);
		TestTrue(TEXT("Late cancelled reply cannot mutate state"), DialogueVisibleStateMatches(AfterCancel, AfterLateReply));
	}

	{
		FScopedStateSubsystem Fixture;
		UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
		if (!TestNotNull(TEXT("Revision subsystem initializes"), StateSubsystem))
		{
			return false;
		}
		if (!TestTrue(TEXT("Revision day phase starts"), Fixture.IsReady()))
		{
			return false;
		}

		FWSAgentReply DeferredReply;
		FWSDialogueRealizeTestCallback DeferredCallback;
		StateSubsystem->SetDialogueRealizeTestHook(
			[&DeferredReply, &DeferredCallback](
				const FWSPreparedDialogue& Prepared,
				FWSDialogueRealizeTestCallback Reply)
			{
				DeferredReply = Prepared.LocalFallback;
				DeferredCallback = MoveTemp(Reply);
			});
		int32 CompletionCount = 0;
		FWSActionResult CompletionResult;
		const FWSActionResult PendingResult = StateSubsystem->SubmitDialogueAction(
			MakeTalkRequest(),
			[&CompletionCount, &CompletionResult](const FWSActionResult& Result)
			{
				++CompletionCount;
				CompletionResult = Result;
			});
		TestTrue(TEXT("Revision case enters pending"), PendingResult.bPendingDialogue);
		if (!TestTrue(
			TEXT("Revision case captures a delayed callback"),
			static_cast<bool>(DeferredCallback)))
		{
			StateSubsystem->CancelPendingDialogue();
			return false;
		}
		if (!TestTrue(
			TEXT("A real state mutation advances the revision"),
			StateSubsystem->SetRequirementPinned(
				TEXT("repair_generator"),
				true)))
		{
			StateSubsystem->CancelPendingDialogue();
			return false;
		}
		const FWSGameState AfterRevisionChange = StateSubsystem->GetStateSnapshot();
		DeferredCallback(DeferredReply);
		const FWSGameState AfterLateReply = StateSubsystem->GetStateSnapshot();
		TestEqual(TEXT("Stale reply completes the submission once"), CompletionCount, 1);
		TestEqual(TEXT("Stale reply reports revision change"), CompletionResult.ReasonCode, EWSReasonCode::DialogueStateChanged);
		TestFalse(TEXT("Stale reply clears pending"), StateSubsystem->HasPendingDialogue());
		TestTrue(TEXT("Stale reply cannot add dialogue mutations"), DialogueVisibleStateMatches(AfterRevisionChange, AfterLateReply));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueCompletionReentryTest,
	"WhiteoutStation.Dialogue.Transaction.CompletionReentryPreservesOuterResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueCompletionReentryTest::RunTest(const FString& Parameters)
{
	using namespace WhiteoutDialogueTransactionTests;
	FScopedStateSubsystem Fixture;
	UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
	if (!TestNotNull(TEXT("State subsystem initializes"), StateSubsystem))
	{
		return false;
	}
	if (!TestTrue(TEXT("Day phase starts for dialogue"), Fixture.IsReady()))
	{
		return false;
	}
	StateSubsystem->SetDialogueRealizeTestHook(
		[](const FWSPreparedDialogue& Prepared, FWSDialogueRealizeTestCallback Reply)
		{
			Reply(Prepared.LocalFallback);
		});

	const FWSGameState Before = StateSubsystem->GetStateSnapshot();
	const FWSActionRequest OuterRequest = MakeTalkRequest();
	const FWSActionRequest InnerRequest = MakeTalkRequest(TEXT("talk_ye_cheng"));
	int32 OuterCompletionCount = 0;
	int32 InnerCompletionCount = 0;
	FWSActionResult InnerReturned;
	const FWSActionResult OuterReturned = StateSubsystem->SubmitDialogueAction(
		OuterRequest,
		[&](const FWSActionResult& OuterCompletion)
		{
			++OuterCompletionCount;
			if (OuterCompletion.bCommitted && OuterCompletionCount == 1)
			{
				InnerReturned = StateSubsystem->SubmitDialogueAction(
					InnerRequest,
					[&InnerCompletionCount](const FWSActionResult& InnerCompletion)
					{
						if (InnerCompletion.bCommitted)
						{
							++InnerCompletionCount;
						}
					});
			}
		});
	const FWSGameState After = StateSubsystem->GetStateSnapshot();

	TestTrue(TEXT("Outer submission commits"), OuterReturned.bCommitted);
	TestEqual(TEXT("Outer return keeps its own transaction"), OuterReturned.TransactionId, OuterRequest.TransactionId);
	TestEqual(TEXT("Outer completion runs once"), OuterCompletionCount, 1);
	TestTrue(TEXT("Reentrant inner submission commits"), InnerReturned.bCommitted);
	TestEqual(TEXT("Inner return keeps its own transaction"), InnerReturned.TransactionId, InnerRequest.TransactionId);
	TestEqual(TEXT("Inner completion runs once"), InnerCompletionCount, 1);
	TestFalse(TEXT("Reentry leaves no pending dialogue"), StateSubsystem->HasPendingDialogue());
	TestEqual(TEXT("Reentry appends two committed events"), After.EventLog.Num(), Before.EventLog.Num() + 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueCommitDispatchReentryTest,
	"WhiteoutStation.Dialogue.Transaction.CommitDispatchReentryKeepsLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueCommitDispatchReentryTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueTransactionTests;
	FScopedStateSubsystem Fixture;
	UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
	if (!TestNotNull(TEXT("State subsystem initializes"), StateSubsystem)
		|| !TestTrue(TEXT("Day phase starts for dialogue"), Fixture.IsReady()))
	{
		return false;
	}
	StateSubsystem->SetDialogueRealizeTestHook(
		[](const FWSPreparedDialogue& Prepared, FWSDialogueRealizeTestCallback Reply)
		{
			Reply(Prepared.LocalFallback);
		});
	const int32 LinesBefore =
		StateSubsystem->GetDialogueLineBroadcastCountForTest();
	int32 DispatchReentryCount = 0;
	int32 LinesObservedDuringDispatch = -1;
	bool bDispatchMutationSucceeded = true;
	StateSubsystem->SetDialogueCommitDispatchTestHook(
		[StateSubsystem,
			&DispatchReentryCount,
			&LinesObservedDuringDispatch,
			&bDispatchMutationSucceeded]()
		{
			++DispatchReentryCount;
			LinesObservedDuringDispatch =
				StateSubsystem->GetDialogueLineBroadcastCountForTest();
			bDispatchMutationSucceeded = StateSubsystem->SetRequirementPinned(
				TEXT("repair_generator"),
				true);
		});
	const FWSActionResult Result = StateSubsystem->SubmitDialogueAction(
		MakeTalkRequest());

	TestTrue(TEXT("Dialogue commits before dispatch reentry"), Result.bCommitted);
	TestEqual(TEXT("Commit dispatch reentry runs once"), DispatchReentryCount, 1);
	TestEqual(
		TEXT("Action commit dispatch precedes the dialogue line"),
		LinesObservedDuringDispatch,
		LinesBefore);
	TestEqual(
		TEXT("Committed line is broadcast exactly once despite revision reentry"),
		StateSubsystem->GetDialogueLineBroadcastCountForTest(),
		LinesBefore + 1);
	TestFalse(
		TEXT("Commit dispatch rejects nested state mutation"),
		bDispatchMutationSucceeded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueLifecycleTransitionReentryTest,
	"WhiteoutStation.Dialogue.Transaction.EndGameRejectsReentrantSubmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueLifecycleTransitionReentryTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueTransactionTests;
	FScopedStateSubsystem Fixture;
	UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
	if (!TestNotNull(TEXT("State subsystem initializes"), StateSubsystem)
		|| !TestTrue(TEXT("Day phase starts for dialogue"), Fixture.IsReady()))
	{
		return false;
	}

	FWSAgentReply DeferredReply;
	FWSDialogueRealizeTestCallback DeferredCallback;
	StateSubsystem->SetDialogueRealizeTestHook(
		[&DeferredReply, &DeferredCallback](
			const FWSPreparedDialogue& Prepared,
			FWSDialogueRealizeTestCallback Reply)
		{
			DeferredReply = Prepared.LocalFallback;
			DeferredCallback = MoveTemp(Reply);
		});
	int32 CancelCompletionCount = 0;
	int32 ReentrantCompletionCount = 0;
	FWSActionResult ReentrantResult;
	bool bReentrantPinSucceeded = true;
	FWSActionResult ReentrantActionResult;
	const FWSActionResult PendingResult = StateSubsystem->SubmitDialogueAction(
		MakeTalkRequest(),
		[&](const FWSActionResult& Result)
		{
			++CancelCompletionCount;
			if (Result.ReasonCode == EWSReasonCode::DialogueCancelled)
			{
				bReentrantPinSucceeded = StateSubsystem->SetRequirementPinned(
					TEXT("repair_generator"),
					true);
				ReentrantActionResult = StateSubsystem->CommitAction(
					MakeTalkRequest(TEXT("investigate_generator_log")));
				ReentrantResult = StateSubsystem->SubmitDialogueAction(
					MakeTalkRequest(TEXT("talk_ye_cheng")),
					[&ReentrantCompletionCount](const FWSActionResult&)
					{
						++ReentrantCompletionCount;
					});
			}
		});
	TestTrue(TEXT("Dialogue enters pending before end game"), PendingResult.bPendingDialogue);
	const FWSGameState Results = StateSubsystem->EndGame();
	const FWSGameState AfterEndGame = StateSubsystem->GetStateSnapshot();

	TestEqual(TEXT("End game cancels the original completion once"), CancelCompletionCount, 1);
	TestEqual(TEXT("Reentrant rejection completes once"), ReentrantCompletionCount, 1);
	TestFalse(
		TEXT("Lifecycle transition rejects reentrant requirement mutation"),
		bReentrantPinSucceeded);
	TestFalse(
		TEXT("Lifecycle transition rejects reentrant non-dialogue action"),
		ReentrantActionResult.bCommitted);
	TestEqual(
		TEXT("Reentrant non-dialogue action reports cancellation"),
		ReentrantActionResult.ReasonCode,
		EWSReasonCode::DialogueCancelled);
	TestEqual(
		TEXT("Reentrant dialogue is rejected during lifecycle transition"),
		ReentrantResult.ReasonCode,
		EWSReasonCode::DialogueCancelled);
	TestFalse(TEXT("End game leaves no pending dialogue"), StateSubsystem->HasPendingDialogue());
	TestEqual(TEXT("End game reaches results"), Results.Phase, EWSGamePhase::Results);
	if (DeferredCallback)
	{
		DeferredCallback(DeferredReply);
	}
	const FWSGameState AfterLateReply = StateSubsystem->GetStateSnapshot();
	TestTrue(
		TEXT("Late reply after end game cannot mutate dialogue state"),
		DialogueVisibleStateMatches(AfterEndGame, AfterLateReply));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueLoadLifecycleTest,
	"WhiteoutStation.Dialogue.Transaction.LoadFailurePreservesPendingAndSuccessCancels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueLoadLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace WhiteoutDialogueTransactionTests;
	{
		FScopedStateSubsystem Fixture;
		UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
		if (!TestNotNull(TEXT("Failed-load subsystem initializes"), StateSubsystem)
			|| !TestTrue(TEXT("Failed-load day phase starts"), Fixture.IsReady()))
		{
			return false;
		}
		FWSAgentReply DeferredReply;
		FWSDialogueRealizeTestCallback DeferredCallback;
		StateSubsystem->SetDialogueRealizeTestHook(
			[&](const FWSPreparedDialogue& Prepared, FWSDialogueRealizeTestCallback Reply)
			{
				DeferredReply = Prepared.LocalFallback;
				DeferredCallback = MoveTemp(Reply);
			});
		int32 CompletionCount = 0;
		FWSActionResult CompletionResult;
		const FWSActionResult PendingResult = StateSubsystem->SubmitDialogueAction(
			MakeTalkRequest(),
			[&](const FWSActionResult& Result)
			{
				++CompletionCount;
				CompletionResult = Result;
			});
		TestTrue(TEXT("Failed-load case enters pending"), PendingResult.bPendingDialogue);
		UGameplayStatics::DeleteGameInSlot(Fixture.GetSaveSlot(), 0);
		TestFalse(TEXT("Missing snapshot fails to load"), StateSubsystem->LoadSnapshot());
		TestTrue(TEXT("Failed load preserves the pending owner"), StateSubsystem->HasPendingDialogue());
		TestEqual(TEXT("Failed load does not consume completion"), CompletionCount, 0);
		if (!TestTrue(TEXT("Failed-load callback remains available"), static_cast<bool>(DeferredCallback)))
		{
			StateSubsystem->CancelPendingDialogue();
			return false;
		}
		DeferredCallback(DeferredReply);
		TestEqual(TEXT("Preserved callback completes once"), CompletionCount, 1);
		TestTrue(TEXT("Preserved transaction can still commit"), CompletionResult.bCommitted);
	}

	{
		FScopedStateSubsystem Fixture;
		UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
		if (!TestNotNull(TEXT("Successful-load subsystem initializes"), StateSubsystem)
			|| !TestTrue(TEXT("Successful-load day phase starts"), Fixture.IsReady()))
		{
			return false;
		}
		FWSAgentReply DeferredReply;
		FWSDialogueRealizeTestCallback DeferredCallback;
		StateSubsystem->SetDialogueRealizeTestHook(
			[&](const FWSPreparedDialogue& Prepared, FWSDialogueRealizeTestCallback Reply)
			{
				DeferredReply = Prepared.LocalFallback;
				DeferredCallback = MoveTemp(Reply);
			});
		int32 CompletionCount = 0;
		FWSActionResult CompletionResult;
		const FWSActionResult PendingResult = StateSubsystem->SubmitDialogueAction(
			MakeTalkRequest(),
			[&](const FWSActionResult& Result)
			{
				++CompletionCount;
				CompletionResult = Result;
			});
		TestTrue(TEXT("Successful-load case enters pending"), PendingResult.bPendingDialogue);
		TestTrue(TEXT("Saved snapshot loads"), StateSubsystem->LoadSnapshot());
		TestEqual(TEXT("Successful load cancels completion once"), CompletionCount, 1);
		TestEqual(TEXT("Successful load reports cancellation"), CompletionResult.ReasonCode, EWSReasonCode::DialogueCancelled);
		TestFalse(TEXT("Successful load clears pending"), StateSubsystem->HasPendingDialogue());
		const FWSGameState AfterLoad = StateSubsystem->GetStateSnapshot();
		if (DeferredCallback)
		{
			DeferredCallback(DeferredReply);
		}
		const FWSGameState AfterLateReply = StateSubsystem->GetStateSnapshot();
		TestEqual(TEXT("Late loaded reply cannot complete twice"), CompletionCount, 1);
		TestTrue(
			TEXT("Late loaded reply cannot mutate dialogue state"),
			DialogueVisibleStateMatches(AfterLoad, AfterLateReply));
	}
	return true;
}

#endif
