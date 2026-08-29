#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"

struct FWhiteoutActionRule
{
	int32 BaseAP = 1;
	TSet<FName> Tags;
	EWSCharacterId PrimaryExecutor = EWSCharacterId::Player;
	EWSCharacterLocation Location = EWSCharacterLocation::ControlRoom;
	bool bRepeatable = false;
	int32 MaxUses = 1;
	bool bConsumesStamina = false;
	bool bForceAllowed = false;
	TArray<EWSCharacterId> Collaborators;
};

struct FWhiteoutRuleConfig
{
	int32 SchemaVersion = 3;
	FString RulesVersion = TEXT("1.0.0");
	int32 StartingActionPoints = 12;
	int32 ActionPointsPerPhase = 4;
	int32 MidCrisisThreshold = 6;
	int32 GeneratorRequired = 2;
	int32 AntennaRequired = 1;
	int32 ModelCallHardLimit = 10;
	int32 SafeWaitFuel = 1;
	float SafeAntennaTemperature = 5.5f;
	float CriticalHealth = 3.0f;
	float CriticalTemperature = 3.0f;
	float CriticalFatigue = 3.0f;
	float CriticalPressure = 8.5f;
	float WarmTemperature = 6.0f;
	float HypothermicTemperature = 3.5f;
	float HeatedTemperatureDelta = 0.5f;
	TMap<EWSDayPhase, float> UnheatedTemperatureDelta;
	TMap<FName, FWhiteoutActionRule> ActionRules;
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
	bool IsV11() const { return Config.SchemaVersion >= 4; }

	FWSActionPreview Preview(const FWSActionRequest& Request) const;
	FWSActionResult Commit(FWSActionRequest Request);
	bool BeginDayPhase(
		EWSHeatingZone HeatingZone,
		EWSReasonCode& OutReason,
		TArray<FString>& OutChanges);
	bool SettleDayPhase(EWSReasonCode& OutReason, FWSPhaseSummary& OutSummary);
	EWSEndingType ClassifyEnding() const;
	FWSScoreBreakdown CalculateScore() const;
	void EndGame();
	bool TryRecordModelCall();
	FWSActionRequirementReport EvaluateActionRequirements(const FWSActionRequest& Request) const;
	bool SetRequirementPinned(FName ActionId, bool bPinned);
	bool AcceptNegotiationOffer(const FWSAgentReply& Reply, FString& OutMessage);
	void UpdateNegotiationOffersForCommittedAction(
		const FWSActionRequest& Request,
		TArray<FString>& OutChanges);
	void ExpireNegotiationOffers(
		EWSDayPhase SettledPhase,
		TArray<FString>& OutChanges);

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
	EWSReasonCode CanExecuteV11(const FWSActionRequest& Request) const;
	EWSReasonCode EvaluateRepairGeneratorReason(
		const FWSActionRequest& Request,
		FWSActionRequirementReport* OutReport) const;
	FWSActionPreview BuildV11Preview(const FWSActionRequest& Request) const;
	FWSActionResult CommitV11(FWSActionRequest Request);
	void ApplyEffect(const FWSActionRequest& Request, TArray<FString>& OutChanges);
	void ApplyV11Effect(
		const FWSActionRequest& Request,
		const FWSActionPreview& Preview,
		TArray<FString>& OutChanges);
	void ApplyEnvironment(int32 APCost, bool bOutdoors, TArray<FString>& OutChanges);
	void TriggerMidCrisis(TArray<FString>& OutChanges);
	void RecognizePromise(const FWSActionRequest& Request, TArray<FString>& OutChanges);
	void SettlePromises();
	bool SignalAvailable() const;
	bool IsCritical(const FWSCharacterState& CharacterState) const;
	bool IsV11Critical(const FWSCharacterState& CharacterState) const;
	void ConsumeV11Stamina(EWSCharacterId CharacterId);
	void WorsenV11Injury(EWSCharacterId CharacterId, TArray<FString>& OutChanges);
	EWSCharacterId ResolveV11Executor(const FWSActionRequest& Request) const;
	bool IsV11Action(FName ActionId) const;
	bool HasV11Tag(FName ActionId, FName Tag) const;
	bool IsV11CharacterAvailable(EWSCharacterId CharacterId) const;
	bool V11HeatingMatchesLocation(EWSCharacterLocation Location) const;
	static EWSHeatingZone HeatingZoneForLocation(EWSCharacterLocation Location);
	static EWSDayPhase NextDayPhase(EWSDayPhase DayPhase);
	static FString DayPhaseLabel(EWSDayPhase DayPhase);
	static FString HeatingZoneLabel(EWSHeatingZone HeatingZone);
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
