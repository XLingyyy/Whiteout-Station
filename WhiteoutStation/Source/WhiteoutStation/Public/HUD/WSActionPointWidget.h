#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Presentation/WSAPViewModel.h"
#include "WSActionPointWidget.generated.h"

class UTextBlock;
class USizeBox;
class UFont;

UCLASS()
class WHITEOUTSTATION_API UWSActionPointWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Build(UFont* Font);
	void Present(const FWSAPViewModel& Model);
	void AnimateSpend(int32 Cost, bool bReducedMotion);
	const FWSAPViewModel& GetModel() const { return View; }
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
	FWSAPViewModel View;
	double SpendStarted = -1;
	int32 SpendCount = 0;
	UPROPERTY() TObjectPtr<UTextBlock> Phase;
	UPROPERTY() TObjectPtr<UTextBlock> Number;
	UPROPERTY() TObjectPtr<UTextBlock> Hint;
	UPROPERTY() TObjectPtr<UTextBlock> Track;
	UPROPERTY() TObjectPtr<USizeBox> Segments;
};
