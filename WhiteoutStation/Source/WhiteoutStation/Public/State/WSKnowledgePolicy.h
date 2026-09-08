#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"

struct FWSCharacterStateView
{
	bool bInjuryKnown = false;
	EWSInjurySeverity Injury = EWSInjurySeverity::Normal;
	FString TreatmentStatus = TEXT("unknown");
	bool bTemporarySupport = false;
	bool bBandaged = false;
};

struct WHITEOUTSTATION_API FWSKnowledgePolicy
{
	static FWSCharacterStateView CharacterState(EWSCharacterId Id, const FWSGameState& State, bool bMayDiscloseInjury);
	static bool PlayerKnows(
		const FWSGameState& State,
		FName FactId,
		EWSKnowledgeLevel Minimum = EWSKnowledgeLevel::Suspected);

	static bool IsHeatPackOptionVisible(const FWSGameState& State);
	static bool IsRelayRepairRouteVisible(const FWSGameState& State);
	static bool IsGuHengInjuryVisible(const FWSGameState& State);
	static bool IsGuHengTreatmentOptionVisible(const FWSGameState& State);
	static bool IsGuHengInjuryWrapVisible(const FWSGameState& State);
	static bool IsWorldActionVisible(FName ActionId, const FWSGameState& State);
};
