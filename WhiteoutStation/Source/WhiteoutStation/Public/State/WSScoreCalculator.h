#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "State/WindStationTypes.h"
#include "WSScoreCalculator.generated.h"

UCLASS()
class WHITEOUTSTATION_API UWSScoreCalculator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Whiteout Station|Scoring")
	static FWSScoreBreakdown Calculate(const FWSGameState& State);
};
