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
	void SetActionFeedback(const FText& ActionName, const FWSActionResult& Result, const FWSActionPreview& Preview);
	void ShowActionPreview(const FText& ActionName, const FWSActionPreview& Preview);
	void HideActionPreview();
	void ToggleEvidence();
	void ShowDialogueMenu(int32 SelectedIndex, bool bVisible);
	void SetSystemMessage(const FString& Message);
	void DismissOpening();
	void TogglePauseMenu();

private:
	UPROPERTY(Transient)
	TObjectPtr<UWhiteoutHUDWidget> HUDWidget;
};
