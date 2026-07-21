#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WhiteoutGameMode.generated.h"

class UAudioComponent;
class ACameraActor;
class APointLight;
class UWSAgentGateway;

UCLASS()
class WHITEOUTSTATION_API AWhiteoutGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWhiteoutGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	void FinishOpeningPresentation();

private:
	UPROPERTY()
	TObjectPtr<ACameraActor> OpeningCamera;

	UPROPERTY()
	TObjectPtr<UWSAgentGateway> IntentProbeGateway;

	UPROPERTY()
	TObjectPtr<APointLight> LookAtCaptureLight;

	FTimerHandle OpeningFinishTimer;
	bool bOpeningFinished = false;

	TArray<FVector> BaselineLocations;
	TArray<FRotator> BaselineRotations;
	TArray<FString> BaselineNames;
	int32 BaselineCaptureIndex = 0;
	TArray<FString> PresentationCaptureNames;
	FString PresentationCaptureMode;
	int32 PresentationCaptureIndex = 0;
	TArray<float> PerformanceFrameTimesMs;
	double PerformanceStartSeconds = 0.0;
	bool bPerformanceTestActive = false;

	void RunAutomationRoute(const FString& RouteName);
	void CompletePerformanceTest();
	void BeginOpeningPresentation();
	void BeginBaselineCapture();
	void StageBaselineView();
	void CaptureBaselineView();
	void BeginPresentationCapture();
	void StagePresentationCapture();
	void CapturePresentationFrame();
};
