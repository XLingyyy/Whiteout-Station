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
		auto* T = HUD->TutorialWidget.Get();
		FKeyEvent Event(Key, FModifierKeysState(), 0, Repeat, 0, 0);
		T->NativeOnPreviewKeyDown(T->GetCachedGeometry(), Event);
		T->NativeOnKeyUp(T->GetCachedGeometry(), Event);
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
		if (FPlatformTime::Seconds() - Started > 100) { Check(TEXT("fixture completed within timeout"), false); return Finish(); }
		if (PendingKey.IsValid())
		{
			if (FScreenshotRequest::IsScreenshotRequested()) return true;
			Key(PendingKey); if (PendingKey == EKeys::Right) Key(PendingKey, true);
			PendingKey = FKey(); return true;
		}
		if (Step == 0)
		{
			if (!H->bTutorialLease || !FSlateApplication::Get().IsActive()) return true;
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
				H->ToggleGuide(); return Finish();
			}
			H->ReplayTutorial(); break;
		case 12: Key(EKeys::Escape); break;
		case 13:
			if (H->bTutorialLease || H->CurrentLayer != EWSUILayer::Guide) { Check(TEXT("replay cycle closes"), false); return Finish(); }
			++Cycles; Step = 11; break;
		}
		return true;
	}
	bool Finish()
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("checks"), Checks); Result->SetNumberField(TEXT("replay_cycles"), Cycles);
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
	auto Probe = MakeShared<FWSV17RuntimeProbe>(); Probe->HUD = this;
	auto* S = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	Probe->Before = S->GetStateSnapshot(); Probe->Revision = S->GetStateRevision();
	Probe->Position = GetOwningPlayer()->GetPawn()->GetActorLocation();
	GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>()->SetTutorialPreference(FWSTutorialPreference());
	bTutorialBypass = false;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Delta) { return Probe->Tick(Delta); }), .5f);
}
