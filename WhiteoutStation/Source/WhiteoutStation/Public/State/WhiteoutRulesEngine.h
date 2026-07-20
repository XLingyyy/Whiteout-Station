#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"

struct FWhiteoutRuleConfig
{
	int32 StartingActionPoints = 8;
	int32 MidCrisisThreshold = 4;
	int32 GeneratorRequired = 2;
	int32 AntennaRequired = 1;
	int32 ModelCallHardLimit = 10;
	FWSGameState InitialState;
};

class WHITEOUTSTATION_API FWhiteoutRulesEngine
{
public:
	FWhiteoutRulesEngine();

	bool LoadConfig(const FString& ConfigPath, FString& OutError);
	void Reset();
	void SetState(const FWSGameState& InState);

	const FWSGameState& GetState() const { return State; }
	FWSGameState& GetMutableStateForTesting() { return State; }
	const FWhiteoutRuleConfig& GetConfig() const { return Config; }

	FWSActionPreview Preview(const FWSActionRequest& Request) const;
	FWSActionResult Commit(FWSActionRequest Request);
	EWSEndingType ClassifyEnding() const;
	FWSScoreBreakdown CalculateScore() const;
	void EndGame();

	TArray<FName> BuildAllowedFactIds(EWSCharacterId CharacterId) const;
	static bool ValidateAgentResponse(
		const FString& Utterance,
		const TArray<FName>& ReferencedFactIds,
		const TArray<FName>& AllowedFactIds,
		bool bContainsRuleMutation,
		FString& OutReason);

	static int32 GetActionCost(FName ActionId);
	static bool IsCoreAction(FName ActionId);

private:
	FWhiteoutRuleConfig Config;
	FWSGameState State;

	EWSReasonCode CanExecute(const FWSActionRequest& Request) const;
	void ApplyEffect(const FWSActionRequest& Request, TArray<FString>& OutChanges);
	void ApplyEnvironment(int32 APCost, bool bOutdoors, TArray<FString>& OutChanges);
	void TriggerMidCrisis(TArray<FString>& OutChanges);
	void RecognizePromise(const FWSActionRequest& Request, TArray<FString>& OutChanges);
	void SettlePromises();
	bool SignalAvailable() const;
	bool Knows(FName FactId, EWSKnowledgeLevel Minimum = EWSKnowledgeLevel::Suspected) const;
	void DiscoverFact(FName FactId, EWSKnowledgeLevel Level, TArray<FString>* OutChanges = nullptr);
	void AddEvidence(FName EvidenceId, TArray<FString>* OutChanges = nullptr);
	void ChangeCharacter(
		EWSCharacterId CharacterId,
		float Health,
		float Temperature,
		float Hunger,
		float Fatigue,
		float Pressure,
		float Trust);
	FWSCharacterState& Character(EWSCharacterId CharacterId);
	const FWSCharacterState& Character(EWSCharacterId CharacterId) const;
	int32 ActionCount(FName ActionId) const;
	static int32 ActionMaxUses(FName ActionId);
	static bool ActionRepeatable(FName ActionId);
	static FText ActionPreviewText(FName ActionId);
	static FText ActionRiskText(FName ActionId);
};
