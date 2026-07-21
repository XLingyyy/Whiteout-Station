#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "State/WindStationTypes.h"
#include "WhiteoutHUDWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UProgressBar;
class UScrollBox;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UFont;
class USoundBase;
class UTexture2D;

UCLASS()
class WHITEOUTSTATION_API UWhiteoutHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void SetInteractionPrompt(const FText& Prompt);
	void SetInteractionFocus(const FText& ActionName, const FWSActionPreview& Preview);
	void ClearInteractionFocus();
	void SetActionFeedback(const FText& ActionName, const FWSActionResult& Result, const FWSActionPreview& Preview, bool bPromiseCreated = false);
	void ShowActionPreview(const FText& ActionName, const FWSActionPreview& Preview);
	void HideActionPreview();
	void ToggleEvidence();
	void ShowDialogueMenu(int32 SelectedIndex, bool bVisible);
	void SetSystemMessage(const FString& Message);
	void DismissOpening();
	void TogglePauseMenu();
	bool IsPauseMenuVisible() const;
	void ResetPresentationCapture();
	void SetPresentationCaptureState(const FWSGameState& State);
	void ShowEvidenceForCapture();
	void ShowComponentGalleryForCapture();
	void SetOpeningCaptureStage(int32 Stage);
	void SetCrisisCaptureStage(int32 Stage);
	void SetEndingCaptureStage(EWSEndingType Ending, bool bShowResults);

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TopText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectiveText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CrewText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CrewCardTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> CrewStatusBars;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> CrewTrustBars;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FeedbackText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CrosshairText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> FocusBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FocusText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ToastBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ToastText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PreviewBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreviewTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreviewBodyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreviewFooterText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EvidenceBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EvidenceText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EvidenceTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EvidenceFilterText;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> EvidenceCardGrid;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EvidenceProgressText;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> EvidenceScroll;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DialogueBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogueText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> OpeningBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OpeningText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CrisisBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CrisisText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EndingCinematicBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndingCinematicText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ResultsBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultsText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultsTimelineText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultsCrewText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultsAdviceText;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ResultsScroll;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ResultScoreTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> ResultScoreBars;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PauseBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PauseStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PauseHelpText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PauseSituationText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PauseDefaultButton;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ComponentGalleryBorder;

	UPROPERTY(Transient)
	TObjectPtr<UFont> UIFontFamily;

	UPROPERTY(Transient)
	TObjectPtr<UObject> UIStringTableAsset;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> UIHoverSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> UIConfirmSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> UIRejectSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> UIPromiseSound;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> InkBrushTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PlayerPortraitTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> GuHengPortraitTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> YeChengPortraitTexture;

	FText InteractionPrompt;
	FString FocusedActionName;
	FString SystemMessage;
	float OpeningElapsed = 0.0f;
	float ToastRemaining = 0.0f;
	float CrisisElapsed = -1.0f;
	float EndingElapsed = -1.0f;
	int32 ActiveOpeningStage = INDEX_NONE;
	int32 ActiveCrisisStage = INDEX_NONE;
	bool bWasShowingResults = false;
	bool bEndingResultsRevealed = false;
	bool bEndingCinematicCapture = false;
	EWSEndingType ActiveEnding = EWSEndingType::SurvivalWait;
	bool bEvidenceVisible = false;
	bool bDialogueVisible = false;
	bool bPresentationCaptureOverride = false;
	FWSGameState PresentationCaptureState;

	void BuildWidgetTree();
	void InitializeUIFontFamily();
	void UpdateFromState(const FWSGameState& State);
	void UpdateEvidence(const FWSGameState& State);
	void UpdateResults(const FWSGameState& State);
	void ApplyOpeningStage(int32 Stage);
	void ApplyCrisisStage(int32 Stage);
	void BeginEndingCinematic(EWSEndingType Ending);
	void ApplyEndingCinematic(EWSEndingType Ending);
	void PlayUISound(USoundBase* Sound, float Volume = 1.0f);
	UTextBlock* MakeText(const FName Name, int32 Size, const FLinearColor& Color, bool bWrap = true);
	UBorder* MakePanel(UCanvasPanel* Canvas, const FName Name, const FAnchors& Anchors, const FMargin& Offsets, const FLinearColor& Color);
	UButton* MakeButton(UVerticalBox* Box, const FText& Label, const FName Name);
	FSlateFontInfo UIFont(int32 Size, bool bBold = false) const;
	static FString APCells(int32 Remaining);
	static FString ClockForAP(int32 Remaining);
	static float ScoreRatio(float Value, float Maximum);

	UFUNCTION()
	void ResumeGame();

	UFUNCTION()
	void RestartGame();

	UFUNCTION()
	void ToggleControls();

	UFUNCTION()
	void QuitGame();

	UFUNCTION()
	void PlayHoverSound();
};
