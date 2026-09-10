#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Presentation/WSStatusPresenter.h"
#include "WSStatusPanelWidget.generated.h"

class UFont;
class UTextBlock;
class UBorder;
class USizeBox;

UCLASS()
class WHITEOUTSTATION_API UWSStatusPanelWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Build(UFont* Font);
	void Present(const FWSStatusCardViewModel& Player, const TOptional<FWSStatusCardViewModel>& Target);
	void AdvanceAnimation(float DeltaTime, bool ReducedMotion);
	FGeometry CardGeometry(bool bTarget) const;
	virtual int32 NativePaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
private:
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Titles;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Labels;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Values;
	UPROPERTY() TArray<TObjectPtr<USizeBox>> Icons;
	UPROPERTY() TObjectPtr<UBorder> TargetCard;
	UPROPERTY() TObjectPtr<UBorder> PlayerCard;
	UPROPERTY() TObjectPtr<UTextBlock> DiagnosisSource;
	float TargetOpacity = 1.0f;
};
