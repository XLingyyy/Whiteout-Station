#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "State/WindStationTypes.h"
#include "WhiteoutHUD.generated.h"

UCLASS()
class WHITEOUTSTATION_API AWhiteoutHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	void SetInteractionPrompt(const FText& Prompt);
	void SetActionFeedback(const FText& ActionName, const FWSActionResult& Result, const FWSActionPreview& Preview);
	void ToggleEvidence();
	void SetSystemMessage(const FString& Message);

private:
	FText InteractionPrompt;
	FString FeedbackText = TEXT("Reach a station hotspot and press F.");
	bool bShowEvidence = false;

	void DrawPanel(float X, float Y, float Width, float Height, FLinearColor Color);
	void DrawLine(const FString& Text, float X, float& Y, FLinearColor Color = FLinearColor::White, float Scale = 1.0f);
	void DrawResultsOverlay(const FWSGameState& State, float Width, float Height);
	void DrawScoreRow(const FString& Label, float Value, float Maximum, float X, float& Y, float Width, FLinearColor Color);
	static FString StatLabel(float Value, bool bTrust = false);
	static FString PhaseLabel(EWSGamePhase Phase);
	static FString EndingLabel(EWSEndingType Ending);
};
