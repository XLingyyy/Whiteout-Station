#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"

class WHITEOUTSTATION_API FWSPresentationText
{
public:
	static FText UI(FName Key, const TCHAR* Fallback);
	static FText ActionLabel(FName ActionId);
	static FText ActionImpact(FName ActionId);
	static FText ActionExecutor(FName ActionId);
	static FText ActionResourceCost(FName ActionId);
	static FText ReasonCause(EWSReasonCode Reason);
	static FText ReasonNextStep(EWSReasonCode Reason);
	static FText EvidenceLabel(FName EvidenceId);
	static FText FactLabel(FName FactId);
	static FText PromiseLabel(FName ConditionId);
	static FText PhaseLabel(EWSGamePhase Phase);
	static FText EndingTitle(EWSEndingType Ending);
	static FText EndingSummary(EWSEndingType Ending);
	static FText EndingAdvice(EWSEndingType Ending);
	static FText ScoreAttribution(FName ScoreId);
	static FText CharacterName(EWSCharacterId CharacterId);
	static FText ConditionLevel(float Value);
	static FText TrustLevel(float Value);
	static FText KnowledgeLevel(EWSKnowledgeLevel Level);
};
