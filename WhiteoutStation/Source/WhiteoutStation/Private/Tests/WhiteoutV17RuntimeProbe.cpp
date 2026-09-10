#include "HUD/WhiteoutHUDWidget.h"
#include "HUD/WSTutorialWidget.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Save/WindStationSaveGame.h"
#include "HUD/WSActionPointWidget.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/WhiteoutSettingsSubsystem.h"
#include "State/WindStationStateSubsystem.h"
#include "UnrealClient.h"

// Explicit rendered integration fixture. It runs only with -V17Frame=probe and an isolated UserDir.
// Native widget events exercise navigation; OS input/IME checks are recorded separately.
struct FWSV17RuntimeProbe : TSharedFromThis<FWSV17RuntimeProbe>
{
	TWeakObjectPtr<UWhiteoutHUDWidget> HUD;
	FWSGameState Before;
	int64 Revision = 0;
	FVector Position;
	int32 Step = 0, Cycles = 0;
	FKey PendingKey;
	double PhaseStarted = 0;
	TSharedFuture<FString> CsvResult;
	TArray<TSharedPtr<FJsonValue>> MemorySamples;
	int32 OriginalBlurFallback = 0;
	double Started = FPlatformTime::Seconds();
	float GuideOffset = 0;
	TArray<TSharedPtr<FJsonValue>> Checks;
	FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Artifacts/v1.7-evidence"));

	void Check(const TCHAR* Name, bool Passed)
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("check"), Name); Entry->SetBoolField(TEXT("passed"), Passed);
		Checks.Add(MakeShared<FJsonValueObject>(Entry));
		UE_LOG(LogTemp, Display, TEXT("V17 integration %s: %s"), Name, Passed ? TEXT("PASS") : TEXT("FAIL"));
	}
	void Key(FKey Key, bool Repeat = false)
	{
		FKeyEvent Event(Key, FModifierKeysState(), 0, Repeat, 0, 0);
		FSlateApplication::Get().ProcessKeyDownEvent(Event);
		FSlateApplication::Get().ProcessKeyUpEvent(Event);
	}
	bool Unchanged() const
	{
		auto* S = HUD->GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
		const FWSGameState& After = S->GetStateSnapshot();
		return S->GetStateRevision() == Revision && FWSGameState::StaticStruct()->CompareScriptStruct(&Before, &After, 0)
			&& HUD->GetOwningPlayer()->GetPawn()->GetActorLocation().Equals(Position, .01f);
	}
	void Capture(int32 Page)
	{
		FString Suffix; FParse::Value(FCommandLine::Get(), TEXT("V17Label="), Suffix);
		FScreenshotRequest::RequestScreenshot(Directory / FString::Printf(TEXT("frames/tutorial_%d%s.png"), Page, *Suffix), true, false, false, FIntRect(), true);
	}
	bool Tick(float)
	{
		if (!HUD.IsValid()) return false;
		auto* H = HUD.Get(); auto* T = H->TutorialWidget.Get();
		auto* S = H->GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
		auto* Settings = H->GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>();
		if (FPlatformTime::Seconds() - Started > 240) { Check(TEXT("fixture completed within timeout"), false); return Finish(); }
		if (Step >= 100) return Performance();
		if (PendingKey.IsValid())
		{
			if (FScreenshotRequest::IsScreenshotRequested()) return true;
			Key(PendingKey); if (PendingKey == EKeys::Right) Key(PendingKey, true);
			PendingKey = FKey(); return true;
		}
		if (Step == 0)
		{
			if (!H->bTutorialLease) return true;
			// Activate Slate's test session without taking Windows foreground focus.
			FSlateApplication::Get().OnApplicationActivationChanged(true);
			T->SetKeyboardFocus();
			Check(TEXT("first safe game point opens T01 and pauses"), T->GetPageIndex() == 0 && UGameplayStatics::IsGamePaused(H));
			++Step; return true;
		}
		switch (Step++)
		{
		case 1:
			for (FKey K : {EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::F, EKeys::Q, EKeys::E, EKeys::H, EKeys::C, EKeys::R}) Key(K);
			Key(EKeys::Left);
			Check(TEXT("game keys and disabled previous leave T01 and world intact"), T->GetPageIndex() == 0 && Unchanged());
			Capture(1); PendingKey = EKeys::Right; break;
		case 2:
			Check(TEXT("one nonrepeat navigation advances exactly one page"), T->GetPageIndex() == 1);
			Capture(2); PendingKey = EKeys::Escape; break;
		case 3:
			Check(TEXT("skip confirmation keeps tutorial lease"), H->bTutorialLease && UGameplayStatics::IsGamePaused(H));
			Key(EKeys::Escape); Check(TEXT("cancel skip preserves page"), T->GetPageIndex() == 1); PendingKey = EKeys::Right; break;
		case 4: Check(TEXT("T03 page and unchanged full snapshot"), T->GetPageIndex() == 2 && Unchanged()); Capture(3); PendingKey = EKeys::Right; break;
		case 5: Check(TEXT("T04 page"), T->GetPageIndex() == 3); Capture(4); PendingKey = EKeys::Right; break;
		case 6: Check(TEXT("T05 page"), T->GetPageIndex() == 4); Capture(5); PendingKey = EKeys::Enter; break;
		case 7:
			Check(TEXT("completion releases pause and move/look locks"), !H->bTutorialLease && !UGameplayStatics::IsGamePaused(H)
				&& !H->GetOwningPlayer()->IsMoveInputIgnored() && !H->GetOwningPlayer()->IsLookInputIgnored());
			Check(TEXT("completion changes preference only"), Settings->GetTutorialPreference().Status == EWSTutorialStatus::Completed && Unchanged());
			H->ToggleGuide(); H->GuideScrollV17->SetScrollOffset(42); break;
		case 8: GuideOffset = H->GuideScrollV17->GetScrollOffset(); H->ReplayTutorial(); break;
		case 9:
			Check(TEXT("guide replay begins T01"), H->bTutorialLease && T->GetPageIndex() == 0 && T->IsReplay()); Key(EKeys::Escape); break;
		case 10:
			Check(TEXT("replay returns to original guide and scroll"), H->CurrentLayer == EWSUILayer::Guide && FMath::IsNearlyEqual(H->GuideScrollV17->GetScrollOffset(), GuideOffset));
			Check(TEXT("replay preserves Completed and world snapshot"), Settings->GetTutorialPreference().Status == EWSTutorialStatus::Completed && Unchanged()); break;
		case 11:
			if (Cycles == 30)
			{
				Check(TEXT("30 replay cycles release their leases"), !H->bTutorialLease && H->CurrentLayer == EWSUILayer::Guide && Unchanged());
				H->ToggleGuide();
				if (FParse::Param(FCommandLine::Get(), TEXT("V17Performance"))) { Step = 100; PhaseStarted = FPlatformTime::Seconds(); return true; }
				CheckActionsAndSaves(); return Finish();
			}
			H->ReplayTutorial(); break;
		case 12: Key(EKeys::Escape); break;
		case 13:
			if (H->bTutorialLease || H->CurrentLayer != EWSUILayer::Guide) { Check(TEXT("replay cycle closes"), false); return Finish(); }
			++Cycles; Step = 11; break;
		}
		return true;
	}
	void CheckActionsAndSaves()
	{
		auto* H = HUD.Get(); auto* S = H->GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
		for (FName Action : {FName(TEXT("inspect_control_cabinet")), FName(TEXT("investigate_generator_log"))})
		{
			FWSActionRequest Request; Request.ActionId = Action; Request.TransactionId = FGuid::NewGuid();
			const auto Quote = S->PreviewAction(Request); const int32 APBefore = S->GetStateSnapshot().PhaseActionPoints;
			H->ShowActionPreview(FText::FromName(Action), Quote, Request); H->RefreshAP(S->GetStateSnapshot());
			Check(TEXT("quote leaves authoritative AP unchanged"), S->GetStateSnapshot().PhaseActionPoints == APBefore && H->APModel.QuotedCost == Quote.APCost);
			H->HideActionPreview(); Check(TEXT("cancel invalidates quote"), !H->APModel.QuotedCost.IsSet());
			const auto Result = S->CommitAction(Request);
			Check(TEXT("real action pays quoted phase AP"), Result.bCommitted && S->GetStateSnapshot().PhaseActionPoints == APBefore - Quote.APCost);
			H->SetActionFeedback(FText::FromName(Action), Result, Quote); const int32 FeedbackCount = H->FeedbackQueue.Num();
			H->SetActionFeedback(FText::FromName(Action), Result, Quote);
			Check(TEXT("duplicate committed transaction adds no feedback"), H->FeedbackQueue.Num() == FeedbackCount);
		}
		const auto SavedState = S->GetStateSnapshot();
		auto* Legacy = NewObject<UWindStationSaveGame>(); Legacy->SaveVersion = TEXT("1.6.0"); Legacy->State = SavedState;
		Check(TEXT("isolated v1.6 fixture saved"), UGameplayStatics::SaveGameToSlot(Legacy, TEXT("WhiteoutStation_Autosave_v1_6"), 0));
		Check(TEXT("explicit legacy backup loads into independent v1.7 slot"), S->LoadLegacySnapshot());
		const auto Loaded = S->GetStateSnapshot();
		Check(TEXT("legacy load preserves AP, run and model count"), Loaded.PhaseActionPoints == SavedState.PhaseActionPoints && Loaded.ActionPoints == SavedState.ActionPoints && Loaded.RunId == SavedState.RunId && Loaded.ModelCalls == SavedState.ModelCalls);
		auto* LegacyAfter = Cast<UWindStationSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("WhiteoutStation_Autosave_v1_6"), 0));
		Check(TEXT("legacy slot content unchanged"), LegacyAfter && LegacyAfter->SaveVersion == TEXT("1.6.0") && FWSGameState::StaticStruct()->CompareScriptStruct(&Legacy->State, &LegacyAfter->State, 0));
		const FString Slot = FPaths::ProjectSavedDir() / TEXT("SaveGames/WhiteoutStation_Autosave_v1_7.sav");
		TArray<uint8> Original;
		if (FFileHelper::LoadFileToArray(Original, *Slot))
		{
			const TArray<uint8> Invalid{0, 1, 2, 3}; FFileHelper::SaveArrayToFile(Invalid, *Slot);
			Check(TEXT("corrupt current slot does not silently select legacy"), !S->LoadSnapshot());
			const auto AfterFailure = S->GetStateSnapshot();
			Check(TEXT("failed load preserves current state"), FWSGameState::StaticStruct()->CompareScriptStruct(&Loaded, &AfterFailure, 0));
			FFileHelper::SaveArrayToFile(Original, *Slot);
		}
		else Check(TEXT("v1.7 slot available for corruption regression"), false);
	}
	bool Performance()
	{
#if CSV_PROFILER
		const double Now = FPlatformTime::Seconds(); const double Elapsed = Now - PhaseStarted;
		auto* H = HUD.Get(); auto* Csv = FCsvProfiler::Get();
		if (Step == 101 || Step == 104 || Step == 107)
		{
			TSharedRef<FJsonObject> M = MakeShared<FJsonObject>(); M->SetNumberField(TEXT("phase"), Step);
			M->SetNumberField(TEXT("seconds"), Elapsed); M->SetNumberField(TEXT("process_mb"), FPlatformMemory::GetStats().UsedPhysical / 1048576.0);
			MemorySamples.Add(MakeShared<FJsonValueObject>(M));
		}
		if (Step == 100 && Elapsed >= 10)
		{
			Csv->EnableCategoryByString(TEXT("Slate")); Csv->BeginCapture(-1, Directory / TEXT("performance"), TEXT("game.csv")); Step = 101; PhaseStarted = Now;
		}
		else if ((Step == 101 || Step == 104 || Step == 107) && Elapsed >= 30)
		{
			CsvResult = Csv->EndCapture(); ++Step;
		}
		else if (Step == 102 && CsvResult.IsReady())
		{
			H->StartTutorial(true); Step = 103; PhaseStarted = Now;
		}
		else if (Step == 103 && Elapsed >= 5)
		{
			Csv->BeginCapture(-1, Directory / TEXT("performance"), TEXT("tutorial.csv")); Step = 104; PhaseStarted = Now;
		}
		else if (Step == 105 && CsvResult.IsReady())
		{
			auto* C = IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.ForceBackgroundBlurLowQualityOverride")); OriginalBlurFallback = C->GetInt(); C->Set(1);
			Step = 106; PhaseStarted = Now;
		}
		else if (Step == 106 && Elapsed >= 5)
		{
			Csv->BeginCapture(-1, Directory / TEXT("performance"), TEXT("fallback.csv")); Step = 107; PhaseStarted = Now;
		}
		else if (Step == 108 && CsvResult.IsReady())
		{
			IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.ForceBackgroundBlurLowQualityOverride"))->Set(OriginalBlurFallback);
			H->ReleaseTutorial(EWSTutorialStatus::InProgress);
			Check(TEXT("performance sampling leaves game state unchanged"), Unchanged());
			CheckActionsAndSaves(); return Finish();
		}
		return true;
#else
		Check(TEXT("CSV profiler available"), false); return Finish();
#endif
	}
	bool Finish()
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("checks"), Checks); Result->SetNumberField(TEXT("replay_cycles"), Cycles);
		Result->SetArrayField(TEXT("memory_samples"), MemorySamples);
		Result->SetStringField(TEXT("input_driver"), TEXT("native widget events in rendered game; OS checks separate"));
		FString Json; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
		FString Suffix; FParse::Value(FCommandLine::Get(), TEXT("V17Label="), Suffix);
		FFileHelper::SaveStringToFile(Json, *(Directory / (TEXT("runtime-probe") + Suffix + TEXT(".json"))));
		if (FParse::Param(FCommandLine::Get(), TEXT("WhiteoutAutoExit"))) FPlatformMisc::RequestExit(false);
		return false;
	}
};

void UWhiteoutHUDWidget::BeginV17RuntimeProbe()
{
	FString Mode; FParse::Value(FCommandLine::Get(), TEXT("V17Frame="), Mode);
	if (Mode != TEXT("probe")) return;
	FString UserDirectory; FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory);
	if (!FPaths::ConvertRelativePathToFull(UserDirectory).StartsWith(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Artifacts/v1.7-evidence/capture-user-probe"))))
	{
		UE_LOG(LogTemp, Error, TEXT("V17 probe requires its isolated capture UserDir")); return;
	}
	auto Probe = MakeShared<FWSV17RuntimeProbe>(); Probe->HUD = this;
	auto* S = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	Probe->Before = S->GetStateSnapshot(); Probe->Revision = S->GetStateRevision();
	Probe->Position = GetOwningPlayer()->GetPawn()->GetActorLocation();
	GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>()->SetTutorialPreference(FWSTutorialPreference());
	bTutorialBypass = false;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Delta) { return Probe->Tick(Delta); }), .5f);
}
