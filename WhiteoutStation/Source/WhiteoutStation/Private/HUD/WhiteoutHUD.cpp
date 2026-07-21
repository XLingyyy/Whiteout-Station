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

void AWhiteoutHUD::SetActionFeedback(const FText& ActionName, const FWSActionResult& Result, const FWSActionPreview& Preview)
{
	if (HUDWidget) HUDWidget->SetActionFeedback(ActionName, Result, Preview);
}

void AWhiteoutHUD::ShowActionPreview(const FText& ActionName, const FWSActionPreview& Preview)
{
	if (HUDWidget) HUDWidget->ShowActionPreview(ActionName, Preview);
}

void AWhiteoutHUD::HideActionPreview()
{
	if (HUDWidget) HUDWidget->HideActionPreview();
}

void AWhiteoutHUD::ToggleEvidence()
{
	if (HUDWidget) HUDWidget->ToggleEvidence();
}

void AWhiteoutHUD::ShowDialogueMenu(const int32 SelectedIndex, const bool bVisible)
{
	if (HUDWidget) HUDWidget->ShowDialogueMenu(SelectedIndex, bVisible);
}

void AWhiteoutHUD::SetSystemMessage(const FString& Message)
{
	if (HUDWidget) HUDWidget->SetSystemMessage(Message);
}

void AWhiteoutHUD::DismissOpening()
{
	if (HUDWidget) HUDWidget->DismissOpening();
}

void AWhiteoutHUD::TogglePauseMenu()
{
	if (HUDWidget) HUDWidget->TogglePauseMenu();
}

void AWhiteoutHUD::ResetPresentationCapture()
{
	if (HUDWidget) HUDWidget->ResetPresentationCapture();
}

void AWhiteoutHUD::SetPresentationCaptureState(const FWSGameState& State)
{
	if (HUDWidget) HUDWidget->SetPresentationCaptureState(State);
}

void AWhiteoutHUD::ShowEvidenceForCapture()
{
	if (HUDWidget) HUDWidget->ShowEvidenceForCapture();
}

void AWhiteoutHUD::ShowComponentGalleryForCapture()
{
	if (HUDWidget) HUDWidget->ShowComponentGalleryForCapture();
}

void AWhiteoutHUD::SetInterfaceVisibleForCapture(const bool bVisible)
{
	if (HUDWidget) HUDWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
