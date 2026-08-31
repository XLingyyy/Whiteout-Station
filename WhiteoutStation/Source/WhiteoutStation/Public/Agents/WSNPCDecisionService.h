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
	static FWSAgentReply BuildDeterministicReply(const FWSActionRequest& Request, const FWSGameState& State);
	static FWSAgentReply BuildDeterministicReply(
		const FWSActionRequest& Request,
		const FWSGameState& State,
		const FWSActionRequirementReport& RequirementReport);
	static FWSDialogueRealizationContract BuildDialogueContract(
		const FWSActionRequest& Request,
		const FWSGameState& State,
		const FWSActionRequirementReport& RequirementReport,
		FWSAgentReply& OutLocalFallback);
	static FWSNPCDialoguePlan BuildDialoguePlan(
		const FWSActionRequest& Request,
		const FWSGameState& State,
		const FWSActionRequirementReport& RequirementReport);
	static FWSDialogueDisclosureContext BuildDisclosureContext(
		const FWSActionRequest& Request,
		EWSCharacterId Speaker,
		const FWSGameState& State);
	static FWSFactDisclosureDecision ResolveFactDisclosure(
		FName FactId,
		const FWSDialogueDisclosureContext& Context);
	static FWSActionRequirementReport ResolveRequirementVisibility(
		const FWSActionRequirementReport& MechanicalReport,
		const FWSDialogueDisclosureContext& Context);
	static TArray<FName> BuildAllowedFacts(
		const FWSActionRequest& Request,
		EWSCharacterId Speaker,
		const FWSGameState& State);
	static TArray<FName> BuildAllowedFacts(FName ActionId, EWSCharacterId Speaker, const FWSGameState& State);
	static FString SpeakerLabel(EWSCharacterId Speaker);

private:
	static bool PlayerKnows(const FWSGameState& State, FName FactId, EWSKnowledgeLevel Minimum = EWSKnowledgeLevel::Suspected);
};
