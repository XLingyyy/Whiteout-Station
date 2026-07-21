#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "State/WindStationTypes.h"
#include "WSPresentationData.generated.h"

class UMaterialInterface;
class UStaticMesh;

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
