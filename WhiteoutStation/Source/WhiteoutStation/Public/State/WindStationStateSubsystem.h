#pragma once

#include "CoreMinimal.h"
#include "State/WhiteoutRulesEngine.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WindStationStateSubsystem.generated.h"

class UWSActionResolver;
class UWSAgentGateway;

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

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Actions")
	FWSActionResult CommitAction(const FWSActionRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Flow")
	FWSGameState EndGame();

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Save")
	bool SaveSnapshot();

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Save")
	bool LoadSnapshot();

	UFUNCTION(BlueprintPure, Category = "Whiteout Station|Save")
	bool HasSnapshot() const;

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Save")
	bool ExportEventLog(FString& OutFilePath) const;

	UFUNCTION(BlueprintPure, Category = "Whiteout Station|Actions")
	UWSActionResolver* GetActionResolver() const { return ActionResolver; }

	UFUNCTION(BlueprintPure, Category = "Whiteout Station|Dialogue")
	FWSAgentReply GetLatestDialogue() const { return LatestDialogue; }

	void CancelPendingDialogue();

	const FWhiteoutRulesEngine& GetRulesEngine() const { return RulesEngine; }

private:
	static const FString SaveSlot;
	static const FString LegacyV09SaveSlot;
	static const FString LegacyV08SaveSlot;
	static const FString LegacyV07SaveSlot;
	static const FString LegacyV06SaveSlot;
	FWhiteoutRulesEngine RulesEngine;

	UPROPERTY()
	TObjectPtr<UWSActionResolver> ActionResolver;

	UPROPERTY()
	TObjectPtr<UWSAgentGateway> AgentGateway;

	UPROPERTY()
	FWSAgentReply LatestDialogue;

	void BroadcastState();
	void RequestActionExpression(const FWSActionRequest& ActionRequest);
	void HandleAgentReply(const FWSAgentReply& Reply);
};
