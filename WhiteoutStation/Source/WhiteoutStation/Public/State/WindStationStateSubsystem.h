#pragma once

#include "CoreMinimal.h"
#include "State/WhiteoutRulesEngine.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WindStationStateSubsystem.generated.h"

class UWSActionResolver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWSStateChangedSignature, const FWSGameState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWSActionCommittedSignature, const FWSActionResult&, Result);

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

	const FWhiteoutRulesEngine& GetRulesEngine() const { return RulesEngine; }

private:
	static const FString SaveSlot;
	FWhiteoutRulesEngine RulesEngine;

	UPROPERTY()
	TObjectPtr<UWSActionResolver> ActionResolver;

	void BroadcastState();
};
