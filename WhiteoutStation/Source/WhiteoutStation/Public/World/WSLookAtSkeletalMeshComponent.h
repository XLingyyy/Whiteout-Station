#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "WSLookAtSkeletalMeshComponent.generated.h"

/** Applies a small component-space delta to the head branch after animation evaluation. */
UCLASS(ClassGroup = (WhiteoutStation), meta = (BlueprintSpawnableComponent))
class WHITEOUTSTATION_API UWSLookAtSkeletalMeshComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	void SetLookAtAngles(float InYawDegrees, float InPitchDegrees);
	float GetLookAtYaw() const { return LookAtYawDegrees; }
	float GetLookAtPitch() const { return LookAtPitchDegrees; }

	virtual void FinalizeBoneTransform() override;

private:
	float LookAtYawDegrees = 0.0f;
	float LookAtPitchDegrees = 0.0f;
	int32 CachedHeadBoneIndex = INDEX_NONE;
	TWeakObjectPtr<const USkeletalMesh> CachedMesh;

	int32 ResolveHeadBoneIndex();
};
