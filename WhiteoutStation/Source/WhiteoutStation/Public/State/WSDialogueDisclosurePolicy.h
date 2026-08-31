#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"

namespace WSDialogueDisclosurePolicy
{
	WHITEOUTSTATION_API bool IsTargetedGuHengDiagnosisQuestion(
		const FWSActionRequest& Request);
	WHITEOUTSTATION_API bool IsGuHengConditionObservationQuestion(
		const FWSActionRequest& Request);

	WHITEOUTSTATION_API bool IsHeatPackDisclosureQuestion(
		const FWSActionRequest& Request);
}
