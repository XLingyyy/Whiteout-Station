#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"

struct WHITEOUTSTATION_API FWSKnowledgePolicy
{
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
