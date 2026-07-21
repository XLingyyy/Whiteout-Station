#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "State/WindStationTypes.h"
#include "WhiteoutSnowField.generated.h"

class UNiagaraComponent;
class USceneComponent;
struct FWSActionResult;

UCLASS()
class WHITEOUTSTATION_API AWhiteoutSnowField : public AActor
{
	GENERATED_BODY()

public:
	AWhiteoutSnowField();
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UNiagaraComponent>> BlizzardLayers;

	UFUNCTION()
	void HandleActionCommitted(const FWSActionResult& Result);

	UFUNCTION()
	void HandleStateChanged(const FWSGameState& State);

	void SetBlizzardIntensity(bool bCrisis);
	void SetEndingIntensity(EWSEndingType Ending);
};
