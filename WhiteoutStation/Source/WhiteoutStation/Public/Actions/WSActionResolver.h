#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"
#include "UObject/Object.h"
#include "WSActionResolver.generated.h"

class UWindStationStateSubsystem;

UCLASS(BlueprintType)
class WHITEOUTSTATION_API UWSActionResolver : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UWindStationStateSubsystem* InStateSubsystem);

	UFUNCTION(BlueprintPure, Category = "Whiteout Station|Actions")
	FWSActionPreview Preview(const FWSActionRequest& Request) const;

	UFUNCTION(BlueprintCallable, Category = "Whiteout Station|Actions")
	FWSActionResult Commit(const FWSActionRequest& Request);

private:
	UPROPERTY()
	TObjectPtr<UWindStationStateSubsystem> StateSubsystem;
};
