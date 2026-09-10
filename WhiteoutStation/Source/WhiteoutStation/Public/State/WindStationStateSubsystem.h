#pragma once

#include "CoreMinimal.h"
#include "State/WhiteoutRulesEngine.h"
#include "Dialogue/WSAuthoredDialogueRepository.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WindStationStateSubsystem.generated.h"

class UWSActionResolver;
class UWSAgentGateway;
class UWSNPCContextBuilder;
class UWSRoleplayKnowledgeRepository;

struct FWSDialogueSessionRuntimeState
{
	FName ActionId;
	EWSDayPhase DayPhase = EWSDayPhase::Morning;
	int32 CommittedTurns = 0;
	int32 PaidAP = 0;
	bool bPositiveRewardApplied = false;
	TSet<FName> AppliedEffectKeys;
	TOptional<FWSCanonicalIntent> PendingCommitment;
	TOptional<FWSCanonicalIntent> ProposalBeforeMessage;
	bool bProposalChangePending = false;
	int64 PendingCommitmentRevision = 0;
	void RollbackProposal()
	{
		if (bProposalChangePending) PendingCommitment = ProposalBeforeMessage;
		ProposalBeforeMessage.Reset(); bProposalChangePending = false;
	}
	int32 MessageCount = 0;
	FGuid LatestMessageId;
	TOptional<FWSCanonicalIntent> ResolvedMessage;
	TOptional<FWSCanonicalIntent> LastParsedMessage;
	TSet<FGuid> CommittedMessages;
};

#if WITH_DEV_AUTOMATION_TESTS
using FWSDialogueRealizeTestCallback =
	TFunction<void(const FWSAgentReply&)>;
using FWSDialogueRealizeTestHook =
	TFunction<void(
		const FWSPreparedDialogue&,
		FWSDialogueRealizeTestCallback)>;
using FWSDialogueCommitDispatchTestHook = TFunction<void()>;
#endif

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWSStateChangedSignature, const FWSGameState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWSActionCommittedSignature, const FWSActionResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWSDialogueLineSignature, const FWSAgentReply&, Reply);

UCLASS()
class WHITEOUTSTATION_API UWindStationStateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Whiteout Station|State")
	FWSStateChangedSignature OnStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Whiteout Station|State")
	FWSActionCommittedSignature OnActionCommitted;

	UPROPERTY(BlueprintAssignable, Category = "Whiteout Station|Dialogue")
	FWSDialogueLineSignature OnDialogueLine;

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|State")
	void NewGame();

	UFUNCTION(BlueprintPure, Category = "Whiteout Station|State")
	FWSGameState GetStateSnapshot() const;
	TArray<FWSConversationEntry> GetConversationHistory(FName ActionId) const;
	TArray<FString> BuildOnlineConversationHistory(FName ActionId, FGuid SessionId) const;
	int32 GetDialogueTurnsUsed(FName ActionId) const;
	int32 GetDialogueTurnLimit() const { return RulesEngine.GetConfig().DialogueTurnLimit; }

	UFUNCTION(BlueprintPure, Category = "Whiteout Station|Actions")
	FWSActionPreview PreviewAction(const FWSActionRequest& Request) const;

	UFUNCTION(BlueprintPure, Category = "Whiteout Station|Actions")
	FWSActionRequirementReport EvaluateActionRequirements(FName ActionId) const;

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Actions")
	FWSActionResult CommitAction(const FWSActionRequest& Request);
	FWSActionResult SubmitDialogueAction(
		const FWSActionRequest& Request,
		TFunction<void(const FWSActionResult&)> Completion = {});

	UFUNCTION(BlueprintPure, Category = "Whiteout Station|Dialogue")
	bool HasPendingDialogue() const { return bHasPendingDialogue || bHasPendingOnlineIntent; }

	int64 GetStateRevision() const { return StateRevision; }
	bool IsPresentationModalSafe() const { return !HasPendingDialogue() && !bCommitDispatchActive && !bLifecycleTransitionActive; }
	bool WasSnapshotLoaded() const { return bSnapshotLoaded; }
	bool WasLegacySnapshotLoaded() const { return bLegacySnapshotLoaded; }
	static bool CanCommitPreparedDialogue(
		const FWSPreparedDialogue& Candidate,
		const FWSPreparedDialogue& Pending,
		int64 CurrentStateRevision,
		int64 CurrentGeneration,
		const TArray<FGuid>& CommittedTransactions);

#if WITH_DEV_AUTOMATION_TESTS
	void SetDialogueRealizeTestHook(FWSDialogueRealizeTestHook Hook);
	void SetDialogueCommitDispatchTestHook(
		FWSDialogueCommitDispatchTestHook Hook);
	void SetAutomationSaveSlot(FString InSaveSlot);
	void SetDialogueAuditPathForTest(FString InPath);
	void SetEventLogExportPathForTest(FString InPath);
	int32 GetDialogueLineBroadcastCountForTest() const
	{
		return DialogueLineBroadcastCountForTest;
	}
#endif

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Flow")
	bool BeginDayPhase(
		EWSHeatingZone HeatingZone,
		EWSReasonCode& OutReason,
		TArray<FString>& OutChanges);

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Flow")
	bool SettleCurrentDayPhase(
		EWSReasonCode& OutReason,
		FWSPhaseSummary& OutSummary);

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Flow")
	FWSGameState EndGame();

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Save")
	bool SaveSnapshot();

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Save")
	bool LoadSnapshot();
	bool LoadLegacySnapshot() { return LoadSnapshotFrom(true); }
	bool HasLegacySnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Whiteout Station|Save")
	bool HasSnapshot() const;

	static FWSGameState MigrateSaveStateForV13(
		const FWSGameState& SourceState,
		const FString& SourceSaveVersion,
		int32 TargetRulesSchemaVersion,
		const FString& TargetRulesVersion);

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Save")
	bool ExportEventLog(FString& OutFilePath) const;

	UFUNCTION(BlueprintPure, Category = "Whiteout Station|Actions")
	UWSActionResolver* GetActionResolver() const { return ActionResolver; }

	UFUNCTION(BlueprintPure, Category = "Whiteout Station|Dialogue")
	FWSAgentReply GetLatestDialogue() const { return LatestDialogue; }

	void CancelPendingDialogue();
	void EndDialogueSession(const FGuid& DialogueSessionId);
	bool CanContinueDialogueSession(const FGuid& DialogueSessionId) const;
	bool ApplyLLMRuntimeConfiguration(FString& OutError);
	FString GetLLMRuntimeStatus() const;
	bool HasLiveLLMProvider() const;
	EWSDialogueMode GetDialogueMode() const;
	void ResolveOnlineIntent(FName ActionId, const FString& Text, FGuid SessionId,
		TFunction<void(bool, const FWSCanonicalIntent&, const FString&)> Completion);
	bool ResolveParsedOnlineMessage(FName ActionId, const FString& Text, FGuid SessionId,
		const FWSCanonicalIntent& Parsed, FWSCanonicalIntent& Out, FString& Status, bool bMessageCounted = false);
	const FWSDialogueSessionRuntimeState* GetDialogueSessionState(FGuid SessionId) const { return DialogueSessions.Find(SessionId); }
	TArray<FWSAuthoredChoice> GetAuthoredDialogueChoices(FName ActionId) const;
	FWSActionResult SubmitAuthoredDialogueChoice(FName ActionId, FName ChoiceId,
		FGuid SessionId, TFunction<void(const FWSActionResult&)> Completion = {});
	bool SetRequirementPinned(FName ActionId, bool bPinned);
	bool AcceptLatestNegotiationOffer(FString& OutMessage);
	void RequestDialogueIntent(
		const FString& UserText,
		FName CurrentDialogueActionId,
		FName CurrentTopicActionId,
		TFunction<void(const FWSDialogueIntentResult&)> Completion);

	const FWhiteoutRulesEngine& GetRulesEngine() const { return RulesEngine; }

private:
	bool LoadSnapshotFrom(bool bUseLegacyBackup);
	friend class AWhiteoutGameMode;
	friend class FWhiteoutV15MessageStateTest;
	static const FString SaveSlot;
	static const FString LegacySaveSlotV16;
	static const FString LegacySaveSlotV15;
	static const FString LegacySaveSlotV14;
	static const FString LegacySaveSlotV13;
	static const FString LegacySaveSlotV12;
	static const FString LegacySaveSlotV11;
	FWhiteoutRulesEngine RulesEngine;
	FWSAuthoredDialogueRepository AuthoredRepository;

	UPROPERTY()
	TObjectPtr<UWSActionResolver> ActionResolver;

	UPROPERTY()
	TObjectPtr<UWSAgentGateway> AgentGateway;

	UPROPERTY()
	TObjectPtr<UWSRoleplayKnowledgeRepository> RoleplayRepository;

	UPROPERTY()
	TObjectPtr<UWSNPCContextBuilder> RoleplayContextBuilder;

	UPROPERTY()
	FWSAgentReply LatestDialogue;

	FWSPreparedDialogue PendingDialogue;
	bool bHasPendingDialogue = false;
	bool bHasPendingOnlineIntent = false;
	bool bCommitDispatchActive = false;
	bool bLifecycleTransitionActive = false;
	bool bSnapshotLoaded = false;
	bool bLegacySnapshotLoaded = false;
	TFunction<void(const FWSActionResult&)> PendingDialogueCompletion;
	int64 StateRevision = 1;
	int64 DialogueGeneration = 1;
	TMap<FGuid, FWSDialogueSessionRuntimeState> DialogueSessions;

#if WITH_DEV_AUTOMATION_TESTS
	FWSDialogueRealizeTestHook DialogueRealizeTestHook;
	FWSDialogueCommitDispatchTestHook DialogueCommitDispatchTestHook;
	FString AutomationSaveSlot;
	FString DialogueAuditPathForTest;
	FString EventLogExportPathForTest;
	int32 DialogueLineBroadcastCountForTest = 0;
#endif

	FDelegateHandle LLMSettingsChangedHandle;
	FString LLMConfigurationError;

	void BroadcastState();
	bool NormalizeDialogueSessionRequest(
		FWSActionRequest& InOutRequest,
		EWSReasonCode& OutReason) const;
	void RecordCommittedDialogueSession(const FWSActionRequest& Request);
	FWSActionResult PrepareDialogue(const FWSActionRequest& ActionRequest);
	void RealizePreparedDialogue();
	void HandlePreparedDialogueReply(
		const FWSAgentReply& Reply,
		FGuid TransactionId,
		int64 Generation);
	void HandlePreparedDialogueOutcome(
		const FWSDialogueOutcome& Outcome,
		FGuid TransactionId,
		int64 Generation);
	bool CommitDialogueOutcome(
		const FWSPreparedDialogue& Prepared,
		const FWSDialogueOutcome& Outcome,
		FWSActionResult& OutResult);
	bool AppendDialogueAudit(
		const FWSPreparedDialogue& Prepared,
		const FWSDialogueOutcome& Outcome) const;
	FString GetDialogueAuditPath() const;
	void AbortPendingDialogue(
		EWSReasonCode Reason,
		bool bNotifyCompletion,
		bool bResetGateway);
	void CompleteDialogueSubmission(
		const FWSActionResult& Result,
		TFunction<void(const FWSActionResult&)> Completion);
	void BroadcastDialogueLine(const FWSAgentReply& Reply);
	void RequestActionExpression(const FWSActionRequest& ActionRequest);
	void HandleAgentReply(const FWSAgentReply& Reply);
	void HandleLLMSettingsChanged();
	const FString& GetActiveSaveSlot() const;
};
