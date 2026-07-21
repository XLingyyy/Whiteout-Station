#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "State/WindStationTypes.h"
#include "WhiteoutHUDWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;
class UVerticalBox;
class UFont;

UCLASS()
class WHITEOUTSTATION_API UWhiteoutHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void SetInteractionPrompt(const FText& Prompt);
	void SetActionFeedback(const FText& ActionName, const FWSActionResult& Result, const FWSActionPreview& Preview);
	void ShowActionPreview(const FText& ActionName, const FWSActionPreview& Preview);
	void HideActionPreview();
	void ToggleEvidence();
	void ShowDialogueMenu(int32 SelectedIndex, bool bVisible);
	void SetSystemMessage(const FString& Message);
	void DismissOpening();
	void TogglePauseMenu();
	bool IsPauseMenuVisible() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TopText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectiveText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CrewText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FeedbackText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptText;

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
	TObjectPtr<UBorder> DialogueBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogueText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> OpeningBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ResultsBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultsText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PauseBorder;

	UPROPERTY(Transient)
	TObjectPtr<UFont> UIFontFamily;

	FText InteractionPrompt;
	FString SystemMessage = TEXT("靠近带有蓝色轮廓的设备，按 F 查看行动。");
	float OpeningElapsed = 0.0f;
	bool bEvidenceVisible = false;
	bool bDialogueVisible = false;

	void BuildWidgetTree();
	void InitializeUIFontFamily();
	void UpdateFromState(const FWSGameState& State);
	void UpdateEvidence(const FWSGameState& State);
	void UpdateResults(const FWSGameState& State);
	UTextBlock* MakeText(const FName Name, int32 Size, const FLinearColor& Color, bool bWrap = true);
	UBorder* MakePanel(UCanvasPanel* Canvas, const FName Name, const FAnchors& Anchors, const FMargin& Offsets, const FLinearColor& Color);
	UButton* MakeButton(UVerticalBox* Box, const FText& Label, const FName Name);
	FSlateFontInfo UIFont(int32 Size, bool bBold = false) const;
	static FString APCells(int32 Remaining);

	UFUNCTION()
	void ResumeGame();

	UFUNCTION()
	void RestartGame();

	UFUNCTION()
	void QuitGame();
};
