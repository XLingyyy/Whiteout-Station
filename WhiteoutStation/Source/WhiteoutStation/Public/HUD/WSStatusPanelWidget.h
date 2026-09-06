#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Presentation/WSStatusPresenter.h"
#include "WSStatusPanelWidget.generated.h"

class UFont;
class UTextBlock;
class UBorder;

UCLASS()
class WHITEOUTSTATION_API UWSStatusPanelWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Build(UFont* Font);
	void Present(const FWSStatusCardViewModel& Player, const TOptional<FWSStatusCardViewModel>& Target);
	void AdvanceAnimation(float DeltaTime, bool ReducedMotion);
private:
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Titles;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Labels;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Values;
	UPROPERTY() TObjectPtr<UBorder> TargetCard;
	UPROPERTY() TObjectPtr<UTextBlock> DiagnosisSource;
	float TargetOpacity = 1.0f;
};
