#pragma once

#include "CoreMinimal.h"
#include "State/WhiteoutRulesEngine.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WindStationStateSubsystem.generated.h"

class UWSActionResolver;
class UWSAgentGateway;

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
	bool HasPendingDialogue() const { return bHasPendingDialogue; }

	int64 GetStateRevision() const { return StateRevision; }
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
	bool ApplyLLMRuntimeConfiguration(FString& OutError);
	FString GetLLMRuntimeStatus() const;
	bool HasLiveLLMProvider() const;
	bool SetRequirementPinned(FName ActionId, bool bPinned);
	bool AcceptLatestNegotiationOffer(FString& OutMessage);
	void RequestDialogueIntent(
		const FString& UserText,
		FName CurrentDialogueActionId,
		FName CurrentTopicActionId,
		TFunction<void(const FWSDialogueIntentResult&)> Completion);

	const FWhiteoutRulesEngine& GetRulesEngine() const { return RulesEngine; }

private:
	static const FString SaveSlot;
	static const FString LegacySaveSlotV12;
	static const FString LegacySaveSlotV11;
	FWhiteoutRulesEngine RulesEngine;

	UPROPERTY()
	TObjectPtr<UWSActionResolver> ActionResolver;

	UPROPERTY()
	TObjectPtr<UWSAgentGateway> AgentGateway;

	UPROPERTY()
	FWSAgentReply LatestDialogue;

	FWSPreparedDialogue PendingDialogue;
	bool bHasPendingDialogue = false;
	bool bCommitDispatchActive = false;
	bool bLifecycleTransitionActive = false;
	TFunction<void(const FWSActionResult&)> PendingDialogueCompletion;
	int64 StateRevision = 1;
	int64 DialogueGeneration = 1;

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
