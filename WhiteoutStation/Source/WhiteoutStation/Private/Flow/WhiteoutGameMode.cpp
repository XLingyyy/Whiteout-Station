#include "Flow/WhiteoutGameMode.h"

#include "Agents/WSAgentGateway.h"
#include "Algo/Sort.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Components/PointLightComponent.h"
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
		UWSAgentGateway::ResetSessionModelBudget(StateSubsystem->GetStateSnapshot().ModelCalls);
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

	FString IntentProbeText;
	const bool bIntentProbeRequested = FParse::Value(FCommandLine::Get(), TEXT("WhiteoutIntentProbe="), IntentProbeText);
	if (bIntentProbeRequested)
	{
		IntentProbeGateway = NewObject<UWSAgentGateway>(this);
		IntentProbeGateway->Initialize();
		TWeakObjectPtr<AWhiteoutGameMode> WeakThis(this);
		IntentProbeGateway->RequestDialogueIntent(
			IntentProbeText,
			true,
			FWSDialogueIntentCallback::CreateLambda(
				[WeakThis](const FWSDialogueIntentResult& Intent)
				{
					UE_LOG(
						LogTemp,
						Display,
						TEXT("WhiteoutStation IntentProbe: mapped=%s act=%s condition=%s source=%s reason=%s calls=%d"),
						Intent.bMapped ? TEXT("true") : TEXT("false"),
						*StaticEnum<EWSDialogueAct>()->GetNameStringByValue(static_cast<int64>(Intent.DialogueAct)),
						*Intent.PromiseCondition.ToString(),
						*Intent.Source,
						*Intent.Reason,
						UWSAgentGateway::GetSessionModelCalls());
					if (WeakThis.IsValid())
					{
						FTimerHandle ExitTimer;
						WeakThis->GetWorldTimerManager().SetTimer(
							ExitTimer,
							[]() { FPlatformMisc::RequestExit(false); },
							0.4f,
							false);
					}
				}));
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
		&& !bIntentProbeRequested
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
	if (IConsoleVariable* MotionBlurQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
	{
		MotionBlurQuality->Set(0, ECVF_SetByCode);
	}
	if (IConsoleVariable* DepthOfFieldQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DepthOfFieldQuality")))
	{
		DepthOfFieldQuality->Set(0, ECVF_SetByCode);
	}
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
	else if (PresentationCaptureMode.Equals(TEXT("g1suite"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("hud"), TEXT("pause"), TEXT("evidence"),
			TEXT("focus_near"), TEXT("focus_mid"), TEXT("focus_far"), TEXT("focus_blocked"),
			TEXT("preview"), TEXT("reject_generator"),
			TEXT("toast_commit"), TEXT("toast_promise"),
			TEXT("opening_objective"), TEXT("results_task")};
	}
	else if (PresentationCaptureMode.Equals(TEXT("g2suite"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("dialogue_gu_wheel"), TEXT("dialogue_ye_wheel"),
			TEXT("dialogue_promise"), TEXT("dialogue_free"),
			TEXT("dialogue_offline"), TEXT("dialogue_response"),
			TEXT("lookat_near"), TEXT("lookat_side"), TEXT("lookat_far")};
	}
	else if (PresentationCaptureMode.Equals(TEXT("g3suite"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("scene_01_after_control_grounding"), TEXT("scene_02_after_control_storage"),
			TEXT("scene_03_after_central_passage"), TEXT("scene_04_after_quarters_grounding"),
			TEXT("scene_05_after_repair_passage"), TEXT("scene_06_after_gu_idle"),
			TEXT("scene_07_after_repair_grounding"), TEXT("scene_08_after_medical_layout"),
			TEXT("scene_09_after_ye_idle"), TEXT("scene_10_after_bed_passage"),
			TEXT("character_gu_near"), TEXT("character_gu_mid"),
			TEXT("character_ye_near"), TEXT("character_ye_mid")};
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
	HUD->SetInterfaceVisibleForCapture(true);
	HUD->ResetPresentationCapture();
	for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
	{
		It->SetInteractionFocused(false);
		It->SetDialogueLookAtActive(false);
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
		const FName TargetAction = bBlocked ? FName(TEXT("send_signal")) : FName(TEXT("forced_self_repair"));
		for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
		{
			if (It->ActionId != TargetAction)
			{
				continue;
			}
			if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				const FVector LookTarget = It->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
				FVector ViewDirection = Pawn->GetPawnViewLocation() - LookTarget;
				if (!ViewDirection.Normalize())
				{
					ViewDirection = FVector(-1.0f, 0.0f, 0.0f);
				}
				const float FocusDistance = bBlocked ? 300.0f
					: CaptureName.Equals(TEXT("focus_near")) ? 180.0f
					: CaptureName.Equals(TEXT("focus_far")) ? 420.0f
					: 280.0f;
				const FVector DesiredCameraLocation = LookTarget + ViewDirection * FocusDistance;
				const FVector PawnLocation = DesiredCameraLocation - FVector(0.0f, 0.0f, 64.0f);
				Pawn->SetActorLocation(PawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
				if (APlayerController* FocusController = UGameplayStatics::GetPlayerController(this, 0))
				{
					FocusController->SetControlRotation(
						(LookTarget - DesiredCameraLocation).Rotation());
				}
			}
			It->SetInteractionFocused(true);
			FWSActionPreview FocusPreview = It->PreviewInteraction();
			if (!bBlocked)
			{
				FocusPreview.bCanExecute = true;
			}
			HUD->SetInteractionFocus(It->DisplayName, FocusPreview);
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
	else if (CaptureName.Equals(TEXT("pause")))
	{
		HUD->TogglePauseMenu();
		if (PlayerController)
		{
			PlayerController->SetPause(false);
		}
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
		HUD->ShowDialogueMenu(TEXT("talk_gu_heng"), true);
	}
	else if (CaptureName.StartsWith(TEXT("dialogue_")))
	{
		FWSGameState DialogueState = StateSubsystem->GetStateSnapshot();
		if (FWSCharacterState* GuHeng = DialogueState.Characters.Find(EWSCharacterId::GuHeng))
		{
			GuHeng->Trust = 8.0f;
			GuHeng->Health = 62.0f;
			GuHeng->Temperature = 47.0f;
			GuHeng->Pressure = 68.0f;
		}
		if (FWSCharacterState* YeCheng = DialogueState.Characters.Find(EWSCharacterId::YeCheng))
		{
			YeCheng->Trust = 13.0f;
			YeCheng->Health = 88.0f;
			YeCheng->Temperature = 63.0f;
			YeCheng->Pressure = 55.0f;
		}
		DialogueState.Flags.bMedicalRoomHeated = true;
		DialogueState.Flags.bGuHengDiagnosed = true;
		HUD->SetPresentationCaptureState(DialogueState);
		const FName NPCAction = CaptureName.Equals(TEXT("dialogue_ye_wheel"))
			? FName(TEXT("talk_ye_cheng")) : FName(TEXT("talk_gu_heng"));
		HUD->ShowDialogueMenu(NPCAction, true);
		if (CaptureName.Equals(TEXT("dialogue_promise")))
		{
			HUD->ShowDialoguePromiseChoices();
		}
		else if (CaptureName.Equals(TEXT("dialogue_free")))
		{
			HUD->ShowDialogueFreeTextForCapture();
		}
		else if (CaptureName.Equals(TEXT("dialogue_offline")))
		{
			HUD->SetDialogueIntentStatus(TEXT("离线模式｜当前使用本地意图词典；无法可靠识别时将回到安全轮盘。"), false);
		}
		else if (CaptureName.Equals(TEXT("dialogue_response")))
		{
			HUD->SetDialogueIntentStatus(TEXT("顾衡：手还不能精细操作。把维修间升温，我就配合修复发电机。\n\n本地确定性表达｜点击右下角返回现场"), true);
		}
	}
	else if (CaptureName.StartsWith(TEXT("lookat_")))
	{
		HUD->SetInterfaceVisibleForCapture(false);
		for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
		{
			if (It->ActionId != TEXT("talk_gu_heng"))
			{
				continue;
			}
			APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
			APlayerController* LookController = UGameplayStatics::GetPlayerController(this, 0);
			if (!Pawn || !LookController)
			{
				break;
			}
			It->SetActorLocation(FVector(4000.0f, 2000.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
			It->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
			It->SetCharacterPreviewMood(false);
			const FVector Forward = It->GetActorForwardVector();
			const FVector Right = It->GetActorRightVector();
			const FVector Offset = CaptureName.Equals(TEXT("lookat_side"))
				? (Forward * 230.0f + Right * 145.0f)
				: Forward * (CaptureName.Equals(TEXT("lookat_far")) ? 900.0f : 220.0f);
			if (!LookAtCaptureLight)
			{
				LookAtCaptureLight = GetWorld()->SpawnActor<APointLight>(It->GetActorLocation(), FRotator::ZeroRotator);
				if (LookAtCaptureLight && LookAtCaptureLight->PointLightComponent)
				{
					LookAtCaptureLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
					LookAtCaptureLight->PointLightComponent->SetIntensity(4200.0f);
					LookAtCaptureLight->PointLightComponent->SetAttenuationRadius(850.0f);
					LookAtCaptureLight->PointLightComponent->SetLightColor(FLinearColor(0.72f, 0.84f, 1.0f));
					LookAtCaptureLight->PointLightComponent->SetCastShadows(false);
				}
			}
			if (LookAtCaptureLight)
			{
				LookAtCaptureLight->SetActorLocation(It->GetActorLocation() + Forward * 90.0f + Right * 100.0f + FVector(0.0f, 0.0f, 210.0f));
			}
			FVector PawnLocation = It->GetActorLocation() + Offset;
			PawnLocation.Z = 105.0f;
			Pawn->SetActorLocation(PawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
			if (ACharacter* CaptureCharacter = Cast<ACharacter>(Pawn))
			{
				CaptureCharacter->GetCharacterMovement()->DisableMovement();
			}
			const FVector CameraLocation = Pawn->GetPawnViewLocation();
			const FVector LookTarget = It->GetActorLocation() + FVector(0.0f, 0.0f, 145.0f);
			LookController->SetControlRotation((LookTarget - CameraLocation).Rotation());
			It->SetDialogueLookAtActive(!CaptureName.Equals(TEXT("lookat_far")));
			break;
		}
	}
	else if (CaptureName.StartsWith(TEXT("scene_")) || CaptureName.StartsWith(TEXT("character_")))
	{
		HUD->SetInterfaceVisibleForCapture(false);
		const TArray<FString> SceneCaptureNames = {
			TEXT("scene_01_after_control_grounding"), TEXT("scene_02_after_control_storage"),
			TEXT("scene_03_after_central_passage"), TEXT("scene_04_after_quarters_grounding"),
			TEXT("scene_05_after_repair_passage"), TEXT("scene_06_after_gu_idle"),
			TEXT("scene_07_after_repair_grounding"), TEXT("scene_08_after_medical_layout"),
			TEXT("scene_09_after_ye_idle"), TEXT("scene_10_after_bed_passage"),
			TEXT("character_gu_near"), TEXT("character_gu_mid"),
			TEXT("character_ye_near"), TEXT("character_ye_mid")};
		const TArray<FVector> SceneCaptureLocations = {
			FVector(520, 300, 105), FVector(520, 300, 105),
			FVector(520, 300, 105), FVector(860, 720, 115),
			FVector(1000, 350, 120), FVector(620, 300, 105),
			FVector(1600, 300, 105), FVector(-180, 520, 105),
			FVector(120, 1120, 105), FVector(1500, 520, 105),
			FVector(735, 160, 105), FVector(600, 160, 110),
			FVector(120, 690, 105), FVector(120, 560, 110)};
		const TArray<FRotator> SceneCaptureRotations = {
			FRotator(-2, -142, 0), FRotator(-4, -142, 0),
			FRotator(-2, 0, 0), FRotator(-2, 0, 0),
			FRotator(-3, -55, 0), FRotator(-1, -28, 0),
			FRotator(-3, -145, 0), FRotator(-2, 30, 0),
			FRotator(-1, -90, 0), FRotator(-2, 143, 0),
			FRotator(-1, 0, 0), FRotator(-1, 0, 0),
			FRotator(-1, 90, 0), FRotator(-1, 90, 0)};
		const int32 ViewIndex = SceneCaptureNames.IndexOfByKey(CaptureName);
		APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Pawn && PlayerController && SceneCaptureLocations.IsValidIndex(ViewIndex)
			&& SceneCaptureRotations.IsValidIndex(ViewIndex))
		{
			PlayerController->SetViewTarget(Pawn);
			Pawn->SetActorLocation(SceneCaptureLocations[ViewIndex], false, nullptr, ETeleportType::TeleportPhysics);
			PlayerController->SetControlRotation(SceneCaptureRotations[ViewIndex]);
			if (ACharacter* CaptureCharacter = Cast<ACharacter>(Pawn))
			{
				CaptureCharacter->GetCharacterMovement()->DisableMovement();
			}
		}
		if (Pawn && PlayerController && CaptureName.StartsWith(TEXT("character_")))
		{
			if (PlayerController->PlayerCameraManager)
			{
				PlayerController->PlayerCameraManager->SetFOV(55.0f);
			}
			const bool bEngineer = CaptureName.Contains(TEXT("_gu_"));
			const FName TargetAction = bEngineer ? FName(TEXT("talk_gu_heng")) : FName(TEXT("talk_ye_cheng"));
			for (TActorIterator<AWSInteractableActor> CharacterIt(GetWorld()); CharacterIt; ++CharacterIt)
			{
				const bool bIsCharacter = CharacterIt->ActionId == TEXT("talk_gu_heng")
					|| CharacterIt->ActionId == TEXT("talk_ye_cheng");
				if (bIsCharacter)
				{
					CharacterIt->SetActorHiddenInGame(CharacterIt->ActionId != TargetAction);
				}
			}
			for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
			{
				if (It->ActionId != TargetAction)
				{
					continue;
				}
				const FVector InspectionLocation = bEngineer
					? FVector(10000.0f, 10000.0f, 0.0f)
					: FVector(10000.0f, 10600.0f, 0.0f);
				It->SetActorLocation(InspectionLocation, false, nullptr, ETeleportType::TeleportPhysics);
				It->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
				It->SetCharacterPreviewMood(true);
				const FVector Forward = It->GetActorForwardVector();
				const FVector Right = It->GetActorRightVector();
				const FVector ViewDirection = -Right;
				const float Distance = CaptureName.EndsWith(TEXT("_near")) ? 240.0f : 420.0f;
				FVector PawnLocation = It->GetActorLocation() + ViewDirection * Distance;
				PawnLocation.Z = 105.0f;
				Pawn->SetActorLocation(PawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
				const FVector CameraLocation = Pawn->GetPawnViewLocation();
				const float LookTargetHeight = CaptureName.EndsWith(TEXT("_near")) ? 145.0f : 92.0f;
				const FVector LookTarget = It->GetActorLocation() + FVector(0.0f, 0.0f, LookTargetHeight);
				PlayerController->SetControlRotation((LookTarget - CameraLocation).Rotation());
				if (!LookAtCaptureLight)
				{
					LookAtCaptureLight = GetWorld()->SpawnActor<APointLight>(InspectionLocation, FRotator::ZeroRotator);
					if (LookAtCaptureLight && LookAtCaptureLight->PointLightComponent)
					{
						LookAtCaptureLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
						LookAtCaptureLight->PointLightComponent->SetIntensity(1800.0f);
						LookAtCaptureLight->PointLightComponent->SetAttenuationRadius(850.0f);
						LookAtCaptureLight->PointLightComponent->SetLightColor(FLinearColor(0.62f, 0.74f, 1.0f));
						LookAtCaptureLight->PointLightComponent->SetCastShadows(false);
					}
				}
				if (LookAtCaptureLight)
				{
					LookAtCaptureLight->SetActorLocation(
						It->GetActorLocation() + ViewDirection * 90.0f + Forward * 90.0f + FVector(0.0f, 0.0f, 205.0f));
				}
				FVector CaptureBoundsOrigin = FVector::ZeroVector;
				FVector CaptureBoundsExtent = FVector::ZeroVector;
				It->GetActorBounds(false, CaptureBoundsOrigin, CaptureBoundsExtent);
				UE_LOG(LogTemp, Display,
					TEXT("WhiteoutStation v0.3 G3 character capture %s actor=%s pawn=%s bounds_origin=%s bounds_extent=%s"),
					*CaptureName,
					*It->GetActorLocation().ToCompactString(),
					*Pawn->GetActorLocation().ToCompactString(),
					*CaptureBoundsOrigin.ToCompactString(),
					*CaptureBoundsExtent.ToCompactString());
				break;
			}
		}
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
	GetWorldTimerManager().SetTimer(
		SettleTimer,
		this,
		&AWhiteoutGameMode::CapturePresentationFrame,
		CaptureName.StartsWith(TEXT("lookat_")) ? 1.0f
			: CaptureName.StartsWith(TEXT("scene_")) ? 1.2f
			: 0.45f,
		false);
}

void AWhiteoutGameMode::CapturePresentationFrame()
{
	const FString& CaptureName = PresentationCaptureNames[PresentationCaptureIndex];
	if (CaptureName.StartsWith(TEXT("scene_")) || CaptureName.StartsWith(TEXT("character_")))
	{
		if (APlayerController* CaptureController = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (APawn* CapturePawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				const FVector CameraLocation = CapturePawn->GetPawnViewLocation();
				const FRotator CameraRotation = CaptureController->GetControlRotation();
				ACameraActor* CaptureCamera = GetWorld()->SpawnActor<ACameraActor>(CameraLocation, CameraRotation);
				if (CaptureCamera && CaptureCamera->GetCameraComponent())
				{
					CaptureCamera->GetCameraComponent()->SetFieldOfView(
						CaptureName.StartsWith(TEXT("character_")) ? 55.0f : 90.0f);
					CaptureCamera->SetLifeSpan(1.0f);
					CaptureController->SetViewTarget(CaptureCamera);
				}
				if (CaptureController->PlayerCameraManager)
				{
					CaptureController->PlayerCameraManager->UpdateCamera(0.0f);
				}
			}
		}
	}
	if (CaptureName.StartsWith(TEXT("focus_")))
	{
		const FName TargetAction = CaptureName.Equals(TEXT("focus_blocked"))
			? FName(TEXT("send_signal"))
			: FName(TEXT("forced_self_repair"));
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
			{
				for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
				{
					if (It->ActionId == TargetAction)
					{
						It->SetInteractionFocused(true);
						FWSActionPreview FocusPreview = It->PreviewInteraction();
						if (!CaptureName.Equals(TEXT("focus_blocked")))
						{
							FocusPreview.bCanExecute = true;
						}
						HUD->SetInteractionFocus(It->DisplayName, FocusPreview);
						break;
					}
				}
			}
		}
	}
	const FIntPoint Size = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		? GEngine->GameViewport->Viewport->GetSizeXY()
		: FIntPoint(0, 0);
	const bool bV03Capture = PresentationCaptureMode.Equals(TEXT("g1suite"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.Equals(TEXT("g2suite"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.Equals(TEXT("g3suite"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.StartsWith(TEXT("focus_"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.StartsWith(TEXT("scene_"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.StartsWith(TEXT("character_"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.Equals(TEXT("hud"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.Equals(TEXT("pause"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.Equals(TEXT("evidence"), ESearchCase::IgnoreCase);
	const TCHAR* CaptureFolder = bV03Capture
		? TEXT("../docs/baseline_v0.3")
		: TEXT("../docs/baseline_v0.2");
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / CaptureFolder);
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
