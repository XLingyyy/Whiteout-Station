#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WSTutorialTypes.generated.h"

class UTexture2D;

UENUM()
enum class EWSTutorialStatus : uint8 { Never, InProgress, Skipped, Completed };
UENUM()
enum class EWSTutorialResourceStatus : uint8 { Placeholder, Captured, Approved };
UENUM()
enum class EWSTutorialMode : uint8 { Any, Offline, OnlineReady };

USTRUCT()
struct FWSTutorialPreference
{
	GENERATED_BODY()
	UPROPERTY() int32 Version = 1;
	UPROPERTY() EWSTutorialStatus Status = EWSTutorialStatus::Never;
	UPROPERTY() FName LastPageId = TEXT("T01");
	UPROPERTY() int32 UpgradeHintShownVersion = 0;
};

USTRUCT(BlueprintType)
struct FWSTutorialPage
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName PageId;
	UPROPERTY(EditAnywhere) FText Title;
	UPROPERTY(EditAnywhere) FText Lead;
	UPROPERTY(EditAnywhere) FText Body;
	UPROPERTY(EditAnywhere) FText Hint;
	UPROPERTY(EditAnywhere) EWSTutorialMode ModeRequirement = EWSTutorialMode::Any;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UTexture2D> PrimaryImage;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UTexture2D> SecondaryImage;
	UPROPERTY(EditAnywhere) FName ImageRole;
	UPROPERTY(EditAnywhere) FText AltText;
	UPROPERTY(EditAnywhere) EWSTutorialResourceStatus ResourceStatus = EWSTutorialResourceStatus::Placeholder;
	UPROPERTY(EditAnywhere) FString CaptureManifestId;
	UPROPERTY(EditAnywhere) int32 UIRevision = 1;
};

UCLASS()
class WHITEOUTSTATION_API UWSTutorialData : public UDataAsset
{
	GENERATED_BODY()
public:
	UWSTutorialData();
	UPROPERTY(EditAnywhere) int32 TutorialVersion = 1;
	UPROPERTY(EditAnywhere) int32 UIRevision = 1;
	UPROPERTY(EditAnywhere) TArray<FWSTutorialPage> Pages;
};
