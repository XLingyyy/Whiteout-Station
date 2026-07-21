#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WhiteoutGameMode.generated.h"

class UAudioComponent;

UCLASS()
class WHITEOUTSTATION_API AWhiteoutGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWhiteoutGameMode();
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UAudioComponent> WindAmbience;

	TArray<FVector> BaselineLocations;
	TArray<FRotator> BaselineRotations;
	TArray<FString> BaselineNames;
	int32 BaselineCaptureIndex = 0;

	void RunAutomationRoute(const FString& RouteName);
	void BeginBaselineCapture();
	void StageBaselineView();
	void CaptureBaselineView();
};
