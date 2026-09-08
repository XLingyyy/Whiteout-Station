#include "State/WSKnowledgePolicy.h"

FWSCharacterStateView FWSKnowledgePolicy::CharacterState(EWSCharacterId Id, const FWSGameState& State, bool bMayDiscloseInjury)
{
	FWSCharacterStateView View;
	const auto* Character = State.Characters.Find(Id);
	if (!Character || !bMayDiscloseInjury) return View;
	View.bInjuryKnown = true; View.Injury = Character->InjurySeverity;
	View.bTemporarySupport = Character->TemporarySupportUses > 0 && Character->TemporarySupportPhase == State.DayPhase;
	View.bBandaged = Character->BandageProtection > 0;
	View.TreatmentStatus = Id == EWSCharacterId::GuHeng && State.Flags.bGuHengTreated ? TEXT("completed")
		: View.bTemporarySupport ? TEXT("temporary_support_only") : View.bBandaged ? TEXT("bandage_only") : TEXT("not_started");
	return View;
}

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
