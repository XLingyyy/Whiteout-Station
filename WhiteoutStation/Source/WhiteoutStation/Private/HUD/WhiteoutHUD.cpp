#include "HUD/WhiteoutHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "State/WindStationStateSubsystem.h"

void AWhiteoutHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GetGameInstance())
	{
		return;
	}
	const UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	if (!StateSubsystem)
	{
		return;
	}
	const FWSGameState State = StateSubsystem->GetStateSnapshot();
	const float Width = Canvas->ClipX;
	const float Height = Canvas->ClipY;

	DrawPanel(20, 20, Width - 40, 76, FLinearColor(0.015f, 0.025f, 0.04f, 0.88f));
	float TopY = 32;
	DrawLine(TEXT("WHITEOUT STATION  //  BEFORE THE BLACKOUT"), 38, TopY, FLinearColor(0.72f, 0.9f, 1.0f), 1.15f);
	DrawLine(
		FString::Printf(
			TEXT("AP %d / 8     PHASE %s     BLIZZARD ETA %s"),
			State.ActionPoints,
			*PhaseLabel(State.Phase),
			State.ActionPoints > 4 ? TEXT("approaching") : TEXT("imminent")),
		38,
		TopY,
		State.ActionPoints <= 4 ? FLinearColor(1.0f, 0.4f, 0.22f) : FLinearColor::White);

	DrawPanel(20, 112, 310, 240, FLinearColor(0.02f, 0.035f, 0.055f, 0.86f));
	float LeftY = 128;
	DrawLine(TEXT("PRIMARY OBJECTIVE"), 38, LeftY, FLinearColor(0.3f, 0.78f, 1.0f), 1.05f);
	DrawLine(FString::Printf(TEXT("Generator       %d / 2"), State.Tasks.GeneratorProgress), 38, LeftY);
	DrawLine(FString::Printf(TEXT("Antenna         %d / 1"), State.Tasks.AntennaCalibration), 38, LeftY);
	DrawLine(FString::Printf(TEXT("Rescue signal   %s"), State.Tasks.bSignalSent ? TEXT("SENT") : TEXT("NOT SENT")), 38, LeftY, State.Tasks.bSignalSent ? FLinearColor(0.2f, 1.0f, 0.6f) : FLinearColor::White);
	LeftY += 8;
	DrawLine(TEXT("RESERVES"), 38, LeftY, FLinearColor(0.95f, 0.75f, 0.3f), 1.05f);
	DrawLine(FString::Printf(TEXT("Fuel %d    Food %d    Medicine %d"), State.Resources.Fuel, State.Resources.Food, State.Resources.Medicine), 38, LeftY);
	DrawLine(FString::Printf(TEXT("Relay %d   Heater %s"), State.Resources.ReplacementRelay, State.Flags.bKitchenHeaterIntact ? TEXT("intact") : TEXT("dismantled")), 38, LeftY);
	DrawLine(FString::Printf(TEXT("Evidence %d   Crisis %s"), State.Evidence.Num(), State.bMidCrisisTriggered ? TEXT("TRIGGERED") : TEXT("pending")), 38, LeftY);

	DrawPanel(Width - 350, 112, 330, 240, FLinearColor(0.02f, 0.035f, 0.055f, 0.86f));
	float RightY = 128;
	DrawLine(TEXT("CREW STATUS"), Width - 332, RightY, FLinearColor(0.3f, 0.78f, 1.0f), 1.05f);
	for (const TPair<EWSCharacterId, FWSCharacterState>& Pair : State.Characters)
	{
		const FString Name = Pair.Key == EWSCharacterId::Player ? TEXT("YOU") : Pair.Key == EWSCharacterId::GuHeng ? TEXT("GU HENG") : TEXT("YE CHENG");
		DrawLine(Name, Width - 332, RightY, FLinearColor(0.85f, 0.9f, 0.95f), 0.95f);
		DrawLine(
			FString::Printf(
				TEXT("  health %s  temp %s  energy %s"),
				*StatLabel(Pair.Value.Health),
				*StatLabel(Pair.Value.Temperature),
				*StatLabel(Pair.Value.Fatigue)),
			Width - 332,
			RightY,
			FLinearColor(0.7f, 0.78f, 0.84f),
			0.82f);
		if (Pair.Key != EWSCharacterId::Player)
		{
			DrawLine(FString::Printf(TEXT("  trust %s"), *StatLabel(Pair.Value.Trust, true)), Width - 332, RightY, FLinearColor(0.7f, 0.78f, 0.84f), 0.82f);
		}
	}

	if (bShowEvidence)
	{
		DrawPanel(Width * 0.25f, Height * 0.18f, Width * 0.5f, Height * 0.58f, FLinearColor(0.01f, 0.02f, 0.035f, 0.96f));
		float EvidenceY = Height * 0.18f + 22;
		DrawLine(TEXT("EVIDENCE BOARD  [E to close]"), Width * 0.25f + 22, EvidenceY, FLinearColor(0.3f, 0.78f, 1.0f), 1.15f);
		for (const FName EvidenceId : State.Evidence)
		{
			DrawLine(FString::Printf(TEXT("+ %s"), *EvidenceId.ToString()), Width * 0.25f + 24, EvidenceY, FLinearColor(0.82f, 0.88f, 0.92f), 0.88f);
		}
		EvidenceY += 10;
		DrawLine(TEXT("CONFIRMED / SUSPECTED FACTS"), Width * 0.25f + 22, EvidenceY, FLinearColor(0.95f, 0.75f, 0.3f), 1.0f);
		for (const TPair<FName, EWSKnowledgeLevel>& Pair : State.PlayerKnowledge)
		{
			const FString Level = StaticEnum<EWSKnowledgeLevel>()->GetNameStringByValue(static_cast<int64>(Pair.Value));
			DrawLine(FString::Printf(TEXT("%s  //  %s"), *Pair.Key.ToString(), *Level), Width * 0.25f + 24, EvidenceY, FLinearColor(0.82f, 0.88f, 0.92f), 0.85f);
		}
	}

	DrawPanel(20, Height - 146, Width - 40, 126, FLinearColor(0.015f, 0.025f, 0.04f, 0.9f));
	float BottomY = Height - 134;
	DrawLine(FeedbackText, 38, BottomY, FLinearColor(0.9f, 0.93f, 0.96f), 0.95f);
	if (!InteractionPrompt.IsEmpty())
	{
		DrawLine(InteractionPrompt.ToString(), 38, BottomY, FLinearColor(0.3f, 0.85f, 1.0f), 1.12f);
	}
	DrawLine(TEXT("WASD move  |  Mouse look  |  F interact  |  E evidence  |  Enter settle  |  R restart"), 38, BottomY, FLinearColor(0.55f, 0.64f, 0.72f), 0.82f);
}

void AWhiteoutHUD::SetInteractionPrompt(const FText& Prompt)
{
	InteractionPrompt = Prompt;
}

void AWhiteoutHUD::SetActionFeedback(const FText& ActionName, const FWSActionResult& Result, const FWSActionPreview& Preview)
{
	if (Result.bCommitted)
	{
		FeedbackText = FString::Printf(TEXT("COMMITTED: %s  //  AP %d -> %d"), *ActionName.ToString(), Result.APBefore, Result.APAfter);
		if (Result.bCrisisTriggered)
		{
			FeedbackText += TEXT("  //  BACKUP BATTERY VOLTAGE COLLAPSED");
		}
	}
	else
	{
		FeedbackText = FString::Printf(
			TEXT("BLOCKED: %s  //  %s  //  %s"),
			*ActionName.ToString(),
			*StaticEnum<EWSReasonCode>()->GetNameStringByValue(static_cast<int64>(Result.ReasonCode)),
			*Preview.RiskText.ToString());
	}
}

void AWhiteoutHUD::ToggleEvidence()
{
	bShowEvidence = !bShowEvidence;
}

void AWhiteoutHUD::SetSystemMessage(const FString& Message)
{
	FeedbackText = Message;
}

void AWhiteoutHUD::DrawPanel(const float X, const float Y, const float Width, const float Height, const FLinearColor Color)
{
	DrawRect(Color, X, Y, Width, Height);
}

void AWhiteoutHUD::DrawLine(const FString& Text, const float X, float& Y, const FLinearColor Color, const float Scale)
{
	DrawText(Text, Color, X, Y, GEngine->GetSmallFont(), Scale, false);
	Y += 22.0f * Scale;
}

FString AWhiteoutHUD::StatLabel(const float Value, const bool bTrust)
{
	if (bTrust)
	{
		return Value >= 35.0f ? TEXT("trusted") : Value >= 0.0f ? TEXT("neutral") : Value >= -35.0f ? TEXT("guarded") : TEXT("hostile");
	}
	return Value >= 70.0f ? TEXT("stable") : Value >= 45.0f ? TEXT("strained") : Value >= 30.0f ? TEXT("danger") : TEXT("critical");
}

FString AWhiteoutHUD::PhaseLabel(const EWSGamePhase Phase)
{
	return StaticEnum<EWSGamePhase>()->GetNameStringByValue(static_cast<int64>(Phase));
}
