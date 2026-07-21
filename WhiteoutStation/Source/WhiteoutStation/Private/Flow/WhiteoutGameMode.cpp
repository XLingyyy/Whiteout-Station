#include "Flow/WhiteoutGameMode.h"

#include "Algo/Sort.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "HUD/WhiteoutHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Player/WhiteoutCharacter.h"
#include "Player/WhiteoutPlayerController.h"
#include "Presentation/WhiteoutAudioDirector.h"
#include "Presentation/WSPresentationText.h"
#include "State/WindStationStateSubsystem.h"
#include "World/WSInteractableActor.h"
#include "World/WhiteoutStationBuilder.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
	void AddPresentationEvent(FWSGameState& State, const TCHAR* ActionId, const int32 APBefore, const int32 APAfter, const bool bCrisis = false)
	{
		FWSEventRecord& Event = State.EventLog.Emplace_GetRef();
		Event.Index = State.EventLog.Num();
		Event.ActionId = FName(ActionId);
		Event.APBefore = APBefore;
		Event.APAfter = APAfter;
		Event.ReasonCode = EWSReasonCode::Committed;
		Event.bCrisisTriggered = bCrisis;
	}

	FWSGameState MakePresentationResultsState(const FWSGameState& BaseState, const EWSEndingType Ending)
	{
		FWSGameState State = BaseState;
		State.Phase = EWSGamePhase::Results;
		State.Ending = Ending;
		State.EventLog.Reset();
		State.bMidCrisisTriggered = true;
		State.Score.Rating = TEXT("C");
		State.Tasks = FWSTaskState();
		State.ActionPoints = 0;

		if (Ending == EWSEndingType::TaskSuccess)
		{
			State.Tasks.GeneratorProgress = 2;
			State.Tasks.AntennaCalibration = 1;
			State.Tasks.bSignalSent = true;
			State.ActionPoints = 1;
			State.Score.TaskQuality = 29.0f;
			State.Score.People = 23.4f;
			State.Score.EffectiveReserves = 16.0f;
			State.Score.SocialStability = 9.0f;
			State.Score.InformationResponsibility = 7.0f;
			State.Score.Total = 84.4f;
			State.Score.Rating = TEXT("A");
			AddPresentationEvent(State, TEXT("investigate_generator_log"), 8, 7);
			AddPresentationEvent(State, TEXT("inspect_control_cabinet"), 7, 6);
			AddPresentationEvent(State, TEXT("talk_gu_heng"), 6, 5);
			AddPresentationEvent(State, TEXT("heat_repair_room"), 5, 4, true);
			AddPresentationEvent(State, TEXT("repair_generator"), 4, 3);
			AddPresentationEvent(State, TEXT("calibrate_antenna"), 3, 1);
			AddPresentationEvent(State, TEXT("send_signal"), 1, 1);
		}
		else if (Ending == EWSEndingType::SurvivalWait)
		{
			State.Tasks.GeneratorProgress = 1;
			State.Score.TaskQuality = 10.0f;
			State.Score.People = 24.5f;
			State.Score.EffectiveReserves = 18.0f;
			State.Score.SocialStability = 7.2f;
			State.Score.InformationResponsibility = 3.0f;
			State.Score.Total = 62.7f;
			State.Score.Rating = TEXT("C");
			AddPresentationEvent(State, TEXT("talk_ye_cheng"), 8, 7);
			AddPresentationEvent(State, TEXT("heat_medical_room"), 7, 6);
			AddPresentationEvent(State, TEXT("investigate_generator_log"), 6, 5);
			AddPresentationEvent(State, TEXT("repair_generator"), 5, 4, true);
		}
		else if (Ending == EWSEndingType::CostUncontrolled)
		{
			State.Tasks.GeneratorProgress = 2;
			State.Tasks.AntennaCalibration = 1;
			State.Tasks.bSignalSent = true;
			State.Score.TaskQuality = 25.0f;
			State.Score.People = 8.5f;
			State.Score.EffectiveReserves = 4.0f;
			State.Score.SocialStability = 2.0f;
			State.Score.InformationResponsibility = 1.0f;
			State.Score.Total = 40.5f;
			State.Score.Rating = TEXT("D");
			if (FWSCharacterState* Player = State.Characters.Find(EWSCharacterId::Player))
			{
				Player->Health = 26.0f;
				Player->Temperature = 24.0f;
			}
			AddPresentationEvent(State, TEXT("forced_self_repair"), 8, 6);
			AddPresentationEvent(State, TEXT("dismantle_kitchen_heater"), 6, 5);
			AddPresentationEvent(State, TEXT("repair_generator"), 5, 4, true);
			AddPresentationEvent(State, TEXT("calibrate_antenna"), 4, 2);
			AddPresentationEvent(State, TEXT("send_signal"), 2, 2);
		}
		else
		{
			State.Score.TaskQuality = 0.0f;
			State.Score.People = 5.0f;
			State.Score.EffectiveReserves = 2.0f;
			State.Score.SocialStability = 1.0f;
			State.Score.InformationResponsibility = 0.0f;
			State.Score.Total = 8.0f;
			State.Score.Rating = TEXT("D");
			for (TPair<EWSCharacterId, FWSCharacterState>& Pair : State.Characters)
			{
				Pair.Value.Health = 25.0f;
				Pair.Value.Temperature = 22.0f;
			}
			AddPresentationEvent(State, TEXT("forced_self_repair"), 8, 6);
			AddPresentationEvent(State, TEXT("distribute_food"), 6, 5);
			AddPresentationEvent(State, TEXT("talk_gu_heng"), 5, 4, true);
		}
		return State;
	}
}

AWhiteoutGameMode::AWhiteoutGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AWhiteoutCharacter::StaticClass();
	PlayerControllerClass = AWhiteoutPlayerController::StaticClass();
	HUDClass = AWhiteoutHUD::StaticClass();
}

void AWhiteoutGameMode::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation: starting playable v0.2 presentation flow"));
	if (UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		const bool bContinueRequested = FParse::Param(FCommandLine::Get(), TEXT("WhiteoutContinue"));
		if (!bContinueRequested || !StateSubsystem->LoadSnapshot())
		{
			StateSubsystem->NewGame();
		}
	}

	bool bHasBuilder = false;
	for (TActorIterator<AWhiteoutStationBuilder> It(GetWorld()); It; ++It)
	{
		bHasBuilder = true;
		break;
	}
	if (!bHasBuilder)
	{
		GetWorld()->SpawnActor<AWhiteoutStationBuilder>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	bool bHasAudioDirector = false;
	for (TActorIterator<AWhiteoutAudioDirector> It(GetWorld()); It; ++It)
	{
		bHasAudioDirector = true;
		break;
	}
	if (!bHasAudioDirector)
	{
		GetWorld()->SpawnActor<AWhiteoutAudioDirector>(FVector::ZeroVector, FRotator::ZeroRotator);
	}

	bPerformanceTestActive = FParse::Param(FCommandLine::Get(), TEXT("WhiteoutPerformanceTest"));
	if (bPerformanceTestActive)
	{
		PerformanceFrameTimesMs.Reset();
		PerformanceFrameTimesMs.Reserve(2400);
		PerformanceStartSeconds = FPlatformTime::Seconds();
		if (APawn* PerformancePawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			PerformancePawn->SetActorLocation(FVector(2650.0f, 900.0f, 125.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		if (APlayerController* PerformanceController = UGameplayStatics::GetPlayerController(this, 0))
		{
			PerformanceController->SetControlRotation(FRotator(-5.0f, -129.0f, 0.0f));
		}
		UE_LOG(LogTemp, Display, TEXT("WhiteoutStation Performance: 1080p outdoor benchmark warmup started"));
	}

	FString AutoRoute;
	if (FParse::Value(FCommandLine::Get(), TEXT("WhiteoutAutoRoute="), AutoRoute))
	{
		RunAutomationRoute(AutoRoute);
		if (APlayerController* RouteController = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (AWhiteoutHUD* RouteHUD = Cast<AWhiteoutHUD>(RouteController->GetHUD()))
			{
				RouteHUD->DismissOpening();
				if (UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
				{
					RouteHUD->SetEndingCaptureStage(StateSubsystem->GetStateSnapshot().Ending, true);
				}
			}
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("WhiteoutAutoCapture")))
	{
		FTimerHandle CaptureTimer;
		GetWorldTimerManager().SetTimer(
			CaptureTimer,
			[this]()
			{
				const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("WhiteoutRuntimeSmoke.png");
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false, false, FIntRect(), true);
				UE_LOG(LogTemp, Display, TEXT("WhiteoutStation: requested runtime screenshot %s"), *ScreenshotPath);
			},
			2.0f,
			false);
		FTimerHandle ExitTimer;
		GetWorldTimerManager().SetTimer(
			ExitTimer,
			[]() { FPlatformMisc::RequestExit(false); },
			4.0f,
			false);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("WhiteoutBaselineCapture")))
	{
		FTimerHandle BaselineTimer;
		GetWorldTimerManager().SetTimer(BaselineTimer, this, &AWhiteoutGameMode::BeginBaselineCapture, 2.0f, false);
	}

	if (FParse::Value(FCommandLine::Get(), TEXT("WhiteoutPresentationCapture="), PresentationCaptureMode))
	{
		FTimerHandle PresentationTimer;
		GetWorldTimerManager().SetTimer(PresentationTimer, this, &AWhiteoutGameMode::BeginPresentationCapture, 2.0f, false);
	}

	if (AutoRoute.IsEmpty()
		&& !FParse::Param(FCommandLine::Get(), TEXT("WhiteoutBaselineCapture"))
		&& !bPerformanceTestActive)
	{
		FTimerHandle OpeningStartTimer;
		GetWorldTimerManager().SetTimer(OpeningStartTimer, this, &AWhiteoutGameMode::BeginOpeningPresentation, 0.25f, false);
	}
}

void AWhiteoutGameMode::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPerformanceTestActive)
	{
		return;
	}
	constexpr double WarmupSeconds = 5.0;
	constexpr double SampleSeconds = 15.0;
	const double Elapsed = FPlatformTime::Seconds() - PerformanceStartSeconds;
	if (Elapsed >= WarmupSeconds && DeltaSeconds > 0.0f && DeltaSeconds < 0.25f)
	{
		PerformanceFrameTimesMs.Add(DeltaSeconds * 1000.0f);
	}
	if (Elapsed >= WarmupSeconds + SampleSeconds)
	{
		CompletePerformanceTest();
	}
}

void AWhiteoutGameMode::CompletePerformanceTest()
{
	bPerformanceTestActive = false;
	if (PerformanceFrameTimesMs.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation Performance: no frame samples collected"));
		FPlatformMisc::RequestExit(false);
		return;
	}
	TArray<float> SortedFrameTimes = PerformanceFrameTimesMs;
	SortedFrameTimes.Sort();
	double TotalMs = 0.0;
	for (const float FrameMs : SortedFrameTimes)
	{
		TotalMs += FrameMs;
	}
	const float MeanMs = static_cast<float>(TotalMs / SortedFrameTimes.Num());
	const int32 P95Index = FMath::Clamp(FMath::FloorToInt((SortedFrameTimes.Num() - 1) * 0.95f), 0, SortedFrameTimes.Num() - 1);
	const int32 P99Index = FMath::Clamp(FMath::FloorToInt((SortedFrameTimes.Num() - 1) * 0.99f), 0, SortedFrameTimes.Num() - 1);
	const int32 SlowTailStart = P99Index;
	double SlowTailTotalMs = 0.0;
	for (int32 Index = SlowTailStart; Index < SortedFrameTimes.Num(); ++Index)
	{
		SlowTailTotalMs += SortedFrameTimes[Index];
	}
	const int32 SlowTailCount = SortedFrameTimes.Num() - SlowTailStart;
	const float SlowTailMeanMs = static_cast<float>(SlowTailTotalMs / FMath::Max(SlowTailCount, 1));
	const FIntPoint Resolution = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		? GEngine->GameViewport->Viewport->GetSizeXY()
		: FIntPoint::ZeroValue;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("WhiteoutStation Performance: resolution=%dx%d samples=%d avg_fps=%.2f one_percent_low_fps=%.2f p95_ms=%.3f p99_ms=%.3f max_ms=%.3f"),
		Resolution.X,
		Resolution.Y,
		SortedFrameTimes.Num(),
		1000.0f / FMath::Max(MeanMs, 0.001f),
		1000.0f / FMath::Max(SlowTailMeanMs, 0.001f),
		SortedFrameTimes[P95Index],
		SortedFrameTimes[P99Index],
		SortedFrameTimes.Last());
	FPlatformMisc::RequestExit(false);
}

void AWhiteoutGameMode::BeginOpeningPresentation()
{
	if (bOpeningFinished || OpeningCamera)
	{
		return;
	}
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerController || !PlayerPawn)
	{
		return;
	}
	const FVector CameraLocation(3160.0f, -720.0f, 560.0f);
	const FVector LookTarget(1040.0f, 390.0f, 105.0f);
	OpeningCamera = GetWorld()->SpawnActor<ACameraActor>(CameraLocation, (LookTarget - CameraLocation).Rotation());
	if (!OpeningCamera)
	{
		return;
	}
	OpeningCamera->Tags.Add(TEXT("WSRuntimeOpeningCamera"));
	OpeningCamera->GetCameraComponent()->SetFieldOfView(69.0f);
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
	PlayerController->SetViewTargetWithBlend(OpeningCamera, 0.75f, EViewTargetBlendFunction::VTBlend_Cubic);
	GetWorldTimerManager().SetTimer(OpeningFinishTimer, this, &AWhiteoutGameMode::FinishOpeningPresentation, 14.0f, false);
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: opening establishing camera started"));
}

void AWhiteoutGameMode::FinishOpeningPresentation()
{
	if (bOpeningFinished)
	{
		return;
	}
	bOpeningFinished = true;
	GetWorldTimerManager().ClearTimer(OpeningFinishTimer);
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			PlayerController->SetViewTargetWithBlend(PlayerPawn, 0.85f, EViewTargetBlendFunction::VTBlend_Cubic);
		}
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
	}
	if (OpeningCamera)
	{
		OpeningCamera->SetLifeSpan(1.1f);
		OpeningCamera = nullptr;
	}
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: opening handed control to player"));
}

void AWhiteoutGameMode::BeginPresentationCapture()
{
	PresentationCaptureNames.Reset();
	if (PresentationCaptureMode.Equals(TEXT("suite"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("opening"), TEXT("hud"), TEXT("components"), TEXT("preview"),
			TEXT("reject_generator"), TEXT("reject_medical"), TEXT("reject_relay"),
			TEXT("dialogue"), TEXT("evidence"),
			TEXT("results_task"), TEXT("results_survival"), TEXT("results_cost"), TEXT("results_collapse")};
	}
	else if (PresentationCaptureMode.Equals(TEXT("g4suite"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("opening_title"), TEXT("opening_establishing"), TEXT("opening_objective"), TEXT("opening_controls"),
			TEXT("focus_available"), TEXT("focus_blocked"), TEXT("toast_commit"), TEXT("toast_promise"),
			TEXT("crisis_flash"), TEXT("crisis_blackout"), TEXT("crisis_emergency"),
			TEXT("ending_task_cinematic"), TEXT("ending_survival_cinematic"),
			TEXT("ending_cost_cinematic"), TEXT("ending_collapse_cinematic")};
	}
	else
	{
		PresentationCaptureNames.Add(PresentationCaptureMode);
	}
	PresentationCaptureIndex = 0;
	StagePresentationCapture();
}

void AWhiteoutGameMode::StagePresentationCapture()
{
	if (!PresentationCaptureNames.IsValidIndex(PresentationCaptureIndex))
	{
		UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: presentation capture completed"));
		FPlatformMisc::RequestExit(false);
		return;
	}
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	AWhiteoutHUD* HUD = PlayerController ? Cast<AWhiteoutHUD>(PlayerController->GetHUD()) : nullptr;
	UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	if (!HUD || !StateSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation v0.2: presentation capture could not find HUD/state"));
		FPlatformMisc::RequestExit(false);
		return;
	}

	const FString& CaptureName = PresentationCaptureNames[PresentationCaptureIndex];
	HUD->ResetPresentationCapture();
	for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
	{
		It->SetInteractionFocused(false);
	}
	if (!CaptureName.Equals(TEXT("opening")) && !CaptureName.StartsWith(TEXT("opening_")))
	{
		HUD->DismissOpening();
		if (APlayerController* CaptureController = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (APawn* CapturePawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				CaptureController->SetViewTarget(CapturePawn);
			}
		}
	}

	if (CaptureName.StartsWith(TEXT("opening_")))
	{
		const int32 Stage = CaptureName.Equals(TEXT("opening_title")) ? 0
			: CaptureName.Equals(TEXT("opening_establishing")) ? 1
			: CaptureName.Equals(TEXT("opening_objective")) ? 2
			: 3;
		HUD->SetOpeningCaptureStage(Stage);
	}
	else if (CaptureName.StartsWith(TEXT("focus_")))
	{
		const bool bBlocked = CaptureName.Equals(TEXT("focus_blocked"));
		const FName TargetAction = bBlocked ? FName(TEXT("send_signal")) : FName(TEXT("repair_generator"));
		for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
		{
			if (It->ActionId != TargetAction)
			{
				continue;
			}
			if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				const FVector DesiredCameraLocation = It->GetActorLocation() + FVector(-235.0f, 0.0f, 105.0f);
				const FVector PawnLocation = DesiredCameraLocation - FVector(0.0f, 0.0f, 64.0f);
				Pawn->SetActorLocation(PawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
				if (APlayerController* FocusController = UGameplayStatics::GetPlayerController(this, 0))
				{
					FocusController->SetControlRotation(
						(It->GetActorLocation() + FVector(0.0f, 0.0f, 75.0f) - DesiredCameraLocation).Rotation());
				}
			}
			It->SetInteractionFocused(true);
			HUD->SetInteractionFocus(It->DisplayName, It->PreviewInteraction());
			break;
		}
	}
	else if (CaptureName.StartsWith(TEXT("toast_")))
	{
		const bool bPromise = CaptureName.Equals(TEXT("toast_promise"));
		FWSActionPreview Preview;
		Preview.ActionId = bPromise ? FName(TEXT("talk_gu_heng")) : FName(TEXT("investigate_generator_log"));
		Preview.APCost = 1;
		Preview.bCanExecute = true;
		FWSActionResult Result;
		Result.ActionId = Preview.ActionId;
		Result.bCommitted = true;
		Result.APBefore = 7;
		Result.APAfter = 6;
		HUD->SetActionFeedback(FWSPresentationText::ActionLabel(Preview.ActionId), Result, Preview, bPromise);
	}
	else if (CaptureName.StartsWith(TEXT("crisis_")))
	{
		const int32 Stage = CaptureName.Equals(TEXT("crisis_flash")) ? 0
			: CaptureName.Equals(TEXT("crisis_blackout")) ? 1
			: 2;
		HUD->SetCrisisCaptureStage(Stage);
		for (TActorIterator<AWhiteoutStationBuilder> It(GetWorld()); It; ++It)
		{
			It->SetLightingPreviewState(true, false);
			break;
		}
	}
	else if (CaptureName.StartsWith(TEXT("ending_")))
	{
		EWSEndingType Ending = EWSEndingType::TaskSuccess;
		if (CaptureName.Contains(TEXT("survival"))) Ending = EWSEndingType::SurvivalWait;
		else if (CaptureName.Contains(TEXT("cost"))) Ending = EWSEndingType::CostUncontrolled;
		else if (CaptureName.Contains(TEXT("collapse"))) Ending = EWSEndingType::TotalCollapse;
		HUD->SetPresentationCaptureState(MakePresentationResultsState(StateSubsystem->GetStateSnapshot(), Ending));
		HUD->SetEndingCaptureStage(Ending, false);
	}
	else if (CaptureName.Equals(TEXT("preview")))
	{
		FWSActionRequest Request;
		Request.ActionId = TEXT("investigate_generator_log");
		const FWSActionPreview Preview = StateSubsystem->PreviewAction(Request);
		HUD->ShowActionPreview(FWSPresentationText::ActionLabel(Request.ActionId), Preview);
	}
	else if (CaptureName.Equals(TEXT("components")))
	{
		HUD->ShowComponentGalleryForCapture();
	}
	else if (CaptureName.StartsWith(TEXT("reject_")))
	{
		FWSActionRequest Request;
		if (CaptureName.Equals(TEXT("reject_generator"))) Request.ActionId = TEXT("calibrate_antenna");
		else if (CaptureName.Equals(TEXT("reject_medical")))
		{
			Request.ActionId = TEXT("treat_gu_heng");
			Request.TreatmentResource = EWSResourceType::Medicine;
		}
		else Request.ActionId = TEXT("dismantle_kitchen_heater");
		const FWSActionPreview Preview = StateSubsystem->PreviewAction(Request);
		HUD->ShowActionPreview(FWSPresentationText::ActionLabel(Request.ActionId), Preview);
	}
	else if (CaptureName.Equals(TEXT("dialogue")))
	{
		HUD->ShowDialogueMenu(2, true);
	}
	else if (CaptureName.Equals(TEXT("evidence")))
	{
		FWSGameState EvidenceState = StateSubsystem->GetStateSnapshot();
		EvidenceState.Evidence = {TEXT("generator_log"), TEXT("relay_burn_pattern"), TEXT("relay_compatibility")};
		EvidenceState.PlayerKnowledge.Add(TEXT("generator_fault"), EWSKnowledgeLevel::Confirmed);
		EvidenceState.PlayerKnowledge.Add(TEXT("relay_compatible"), EWSKnowledgeLevel::Confirmed);
		EvidenceState.PlayerKnowledge.Add(TEXT("gu_heng_condition"), EWSKnowledgeLevel::Suspected);
		EvidenceState.PlayerKnowledge.Add(TEXT("records_accountability"), EWSKnowledgeLevel::Claimed);
		FWSPromiseRecord& Promise = EvidenceState.Promises.Emplace_GetRef();
		Promise.PromiseId = TEXT("capture_promise");
		Promise.ConditionId = TEXT("heat_repair_room");
		Promise.bRecognized = true;
		HUD->SetPresentationCaptureState(EvidenceState);
		HUD->ShowEvidenceForCapture();
	}
	else if (CaptureName.StartsWith(TEXT("results_")))
	{
		EWSEndingType Ending = EWSEndingType::TaskSuccess;
		if (CaptureName.Equals(TEXT("results_survival"))) Ending = EWSEndingType::SurvivalWait;
		else if (CaptureName.Equals(TEXT("results_cost"))) Ending = EWSEndingType::CostUncontrolled;
		else if (CaptureName.Equals(TEXT("results_collapse"))) Ending = EWSEndingType::TotalCollapse;
		HUD->SetPresentationCaptureState(MakePresentationResultsState(StateSubsystem->GetStateSnapshot(), Ending));
	}

	FTimerHandle SettleTimer;
	GetWorldTimerManager().SetTimer(SettleTimer, this, &AWhiteoutGameMode::CapturePresentationFrame, 0.45f, false);
}

void AWhiteoutGameMode::CapturePresentationFrame()
{
	const FString& CaptureName = PresentationCaptureNames[PresentationCaptureIndex];
	if (CaptureName.StartsWith(TEXT("focus_")))
	{
		const FName TargetAction = CaptureName.Equals(TEXT("focus_blocked"))
			? FName(TEXT("send_signal"))
			: FName(TEXT("repair_generator"));
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
			{
				for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
				{
					if (It->ActionId == TargetAction)
					{
						It->SetInteractionFocused(true);
						HUD->SetInteractionFocus(It->DisplayName, It->PreviewInteraction());
						break;
					}
				}
			}
		}
	}
	const FIntPoint Size = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		? GEngine->GameViewport->Viewport->GetSizeXY()
		: FIntPoint(0, 0);
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../docs/baseline_v0.2"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString ScreenshotPath = Directory / FString::Printf(
		TEXT("UI_%s_%dx%d.png"),
		*CaptureName, Size.X, Size.Y);
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false, false, FIntRect(), true);
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: requested presentation screenshot %s"), *ScreenshotPath);
	++PresentationCaptureIndex;
	FTimerHandle NextTimer;
	GetWorldTimerManager().SetTimer(NextTimer, this, &AWhiteoutGameMode::StagePresentationCapture, 1.1f, false);
}

void AWhiteoutGameMode::BeginBaselineCapture()
{
	BaselineLocations = {
		FVector(-190, 245, 105), FVector(520, 300, 105),
		FVector(1000, 350, 120), FVector(1600, 300, 105),
		FVector(-180, 520, 105), FVector(520, 1020, 105),
		FVector(860, 720, 115), FVector(1600, 520, 105),
		FVector(1810, 400, 115), FVector(2650, 900, 125),
		FVector(1000, 350, 120), FVector(1000, 350, 120), FVector(200, 350, 105),
		FVector(740, 160, 105), FVector(740, 160, 105),
		FVector(120, 570, 105), FVector(120, 570, 105)};
	BaselineRotations = {
		FRotator(0, -72, 0), FRotator(-4, -142, 0),
		FRotator(-3, -55, 0), FRotator(-3, -145, 0),
		FRotator(-2, 30, 0), FRotator(-2, -153, 0),
		FRotator(-2, 0, 0), FRotator(-2, 143, 0),
		FRotator(-4, 0, 0), FRotator(-5, -129, 0),
		FRotator(-3, -55, 0), FRotator(-3, -55, 0), FRotator(-3, -106, 0),
		FRotator(-1, 0, 0), FRotator(-1, 0, 0),
		FRotator(-1, 90, 0), FRotator(-1, 90, 0)};
	BaselineNames = {
		TEXT("Zone_Control_01"), TEXT("Zone_Control_02"),
		TEXT("Zone_Repair_01"), TEXT("Zone_Repair_02"),
		TEXT("Zone_Medical_01"), TEXT("Zone_Medical_02"),
		TEXT("Zone_Quarters_01"), TEXT("Zone_Quarters_02"),
		TEXT("Zone_Outdoor_01"), TEXT("Zone_Outdoor_02"),
		TEXT("Lighting_Repair_Unpowered"), TEXT("Lighting_Repair_Restored"), TEXT("Lighting_Crisis"),
		TEXT("Character_Engineer_HighTrust"), TEXT("Character_Engineer_LowTrust"),
		TEXT("Character_Doctor_HighTrust"), TEXT("Character_Doctor_LowTrust")};
	BaselineCaptureIndex = FParse::Param(FCommandLine::Get(), TEXT("WhiteoutCharacterCapture")) ? 13 : 0;
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
			if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
			{
				HUD->DismissOpening();
				HUD->SetInterfaceVisibleForCapture(false);
			}
	}
	StageBaselineView();
}

void AWhiteoutGameMode::StageBaselineView()
{
	if (!BaselineLocations.IsValidIndex(BaselineCaptureIndex))
	{
		UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: baseline capture completed"));
		if (FParse::Param(FCommandLine::Get(), TEXT("WhiteoutAutoExit")))
		{
			FPlatformMisc::RequestExit(false);
		}
		return;
	}
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	for (TActorIterator<AWhiteoutStationBuilder> It(GetWorld()); It; ++It)
	{
		const FString& CaptureName = BaselineNames[BaselineCaptureIndex];
		It->SetLightingPreviewState(
			CaptureName.Equals(TEXT("Lighting_Crisis")),
			CaptureName.Equals(TEXT("Lighting_Repair_Restored")));
		break;
	}
	const FString& CaptureName = BaselineNames[BaselineCaptureIndex];
	if (CaptureName.StartsWith(TEXT("Character_")))
	{
		const bool bEngineer = CaptureName.Contains(TEXT("Engineer"));
		const bool bHighTrust = CaptureName.Contains(TEXT("HighTrust"));
		for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
		{
			const bool bTarget = (bEngineer && It->ActionId == TEXT("talk_gu_heng"))
				|| (!bEngineer && It->ActionId == TEXT("talk_ye_cheng"));
			if (bTarget)
			{
				It->SetCharacterPreviewMood(bHighTrust);
				break;
			}
		}
	}
	if (Pawn)
	{
		Pawn->SetActorLocation(BaselineLocations[BaselineCaptureIndex], false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (PlayerController)
	{
		PlayerController->SetControlRotation(BaselineRotations[BaselineCaptureIndex]);
	}
	FTimerHandle SettleTimer;
	GetWorldTimerManager().SetTimer(SettleTimer, this, &AWhiteoutGameMode::CaptureBaselineView, 0.6f, false);
}

void AWhiteoutGameMode::CaptureBaselineView()
{
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../docs/baseline_v0.2"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString ScreenshotPath = Directory / (BaselineNames[BaselineCaptureIndex] + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false, false, FIntRect(), true);
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: requested baseline screenshot %s"), *ScreenshotPath);
	++BaselineCaptureIndex;
	FTimerHandle NextTimer;
	GetWorldTimerManager().SetTimer(NextTimer, this, &AWhiteoutGameMode::StageBaselineView, 1.25f, false);
}

void AWhiteoutGameMode::RunAutomationRoute(const FString& RouteName)
{
	UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	if (!StateSubsystem)
	{
		return;
	}
	const auto Commit = [StateSubsystem](const TCHAR* ActionId, const TFunction<void(FWSActionRequest&)>& Configure = nullptr)
	{
		FWSActionRequest Request;
		Request.ActionId = FName(ActionId);
		Request.TransactionId = FGuid::NewGuid();
		if (Configure)
		{
			Configure(Request);
		}
		const FWSActionResult Result = StateSubsystem->CommitAction(Request);
		UE_LOG(LogTemp, Display, TEXT("WhiteoutStation AutoRoute: %s committed=%d AP=%d->%d"), ActionId, Result.bCommitted, Result.APBefore, Result.APAfter);
		return Result.bCommitted;
	};

	bool bSucceeded = true;
	if (RouteName.Equals(TEXT("medical"), ESearchCase::IgnoreCase))
	{
		bSucceeded &= Commit(TEXT("talk_ye_cheng"));
		bSucceeded &= Commit(TEXT("heat_medical_room"));
		bSucceeded &= Commit(TEXT("treat_gu_heng"), [](FWSActionRequest& Request) { Request.TreatmentResource = EWSResourceType::Medicine; });
		bSucceeded &= Commit(TEXT("talk_gu_heng"), [](FWSActionRequest& Request)
		{
			Request.DialogueAct = EWSDialogueAct::Promise;
			Request.PromiseCondition = TEXT("heat_repair_room");
		});
		bSucceeded &= Commit(TEXT("heat_repair_room"));
		bSucceeded &= Commit(TEXT("repair_generator"));
		bSucceeded &= Commit(TEXT("calibrate_antenna"));
		bSucceeded &= Commit(TEXT("send_signal"));
	}
	else if (RouteName.Equals(TEXT("technical"), ESearchCase::IgnoreCase))
	{
		bSucceeded &= Commit(TEXT("investigate_generator_log"));
		bSucceeded &= Commit(TEXT("inspect_control_cabinet"));
		bSucceeded &= Commit(TEXT("talk_gu_heng"));
		bSucceeded &= Commit(TEXT("dismantle_kitchen_heater"));
		bSucceeded &= Commit(TEXT("heat_repair_room"));
		bSucceeded &= Commit(TEXT("repair_generator"));
		bSucceeded &= Commit(TEXT("calibrate_antenna"));
		bSucceeded &= Commit(TEXT("send_signal"));
	}
	else if (RouteName.Equals(TEXT("quick"), ESearchCase::IgnoreCase))
	{
		bSucceeded &= Commit(TEXT("heat_repair_room"));
		bSucceeded &= Commit(TEXT("distribute_food"), [](FWSActionRequest& Request)
		{
			Request.FoodForPlayer = 1;
			Request.FoodForGuHeng = 1;
		});
		bSucceeded &= Commit(TEXT("repair_generator"));
		bSucceeded &= Commit(TEXT("repair_generator"));
		bSucceeded &= Commit(TEXT("calibrate_antenna"));
		bSucceeded &= Commit(TEXT("send_signal"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation AutoRoute: unknown route '%s'"), *RouteName);
		return;
	}

	const FWSGameState Results = StateSubsystem->EndGame();
	FString EventLogPath;
	StateSubsystem->ExportEventLog(EventLogPath);
	const FString Summary = FString::Printf(
		TEXT("route=%s success=%d ending=%s score=%.2f log=%s"),
		*RouteName,
		bSucceeded && Results.Tasks.bSignalSent,
		*StaticEnum<EWSEndingType>()->GetNameStringByValue(static_cast<int64>(Results.Ending)),
		Results.Score.Total,
		*EventLogPath);
	if (bSucceeded && Results.Tasks.bSignalSent)
	{
		UE_LOG(LogTemp, Display, TEXT("WhiteoutStation AutoRoute: %s"), *Summary);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation AutoRoute: %s"), *Summary);
	}
}
