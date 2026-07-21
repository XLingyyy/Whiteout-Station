#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "State/WindStationTypes.h"
#include "WhiteoutHUD.generated.h"

class UWhiteoutHUDWidget;

UCLASS()
class WHITEOUTSTATION_API AWhiteoutHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	void SetInteractionPrompt(const FText& Prompt);
	void SetInteractionFocus(const FText& ActionName, const FWSActionPreview& Preview, bool bDialogue = false);
	void ClearInteractionFocus();
	void SetActionFeedback(const FText& ActionName, const FWSActionResult& Result, const FWSActionPreview& Preview, bool bPromiseCreated = false);
	void ShowActionPreview(const FText& ActionName, const FWSActionPreview& Preview);
	void HideActionPreview();
	void ToggleEvidence();
	void ShowDialogueMenu(FName NPCActionId, bool bVisible);
	void ShowDialoguePromiseChoices();
	void ShowDialogueWheelChoices();
	void ShowDialogueFreeTextForCapture();
	void SetDialogueIntentStatus(const FString& Message, bool bProcessing);
	void SetSystemMessage(const FString& Message);
	void DismissOpening();
	void TogglePauseMenu();
	void ResetPresentationCapture();
	void SetPresentationCaptureState(const FWSGameState& State);
	void ShowEvidenceForCapture();
	void ShowComponentGalleryForCapture();
	void ShowSettingsForCapture();
	void SetOpeningCaptureStage(int32 Stage);
	void SetCrisisCaptureStage(int32 Stage);
	void SetEndingCaptureStage(EWSEndingType Ending, bool bShowResults);
	void SetInterfaceVisibleForCapture(bool bVisible);

private:
	UPROPERTY(Transient)
	TObjectPtr<UWhiteoutHUDWidget> HUDWidget;
};
