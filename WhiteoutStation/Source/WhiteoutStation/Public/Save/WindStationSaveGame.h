#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "State/WindStationTypes.h"
#include "WindStationSaveGame.generated.h"

UCLASS()
class WHITEOUTSTATION_API UWindStationSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FString SaveVersion = TEXT("0.8.0");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 DeterministicSeed = 17012026;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FWSGameState State;
};
