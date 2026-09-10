#pragma once
#include "CoreMinimal.h"
#include "State/WindStationTypes.h"

enum class EWSAPCell : uint8 { Available, PreviewSpend, Spent };
enum class EWSAPRefreshReason : uint8 { Snapshot, Preview, Committed, Failed, PhaseChanged, Loaded };

struct FWSAPViewModel
{
	EWSDayPhase PhaseId = EWSDayPhase::Morning;
	int32 PhaseRemaining = 0;
	int32 PhaseCapacity = 4;
	bool bPhaseStarted = false;
	int64 StateRevision = 0;
	FGuid RunId;
	TOptional<int32> QuotedCost;
	bool bCanExecute = true;
	FString BlockingReason;
	FString QuoteToken;
	FGuid TransactionId;
	EWSAPRefreshReason RefreshReason = EWSAPRefreshReason::Snapshot;

	bool IsValid() const { return PhaseCapacity > 0 && PhaseRemaining >= 0 && PhaseRemaining <= PhaseCapacity; }
	EWSAPCell Cell(int32 I) const
	{
		const int32 A = FMath::Clamp(PhaseRemaining, 0, FMath::Max(0, PhaseCapacity));
		if (I >= A) return EWSAPCell::Spent;
		return QuotedCost.IsSet() && I >= FMath::Max(0, A - QuotedCost.GetValue())
			? EWSAPCell::PreviewSpend : EWSAPCell::Available;
	}
};
