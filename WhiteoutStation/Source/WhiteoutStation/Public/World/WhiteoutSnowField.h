#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WhiteoutSnowField.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class WHITEOUTSTATION_API AWhiteoutSnowField : public AActor
{
	GENERATED_BODY()

public:
	AWhiteoutSnowField();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SnowInstances;

	TArray<FVector> ParticlePositions;
	TArray<float> ParticleSpeeds;
	FRandomStream RandomStream{1701};

	static constexpr int32 ParticleCount = 220;
	static constexpr float MinX = 1650.0f;
	static constexpr float MaxX = 3400.0f;
	static constexpr float MinY = -950.0f;
	static constexpr float MaxY = 1750.0f;
	static constexpr float MinZ = 10.0f;
	static constexpr float MaxZ = 720.0f;
};
