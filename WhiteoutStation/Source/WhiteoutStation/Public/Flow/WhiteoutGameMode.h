#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WhiteoutGameMode.generated.h"

class UAudioComponent;
class ACameraActor;
class APointLight;
class AStaticMeshActor;
class UWSAgentGateway;

UCLASS()
class WHITEOUTSTATION_API AWhiteoutGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWhiteoutGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	void PrepareOpeningReveal();
	void FinishOpeningPresentation();

private:
	UPROPERTY()
	TObjectPtr<ACameraActor> OpeningCamera;

	UPROPERTY()
	TObjectPtr<UWSAgentGateway> IntentProbeGateway;

	UPROPERTY()
	TObjectPtr<APointLight> LookAtCaptureLight;

	UPROPERTY()
	TObjectPtr<AStaticMeshActor> CharacterCaptureGround;

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
	int32 JumpCaptureIndex = 0;
	float JumpStartZ = 0.0f;
	float JumpMaxZ = 0.0f;
	bool bJumpOverlapDetected = false;
	TArray<float> JumpHeightSamples;
	FGuid DialogueHistoryProbeSessionId;

	void RunAutomationRoute(const FString& RouteName);
	void RunDialogueHistoryProbeStep(int32 Step);
	void SetupInputSmokeTarget(const FString& ActionId);
	void CompletePerformanceTest();
	void BeginOpeningPresentation();
	void BeginBaselineCapture();
	void StageBaselineView();
	void CaptureBaselineView();
	void BeginPresentationCapture();
	void StagePresentationCapture();
	void CapturePresentationFrame();
	void RunSettingsAudit(const FString& Mode);
	void BeginJumpCapture();
	void CaptureJumpFrame();
};
