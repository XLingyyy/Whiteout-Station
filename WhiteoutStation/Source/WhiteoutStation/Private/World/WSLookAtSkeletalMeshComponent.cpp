#include "World/WSLookAtSkeletalMeshComponent.h"

#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"

void UWSLookAtSkeletalMeshComponent::SetLookAtAngles(const float InYawDegrees, const float InPitchDegrees)
{
	LookAtYawDegrees = FMath::Clamp(InYawDegrees, -20.0f, 20.0f);
	LookAtPitchDegrees = FMath::Clamp(InPitchDegrees, -12.0f, 15.0f);
}

int32 UWSLookAtSkeletalMeshComponent::ResolveHeadBoneIndex()
{
	const USkeletalMesh* MeshAsset = GetSkeletalMeshAsset();
	if (!MeshAsset)
	{
		return INDEX_NONE;
	}
	if (CachedMesh.Get() == MeshAsset && CachedHeadBoneIndex != INDEX_NONE)
	{
		return CachedHeadBoneIndex;
	}
	CachedMesh = MeshAsset;
	CachedHeadBoneIndex = INDEX_NONE;
	const FReferenceSkeleton& Skeleton = MeshAsset->GetRefSkeleton();
	for (const FName Candidate : {
		FName(TEXT("head")), FName(TEXT("Head")), FName(TEXT("head.x")), FName(TEXT("J_Bip_C_Head")),
		FName(TEXT("neck_01")), FName(TEXT("neck")), FName(TEXT("Neck"))})
	{
		const int32 Index = Skeleton.FindBoneIndex(Candidate);
		if (Index != INDEX_NONE)
		{
			CachedHeadBoneIndex = Index;
			break;
		}
	}
	return CachedHeadBoneIndex;
}

void UWSLookAtSkeletalMeshComponent::FinalizeBoneTransform()
{
	TArray<FTransform>& ComponentTransforms = GetEditableComponentSpaceTransforms();
	const int32 HeadIndex = ResolveHeadBoneIndex();
	const USkeletalMesh* MeshAsset = GetSkeletalMeshAsset();
	if (MeshAsset && ComponentTransforms.IsValidIndex(HeadIndex)
		&& (!FMath::IsNearlyZero(LookAtYawDegrees, 0.01f) || !FMath::IsNearlyZero(LookAtPitchDegrees, 0.01f)))
	{
		const FReferenceSkeleton& Skeleton = MeshAsset->GetRefSkeleton();
		const FTransform OldHead = ComponentTransforms[HeadIndex];
		// Imported VRM characters visually face component-local +Y, making
		// component-local -X their visual right axis.
		const FVector WorldVisualRight = -GetForwardVector();
		const FQuat WorldYaw(FVector::UpVector, FMath::DegreesToRadians(LookAtYawDegrees));
		const FQuat WorldPitch(WorldVisualRight, FMath::DegreesToRadians(-LookAtPitchDegrees));
		const FQuat MeshComponentToWorld = GetComponentTransform().GetRotation();
		const FQuat ComponentDelta = MeshComponentToWorld.Inverse() * (WorldPitch * WorldYaw) * MeshComponentToWorld;
		FTransform NewHead = OldHead;
		NewHead.SetRotation((ComponentDelta * OldHead.GetRotation()).GetNormalized());

		TArray<FTransform> DescendantRelativeTransforms;
		TArray<int32> DescendantIndices;
		for (int32 BoneIndex = HeadIndex + 1; BoneIndex < ComponentTransforms.Num(); ++BoneIndex)
		{
			int32 ParentIndex = Skeleton.GetParentIndex(BoneIndex);
			while (ParentIndex != INDEX_NONE && ParentIndex != HeadIndex)
			{
				ParentIndex = Skeleton.GetParentIndex(ParentIndex);
			}
			if (ParentIndex == HeadIndex)
			{
				DescendantIndices.Add(BoneIndex);
				DescendantRelativeTransforms.Add(ComponentTransforms[BoneIndex].GetRelativeTransform(OldHead));
			}
		}
		ComponentTransforms[HeadIndex] = NewHead;
		for (int32 Index = 0; Index < DescendantIndices.Num(); ++Index)
		{
			ComponentTransforms[DescendantIndices[Index]] = DescendantRelativeTransforms[Index] * NewHead;
		}
	}
	Super::FinalizeBoneTransform();
}
