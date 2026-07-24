#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "State/WindStationTypes.h"
#include "WSPresentationData.generated.h"

class UMaterialInterface;
class UStaticMesh;
class USkeletalMesh;
class UAnimSequence;
class UAnimBlueprint;

USTRUCT(BlueprintType)
struct FWSStationMeshPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Zone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTransform Transform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCollision = true;
};

USTRUCT(BlueprintType)
struct FWSStationLightPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Zone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Intensity = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Radius = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bEmergencyRed = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bGeneratorPowered = false;
};

USTRUCT(BlueprintType)
struct FWSReasonPresentation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Cause;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText ChangeCondition;
};

UCLASS(BlueprintType)
class WHITEOUTSTATION_API UWSStationAssemblyData : public UDataAsset
{
	GENERATED_BODY()

public:
	UWSStationAssemblyData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Assembly")
	TArray<FWSStationMeshPlacement> Placements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Assembly")
	TArray<FWSStationLightPlacement> Lights;
};

UCLASS(BlueprintType)
class WHITEOUTSTATION_API UWSUIDesignData : public UDataAsset
{
	GENERATED_BODY()

public:
	UWSUIDesignData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Panel = FLinearColor(0.012f, 0.025f, 0.04f, 0.94f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Cyan = FLinearColor(0.25f, 0.78f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Amber = FLinearColor(1.0f, 0.68f, 0.20f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Danger = FLinearColor(1.0f, 0.20f, 0.09f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float PanelPadding = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float SmallTextSize = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float BodyTextSize = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float HeadingTextSize = 30.0f;
};

UCLASS(BlueprintType)
class WHITEOUTSTATION_API UWSReasonPresentationData : public UDataAsset
{
	GENERATED_BODY()

public:
	UWSReasonPresentationData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rejection Copy")
	TMap<EWSReasonCode, FWSReasonPresentation> Reasons;
};

// ============================================================================
// 角色资产表 —— 改一个数据资产即可换角色，无需改 C++
// 在编辑器里 Content Browser 右键 → Miscellaneous → Data Asset → 选 WSCharacterAssetData
// 新建实例后填入新模型的路径，再把这个资产拖到关卡里对应 NPC 的
// "Character Asset" 字段即可。
// ============================================================================

USTRUCT(BlueprintType)
struct FWSAnimSetConfig
{
	GENERATED_BODY()

	// 待机循环（必填）。其余为可选反应动画，留空即跳过。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> Idle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> Gesture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> Guarded;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> Work;
};

USTRUCT(BlueprintType)
struct FWSInjuryWrapConfig
{
	GENERATED_BODY()

	// 留空 Mesh 即可关闭绷带；填了就挂在角色手部 socket 上。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Injury Wrap")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Injury Wrap")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Injury Wrap")
	FName AttachSocket = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Injury Wrap")
	FVector RelativeScale = FVector(0.006f, 0.006f, 0.0024f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Injury Wrap")
	FRotator RelativeRotation = FRotator(0.0f, 0.0f, 90.0f);
};

UCLASS(BlueprintType)
class WHITEOUTSTATION_API UWSCharacterAssetData : public UDataAsset
{
	GENERATED_BODY()

public:
	UWSCharacterAssetData();

	// 必填：骨骼网格资源。可以是 MakeHuman/MetaHuman/VRM4U 转化的任意 SK 资源。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	// 动画蓝图。如果新模型用项目自带动画，留空会回退到旧路径或播放 AnimSet。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimBlueprint> AnimBlueprint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FWSAnimSetConfig Animations;

	// 眼睛材质槽覆盖。SlotName 匹配不上就跳过。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	FName EyeMaterialSlotName = TEXT("high-poly");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> EyeMaterial;

	// 角色网格相对 Root 的变换。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FVector MeshLocation = FVector(0.0f, -11.6f, 2.7f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FRotator MeshRotation = FRotator(0.0f, -90.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FVector MeshScale = FVector(0.1f);

	// 角色整体的初始缩放（站立高度）。默认 1.0。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FVector ActorScale = FVector::OneVector;

	// 绷带/外饰物（顾衡的应急包等）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Injury Wrap")
	FWSInjuryWrapConfig InjuryWrap;
};
