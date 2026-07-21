#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WhiteoutStationBuilder.generated.h"

class AWSInteractableActor;
class UDirectionalLightComponent;
class UPointLightComponent;
class UWSStationAssemblyData;
struct FWSStationMeshPlacement;
struct FWSActionResult;

UCLASS()
class WHITEOUTSTATION_API AWhiteoutStationBuilder : public AActor
{
	GENERATED_BODY()

public:
	AWhiteoutStationBuilder();
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> RuntimeLights;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> ExteriorLight;

	void BuildStation();
	void SpawnStationAssembly();
	void SpawnAssemblyMesh(const FWSStationMeshPlacement& Placement);
	void SpawnBlock(const FString& Label, FVector Location, FVector Scale, FLinearColor Color);
	void SpawnSign(const FString& Text, FVector Location, FRotator Rotation, FLinearColor Color);
	void SpawnPointLight(const FString& Label, FVector Location, FLinearColor Color, float Intensity, float Radius);
	AWSInteractableActor* SpawnHotspot(
		const TCHAR* ActionId,
		const TCHAR* Label,
		FVector Location,
		FLinearColor Color,
		FVector Scale = FVector(0.55f, 0.55f, 0.9f));

	UFUNCTION()
	void HandleActionCommitted(const FWSActionResult& Result);

	void ApplyCrisisLighting();
};
