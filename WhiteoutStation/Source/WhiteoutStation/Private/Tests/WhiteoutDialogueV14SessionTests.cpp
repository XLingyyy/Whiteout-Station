#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Agents/WSAgentGateway.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "State/WindStationStateSubsystem.h"

namespace WhiteoutDialogueV14SessionTests
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
				TEXT("WhiteoutStation_V14Session_%s"),
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

	private:
		UGameInstance* GameInstance = nullptr;
		UWindStationStateSubsystem* StateSubsystem = nullptr;
		FString SaveSlot;
		bool bReady = false;
	};

	FWSActionRequest MakeTalkRequest(
		const FGuid& DialogueSessionId,
		const TCHAR* PlayerSaid)
	{
		FWSActionRequest Request;
		Request.ActionId = TEXT("talk_gu_heng");
		Request.TransactionId = FGuid::NewGuid();
		Request.DialogueSessionId = DialogueSessionId;
		Request.DialogueAct = EWSDialogueAct::Ask;
		Request.PlayerSaid = PlayerSaid;
		Request.SemanticFrame.SpeechAct = EWSDialogueAct::Ask;
		Request.SemanticFrame.QueryType = EWSDialogueQueryType::Status;
		Request.SemanticFrame.TargetCharacter = EWSCharacterId::GuHeng;
		Request.SemanticFrame.Confidence = 1.0f;
		Request.SemanticFrame.Source = TEXT("automation_test");
		return Request;
	}

	bool MemoryMatches(
		const FWSRoleplayMemoryEntry& Left,
		const FWSRoleplayMemoryEntry& Right)
	{
		return Left.MemoryId == Right.MemoryId
			&& Left.Owner == Right.Owner
			&& Left.TopicId == Right.TopicId
			&& Left.ClaimMode == Right.ClaimMode
			&& Left.KnowledgeIds == Right.KnowledgeIds
			&& Left.SafeSummary == Right.SafeSummary
			&& FMath::IsNearlyEqual(Left.Importance, Right.Importance)
			&& Left.TurnIndex == Right.TurnIndex
			&& Left.bPublic == Right.bPublic;
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

	bool ActionCountsMatch(
		const TMap<FName, int32>& Left,
		const TMap<FName, int32>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (const TPair<FName, int32>& Pair : Left)
		{
			const int32* Other = Right.Find(Pair.Key);
			if (!Other || *Other != Pair.Value)
			{
				return false;
			}
		}
		return true;
	}

	bool CharacterStateMatches(
		const FWSCharacterState& Left,
		const FWSCharacterState& Right)
	{
		return FMath::IsNearlyEqual(Left.Health, Right.Health)
			&& FMath::IsNearlyEqual(Left.Temperature, Right.Temperature)
			&& FMath::IsNearlyEqual(Left.Hunger, Right.Hunger)
			&& FMath::IsNearlyEqual(Left.Fatigue, Right.Fatigue)
			&& FMath::IsNearlyEqual(Left.Pressure, Right.Pressure)
			&& FMath::IsNearlyEqual(Left.Trust, Right.Trust)
			&& Left.Stamina == Right.Stamina
			&& Left.InjurySeverity == Right.InjurySeverity
			&& Left.InjuryId == Right.InjuryId
			&& Left.InjuryWorseningMarks == Right.InjuryWorseningMarks
			&& Left.BandageProtection == Right.BandageProtection
			&& Left.TemporarySupportUses == Right.TemporarySupportUses
			&& Left.TemporarySupportPhase == Right.TemporarySupportPhase
			&& Left.Location == Right.Location;
	}

	bool AllCharacterStatesMatch(
		const FWSGameState& Left,
		const FWSGameState& Right)
	{
		for (const EWSCharacterId CharacterId : {
				EWSCharacterId::Player,
				EWSCharacterId::GuHeng,
				EWSCharacterId::YeCheng})
		{
			if (!CharacterStateMatches(
					Left.Characters.FindRef(CharacterId),
					Right.Characters.FindRef(CharacterId)))
			{
				return false;
			}
		}
		return true;
	}

	bool ResourcesMatch(
		const FWSResourceState& Left,
		const FWSResourceState& Right)
	{
		return Left.Fuel == Right.Fuel
			&& Left.Food == Right.Food
			&& Left.Medicine == Right.Medicine
			&& Left.HeatPack == Right.HeatPack
			&& Left.ReplacementRelay == Right.ReplacementRelay;
	}

	bool TasksMatch(
		const FWSTaskState& Left,
		const FWSTaskState& Right)
	{
		return Left.GeneratorProgress == Right.GeneratorProgress
			&& Left.AntennaCalibration == Right.AntennaCalibration
			&& Left.bSignalSent == Right.bSignalSent
			&& Left.bGeneratorStable == Right.bGeneratorStable;
	}

	bool WorldFlagsMatch(
		const FWSWorldFlags& Left,
		const FWSWorldFlags& Right)
	{
		return Left.bKitchenHeaterIntact == Right.bKitchenHeaterIntact
			&& Left.bHeatPackRevealed == Right.bHeatPackRevealed
			&& Left.bRepairRoomHeated == Right.bRepairRoomHeated
			&& Left.bMedicalRoomHeated == Right.bMedicalRoomHeated
			&& Left.bGuHengDiagnosed == Right.bGuHengDiagnosed
			&& Left.bGuHengTreated == Right.bGuHengTreated
			&& Left.bGuHengFed == Right.bGuHengFed
			&& Left.bGuHengCooperative == Right.bGuHengCooperative
			&& Left.bRelayCompatibilityKnown == Right.bRelayCompatibilityKnown
			&& Left.bRelayInstalled == Right.bRelayInstalled
			&& Left.bSelfRepairUsed == Right.bSelfRepairUsed
			&& Left.bRecordsPreserved == Right.bRecordsPreserved
			&& Left.bPlayerFed == Right.bPlayerFed
			&& Left.bYeChengFed == Right.bYeChengFed
			&& Left.bCabinetInspected == Right.bCabinetInspected
			&& Left.bLogPenaltyActive == Right.bLogPenaltyActive
			&& Left.ForcedActionCount == Right.ForcedActionCount
			&& Left.RiskyRepairCount == Right.RiskyRepairCount;
	}

	FWSDialogueOutcome MakeOutcome(const FWSAgentReply& Reply)
	{
		FWSDialogueOutcome Outcome;
		Outcome.FinalReply = Reply;
		Outcome.DisclosedFactIds = Reply.DisclosedFactIds;
		Outcome.RealizedAtomIds = Reply.RealizedAtomIds;
		Outcome.AnswerSource = Reply.AnswerSource;
		return Outcome;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueV14ThreeTurnSessionTest,
	"WhiteoutStation.Dialogue.V14.Session.ThreeTurnEconomyRelationshipAndMemory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueV14ThreeTurnSessionTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueV14SessionTests;
	FScopedStateSubsystem Fixture;
	UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
	if (!TestNotNull(TEXT("State subsystem initializes"), StateSubsystem)
		|| !TestTrue(TEXT("Day phase starts for dialogue"), Fixture.IsReady()))
	{
		return false;
	}

	int32 RealizeCount = 0;
	TArray<int32> PreparedTurns;
	TArray<bool> PreparedFollowUpFlags;
	StateSubsystem->SetDialogueRealizeTestHook(
		[&](const FWSPreparedDialogue& Prepared, FWSDialogueRealizeTestCallback Reply)
		{
			++RealizeCount;
			PreparedTurns.Add(Prepared.OriginalRequest.DialogueTurnIndex);
			PreparedFollowUpFlags.Add(
				Prepared.OriginalRequest.bDialogueSessionFollowUp);
			Reply(Prepared.LocalFallback);
		});

	const FGuid DialogueSessionId = FGuid::NewGuid();
	const FWSGameState Before = StateSubsystem->GetStateSnapshot();
	const FWSCharacterState BeforeGu =
		Before.Characters.FindRef(EWSCharacterId::GuHeng);
	const int32 InitialMemoryCount = Before.DialogueMemories.Num();

	const FWSActionResult First = StateSubsystem->SubmitDialogueAction(
		MakeTalkRequest(DialogueSessionId, TEXT("你现在怎么样？")));
	const FWSGameState AfterFirst = StateSubsystem->GetStateSnapshot();
	const FWSCharacterState AfterFirstGu =
		AfterFirst.Characters.FindRef(EWSCharacterId::GuHeng);
	TestTrue(TEXT("First turn commits"), First.bCommitted);
	TestEqual(TEXT("First turn costs one AP"), First.ActualAP, 1);
	TestEqual(
		TEXT("First turn deducts one phase AP"),
		AfterFirst.PhaseActionPoints,
		Before.PhaseActionPoints - 1);
	TestTrue(
		TEXT("First turn settles the relationship"),
		!FMath::IsNearlyEqual(AfterFirstGu.Trust, BeforeGu.Trust)
			|| !FMath::IsNearlyEqual(AfterFirstGu.Pressure, BeforeGu.Pressure));
	TestTrue(
		TEXT("Session can continue after the first turn"),
		StateSubsystem->CanContinueDialogueSession(DialogueSessionId));

	const FWSActionResult Second = StateSubsystem->SubmitDialogueAction(
		MakeTalkRequest(DialogueSessionId, TEXT("那发电机现在是什么情况？")));
	const FWSGameState AfterSecond = StateSubsystem->GetStateSnapshot();
	const FWSCharacterState AfterSecondGu =
		AfterSecond.Characters.FindRef(EWSCharacterId::GuHeng);
	TestTrue(TEXT("Second turn commits"), Second.bCommitted);
	TestEqual(TEXT("Second turn is free"), Second.ActualAP, 0);
	TestEqual(
		TEXT("Second turn preserves phase AP"),
		AfterSecond.PhaseActionPoints,
		AfterFirst.PhaseActionPoints);
	TestTrue(
		TEXT("Second turn does not settle trust again"),
		FMath::IsNearlyEqual(AfterSecondGu.Trust, AfterFirstGu.Trust));
	TestTrue(
		TEXT("Second turn does not settle pressure again"),
		FMath::IsNearlyEqual(AfterSecondGu.Pressure, AfterFirstGu.Pressure));
	TestTrue(
		TEXT("Session can continue after the second turn"),
		StateSubsystem->CanContinueDialogueSession(DialogueSessionId));

	const FWSActionResult Third = StateSubsystem->SubmitDialogueAction(
		MakeTalkRequest(DialogueSessionId, TEXT("你希望我接下来做什么？")));
	const FWSGameState AfterThird = StateSubsystem->GetStateSnapshot();
	const FWSCharacterState AfterThirdGu =
		AfterThird.Characters.FindRef(EWSCharacterId::GuHeng);
	TestTrue(TEXT("Third turn commits"), Third.bCommitted);
	TestEqual(TEXT("Third turn is free"), Third.ActualAP, 0);
	TestEqual(
		TEXT("Third turn preserves phase AP"),
		AfterThird.PhaseActionPoints,
		AfterFirst.PhaseActionPoints);
	TestTrue(
		TEXT("Third turn does not settle trust again"),
		FMath::IsNearlyEqual(AfterThirdGu.Trust, AfterFirstGu.Trust));
	TestTrue(
		TEXT("Third turn does not settle pressure again"),
		FMath::IsNearlyEqual(AfterThirdGu.Pressure, AfterFirstGu.Pressure));
	TestEqual(
		TEXT("Dialogue use count advances once per session"),
		AfterThird.ActionCounts.FindRef(TEXT("talk_gu_heng")),
		Before.ActionCounts.FindRef(TEXT("talk_gu_heng")) + 1);
	TestFalse(
		TEXT("Session closes after the third turn"),
		StateSubsystem->CanContinueDialogueSession(DialogueSessionId));

	TestEqual(TEXT("Three turns reach realization"), RealizeCount, 3);
	TestEqual(TEXT("Prepared turn count"), PreparedTurns.Num(), 3);
	if (PreparedTurns.Num() == 3 && PreparedFollowUpFlags.Num() == 3)
	{
		TestEqual(TEXT("First prepared turn index"), PreparedTurns[0], 1);
		TestEqual(TEXT("Second prepared turn index"), PreparedTurns[1], 2);
		TestEqual(TEXT("Third prepared turn index"), PreparedTurns[2], 3);
		TestFalse(TEXT("First turn is not a follow-up"), PreparedFollowUpFlags[0]);
		TestTrue(TEXT("Second turn is a follow-up"), PreparedFollowUpFlags[1]);
		TestTrue(TEXT("Third turn is a follow-up"), PreparedFollowUpFlags[2]);
	}

	const int32 APBeforeFourth = AfterThird.PhaseActionPoints;
	const int32 MemoriesBeforeFourth = AfterThird.DialogueMemories.Num();
	const FWSActionResult Fourth = StateSubsystem->SubmitDialogueAction(
		MakeTalkRequest(DialogueSessionId, TEXT("再说一句。")));
	const FWSGameState AfterFourth = StateSubsystem->GetStateSnapshot();
	TestFalse(TEXT("Fourth turn cannot commit"), Fourth.bCommitted);
	TestFalse(TEXT("Fourth turn does not enter pending"), Fourth.bPendingDialogue);
	TestEqual(
		TEXT("Fourth turn reports a completed session"),
		Fourth.ReasonCode,
		EWSReasonCode::DialogueSessionComplete);
	TestEqual(TEXT("Fourth turn does not realize"), RealizeCount, 3);
	TestEqual(
		TEXT("Fourth turn does not consume AP"),
		AfterFourth.PhaseActionPoints,
		APBeforeFourth);
	TestEqual(
		TEXT("Fourth turn does not append memory"),
		AfterFourth.DialogueMemories.Num(),
		MemoriesBeforeFourth);

	TestEqual(
		TEXT("Each committed turn appends one memory"),
		AfterThird.DialogueMemories.Num(),
		InitialMemoryCount + 3);
	if (AfterThird.DialogueMemories.Num() >= InitialMemoryCount + 3)
	{
		for (int32 Turn = 1; Turn <= 3; ++Turn)
		{
			const FWSRoleplayMemoryEntry& Memory =
				AfterThird.DialogueMemories[InitialMemoryCount + Turn - 1];
			TestEqual(
				FString::Printf(TEXT("Turn %d memory owner"), Turn),
				Memory.Owner,
				FName(TEXT("gu_heng")));
			TestEqual(
				FString::Printf(TEXT("Turn %d memory index"), Turn),
				Memory.TurnIndex,
				Turn);
			TestFalse(
				FString::Printf(TEXT("Turn %d memory has an id"), Turn),
				Memory.MemoryId.IsNone());
			TestFalse(
				FString::Printf(TEXT("Turn %d memory has a safe summary"), Turn),
				Memory.SafeSummary.IsEmpty());
		}
	}

	const TArray<FWSRoleplayMemoryEntry> SavedMemories =
		AfterThird.DialogueMemories;
	StateSubsystem->NewGame();
	TestEqual(
		TEXT("New game clears runtime dialogue memory"),
		StateSubsystem->GetStateSnapshot().DialogueMemories.Num(),
		0);
	TestTrue(TEXT("Autosave reload succeeds"), StateSubsystem->LoadSnapshot());
	const FWSGameState ReloadedState = StateSubsystem->GetStateSnapshot();
	const TArray<FWSRoleplayMemoryEntry>& ReloadedMemories =
		ReloadedState.DialogueMemories;
	TestEqual(
		TEXT("Reload restores the saved memory count"),
		ReloadedMemories.Num(),
		SavedMemories.Num());
	if (ReloadedMemories.Num() == SavedMemories.Num())
	{
		for (int32 Index = 0; Index < SavedMemories.Num(); ++Index)
		{
			TestTrue(
				FString::Printf(TEXT("Reload preserves memory %d"), Index),
				MemoryMatches(ReloadedMemories[Index], SavedMemories[Index]));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueV14ProviderAuthorityParityTest,
	"WhiteoutStation.Dialogue.V14.Session.ProviderAuthorityParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueV14ProviderAuthorityParityTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueV14SessionTests;
	FScopedStateSubsystem Fixture;
	UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
	if (!TestNotNull(TEXT("State subsystem initializes"), StateSubsystem)
		|| !TestTrue(TEXT("Day phase starts for dialogue"), Fixture.IsReady()))
	{
		return false;
	}

	FWSPreparedDialogue Prepared;
	bool bCapturedPrepared = false;
	StateSubsystem->SetDialogueRealizeTestHook(
		[&](
			const FWSPreparedDialogue& Candidate,
			FWSDialogueRealizeTestCallback)
		{
			Prepared = Candidate;
			bCapturedPrepared = true;
		});
	const FWSActionResult Pending = StateSubsystem->SubmitDialogueAction(
		MakeTalkRequest(FGuid::NewGuid(), TEXT("你现在怎么样？")));
	TestTrue(TEXT("Comparison dialogue reaches realization"), Pending.bPendingDialogue);
	TestTrue(TEXT("Prepared dialogue is captured"), bCapturedPrepared);
	StateSubsystem->CancelPendingDialogue();
	if (!bCapturedPrepared)
	{
		return false;
	}

	FWSAgentReply OnlineReply = Prepared.LocalFallback;
	OnlineReply.Utterance = TEXT("我还能撑住。你要问伤势，还是发电机的情况？");
	OnlineReply.MemorySummary =
		TEXT("顾衡说自己尚能支撑，并请玩家明确想问伤势或发电机。");
	OnlineReply.AnswerSource = TEXT("roleplay_model");
	OnlineReply.Provider = TEXT("online_test_provider");
	OnlineReply.ValidationReason = TEXT("accepted");
	OnlineReply.bFallback = false;

	const FWSDialogueOutcome FallbackOutcome =
		MakeOutcome(Prepared.LocalFallback);
	const FWSDialogueOutcome OnlineOutcome = MakeOutcome(OnlineReply);
	FString FallbackValidationReason;
	FString OnlineValidationReason;
	const bool bFallbackValid = UWSAgentGateway::ValidateDialogueOutcome(
		Prepared,
		FallbackOutcome,
		FallbackValidationReason);
	const bool bOnlineValid = UWSAgentGateway::ValidateDialogueOutcome(
		Prepared,
		OnlineOutcome,
		OnlineValidationReason);
	TestTrue(
		*FString::Printf(
			TEXT("Local fallback validates: %s"),
			*FallbackValidationReason),
		bFallbackValid);
	TestTrue(
		*FString::Printf(
			TEXT("Simulated online reply validates: %s"),
			*OnlineValidationReason),
		bOnlineValid);
	if (!bFallbackValid || !bOnlineValid)
	{
		return false;
	}

	FWhiteoutRulesEngine FallbackRules = StateSubsystem->GetRulesEngine();
	FWhiteoutRulesEngine OnlineRules = StateSubsystem->GetRulesEngine();
	const FWSActionResult FallbackResult =
		FallbackRules.CommitDialogueOutcome(Prepared, FallbackOutcome);
	const FWSActionResult OnlineResult =
		OnlineRules.CommitDialogueOutcome(Prepared, OnlineOutcome);
	TestTrue(TEXT("Local fallback commits"), FallbackResult.bCommitted);
	TestTrue(TEXT("Simulated online reply commits"), OnlineResult.bCommitted);
	TestEqual(
		TEXT("Provider path has the same AP cost"),
		OnlineResult.ActualAP,
		FallbackResult.ActualAP);

	const FWSGameState& FallbackState = FallbackRules.GetState();
	const FWSGameState& OnlineState = OnlineRules.GetState();
	TestTrue(
		TEXT("Provider path preserves AP authority"),
		OnlineState.ActionPoints == FallbackState.ActionPoints
			&& OnlineState.PhaseActionPoints == FallbackState.PhaseActionPoints);
	TestTrue(
		TEXT("Provider path preserves all character and relationship state"),
		AllCharacterStatesMatch(OnlineState, FallbackState));
	TestTrue(
		TEXT("Provider path preserves inventory authority"),
		ResourcesMatch(OnlineState.Resources, FallbackState.Resources));
	TestTrue(
		TEXT("Provider path preserves task authority"),
		TasksMatch(OnlineState.Tasks, FallbackState.Tasks));
	TestTrue(
		TEXT("Provider path preserves world facts"),
		WorldFlagsMatch(OnlineState.Flags, FallbackState.Flags)
			&& KnowledgeMatches(
				OnlineState.PlayerKnowledge,
				FallbackState.PlayerKnowledge)
			&& OnlineState.Evidence == FallbackState.Evidence
			&& OnlineState.PublicFacts == FallbackState.PublicFacts);
	TestTrue(
		TEXT("Provider path preserves authoritative action bookkeeping"),
		ActionCountsMatch(OnlineState.ActionCounts, FallbackState.ActionCounts)
			&& OnlineState.CommittedTransactions
				== FallbackState.CommittedTransactions
			&& OnlineState.Promises.Num() == FallbackState.Promises.Num()
			&& OnlineState.NegotiationOffers.Num()
				== FallbackState.NegotiationOffers.Num());
	TestEqual(
		TEXT("Provider path appends the same number of memories"),
		OnlineState.DialogueMemories.Num(),
		FallbackState.DialogueMemories.Num());
	TestEqual(
		TEXT("Provider path appends the same number of events"),
		OnlineState.EventLog.Num(),
		FallbackState.EventLog.Num());
	return true;
}

#endif
