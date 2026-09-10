#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Flow/WSTutorialFlow.h"
#include "WSTutorialWidget.generated.h"

class UFont;
class UTextBlock;
class UButton;
class UBorder;
class UImage;
class USizeBox;
class UHorizontalBox;
class UVerticalBox;
class UBackgroundBlur;
struct FStreamableHandle;

DECLARE_DELEGATE_OneParam(FWSTutorialClosed, EWSTutorialStatus);
DECLARE_DELEGATE_OneParam(FWSTutorialPageChanged, FName);

UCLASS()
class WHITEOUTSTATION_API UWSTutorialWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Build(UFont* Font);
	void Preload(bool bOnlineReady);
	bool ImagesReady() const { return bImagesReady; }
	void Open(const FWSTutorialPreference& Preference, bool bReplay);
	void Shutdown();
	void Back();
	FWSTutorialClosed OnClosed;
	FWSTutorialPageChanged OnPageChanged;
	int32 GetPageIndex() const { return Flow.GetPage(); }
	bool IsReplay() const { return Flow.IsReplay(); }
	static TArray<FWSTutorialPage> DefaultPages();
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual FReply NativeOnKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual FReply NativeOnKeyUp(const FGeometry&, const FKeyEvent&) override;
private:
	FWSTutorialFlow Flow;
	UPROPERTY() TObjectPtr<UWSTutorialData> Data;
	UPROPERTY() TArray<FWSTutorialPage> Pages;
	UPROPERTY() TArray<TObjectPtr<UTexture2D>> Images;
	UPROPERTY() TObjectPtr<UFont> FontFamily;
	UPROPERTY() TObjectPtr<UBorder> Panel;
	UPROPERTY() TObjectPtr<UBorder> Confirm;
	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TObjectPtr<UTextBlock> PageNumber;
	UPROPERTY() TObjectPtr<UTextBlock> Lead;
	UPROPERTY() TObjectPtr<UTextBlock> Body;
	UPROPERTY() TObjectPtr<UTextBlock> Hint;
	UPROPERTY() TObjectPtr<UTextBlock> MissingImage;
	UPROPERTY() TObjectPtr<UTextBlock> ContinueLabel;
	UPROPERTY() TObjectPtr<UImage> PrimaryImage;
	UPROPERTY() TObjectPtr<UImage> SecondaryImage;
	UPROPERTY() TObjectPtr<UButton> PreviousButton;
	UPROPERTY() TObjectPtr<USizeBox> ImageColumn;
	UPROPERTY() TObjectPtr<UBackgroundBlur> Blur;
	TSharedPtr<FStreamableHandle> LoadHandle;
	uint32 Generation = 0;
	bool bPreloadStarted = false;
	bool bImagesReady = false;
	bool bApplicationActive = true;
	bool bClosePending = false;
	bool bConfirming = false;
	int32 CloseReleaseFrames = 0;
	EWSTutorialStatus CloseReason = EWSTutorialStatus::InProgress;
	TSet<FKey> HeldKeys;
	FVector2D LayoutSize = FVector2D::ZeroVector;
	void FinishPreload(uint32 ExpectedGeneration);
	void ShowPage();
	void RequestClose(EWSTutorialStatus Reason);
	void HandleApplicationActivation(bool bActive);
	UFUNCTION() void Next();
	UFUNCTION() void Previous();
	UFUNCTION() void Skip();
	UFUNCTION() void ConfirmSkip();
	UFUNCTION() void ContinueReading();
};
