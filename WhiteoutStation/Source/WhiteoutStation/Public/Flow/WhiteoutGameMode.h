#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WhiteoutGameMode.generated.h"

class UAudioComponent;
class ACameraActor;

UCLASS()
class WHITEOUTSTATION_API AWhiteoutGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWhiteoutGameMode();
	virtual void BeginPlay() override;
	void FinishOpeningPresentation();

private:
	UPROPERTY()
	TObjectPtr<ACameraActor> OpeningCamera;

	FTimerHandle OpeningFinishTimer;
	bool bOpeningFinished = false;

	TArray<FVector> BaselineLocations;
	TArray<FRotator> BaselineRotations;
	TArray<FString> BaselineNames;
	int32 BaselineCaptureIndex = 0;
	TArray<FString> PresentationCaptureNames;
	FString PresentationCaptureMode;
	int32 PresentationCaptureIndex = 0;

	void RunAutomationRoute(const FString& RouteName);
	void BeginOpeningPresentation();
	void BeginBaselineCapture();
	void StageBaselineView();
	void CaptureBaselineView();
	void BeginPresentationCapture();
	void StagePresentationCapture();
	void CapturePresentationFrame();
};
