#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "State/WindStationTypes.h"
#include "WhiteoutAudioDirector.generated.h"

class UAudioComponent;
class USceneComponent;
class USoundBase;

UCLASS()
class WHITEOUTSTATION_API AWhiteoutAudioDirector : public AActor
{
	GENERATED_BODY()

public:
	AWhiteoutAudioDirector();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UAudioComponent> OutdoorWind;

	UPROPERTY()
	TObjectPtr<UAudioComponent> IndoorWind;

	UPROPERTY()
	TObjectPtr<UAudioComponent> GeneratorLoop;

	UPROPERTY()
	TObjectPtr<USoundBase> CrisisStinger;

	UPROPERTY()
	TObjectPtr<USoundBase> RadioReply;

	UPROPERTY()
	TObjectPtr<USoundBase> EndingSuccess;

	UPROPERTY()
	TObjectPtr<USoundBase> EndingSurvival;

	UPROPERTY()
	TObjectPtr<USoundBase> EndingCost;

	UPROPERTY()
	TObjectPtr<USoundBase> EndingCollapse;

	bool bCrisisActive = false;
	bool bGeneratorOnline = false;
	bool bEndingAudioPlayed = false;

	UFUNCTION()
	void HandleActionCommitted(const FWSActionResult& Result);

	UFUNCTION()
	void HandleStateChanged(const FWSGameState& State);

	void ApplyState(const FWSGameState& State);
	void PlayEndingAudio(EWSEndingType Ending);
};
