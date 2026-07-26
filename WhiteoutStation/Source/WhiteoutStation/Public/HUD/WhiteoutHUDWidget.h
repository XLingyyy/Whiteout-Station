#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "State/WindStationTypes.h"
#include "WhiteoutHUDWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UImage;
class UProgressBar;
class UScrollBox;
class USlider;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UFont;
class USoundBase;
class UTexture2D;

enum class EWSUILayer : uint8
{
	Game,
	Preview,
	Evidence,
	Dialogue,
	Guide,
	Pause,
	Settings,
	Results
};

enum class EWSDialogueStage : uint8
{
	Opening,
	IntentPick,
	TextEntry,
	Reply
};

enum class EWSOpeningPhase : uint8
{
	FadingInLine,
	AwaitingAdvance,
	FadingOutLine,
	RevealingStation,
	Complete
};

// 轻量 tick 驱动的面板过渡动效
struct FPanelAnimation
{
	TWeakObjectPtr<UBorder> Panel;
	float StartOpacity = 0.0f;
	float TargetOpacity = 1.0f;
	float Elapsed = 0.0f;
	float Duration = 0.28f;
	bool bCollapseOnComplete = false;
	bool bScaleWithFade = true;
};

UCLASS()
class WHITEOUTSTATION_API UWhiteoutHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void SetInteractionPrompt(const FText& Prompt);
	void SetInteractionFocus(const FText& ActionName, const FWSActionPreview& Preview, bool bDialogue = false);
	void ClearInteractionFocus();
	void SetActionFeedback(const FText& ActionName, const FWSActionResult& Result, const FWSActionPreview& Preview, bool bPromiseCreated = false);
	void ShowActionPreview(
		const FText& ActionName,
		const FWSActionPreview& Preview,
		const FWSActionRequest& Request = FWSActionRequest());
	void HideActionPreview();
	void ToggleEvidence();
	UFUNCTION()
	void CloseEvidence();
	void ShowDialogueMenu(FName NPCActionId, bool bVisible);
	void ShowDialoguePromiseChoices();
	UFUNCTION()
	void ShowDialogueWheelChoices();
	void ShowDialogueFreeTextForCapture();
	void ShowDialogueReplyForCapture(const FString& Speaker, const FString& Line);
	void SetDialogueIntentStatus(const FString& Message, bool bProcessing);
	void SetSystemMessage(const FString& Message);
	bool AdvanceOpening();
	bool IsOpeningVisible() const;
	void DismissOpening();
	void ToggleGuide();
	void TogglePauseMenu();
	void HandleBackRequested();
	bool IsPauseMenuVisible() const;
	void ResetPresentationCapture();
	void SetPresentationCaptureState(const FWSGameState& State);
	void ShowNPCFocusForCapture(const FText& ActionName, const FWSActionPreview& Preview);
	void ShowEvidenceForCapture(int32 FilterIndex = 0, bool bShowFirstDetail = false);
	void ShowComponentGalleryForCapture();
	void ShowSettingsForCapture();
	void SetOpeningCaptureStage(int32 Stage);
	void SetCrisisCaptureStage(int32 Stage);
	void SetEndingCaptureStage(EWSEndingType Ending, bool bShowResults);

private:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> TopPanel;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ObjectivePanel;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CrewPanel;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BottomPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TopText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TopStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TopConditionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectiveText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TutorialTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TutorialText;

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
	TObjectPtr<UTextBlock> FocusAPText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FocusKeyText;

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
	TArray<TObjectPtr<UButton>> EvidenceFilterButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> EvidenceFilterIndicators;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> EvidenceFilterLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> EvidenceFilterCounts;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EvidenceDetailText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> EvidenceCardButtons;

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
	TObjectPtr<UTextBlock> DialogueNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogueLineText;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> DialogueWheelPanel;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DialoguePromiseBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DialogueFreeTextBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DialogueReplyBorder;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> DialogueIntentButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> DialoguePromiseButtons;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> DialogueFreeTextInput;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogueStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogueNPCText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> DialogueNPCBars;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> OpeningBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OpeningText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> OpeningDivider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OpeningSubtitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OpeningFooterText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> GuideBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GuideContextText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CrisisBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CrisisText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EndingCinematicBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndingCinematicText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EndingDivider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndingSubtitleText;

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
	TArray<TObjectPtr<UTextBlock>> PauseSituationValues;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PauseDefaultButton;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> SettingsBorder;

	UPROPERTY(Transient)
	TObjectPtr<USlider> FOVSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> MasterVolumeSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> AmbienceVolumeSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> EffectsVolumeSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> FeedbackVolumeSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FOVValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MasterVolumeValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AmbienceVolumeValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EffectsVolumeValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FeedbackVolumeValueText;

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

	UPROPERTY(Transient)
	TMap<TObjectPtr<UBorder>, TObjectPtr<UBorder>> GlassPanelContentSlots;

	FText InteractionPrompt;
	FString FocusedActionName;
	FString SystemMessage;
	float OpeningElapsed = 0.0f;
	float ToastRemaining = 0.0f;
	float CrisisElapsed = -1.0f;
	float EndingElapsed = -1.0f;
	int32 ActiveOpeningStage = INDEX_NONE;
	EWSOpeningPhase OpeningPhase = EWSOpeningPhase::FadingInLine;
	TArray<FText> OpeningLines;
	int32 ActiveCrisisStage = INDEX_NONE;
	bool bWasShowingResults = false;
	bool bEndingResultsRevealed = false;
	bool bEndingCinematicCapture = false;
	EWSEndingType ActiveEnding = EWSEndingType::SurvivalWait;
	bool bEvidenceVisible = false;
	bool bDialogueVisible = false;
	bool bUpdatingSettings = false;
	int32 EvidenceFilterIndex = 0;
	EWSUILayer CurrentLayer = EWSUILayer::Game;
	EWSDialogueStage DialogueStage = EWSDialogueStage::Opening;
	EWSDialogueAct PendingDialogueAct = EWSDialogueAct::Ask;
	FName PendingPromiseCondition;
	TArray<FString> EvidenceCardDetailCopies;
	FName ActiveDialogueActionId;
	bool bPresentationCaptureOverride = false;
	bool bInteractionFocusCaptureLock = false;
	FString InteractionFocusCaptureName;
	FWSGameState PresentationCaptureState;

	// 动效系统
	TArray<FPanelAnimation> ActivePanelAnimations;

	void BuildWidgetTree();
	void InitializeUIFontFamily();
	void UpdateFromState(const FWSGameState& State);
	void UpdateEvidence(const FWSGameState& State);
	void SetLayer(EWSUILayer Layer);
	void SetBaseHudHidden(bool bHidden);
	void SetEvidenceFilter(int32 FilterIndex);
	void ShowEvidenceDetail(const FString& DetailCopy);
	void OpenDialogueTextEntry(EWSDialogueAct DialogueAct, FName PromiseCondition = NAME_None);
	void RefreshDialogueAvailability();
	void ReflowDialogueIntentButtons(const TArray<UButton*>& AvailableButtons);
	void ShowDialogueReplyActions();
	void UpdateDialogueCard(const FWSGameState& State);
	void UpdateResults(const FWSGameState& State);
	void TickOpening(float DeltaTime);
	void ApplyOpeningStage(int32 Stage);
	FString BuildTutorialHint(const FWSGameState& State) const;
	void UpdateGuideContext(const FWSGameState& State);
	void ApplyCrisisStage(int32 Stage);
	void BeginEndingCinematic(EWSEndingType Ending);
	void ApplyEndingCinematic(EWSEndingType Ending);
	void PlayUISound(USoundBase* Sound, float Volume = 1.0f);
	void ShowPanelAnimated(UBorder* Panel, bool bShow, float Duration = 0.28f, bool bScaleWithFade = true);
	void TickPanelAnimations(float DeltaTime);
	void CancelPanelAnimation(UBorder* Panel);
	void ShowPanelInstant(UBorder* Panel, bool bShow);
	UTextBlock* MakeText(const FName Name, int32 Size, const FLinearColor& Color, bool bWrap = true);
	UBorder* MakePanel(UCanvasPanel* Canvas, const FName Name, const FAnchors& Anchors, const FMargin& Offsets, const FLinearColor& Color);
	UBorder* MakeGlassPanel(UCanvasPanel* Canvas, FName Name, const FAnchors& Anchors,
		const FMargin& Offsets, float BlurStrength, const FLinearColor& Tint, bool bHairline = true);
	void SetGlassPanelContent(UBorder* Panel, UWidget* Content);
	void SetGlassPanelPadding(UBorder* Panel, const FMargin& ContentPadding);
	UButton* MakeButton(UVerticalBox* Box, const FText& Label, const FName Name);
	UButton* MakeDialogueChoiceButton(
		UCanvasPanel* Canvas,
		const FText& Label,
		const FString& IconName,
		const FName Name,
		const FAnchors& Anchors,
		const FMargin& Offsets);
	UProgressBar* MakeProgressBar(const FName Name, const FLinearColor& FillColor, float Height = 7.0f);
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
	void OpenSettings();

	UFUNCTION()
	void CloseSettings();

	UFUNCTION()
	void HandleFOVChanged(float Value);

	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);

	UFUNCTION()
	void HandleAmbienceVolumeChanged(float Value);

	UFUNCTION()
	void HandleEffectsVolumeChanged(float Value);

	UFUNCTION()
	void HandleFeedbackVolumeChanged(float Value);

	void RefreshSettingsUI();

	UFUNCTION()
	void QuitGame();

	UFUNCTION()
	void CloseGuide();

	UFUNCTION()
	void PlayHoverSound();

	UFUNCTION()
	void FilterEvidenceAll();

	UFUNCTION()
	void FilterEvidenceFiles();

	UFUNCTION()
	void FilterEvidenceItems();

	UFUNCTION()
	void FilterEvidenceWitnesses();

	UFUNCTION()
	void FilterEvidenceDialogue();

	UFUNCTION()
	void ShowHoveredEvidenceDetail();

	UFUNCTION()
	void ContinueDialogue();

	UFUNCTION()
	void ChooseDialogueAsk();

	UFUNCTION()
	void ChooseDialogueChallenge();

	UFUNCTION()
	void ChooseDialoguePromise();

	UFUNCTION()
	void ChooseDialogueReassure();

	UFUNCTION()
	void OpenDialogueFreeText();

	UFUNCTION()
	void ChoosePromiseKeepRecords();

	UFUNCTION()
	void ChoosePromisePreventSelfHarm();

	UFUNCTION()
	void ChoosePromiseRepairTogether();

	UFUNCTION()
	void SubmitDialogueFreeText();

	UFUNCTION()
	void CancelDialogue();

	UFUNCTION()
	void HandleDialogueTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleDialogueLine(const FWSAgentReply& Reply);
};
