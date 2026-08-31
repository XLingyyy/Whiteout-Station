#include "State/WSKnowledgePolicy.h"

bool FWSKnowledgePolicy::PlayerKnows(
	const FWSGameState& State,
	const FName FactId,
	const EWSKnowledgeLevel Minimum)
{
	const EWSKnowledgeLevel* Level = State.PlayerKnowledge.Find(FactId);
	return Level && static_cast<uint8>(*Level) >= static_cast<uint8>(Minimum);
}

bool FWSKnowledgePolicy::IsHeatPackOptionVisible(const FWSGameState& State)
{
	return PlayerKnows(
		State,
		TEXT("FACT_HEAT_PACK"),
		EWSKnowledgeLevel::Confirmed);
}

bool FWSKnowledgePolicy::IsRelayRepairRouteVisible(const FWSGameState& State)
{
	return State.Resources.ReplacementRelay > 0
		|| PlayerKnows(
			State,
			TEXT("FACT_RELAY_COMPATIBILITY"),
			EWSKnowledgeLevel::Confirmed);
}

bool FWSKnowledgePolicy::IsGuHengInjuryVisible(const FWSGameState& State)
{
	return State.Flags.bGuHengDiagnosed
		|| PlayerKnows(
			State,
			TEXT("FACT_HAND_INJURY"),
			EWSKnowledgeLevel::Confirmed);
}

bool FWSKnowledgePolicy::IsGuHengTreatmentOptionVisible(const FWSGameState& State)
{
	return State.Flags.bGuHengDiagnosed
		|| PlayerKnows(
			State,
			TEXT("FACT_MEDICAL_DIAGNOSIS"),
			EWSKnowledgeLevel::Confirmed);
}

bool FWSKnowledgePolicy::IsGuHengInjuryWrapVisible(const FWSGameState& State)
{
	return State.Flags.bGuHengDiagnosed
		|| PlayerKnows(
			State,
			TEXT("FACT_HAND_INJURY"),
			EWSKnowledgeLevel::Suspected);
}

bool FWSKnowledgePolicy::IsWorldActionVisible(
	const FName ActionId,
	const FWSGameState& State)
{
	return ActionId != TEXT("dismantle_kitchen_heater")
		|| IsRelayRepairRouteVisible(State);
}
