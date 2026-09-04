#pragma once

#include "CoreMinimal.h"
#include "Agents/WSRoleplayTypes.h"
#include "State/WindStationTypes.h"
#include "UObject/Object.h"
#include "WSNPCContextBuilder.generated.h"

class UWSRoleplayKnowledgeRepository;

UCLASS()
class WHITEOUTSTATION_API UWSNPCContextBuilder : public UObject
{
	GENERATED_BODY()

public:
	static bool BuildRequest(
		const FWSActionRequest& ActionRequest,
		const FWSGameState& FrozenState,
		const UWSRoleplayKnowledgeRepository& Repository,
		const TArray<FWSRoleplayMemoryEntry>& RecentMemory,
		int32 TurnIndex,
		FWSRoleplayRequest& OutRequest,
		FWSRoleplayFallback& OutFallback,
		FString& OutError);

	static FWSDialogueSemanticFrame BuildSemanticFrame(
		const FString& PlayerLine,
		FName CurrentDialogueActionId);

	static FWSRoleplaySubjectiveState BuildSubjectiveState(
		FName SpeakerId,
		const FWSGameState& FrozenState);
};
