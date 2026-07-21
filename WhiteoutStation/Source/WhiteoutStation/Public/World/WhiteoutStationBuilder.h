#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "State/WindStationTypes.h"
#include "WhiteoutStationBuilder.generated.h"

class AWSInteractableActor;
class AStaticMeshActor;
class UDirectionalLightComponent;
class UPointLightComponent;
class UWSStationAssemblyData;
struct FWSStationMeshPlacement;
struct FWSStationLightPlacement;
struct FWSActionResult;

UCLASS()
class WHITEOUTSTATION_API AWhiteoutStationBuilder : public AActor
{
	GENERATED_BODY()

public:
	AWhiteoutStationBuilder();
	virtual void BeginPlay() override;
	void SetLightingPreviewState(bool bCrisis, bool bGeneratorOnline);

private:
	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> RuntimeLights;

	TArray<bool> RuntimeEmergencyLights;
	TArray<bool> RuntimeGeneratorLights;
	TArray<float> RuntimeBaseLightIntensities;
	TArray<FLinearColor> RuntimeBaseLightColors;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> ExteriorLight;

	UPROPERTY()
	TArray<TObjectPtr<AStaticMeshActor>> RuntimeAssemblyMeshes;

	UPROPERTY()
	TArray<TObjectPtr<AWSInteractableActor>> RuntimeHotspots;

	FTimerHandle CrisisLightingTimer;
	FTimerHandle SceneAuditTimer;
	int32 CrisisLightingStep = 0;
	bool bCrisisSequenceStarted = false;
	bool bEndingPresentationApplied = false;

	void BuildStation();
	void SpawnStationAssembly();
	void SpawnAssemblyMesh(const FWSStationMeshPlacement& Placement);
	void SpawnAssemblyLight(const FWSStationLightPlacement& Placement);
	void SpawnBlock(const FString& Label, FVector Location, FVector Scale, FLinearColor Color);
	void SpawnSign(const FString& Text, FVector Location, FRotator Rotation, FLinearColor Color);
	void SpawnPointLight(const FString& Label, FName Zone, FVector Location, FLinearColor Color, float Intensity, float Radius, bool bEmergencyRed, bool bGeneratorPowered);
	AWSInteractableActor* SpawnHotspot(
		const TCHAR* ActionId,
		const TCHAR* Label,
		FVector Location,
		FLinearColor Color,
		FVector Scale = FVector(0.55f, 0.55f, 0.9f));

	UFUNCTION()
	void HandleActionCommitted(const FWSActionResult& Result);

	UFUNCTION()
	void HandleStateChanged(const FWSGameState& State);

	void BeginCrisisLightingSequence();
	void AdvanceCrisisLightingSequence();
	void ApplyCrisisLighting();
	void RestoreGeneratorLighting();
	void ApplyEndingPresentation(EWSEndingType Ending);
	void AuditStationLayout();
};
