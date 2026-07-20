#include "HUD/WhiteoutHUD.h"

#include "Agents/WSNPCDecisionService.h"
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
			TEXT("AP %d / 8     PHASE %s     %s"),
			State.ActionPoints,
			*PhaseLabel(State.Phase),
			State.bMidCrisisTriggered ? TEXT("BACKUP CELL FAILED // EMERGENCY LOAD ONLY") : TEXT("BLIZZARD ETA approaching")),
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
		if (!State.Promises.IsEmpty())
		{
			EvidenceY += 10;
			DrawLine(TEXT("RECORDED PROMISES"), Width * 0.25f + 22, EvidenceY, FLinearColor(0.95f, 0.75f, 0.3f), 1.0f);
			for (const FWSPromiseRecord& Promise : State.Promises)
			{
				const TCHAR* Status = !Promise.bSettled ? TEXT("PENDING") : Promise.bFulfilled ? TEXT("KEPT") : TEXT("BROKEN");
				DrawLine(
					FString::Printf(TEXT("%s  //  %s"), *Promise.ConditionId.ToString(), Status),
					Width * 0.25f + 24,
					EvidenceY,
					Promise.bSettled && !Promise.bFulfilled ? FLinearColor(1.0f, 0.42f, 0.25f) : FLinearColor(0.82f, 0.88f, 0.92f),
					0.85f);
			}
		}
	}

	DrawPanel(20, Height - 168, Width - 40, 148, FLinearColor(0.015f, 0.025f, 0.04f, 0.9f));
	float BottomY = Height - 156;
	DrawLine(FeedbackText, 38, BottomY, FLinearColor(0.9f, 0.93f, 0.96f), 0.95f);
	const FWSAgentReply Dialogue = StateSubsystem->GetLatestDialogue();
	if (!Dialogue.Utterance.IsEmpty())
	{
		const FString SourceTag = Dialogue.bFallback ? TEXT("LOCAL") : TEXT("MODEL");
		DrawLine(
			FString::Printf(TEXT("%s [%s]: %s"), *UWSNPCDecisionService::SpeakerLabel(Dialogue.Speaker), *SourceTag, *Dialogue.Utterance),
			38,
			BottomY,
			FLinearColor(0.42f, 0.86f, 1.0f),
			0.92f);
	}
	if (!InteractionPrompt.IsEmpty())
	{
		DrawLine(InteractionPrompt.ToString(), 38, BottomY, FLinearColor(0.3f, 0.85f, 1.0f), 1.12f);
	}
	DrawLine(TEXT("WASD move | F interact | Q dialogue | E evidence | C continue | Enter settle | R restart"), 38, BottomY, FLinearColor(0.55f, 0.64f, 0.72f), 0.82f);

	if (State.Phase == EWSGamePhase::Results)
	{
		DrawResultsOverlay(State, Width, Height);
	}
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
		if (Result.Changes.ContainsByPredicate([](const FString& Change) { return Change.StartsWith(TEXT("Promise recognized:")); }))
		{
			FeedbackText += TEXT("  //  PROMISE LOGGED");
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

void AWhiteoutHUD::DrawResultsOverlay(const FWSGameState& State, const float Width, const float Height)
{
	const float PanelWidth = FMath::Min(780.0f, Width - 80.0f);
	const float PanelHeight = FMath::Min(590.0f, Height - 70.0f);
	const float X = (Width - PanelWidth) * 0.5f;
	const float Y = (Height - PanelHeight) * 0.5f;
	DrawPanel(0, 0, Width, Height, FLinearColor(0.0f, 0.006f, 0.012f, 0.72f));
	DrawPanel(X, Y, PanelWidth, PanelHeight, FLinearColor(0.015f, 0.028f, 0.045f, 0.98f));
	DrawPanel(X, Y, 7, PanelHeight, FLinearColor(0.22f, 0.72f, 0.96f, 1.0f));

	float LineY = Y + 24;
	DrawLine(TEXT("AFTER-ACTION REVIEW  //  RUN COMPLETE"), X + 32, LineY, FLinearColor(0.4f, 0.84f, 1.0f), 1.05f);
	DrawLine(EndingLabel(State.Ending), X + 32, LineY, FLinearColor::White, 1.55f);
	DrawLine(
		FString::Printf(TEXT("TOTAL  %.1f / 100     RATING  %s"), State.Score.Total, *State.Score.Rating),
		X + 32,
		LineY,
		State.Score.Total >= 70.0f ? FLinearColor(0.3f, 1.0f, 0.65f) : FLinearColor(1.0f, 0.48f, 0.24f),
		1.22f);
	LineY += 12;

	const float BarWidth = PanelWidth * 0.55f;
	const float ScoreRowHeight = Height < 650.0f ? 31.0f : 43.0f;
	DrawScoreRow(TEXT("TASK QUALITY"), State.Score.TaskQuality, 30.0f, X + 32, LineY, BarWidth, FLinearColor(0.25f, 0.72f, 1.0f), ScoreRowHeight);
	DrawScoreRow(TEXT("PEOPLE"), State.Score.People, 30.0f, X + 32, LineY, BarWidth, FLinearColor(0.28f, 0.95f, 0.67f), ScoreRowHeight);
	DrawScoreRow(TEXT("RESERVES"), State.Score.EffectiveReserves, 20.0f, X + 32, LineY, BarWidth, FLinearColor(0.95f, 0.7f, 0.25f), ScoreRowHeight);
	DrawScoreRow(TEXT("SOCIAL STABILITY"), State.Score.SocialStability, 12.0f, X + 32, LineY, BarWidth, FLinearColor(0.75f, 0.48f, 1.0f), ScoreRowHeight);
	DrawScoreRow(TEXT("INFORMATION"), State.Score.InformationResponsibility, 8.0f, X + 32, LineY, BarWidth, FLinearColor(0.4f, 0.86f, 0.9f), ScoreRowHeight);

	float SummaryX = X + PanelWidth * 0.65f;
	float SummaryY = Y + 148;
	DrawLine(TEXT("FINAL STATE"), SummaryX, SummaryY, FLinearColor(0.95f, 0.75f, 0.3f), 0.95f);
	DrawLine(FString::Printf(TEXT("Generator  %d / 2"), State.Tasks.GeneratorProgress), SummaryX, SummaryY, FLinearColor(0.8f, 0.86f, 0.92f), 0.85f);
	DrawLine(FString::Printf(TEXT("Antenna    %d / 1"), State.Tasks.AntennaCalibration), SummaryX, SummaryY, FLinearColor(0.8f, 0.86f, 0.92f), 0.85f);
	DrawLine(FString::Printf(TEXT("Signal     %s"), State.Tasks.bSignalSent ? TEXT("SENT") : TEXT("FAILED")), SummaryX, SummaryY, FLinearColor(0.8f, 0.86f, 0.92f), 0.85f);
	DrawLine(FString::Printf(TEXT("AP left    %d"), State.ActionPoints), SummaryX, SummaryY, FLinearColor(0.8f, 0.86f, 0.92f), 0.85f);
	DrawLine(FString::Printf(TEXT("Model      %d / 10"), State.ModelCalls), SummaryX, SummaryY, FLinearColor(0.8f, 0.86f, 0.92f), 0.85f);

	LineY += 12;
	DrawLine(TEXT("DECISION TRACE"), X + 32, LineY, FLinearColor(0.95f, 0.75f, 0.3f), 0.95f);
	const int32 FirstEvent = FMath::Max(0, State.EventLog.Num() - 5);
	for (int32 Index = FirstEvent; Index < State.EventLog.Num(); ++Index)
	{
		const FWSEventRecord& Event = State.EventLog[Index];
		DrawLine(
			FString::Printf(TEXT("%02d  %-28s  AP %d -> %d%s"), Event.Index, *Event.ActionId.ToString(), Event.APBefore, Event.APAfter, Event.bCrisisTriggered ? TEXT("  [CRISIS]") : TEXT("")),
			X + 32,
			LineY,
			FLinearColor(0.68f, 0.75f, 0.82f),
			0.78f);
	}
	float FooterY = Y + PanelHeight - 38;
	DrawLine(TEXT("R  START A NEW RUN"), X + 32, FooterY, FLinearColor(0.4f, 0.84f, 1.0f), 0.9f);
}

void AWhiteoutHUD::DrawScoreRow(
	const FString& Label,
	const float Value,
	const float Maximum,
	const float X,
	float& Y,
	const float Width,
	const FLinearColor Color,
	const float RowHeight)
{
	const bool bCompact = RowHeight < 40.0f;
	DrawText(FString::Printf(TEXT("%-22s %5.1f / %.0f"), *Label, Value, Maximum), FLinearColor(0.82f, 0.88f, 0.93f), X, Y, GEngine->GetSmallFont(), bCompact ? 0.76f : 0.86f, false);
	const float BarY = Y + (bCompact ? 15.0f : 18.0f);
	const float BarHeight = bCompact ? 6.0f : 8.0f;
	DrawRect(FLinearColor(0.08f, 0.12f, 0.16f, 1.0f), X, BarY, Width, BarHeight);
	DrawRect(Color, X, BarY, Width * FMath::Clamp(Value / Maximum, 0.0f, 1.0f), BarHeight);
	Y += RowHeight;
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

FString AWhiteoutHUD::EndingLabel(const EWSEndingType Ending)
{
	if (Ending == EWSEndingType::TaskSuccess) return TEXT("SIGNAL THROUGH  //  CONTROLLED SURVIVAL");
	if (Ending == EWSEndingType::SurvivalWait) return TEXT("NO SIGNAL  //  HOLDING FOR DAWN");
	if (Ending == EWSEndingType::CostUncontrolled) return TEXT("SIGNAL THROUGH  //  COST UNCONTROLLED");
	return TEXT("STATION COLLAPSE  //  NO SAFE MARGIN");
}
