#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"
#include "UObject/Object.h"
#include "WSNPCDecisionService.generated.h"

UCLASS()
class WHITEOUTSTATION_API UWSNPCDecisionService : public UObject
{
	GENERATED_BODY()

public:
	static bool RequiresExpression(FName ActionId);
	static FWSAgentReply BuildDeterministicReply(FName ActionId, const FWSGameState& State);
	static TArray<FName> BuildAllowedFacts(FName ActionId, EWSCharacterId Speaker, const FWSGameState& State);
	static FString SpeakerLabel(EWSCharacterId Speaker);

private:
	static bool PlayerKnows(const FWSGameState& State, FName FactId, EWSKnowledgeLevel Minimum = EWSKnowledgeLevel::Suspected);
};
