#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HUD/WhiteoutHUDWidget.h"
#include "State/WindStationStateSubsystem.h"
#include "Settings/WhiteoutSettingsSubsystem.h"
#include "Widgets/SWindow.h"
#include "UnrealClient.h"

namespace
{
	class FWSStartBalancePIE : public IAutomationLatentCommand
	{
		bool Update() override
		{
			FRequestPlaySessionParams Params;
			Params.bAllowOnlineSubsystem = false;
			Params.GlobalMapOverride = TEXT("/Game/WindStation/World/MVP_StationMap");
			GEditor->RequestPlaySession(Params);
			return true;
		}
	};

	class FWSVerifyBalancePIE : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		int32 Stage = 0;
		double Next = 0, Started = FPlatformTime::Seconds();
		float ScrollOffset = 0;
		bool OldReducedMotion = false;
		TWeakObjectPtr<UWhiteoutHUDWidget> HUD;
		template<class T> T* Widget(const TCHAR* Name) const { return Cast<T>(HUD->WidgetTree->FindWidget(FName(Name))); }
		void Capture(const TCHAR* Name)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir() / TEXT("../Artifacts/v1.6-editor-rebalance/ui/") / Name, true, false);
		}
		void CheckIcon()
		{
			const auto* Icon = Widget<UWidget>(TEXT("EvidenceIconBox0"));
			if (!Test->TestNotNull(TEXT("Evidence icon exists"), Icon)) return;
			const FVector2D Size = Icon->GetCachedGeometry().GetLocalSize();
			Test->TestTrue(TEXT("Rendered evidence slot remains 38 x 38"), FMath::IsNearlyEqual(Size.X, 38.0, .5) && FMath::IsNearlyEqual(Size.Y, 38.0, .5));
		}
	public:
		explicit FWSVerifyBalancePIE(FAutomationTestBase* InTest) : Test(InTest) {}
		bool Update() override
		{
			if (FPlatformTime::Seconds() - Started > 120) { Test->AddError(TEXT("PIE UI verification timed out")); return true; }
			if (FPlatformTime::Seconds() < Next) return false;
			UWorld* World = GEditor->PlayWorld;
			if (!World || !World->GetFirstPlayerController()) return false;
			for (TObjectIterator<UWhiteoutHUDWidget> It; It; ++It)
				if (It->GetWorld() == World && It->IsInViewport()) { HUD = *It; break; }
			if (!HUD.IsValid()) return false;
			auto* PC = World->GetFirstPlayerController();
			auto* State = World->GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
			auto* Settings = World->GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>();
			Next = FPlatformTime::Seconds() + 1.0;
			switch (Stage++)
			{
			case 0:
			{
				HUD->DismissOpening(); HUD->ResetPresentationCapture();
				OldReducedMotion = Settings->IsReducedMotionEnabled(); Settings->SetReducedMotionEnabled(false);
				State->SetAutomationSaveSlot(TEXT("WhiteoutV16BalanceEditorUITest"));
				EWSReasonCode Reason; TArray<FString> Changes;
				State->BeginDayPhase(EWSHeatingZone::MedicalRoom, Reason, Changes);
				FWSActionRequest Request; Request.ActionId = TEXT("inspect_control_cabinet");
				Test->TestTrue(TEXT("Real cabinet inspection commits in PIE"), State->CommitAction(Request).bCommitted);
				World->GetGameViewport()->GetWindow()->Resize(FVector2D(1280, 720));
				HUD->ToggleEvidence();
				break;
			}
			case 1:
				CheckIcon(); Capture(TEXT("evidence-16x9.png")); break;
			case 2:
			{
				auto* Copy = Widget<UTextBlock>(TEXT("EvidenceCopy0"));
				if (Copy)
				{
					Copy->SetText(FText::FromString(TEXT("长证据排版测试：控制柜触点熔毁，检查记录已经保存。三个人需要在任务成功的前提下保持温暖、恢复体能并降低压力。此段用于验证多行正文与较大字号不会压缩图标。")));
					auto Font = Copy->GetFont(); Font.Size = 22; Copy->SetFont(Font);
				}
				World->GetGameViewport()->GetWindow()->Resize(FVector2D(1280, 800));
				break;
			}
			case 3:
				CheckIcon(); Capture(TEXT("evidence-16x10-large-text.png")); break;
			case 4:
			{
				HUD->CloseEvidence();
				FWSActionRequest Request; Request.ActionId = TEXT("rest"); Request.RestTarget = EWSCharacterId::YeCheng; Request.RestLocation = EWSCharacterLocation::MedicalRoom;
				auto Quote = State->PreviewAction(Request); Quote.PreviewText = FText::FromString(TEXT("FORBIDDEN_FORECAST")); Quote.RiskText = Quote.PreviewText;
				HUD->ShowActionPreview(FText::FromString(TEXT("休息")), Quote, Request);
				auto* Body = Widget<UTextBlock>(TEXT("PreviewBody"));
				if (Test->TestNotNull(TEXT("Selection body exists"), Body))
				{
					const FString Text = Body->GetText().ToString();
					Test->TestTrue(TEXT("Selection contains exact AP"), Text.Contains(TEXT("1 AP")));
					Test->TestFalse(TEXT("Selection excludes predicted outcome"), Text.Contains(TEXT("FORBIDDEN_FORECAST")) || Text.Contains(TEXT("预期结果")) || Text.Contains(TEXT("可预见风险")));
				}
				break;
			}
			case 5: Capture(TEXT("action-selection.png")); break;
			case 6:
			{
				HUD->HideActionPreview(); HUD->ResetPresentationCapture();
				FWSActionRequest Request; Request.ActionId = TEXT("rest"); Request.RestTarget = EWSCharacterId::YeCheng; Request.RestLocation = EWSCharacterLocation::MedicalRoom;
				const auto Quote = State->PreviewAction(Request);
				const auto Result = State->CommitAction(Request);
				HUD->SetActionFeedback(FText::FromString(TEXT("回温测试")), Result, Quote);
				HUD->SetSystemMessage(TEXT("队列第二条反馈"));
				break;
			}
			case 7:
				Capture(TEXT("rest-feedback.png")); break;
			case 8: HUD->ToggleGuide(); Next += 2; break;
			case 9:
				Test->TestFalse(TEXT("Covered feedback hidden"), Widget<UBorder>(TEXT("ActionToast"))->IsVisible());
				HUD->HandleBackRequested(); Next += 6; break;
			case 10:
				Test->TestTrue(TEXT("Feedback still visible before 10 visible seconds"), Widget<UTextBlock>(TEXT("ActionToastText"))->GetText().ToString().Contains(TEXT("回温测试")));
				Next += 3; break;
			case 11:
				Test->TestEqual(TEXT("Queued feedback not lost"), Widget<UTextBlock>(TEXT("ActionToastText"))->GetText().ToString(), FString(TEXT("队列第二条反馈")));
				State->EndGame(); Next += 5; break;
			case 12:
				Test->TestTrue(TEXT("Normal ending reveals cursor"), PC->bShowMouseCursor);
				PC->SetMouseLocation(250, 310);
				Widget<UScrollBox>(TEXT("ResultsScroll"))->SetScrollOffset(160); break;
			case 13:
			{
				const FString Timeline = Widget<UTextBlock>(TEXT("ResultsTimelineText"))->GetText().ToString();
				Test->TestFalse(TEXT("Player timeline excludes internal evidence and fact identifiers"), Timeline.Contains(TEXT("EVIDENCE_")) || Timeline.Contains(TEXT("FACT_")));
				float X = 0, Y = 0; PC->GetMousePosition(X, Y);
				Test->TestTrue(TEXT("Results refresh does not recenter mouse"), FMath::IsNearlyEqual(X, 250.0f, 3) && FMath::IsNearlyEqual(Y, 310.0f, 3));
				ScrollOffset = Widget<UScrollBox>(TEXT("ResultsScroll"))->GetScrollOffset();
				Test->TestTrue(TEXT("Results content can scroll"), ScrollOffset > 0);
				Capture(TEXT("results-scrolled.png")); break;
			}
			case 14: HUD->TogglePauseMenu(); break;
			case 15: HUD->HandleBackRequested(); break;
			case 16:
				Test->TestTrue(TEXT("Pause resume preserves cursor"), PC->bShowMouseCursor);
				Test->TestEqual(TEXT("Pause resume preserves scroll"), Widget<UScrollBox>(TEXT("ResultsScroll"))->GetScrollOffset(), ScrollOffset);
				Test->TestTrue(TEXT("Save results"), State->SaveSnapshot());
				Widget<UButton>(TEXT("RestartButton"))->OnClicked.Broadcast(); Next += 3; break;
			case 17:
				HUD->DismissOpening();
				Test->TestFalse(TEXT("Restart restores hidden cursor"), PC->bShowMouseCursor);
				Settings->SetReducedMotionEnabled(true);
				Test->TestTrue(TEXT("Load results snapshot"), State->LoadSnapshot()); Next += 3; break;
			case 18:
				Test->TestTrue(TEXT("Loaded results with reduced motion reveals cursor"), PC->bShowMouseCursor);
				Capture(TEXT("results-loaded-reduced-motion.png"));
				Settings->SetReducedMotionEnabled(OldReducedMotion);
				return true;
			}
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWSBalanceEditorUI, "Whiteout.V16.EditorUI.PIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FWSBalanceEditorUI::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/WindStation/World/MVP_StationMap")));
	ADD_LATENT_AUTOMATION_COMMAND(FWSStartBalancePIE());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FWSVerifyBalancePIE(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
