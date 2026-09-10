#include "HUD/WhiteoutHUDWidget.h"
#include "HUD/WSTutorialWidget.h"
#include "HUD/WSActionPointWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/WhiteoutSettingsSubsystem.h"
#include "State/WindStationStateSubsystem.h"
#include "HUD/WSStatusPanelWidget.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

void UWhiteoutHUDWidget::ExportV17Geometry(const FString& Path) const
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	const auto Add = [&](const TCHAR* Name, const FGeometry& G)
	{
		FVector2D Pixel, Viewport, End, EndViewport;
		USlateBlueprintLibrary::LocalToViewport(this, G, FVector2D::ZeroVector, Pixel, Viewport);
		USlateBlueprintLibrary::LocalToViewport(this, G, G.GetLocalSize(), End, EndViewport);
		TArray<TSharedPtr<FJsonValue>> Rect;
		for (double V : {Pixel.X, Pixel.Y, End.X - Pixel.X, End.Y - Pixel.Y}) Rect.Add(MakeShared<FJsonValueNumber>(V));
		Object->SetArrayField(Name, Rect);
	};
	Add(TEXT("ap"), TopPanel->GetCachedGeometry());
	Add(TEXT("player"), StatusPanelV15->CardGeometry(false)); Add(TEXT("npc"), StatusPanelV15->CardGeometry(true));
	Add(TEXT("preview"), PreviewBorder->GetCachedGeometry()); Add(TEXT("dialogue"), DialogueBorder->GetCachedGeometry());
	Add(TEXT("focus"), FocusBorder->GetCachedGeometry());
	Object->SetNumberField(TEXT("dpi_scale"), UWidgetLayoutLibrary::GetViewportScale(this));
	FString Json; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Json));
	FFileHelper::SaveStringToFile(Json, *Path);
}

void UWhiteoutHUDWidget::TryStartTutorial()
{
	if (bTutorialBypass || bTutorialLease || !TutorialWidget || !GetGameInstance()) return;
	const auto* State = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	auto* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>();
	if (!State || !Settings) return;
	TutorialWidget->Preload(State->HasLiveLLMProvider() && State->GetDialogueMode() == EWSDialogueMode::Online);
	const bool bSafe = State->IsPresentationModalSafe() && !IsOpeningVisible()
		&& CurrentLayer == EWSUILayer::Game && GetOwningPlayer() && GetOwningPlayer()->GetPawn()
		&& State->GetStateSnapshot().Phase != EWSGamePhase::Results && EndingElapsed < 0;
	if (!bSafe) return;
	if (State->WasLegacySnapshotLoaded() && Settings->GetTutorialPreference().UpgradeHintShownVersion < FWSTutorialFlow::Version)
	{
		auto Preference = Settings->GetTutorialPreference(); Preference.UpgradeHintShownVersion = FWSTutorialFlow::Version;
		Settings->SetTutorialPreference(Preference);
		SetSystemMessage(TEXT("新增入门教程，可在 H 生存手册中查看。"));
	}
	if (TutorialWidget->ImagesReady() && FWSTutorialFlow::NeedsAutoShow(Settings->GetTutorialPreference(), State->WasSnapshotLoaded())) StartTutorial(false);
}

void UWhiteoutHUDWidget::ReplayTutorial()
{
	if (CurrentLayer != EWSUILayer::Guide || bTutorialLease || !TutorialWidget || !TutorialWidget->ImagesReady()) return;
	const auto* State = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	if (!State || !State->IsPresentationModalSafe())
	{
		SetSystemMessage(TEXT("请等待当前对话或行动处理完成，再重看教程。")); return;
	}
	StartTutorial(true);
}

void UWhiteoutHUDWidget::StartTutorial(bool bReplay)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || bTutorialLease) return;
	auto* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>();
	if (!Settings) return;
	TutorialReturnLayer = CurrentLayer; bTutorialReturnCursor = PC->bShowMouseCursor;
	TutorialReturnFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	bTutorialReplay = bReplay; bTutorialLease = true;
	bTutorialOwnsPause = !UGameplayStatics::IsGamePaused(this) && PC->SetPause(true);
	if (!bTutorialOwnsPause && !UGameplayStatics::IsGamePaused(this))
	{
		bTutorialLease = false;
		UE_LOG(LogTemp, Warning, TEXT("Tutorial deferred: unable to obtain world pause")); return;
	}
	PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
	CurrentLayer = EWSUILayer::Tutorial;
	if (bReplay && GuideBorder) GuideBorder->SetVisibility(ESlateVisibility::Hidden);
	PC->FlushPressedKeys(); PC->bShowMouseCursor = true;
	FInputModeUIOnly Mode; Mode.SetWidgetToFocus(TutorialWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); PC->SetInputMode(Mode);
	TutorialWidget->Open(Settings->GetTutorialPreference(), bReplay);
}

void UWhiteoutHUDWidget::PersistTutorialPage(FName PageId)
{
	if (bTutorialReplay || !GetGameInstance()) return;
	if (auto* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		auto Preference = Settings->GetTutorialPreference();
		Preference.Version = FWSTutorialFlow::Version; Preference.Status = EWSTutorialStatus::InProgress; Preference.LastPageId = PageId;
		Settings->SetTutorialPreference(Preference);
	}
}

void UWhiteoutHUDWidget::ReleaseTutorial(EWSTutorialStatus Reason)
{
	if (!bTutorialLease) return;
	if (!bTutorialReplay && GetGameInstance())
		if (auto* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
		{
			auto Preference = Settings->GetTutorialPreference(); Preference.Status = Reason;
			Settings->SetTutorialPreference(Preference);
		}
	if (TutorialWidget) TutorialWidget->Shutdown();
	bTutorialLease = false;
	CurrentLayer = TutorialReturnLayer;
	if (bTutorialReplay && GuideBorder) GuideBorder->SetVisibility(ESlateVisibility::Visible);
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->FlushPressedKeys(); PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false);
		if (bTutorialOwnsPause) PC->SetPause(false);
		PC->bShowMouseCursor = bTutorialReturnCursor;
		if (TutorialReturnLayer == EWSUILayer::Guide)
		{
			FInputModeUIOnly Mode; Mode.SetWidgetToFocus(TakeWidget()); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); PC->SetInputMode(Mode);
			if (TutorialReturnFocus.IsValid()) FSlateApplication::Get().SetKeyboardFocus(TutorialReturnFocus.Pin());
			else SetKeyboardFocus();
		}
		else { PC->SetInputMode(FInputModeGameOnly()); ResetMouseToViewportCenter(); }
	}
	bTutorialOwnsPause = false; TutorialReturnFocus.Reset();
}

void UWhiteoutHUDWidget::RefreshAP(const FWSGameState& State)
{
	if (!ActionPointWidget || !GetGameInstance()) return;
	const auto* Subsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	if (!Subsystem) return;
	const int64 Revision = Subsystem->GetStateRevision();
	if (APModel.RunId != State.RunId) PresentedTransactions.Reset();
	if (APModel.RunId != State.RunId || APModel.PhaseId != State.DayPhase || APModel.StateRevision != Revision)
	{
		APModel.QuotedCost.Reset(); APModel.QuoteToken.Reset();
	}
	APModel.RunId = State.RunId; APModel.PhaseId = State.DayPhase;
	APModel.PhaseRemaining = State.PhaseActionPoints;
	APModel.PhaseCapacity = Subsystem->GetRulesEngine().GetConfig().ActionPointsPerPhase;
	APModel.bPhaseStarted = State.bDayPhaseStarted; APModel.StateRevision = Revision;
	ActionPointWidget->Present(APModel);
}

void UWhiteoutHUDWidget::LoadLegacyBackup()
{
	if (bTutorialLease || !GetGameInstance()) return;
	auto* State = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	if (State && State->LoadLegacySnapshot())
	{
		LegacyBackupButton->SetVisibility(ESlateVisibility::Collapsed);
		SetSystemMessage(TEXT("已读取旧版备份，并写入独立 v1.7 槽。旧槽保留。")); ResumeGame();
	}
	else SetSystemMessage(TEXT("旧版备份读取失败，当前状态保持不变。"));
}
