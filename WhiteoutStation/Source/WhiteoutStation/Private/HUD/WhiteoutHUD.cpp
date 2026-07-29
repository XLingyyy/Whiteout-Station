#include "HUD/WhiteoutHUD.h"

#include "HUD/WhiteoutHUDWidget.h"

void AWhiteoutHUD::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		HUDWidget = CreateWidget<UWhiteoutHUDWidget>(PlayerController, UWhiteoutHUDWidget::StaticClass());
		if (HUDWidget)
		{
			HUDWidget->AddToViewport(20);
			UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: HUD host created native UMG widget"));
		}
	}
}

void AWhiteoutHUD::DrawHUD()
{
	Super::DrawHUD();
}

void AWhiteoutHUD::SetInteractionPrompt(const FText& Prompt)
{
	if (HUDWidget) HUDWidget->SetInteractionPrompt(Prompt);
}

void AWhiteoutHUD::SetInteractionFocus(const FText& ActionName, const FWSActionPreview& Preview, const bool bDialogue)
{
	if (HUDWidget) HUDWidget->SetInteractionFocus(ActionName, Preview, bDialogue);
}

void AWhiteoutHUD::ClearInteractionFocus()
{
	if (HUDWidget) HUDWidget->ClearInteractionFocus();
}

void AWhiteoutHUD::SetActionFeedback(
	const FText& ActionName,
	const FWSActionResult& Result,
	const FWSActionPreview& Preview,
	const bool bPromiseCreated)
{
	if (HUDWidget) HUDWidget->SetActionFeedback(ActionName, Result, Preview, bPromiseCreated);
}

void AWhiteoutHUD::ShowActionPreview(
	const FText& ActionName,
	const FWSActionPreview& Preview,
	const FWSActionRequest& Request)
{
	if (HUDWidget) HUDWidget->ShowActionPreview(ActionName, Preview, Request);
}

void AWhiteoutHUD::HideActionPreview()
{
	if (HUDWidget) HUDWidget->HideActionPreview();
}

void AWhiteoutHUD::ToggleEvidence()
{
	if (HUDWidget) HUDWidget->ToggleEvidence();
}

void AWhiteoutHUD::ShowDialogueMenu(const FName NPCActionId, const bool bVisible)
{
	if (HUDWidget) HUDWidget->ShowDialogueMenu(NPCActionId, bVisible);
}

void AWhiteoutHUD::ShowDialoguePromiseChoices()
{
	if (HUDWidget) HUDWidget->ShowDialoguePromiseChoices();
}

void AWhiteoutHUD::ShowDialogueWheelChoices()
{
	if (HUDWidget) HUDWidget->ShowDialogueWheelChoices();
}

void AWhiteoutHUD::ShowDialogueFreeTextForCapture()
{
	if (HUDWidget) HUDWidget->ShowDialogueFreeTextForCapture();
}

void AWhiteoutHUD::ShowDialogueReplyForCapture(const FString& Speaker, const FString& Line)
{
	if (HUDWidget) HUDWidget->ShowDialogueReplyForCapture(Speaker, Line);
}

void AWhiteoutHUD::SetDialogueIntentStatus(const FString& Message, const bool bProcessing)
{
	if (HUDWidget) HUDWidget->SetDialogueIntentStatus(Message, bProcessing);
}

void AWhiteoutHUD::SetSystemMessage(const FString& Message)
{
	if (HUDWidget) HUDWidget->SetSystemMessage(Message);
}

bool AWhiteoutHUD::AdvanceOpening()
{
	return HUDWidget && HUDWidget->AdvanceOpening();
}

bool AWhiteoutHUD::IsOpeningVisible() const
{
	return HUDWidget && HUDWidget->IsOpeningVisible();
}

void AWhiteoutHUD::DismissOpening()
{
	if (HUDWidget) HUDWidget->DismissOpening();
}

void AWhiteoutHUD::ToggleGuide()
{
	if (HUDWidget) HUDWidget->ToggleGuide();
}

void AWhiteoutHUD::TogglePauseMenu()
{
	if (HUDWidget) HUDWidget->TogglePauseMenu();
}

void AWhiteoutHUD::HandleBackRequested()
{
	if (HUDWidget) HUDWidget->HandleBackRequested();
}

void AWhiteoutHUD::ResetPresentationCapture()
{
	if (HUDWidget) HUDWidget->ResetPresentationCapture();
}

void AWhiteoutHUD::SetPresentationCaptureState(const FWSGameState& State)
{
	if (HUDWidget) HUDWidget->SetPresentationCaptureState(State);
}

void AWhiteoutHUD::ShowNPCFocusForCapture(const FText& ActionName, const FWSActionPreview& Preview)
{
	if (HUDWidget) HUDWidget->ShowNPCFocusForCapture(ActionName, Preview);
}

void AWhiteoutHUD::ShowEvidenceForCapture(const int32 FilterIndex, const bool bShowFirstDetail)
{
	if (HUDWidget) HUDWidget->ShowEvidenceForCapture(FilterIndex, bShowFirstDetail);
}

void AWhiteoutHUD::ShowComponentGalleryForCapture()
{
	if (HUDWidget) HUDWidget->ShowComponentGalleryForCapture();
}

void AWhiteoutHUD::ShowSettingsForCapture()
{
	if (HUDWidget) HUDWidget->ShowSettingsForCapture();
}

void AWhiteoutHUD::SetOpeningCaptureStage(const int32 Stage)
{
	if (HUDWidget) HUDWidget->SetOpeningCaptureStage(Stage);
}

void AWhiteoutHUD::SetCrisisCaptureStage(const int32 Stage)
{
	if (HUDWidget) HUDWidget->SetCrisisCaptureStage(Stage);
}

void AWhiteoutHUD::SetEndingCaptureStage(const EWSEndingType Ending, const bool bShowResults)
{
	if (HUDWidget) HUDWidget->SetEndingCaptureStage(Ending, bShowResults);
}

void AWhiteoutHUD::SetInterfaceVisibleForCapture(const bool bVisible)
{
	if (HUDWidget) HUDWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
