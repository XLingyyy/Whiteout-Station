#include "Flow/WhiteoutGameMode.h"

#include "Agents/WSAgentGateway.h"
#include "Algo/Sort.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Components/PointLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "HUD/WhiteoutHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Player/WhiteoutCharacter.h"
#include "Player/WhiteoutPlayerController.h"
#include "Presentation/WhiteoutAudioDirector.h"
#include "Presentation/WSPresentationText.h"
#include "Settings/WhiteoutSettingsSubsystem.h"
#include "State/WindStationStateSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "World/WSInteractableActor.h"
#include "World/WhiteoutStationBuilder.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
	FString PerformanceMovementToken(const EWSNPCMovementIntent Intent)
	{
		switch (Intent)
		{
		case EWSNPCMovementIntent::StepCloser: return TEXT("step_closer");
		case EWSNPCMovementIntent::StepBack: return TEXT("step_back");
		case EWSNPCMovementIntent::ReturnToPost: return TEXT("return_to_post");
		default: return TEXT("stay");
		}
	}

	FString PerformanceReactionToken(const EWSNPCReaction Reaction)
	{
		switch (Reaction)
		{
		case EWSNPCReaction::Acknowledge: return TEXT("acknowledge");
		case EWSNPCReaction::Consider: return TEXT("consider");
		case EWSNPCReaction::Reassure: return TEXT("reassure");
		case EWSNPCReaction::Reject: return TEXT("reject");
		case EWSNPCReaction::Alarmed: return TEXT("alarmed");
		default: return TEXT("neutral");
		}
	}

	void SavePerformanceProbeEvidence(
		const FName ActionId,
		const FWSAgentReply& Reply,
		const bool bApplied,
		const float AppliedDistance)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("schema"), TEXT("whiteout.v0.7.performance-probe.v1"));
		Root->SetStringField(TEXT("action_id"), ActionId.ToString());
		Root->SetStringField(TEXT("provider"), Reply.Provider);
		Root->SetBoolField(TEXT("fallback"), Reply.bFallback);
		Root->SetStringField(TEXT("validation_reason"), Reply.ValidationReason);
		Root->SetStringField(TEXT("movement_intent"), PerformanceMovementToken(Reply.MovementIntent));
		Root->SetStringField(TEXT("reaction_action"), PerformanceReactionToken(Reply.Reaction));
		Root->SetBoolField(TEXT("performance_applied"), bApplied);
		Root->SetNumberField(TEXT("applied_distance_cm"), AppliedDistance);

		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		if (!FJsonSerializer::Serialize(Root, Writer))
		{
			UE_LOG(LogTemp, Error, TEXT("WhiteoutStation ExpressionProbe: evidence serialization failed"));
			return;
		}
		const FString EvidencePath =
			FPaths::ProjectSavedDir() / TEXT("Logs/WhiteoutStation_PerformanceProbe.json");
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(EvidencePath), true);
		if (!FFileHelper::SaveStringToFile(
			Serialized + LINE_TERMINATOR,
			*EvidencePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogTemp, Error, TEXT("WhiteoutStation ExpressionProbe: evidence write failed"));
		}
	}

	void AddPresentationEvent(FWSGameState& State, const TCHAR* ActionId, const int32 APBefore, const int32 APAfter, const bool bCrisis = false)
	{
		FWSEventRecord& Event = State.EventLog.Emplace_GetRef();
		Event.Index = State.EventLog.Num();
		Event.ActionId = FName(ActionId);
		Event.APBefore = APBefore;
		Event.APAfter = APAfter;
		Event.ReasonCode = EWSReasonCode::Committed;
		Event.bCrisisTriggered = bCrisis;
		State.ActionCounts.FindOrAdd(Event.ActionId) += 1;
	}

	FWSGameState MakePresentationResultsState(const FWSGameState& BaseState, const EWSEndingType Ending)
	{
		FWSGameState State = BaseState;
		State.Phase = EWSGamePhase::Results;
		State.Ending = Ending;
		State.EventLog.Reset();
		State.ActionCounts.Reset();
		State.bMidCrisisTriggered = true;
		State.Score.Rating = TEXT("C");
		State.Tasks = FWSTaskState();
		State.ActionPoints = 0;

		if (Ending == EWSEndingType::TaskSuccess)
		{
			State.Flags.bRelayInstalled = true;
			State.Flags.bKitchenHeaterIntact = false;
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
			State.Flags.bMedicalRoomHeated = true;
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
			State.Flags.bSelfRepairUsed = true;
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
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation: starting playable v0.7 flow"));
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
						TEXT("WhiteoutStation IntentProbe: mapped=%s act=%s condition=%s source=%s reason=%s"),
						Intent.bMapped ? TEXT("true") : TEXT("false"),
						*StaticEnum<EWSDialogueAct>()->GetNameStringByValue(static_cast<int64>(Intent.DialogueAct)),
						*Intent.PromiseCondition.ToString(),
						*Intent.Source,
						*Intent.Reason);
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

	FString ExpressionProbeText;
	const bool bExpressionProbeRequested = FParse::Value(
		FCommandLine::Get(),
		TEXT("WhiteoutExpressionProbe="),
		ExpressionProbeText);
	if (bExpressionProbeRequested)
	{
		FString ExpressionProbeActionText = TEXT("talk_gu_heng");
		FParse::Value(
			FCommandLine::Get(),
			TEXT("WhiteoutExpressionAction="),
			ExpressionProbeActionText);
		const FName ExpressionProbeAction = ExpressionProbeActionText.Equals(
			TEXT("talk_ye_cheng"),
			ESearchCase::IgnoreCase)
			? FName(TEXT("talk_ye_cheng"))
			: FName(TEXT("talk_gu_heng"));
		bool bApplyExpressionProbe = FParse::Param(
			FCommandLine::Get(),
			TEXT("WhiteoutApplyExpressionProbe"));
		TWeakObjectPtr<AWSInteractableActor> ExpressionProbeActor;
		FVector ExpressionProbeStart = FVector::ZeroVector;
		if (bApplyExpressionProbe)
		{
			for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
			{
				if (It->ActionId == ExpressionProbeAction)
				{
					ExpressionProbeActor = *It;
					ExpressionProbeStart = It->GetActorLocation();
					break;
				}
			}
			APawn* ProbePawn = UGameplayStatics::GetPlayerPawn(this, 0);
			APlayerController* ProbeController = UGameplayStatics::GetPlayerController(this, 0);
			if (!ExpressionProbeActor.IsValid() || !ProbePawn || !ProbeController)
			{
				bApplyExpressionProbe = false;
				UE_LOG(LogTemp, Error, TEXT("WhiteoutStation ExpressionProbe: performance target unavailable"));
			}
			else
			{
				const FVector ActorLocation = ExpressionProbeActor->GetActorLocation();
				FVector ProbeDirection = ExpressionProbeActor->GetActorForwardVector().GetSafeNormal2D();
				bool bProbePositioned = false;
				FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WhiteoutExpressionProbe), false);
				QueryParams.AddIgnoredActor(ExpressionProbeActor.Get());
				QueryParams.AddIgnoredActor(ProbePawn);
				for (int32 DirectionIndex = 0; DirectionIndex < 8; ++DirectionIndex)
				{
					const FVector CandidateDirection =
						ProbeDirection.RotateAngleAxis(DirectionIndex * 45.0f, FVector::UpVector).GetSafeNormal2D();
					const FVector PlayerLocation = ActorLocation + CandidateDirection * 280.0f;
					const FVector CapsuleOffset(0.0f, 0.0f, 86.0f);
					const bool bMoveBlocked = GetWorld()->SweepTestByChannel(
						ActorLocation + CapsuleOffset,
						ActorLocation + CandidateDirection * 85.0f + CapsuleOffset,
						FQuat::Identity,
						ECC_WorldStatic,
						FCollisionShape::MakeCapsule(28.0f, 78.0f),
						QueryParams);
					const bool bSightBlocked = GetWorld()->LineTraceTestByChannel(
						ActorLocation + FVector(0.0f, 0.0f, 105.0f),
						PlayerLocation + FVector(0.0f, 0.0f, 105.0f),
						ECC_Visibility,
						QueryParams);
					const bool bPlayerBlocked = GetWorld()->OverlapBlockingTestByChannel(
						PlayerLocation + FVector(0.0f, 0.0f, 92.0f),
						FQuat::Identity,
						ECC_WorldStatic,
						FCollisionShape::MakeCapsule(34.0f, 88.0f),
						QueryParams);
					if (!bMoveBlocked && !bSightBlocked && !bPlayerBlocked)
					{
						ProbeDirection = CandidateDirection;
						ProbePawn->SetActorLocation(
							PlayerLocation + FVector(0.0f, 0.0f, 92.0f),
							false,
							nullptr,
							ETeleportType::TeleportPhysics);
						bProbePositioned = true;
						break;
					}
				}
				if (!bProbePositioned)
				{
					bApplyExpressionProbe = false;
					UE_LOG(LogTemp, Error, TEXT("WhiteoutStation ExpressionProbe: no safe performance direction"));
				}
				const FVector LookTarget = ActorLocation + FVector(0.0f, 0.0f, 105.0f);
				ProbeController->SetControlRotation(
					(LookTarget - ProbePawn->GetPawnViewLocation()).Rotation());
				if (AWhiteoutHUD* ProbeHUD = Cast<AWhiteoutHUD>(ProbeController->GetHUD()))
				{
					ProbeHUD->DismissOpening();
					ProbeHUD->SetInterfaceVisibleForCapture(false);
				}
			}
		}
		IntentProbeGateway = NewObject<UWSAgentGateway>(this);
		IntentProbeGateway->Initialize();
		const UWindStationStateSubsystem* ProbeSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
		const FWSGameState ProbeState = ProbeSubsystem
			? ProbeSubsystem->GetStateSnapshot()
			: FWSGameState();
		TWeakObjectPtr<AWhiteoutGameMode> WeakThis(this);
		IntentProbeGateway->RequestExpression(
			ExpressionProbeAction,
			ProbeState,
			true,
			FWSAgentReplyCallback::CreateLambda(
				[WeakThis, bApplyExpressionProbe, ExpressionProbeActor, ExpressionProbeStart, ExpressionProbeAction](const FWSAgentReply& Reply)
				{
					UE_LOG(
						LogTemp,
						Display,
						TEXT("WhiteoutStation ExpressionProbe: provider=%s fallback=%s validation=%s movement=%s reaction=%s line_chars=%d"),
						*Reply.Provider,
						Reply.bFallback ? TEXT("true") : TEXT("false"),
						*Reply.ValidationReason,
						*StaticEnum<EWSNPCMovementIntent>()->GetNameStringByValue(static_cast<int64>(Reply.MovementIntent)),
						*StaticEnum<EWSNPCReaction>()->GetNameStringByValue(static_cast<int64>(Reply.Reaction)),
						Reply.Utterance.Len());
					if (WeakThis.IsValid())
					{
						if (bApplyExpressionProbe)
						{
							if (APlayerController* ProbeController =
								UGameplayStatics::GetPlayerController(WeakThis.Get(), 0))
							{
								if (AWhiteoutHUD* ProbeHUD = Cast<AWhiteoutHUD>(ProbeController->GetHUD()))
								{
									ProbeHUD->DismissOpening();
									ProbeHUD->SetInterfaceVisibleForCapture(false);
								}
								if (APawn* ProbePawn = UGameplayStatics::GetPlayerPawn(WeakThis.Get(), 0))
								{
									ProbeController->SetViewTarget(ProbePawn);
									if (ExpressionProbeActor.IsValid())
									{
										const FVector LookTarget =
											ExpressionProbeActor->GetActorLocation() + FVector(0.0f, 0.0f, 105.0f);
										ProbeController->SetControlRotation(
											(LookTarget - ProbePawn->GetPawnViewLocation()).Rotation());
									}
								}
							}
							if (UWindStationStateSubsystem* StateSubsystem =
								WeakThis->GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
							{
								StateSubsystem->OnDialogueLine.Broadcast(Reply);
							}
							const bool bCaptureToSaved = FParse::Param(
								FCommandLine::Get(),
								TEXT("WhiteoutPerformanceCaptureToSaved"));
							if (
								bCaptureToSaved
								|| FParse::Param(FCommandLine::Get(), TEXT("WhiteoutPerformanceCapture")))
							{
								const FString EvidenceDirectory = bCaptureToSaved
									? FPaths::ProjectSavedDir() / TEXT("PerformanceProbe")
									: FPaths::ConvertRelativePathToFull(
										FPaths::ProjectDir() / TEXT("../docs/evidence_v0.7"));
								IFileManager::Get().MakeDirectory(*EvidenceDirectory, true);
								const FString EvidenceToken =
									ExpressionProbeActor.IsValid()
									&& ExpressionProbeActor->ActionId == TEXT("talk_ye_cheng")
									? TEXT("YeCheng")
									: TEXT("GuHeng");
								FTimerHandle MoveCaptureTimer;
								WeakThis->GetWorldTimerManager().SetTimer(
									MoveCaptureTimer,
									[EvidenceDirectory, EvidenceToken]()
									{
										FScreenshotRequest::RequestScreenshot(
											EvidenceDirectory / FString::Printf(
												TEXT("NPC_%s_Walk.png"),
												*EvidenceToken),
											true,
											false,
											false,
											FIntRect(),
											true);
									},
									0.45f,
									false);
								FTimerHandle ReactionCaptureTimer;
								WeakThis->GetWorldTimerManager().SetTimer(
									ReactionCaptureTimer,
									[EvidenceDirectory, EvidenceToken]()
									{
										FScreenshotRequest::RequestScreenshot(
											EvidenceDirectory / FString::Printf(
												TEXT("NPC_%s_Acknowledge.png"),
												*EvidenceToken),
											true,
											false,
											false,
											FIntRect(),
											true);
									},
									1.35f,
									false);
							}
						}
						FTimerHandle ExitTimer;
						WeakThis->GetWorldTimerManager().SetTimer(
							ExitTimer,
							[
								ExpressionProbeActor,
								ExpressionProbeStart,
								ExpressionProbeAction,
								bApplyExpressionProbe,
								PerformanceReply = Reply
							]()
							{
								float AppliedDistance = 0.0f;
								if (ExpressionProbeActor.IsValid())
								{
									AppliedDistance = FVector::Dist2D(
										ExpressionProbeStart,
										ExpressionProbeActor->GetActorLocation());
									UE_LOG(
										LogTemp,
										Display,
										TEXT("WhiteoutStation ExpressionProbe: applied_distance=%.2f"),
										AppliedDistance);
								}
								SavePerformanceProbeEvidence(
									ExpressionProbeAction,
									PerformanceReply,
									bApplyExpressionProbe && ExpressionProbeActor.IsValid(),
									AppliedDistance);
								FPlatformMisc::RequestExit(false);
							},
							bApplyExpressionProbe ? 2.9f : 0.4f,
							false);
					}
				}),
			ExpressionProbeText);
	}

	const bool bDialogueHistoryProbeRequested = FParse::Param(
		FCommandLine::Get(),
		TEXT("WhiteoutDialogueHistoryProbe"));
	if (bDialogueHistoryProbeRequested)
	{
		IntentProbeGateway = NewObject<UWSAgentGateway>(this);
		IntentProbeGateway->Initialize();
		DialogueHistoryProbeSessionId = FGuid::NewGuid();
		RunDialogueHistoryProbeStep(0);
	}

	FString InputSmokeTarget;
	if (FParse::Value(
		FCommandLine::Get(),
		TEXT("WhiteoutInputSmokeTarget="),
		InputSmokeTarget))
	{
		FTimerHandle InputSmokeSetupTimer;
		GetWorldTimerManager().SetTimer(
			InputSmokeSetupTimer,
			[this, InputSmokeTarget]()
			{
				SetupInputSmokeTarget(InputSmokeTarget);
			},
			0.75f,
			false);
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
		GetWorldTimerManager().SetTimer(PresentationTimer, this, &AWhiteoutGameMode::BeginPresentationCapture, 4.0f, false);
	}

	FString SettingsAuditMode;
	if (FParse::Value(FCommandLine::Get(), TEXT("WhiteoutSettingsAudit="), SettingsAuditMode))
	{
		FTimerHandle SettingsAuditTimer;
		GetWorldTimerManager().SetTimer(
			SettingsAuditTimer,
			[this, SettingsAuditMode]() { RunSettingsAudit(SettingsAuditMode); },
			1.0f,
			false);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("WhiteoutJumpAudit")))
	{
		FTimerHandle JumpAuditTimer;
		GetWorldTimerManager().SetTimer(JumpAuditTimer, this, &AWhiteoutGameMode::BeginJumpCapture, 2.0f, false);
	}

	if (AutoRoute.IsEmpty()
		&& !bIntentProbeRequested
		&& !bExpressionProbeRequested
		&& !bDialogueHistoryProbeRequested
		&& !FParse::Param(FCommandLine::Get(), TEXT("WhiteoutBaselineCapture"))
		&& SettingsAuditMode.IsEmpty()
		&& !FParse::Param(FCommandLine::Get(), TEXT("WhiteoutJumpAudit"))
		&& !bPerformanceTestActive)
	{
		FTimerHandle OpeningStartTimer;
		GetWorldTimerManager().SetTimer(OpeningStartTimer, this, &AWhiteoutGameMode::BeginOpeningPresentation, 0.25f, false);
	}
}

void AWhiteoutGameMode::RunDialogueHistoryProbeStep(const int32 Step)
{
	if (!IntentProbeGateway || !DialogueHistoryProbeSessionId.IsValid())
	{
		FPlatformMisc::RequestExit(false);
		return;
	}
	const UWindStationStateSubsystem* StateSubsystem =
		GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	const FWSGameState State = StateSubsystem
		? StateSubsystem->GetStateSnapshot()
		: FWSGameState();
	FWSActionRequest Request;
	Request.ActionId = TEXT("talk_gu_heng");
	Request.DialogueAct = Step == 0
		? EWSDialogueAct::Ask
		: EWSDialogueAct::Reassure;
	Request.PlayerSaid = Step == 0
		? TEXT("继电器现在是什么状态？")
		: TEXT("先稳住，我们会按顺序处理。");
	Request.TransactionId = FGuid::NewGuid();
	Request.DialogueSessionId = DialogueHistoryProbeSessionId;
	TWeakObjectPtr<AWhiteoutGameMode> WeakThis(this);
	IntentProbeGateway->RequestExpression(
		Request,
		State,
		true,
		FWSAgentReplyCallback::CreateLambda(
			[WeakThis, Step](const FWSAgentReply& Reply)
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("WhiteoutStation DialogueHistoryProbe: step=%d provider=%s fallback=%s validation=%s line_chars=%d"),
					Step + 1,
					*Reply.Provider,
					Reply.bFallback ? TEXT("true") : TEXT("false"),
					*Reply.ValidationReason,
					Reply.Utterance.Len());
				if (!WeakThis.IsValid())
				{
					return;
				}
				if (Step == 0)
				{
					WeakThis->RunDialogueHistoryProbeStep(1);
					return;
				}
				FTimerHandle ExitTimer;
				WeakThis->GetWorldTimerManager().SetTimer(
					ExitTimer,
					[]() { FPlatformMisc::RequestExit(false); },
					0.4f,
					false);
			}));
}

void AWhiteoutGameMode::SetupInputSmokeTarget(const FString& ActionId)
{
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	APlayerController* PlayerController =
		UGameplayStatics::GetPlayerController(this, 0);
	if (!Pawn || !PlayerController)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("WhiteoutStation InputSmokeSetup: missing pawn/controller"));
		return;
	}
	for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActionId.ToString().Equals(ActionId, ESearchCase::IgnoreCase))
		{
			continue;
		}
		FVector BoundsOrigin = It->GetActorLocation();
		FVector BoundsExtent = FVector::ZeroVector;
		It->GetActorBounds(false, BoundsOrigin, BoundsExtent);
		const FVector OriginalPawnLocation = Pawn->GetActorLocation();
		const float CandidateDistance = FMath::Clamp(
			FMath::Max(BoundsExtent.X, BoundsExtent.Y) + 260.0f,
			300.0f,
			380.0f);
		FVector PawnLocation = It->GetActorLocation()
			+ FVector(CandidateDistance, 0.0f, 0.0f);
		PawnLocation.Z = OriginalPawnLocation.Z;
		int32 SelectedCandidate = INDEX_NONE;
		FCollisionQueryParams SightParams(
			SCENE_QUERY_STAT(WhiteoutInputSmokeSight),
			false,
			Pawn);
		constexpr int32 CandidateCount = 16;
		for (int32 CandidateIndex = 0;
			CandidateIndex < CandidateCount;
			++CandidateIndex)
		{
			const float Angle = 2.0f * PI
				* static_cast<float>(CandidateIndex)
				/ static_cast<float>(CandidateCount);
			const FVector Direction(
				FMath::Cos(Angle),
				FMath::Sin(Angle),
				0.0f);
			FVector CandidateLocation =
				BoundsOrigin + Direction * CandidateDistance;
			CandidateLocation.Z = OriginalPawnLocation.Z;
			if (!GetWorld()->FindTeleportSpot(
				Pawn,
				CandidateLocation,
				Pawn->GetActorRotation()))
			{
				continue;
			}
			const FVector CandidateCameraLocation =
				CandidateLocation + FVector(0.0f, 0.0f, 64.0f);
			FHitResult SightHit;
			if (!GetWorld()->LineTraceSingleByChannel(
					SightHit,
					CandidateCameraLocation,
					BoundsOrigin,
					ECC_Visibility,
					SightParams)
				|| SightHit.GetActor() != *It)
			{
				continue;
			}
			PawnLocation = CandidateLocation;
			SelectedCandidate = CandidateIndex;
			break;
		}
		Pawn->SetActorLocation(
			PawnLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		const FVector CameraLocation =
			PawnLocation + FVector(0.0f, 0.0f, 64.0f);
		PlayerController->SetControlRotation(
			(BoundsOrigin - CameraLocation).Rotation());
		UE_LOG(
			LogTemp,
			Display,
			TEXT("WhiteoutStation InputSmokeSetup: target=%s pawn=%s focus=%s candidate=%d"),
			*It->ActionId.ToString(),
			*PawnLocation.ToCompactString(),
			*BoundsOrigin.ToCompactString(),
			SelectedCandidate);
		return;
	}
	UE_LOG(
		LogTemp,
		Error,
		TEXT("WhiteoutStation InputSmokeSetup: target not found (%s)"),
		*ActionId);
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
	const float AverageFps = 1000.0f / FMath::Max(MeanMs, 0.001f);
	const float OnePercentLowFps = 1000.0f / FMath::Max(SlowTailMeanMs, 0.001f);
	const float P95Ms = SortedFrameTimes[P95Index];
	const float P99Ms = SortedFrameTimes[P99Index];
	const float MaxMs = SortedFrameTimes.Last();
	const bool bPassed = Resolution == FIntPoint(1920, 1080)
		&& AverageFps >= 60.0f
		&& OnePercentLowFps >= 60.0f;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("WhiteoutStation Performance: resolution=%dx%d samples=%d avg_fps=%.2f one_percent_low_fps=%.2f p95_ms=%.3f p99_ms=%.3f max_ms=%.3f"),
		Resolution.X,
		Resolution.Y,
		SortedFrameTimes.Num(),
		AverageFps,
		OnePercentLowFps,
		P95Ms,
		P99Ms,
		MaxMs);
	const FString Json = FString::Printf(
		TEXT("{\n  \"schema\": \"whiteout.v0.3.performance.v1\",\n  \"passed\": %s,\n  \"resolution\": \"%dx%d\",\n  \"samples\": %d,\n  \"average_fps\": %.2f,\n  \"one_percent_low_fps\": %.2f,\n  \"p95_frame_ms\": %.3f,\n  \"p99_frame_ms\": %.3f,\n  \"max_frame_ms\": %.3f,\n  \"minimum_average_fps\": 60.0,\n  \"minimum_one_percent_low_fps\": 60.0\n}\n"),
		bPassed ? TEXT("true") : TEXT("false"),
		Resolution.X,
		Resolution.Y,
		SortedFrameTimes.Num(),
		AverageFps,
		OnePercentLowFps,
		P95Ms,
		P99Ms,
		MaxMs);
	const FString OutputPath = FPaths::ProjectSavedDir() / TEXT("WhiteoutPerformance.json");
	FFileHelper::SaveStringToFile(Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
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
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.7: manual opening presentation started"));
}

void AWhiteoutGameMode::PrepareOpeningReveal()
{
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			PlayerController->SetViewTarget(PlayerPawn);
		}
	}
}

void AWhiteoutGameMode::FinishOpeningPresentation()
{
	if (bOpeningFinished)
	{
		return;
	}
	bOpeningFinished = true;
	GetWorldTimerManager().ClearTimer(OpeningFinishTimer);
	PrepareOpeningReveal();
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
	}
	if (OpeningCamera)
	{
		OpeningCamera->SetLifeSpan(1.1f);
		OpeningCamera = nullptr;
	}
	FString InputSmokeTarget;
	if (FParse::Value(
			FCommandLine::Get(),
			TEXT("WhiteoutInputSmokeTarget="),
			InputSmokeTarget))
	{
		SetupInputSmokeTarget(InputSmokeTarget);
	}
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.7: opening handed control to player"));
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
	else if (PresentationCaptureMode.Equals(TEXT("v04dialogue"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("dialogue_gu_default"), TEXT("dialogue_gu_diagnosed"),
			TEXT("dialogue_gu_treated"), TEXT("dialogue_gu_cooperative"),
			TEXT("dialogue_ye_default"), TEXT("dialogue_ye_heated"), TEXT("dialogue_ye_treated"),
			TEXT("dialogue_promise"), TEXT("dialogue_ask_entry"), TEXT("dialogue_reply")};
	}
	else if (PresentationCaptureMode.Equals(TEXT("v04characters"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("character_gu_front"), TEXT("character_gu_side"), TEXT("character_gu_feet"),
			TEXT("character_ye_front"), TEXT("character_ye_side"), TEXT("character_ye_feet"),
			TEXT("character_ye_back"), TEXT("character_ye_arm")};
	}
	else if (PresentationCaptureMode.Equals(TEXT("v04evidence"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("evidence_all"), TEXT("evidence_files"), TEXT("evidence_items"),
			TEXT("evidence_witnesses"), TEXT("evidence_dialogue"), TEXT("evidence_detail")};
	}
	else if (PresentationCaptureMode.Equals(TEXT("v04focus"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {TEXT("focus_npc")};
	}
	else if (PresentationCaptureMode.Equals(TEXT("g4systems"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("settings_default"), TEXT("settings_adjusted"),
			TEXT("clock_0815"), TEXT("clock_1815"),
			TEXT("opening_controls_time"), TEXT("crisis_emergency_time"), TEXT("results_timeline_time"),
			TEXT("lighting_control_ceiling"), TEXT("lighting_antenna_front"), TEXT("lighting_antenna_side")};
	}
	else if (PresentationCaptureMode.Equals(TEXT("g4lighting"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("lighting_control_ceiling"), TEXT("lighting_antenna_front"), TEXT("lighting_antenna_side")};
	}
	else if (PresentationCaptureMode.Equals(TEXT("v06ux"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("opening_story_01"), TEXT("opening_story_04"), TEXT("opening_story_07"),
			TEXT("guide"),
			TEXT("dialogue_gu_initial"), TEXT("dialogue_ye_initial"), TEXT("dialogue_gu_unlocked")};
	}
	else if (PresentationCaptureMode.Equals(TEXT("v06product"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("pause"), TEXT("settings_default"),
			TEXT("results_task"), TEXT("results_survival"),
			TEXT("results_cost"), TEXT("results_collapse")};
	}
	else if (PresentationCaptureMode.Equals(TEXT("v07ui"), ESearchCase::IgnoreCase))
	{
		PresentationCaptureNames = {
			TEXT("hud"), TEXT("focus_near"), TEXT("focus_npc"), TEXT("dialogue_response")};
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
		const int32 Stage = CaptureName.StartsWith(TEXT("opening_story_"))
			? FMath::Clamp(FCString::Atoi(*CaptureName.Right(2)) - 1, 0, 6)
			: CaptureName.Equals(TEXT("opening_title")) ? 0
				: CaptureName.Equals(TEXT("opening_establishing")) ? 1
				: CaptureName.Equals(TEXT("opening_objective")) ? 2
				: 3;
		HUD->SetOpeningCaptureStage(Stage);
	}
	else if (CaptureName.StartsWith(TEXT("focus_")))
	{
		const bool bBlocked = CaptureName.Equals(TEXT("focus_blocked"));
		const bool bNPC = CaptureName.Equals(TEXT("focus_npc"));
		const FName TargetAction = bNPC ? FName(TEXT("talk_gu_heng"))
			: bBlocked ? FName(TEXT("send_signal")) : FName(TEXT("forced_self_repair"));
		UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.4 focus capture name=%s npc=%d target=%s"),
			*CaptureName, bNPC ? 1 : 0, *TargetAction.ToString());
		for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
		{
			if (It->ActionId != TargetAction)
			{
				continue;
			}
			if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				if (bNPC)
				{
					const FVector FocusInspectionLocation(10000.0f, 10000.0f, 0.0f);
					It->SetActorLocation(FocusInspectionLocation, false, nullptr, ETeleportType::TeleportPhysics);
					It->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
					if (PlayerController && PlayerController->PlayerCameraManager)
					{
						PlayerController->PlayerCameraManager->SetFOV(55.0f);
					}
					if (!LookAtCaptureLight)
					{
						LookAtCaptureLight = GetWorld()->SpawnActor<APointLight>(FocusInspectionLocation, FRotator::ZeroRotator);
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
						LookAtCaptureLight->SetActorLocation(FocusInspectionLocation + FVector(70.0f, 110.0f, 205.0f));
					}
				}
				const FVector LookTarget = It->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
				FVector ViewDirection = bNPC ? -It->GetActorRightVector() : Pawn->GetPawnViewLocation() - LookTarget;
				if (!ViewDirection.Normalize())
				{
					ViewDirection = FVector(-1.0f, 0.0f, 0.0f);
				}
				const float FocusDistance = bNPC ? 220.0f : bBlocked ? 300.0f
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
			It->SetDialogueLookAtActive(bNPC);
			FWSActionPreview FocusPreview = It->PreviewInteraction();
			if (!bBlocked)
			{
				FocusPreview.bCanExecute = true;
			}
			if (bNPC)
			{
				HUD->ShowNPCFocusForCapture(It->DisplayName, FocusPreview);
			}
			else
			{
				HUD->SetInteractionFocus(It->DisplayName, FocusPreview);
			}
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
	else if (CaptureName.Equals(TEXT("guide")))
	{
		HUD->SetPresentationCaptureState(StateSubsystem->GetStateSnapshot());
		HUD->ToggleGuide();
	}
	else if (CaptureName.StartsWith(TEXT("settings_")))
	{
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			Pawn->SetActorLocation(FVector(-190.0f, 245.0f, 105.0f), false, nullptr, ETeleportType::TeleportPhysics);
			if (PlayerController)
			{
				PlayerController->SetControlRotation(FRotator(-2.0f, -72.0f, 0.0f));
			}
		}
		if (UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
		{
			const bool bAdjusted = CaptureName.Equals(TEXT("settings_adjusted"));
			Settings->SetFieldOfView(bAdjusted ? 103.0f : 90.0f, this);
			Settings->SetMasterVolume(bAdjusted ? 0.76f : 1.0f, this);
			Settings->SetAmbienceVolume(bAdjusted ? 0.48f : 1.0f, this);
			Settings->SetEffectsVolume(bAdjusted ? 0.64f : 1.0f, this);
			Settings->SetFeedbackVolume(bAdjusted ? 0.82f : 1.0f, this);
		}
		HUD->ShowSettingsForCapture();
		if (PlayerController)
		{
			PlayerController->SetPause(false);
		}
	}
	else if (CaptureName.StartsWith(TEXT("clock_")))
	{
		FWSGameState ClockState = StateSubsystem->GetStateSnapshot();
		ClockState.ActionPoints = CaptureName.Equals(TEXT("clock_1815")) ? 0 : 8;
		ClockState.bMidCrisisTriggered = ClockState.ActionPoints == 0;
		HUD->SetPresentationCaptureState(ClockState);
	}
	else if (CaptureName.StartsWith(TEXT("lighting_")))
	{
		HUD->SetInterfaceVisibleForCapture(false);
		for (TActorIterator<AWhiteoutStationBuilder> It(GetWorld()); It; ++It)
		{
			It->SetLightingPreviewState(false, false);
			break;
		}
		APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Pawn && PlayerController)
		{
			FVector Location(-190.0f, 245.0f, 105.0f);
			FRotator Rotation(-8.0f, -72.0f, 0.0f);
			if (CaptureName.Equals(TEXT("lighting_antenna_front")))
			{
				Location = FVector(1810.0f, 400.0f, 115.0f);
				Rotation = FRotator(-4.0f, 0.0f, 0.0f);
			}
			else if (CaptureName.Equals(TEXT("lighting_antenna_side")))
			{
				Location = FVector(2650.0f, 900.0f, 125.0f);
				Rotation = FRotator(-5.0f, -129.0f, 0.0f);
			}
			PlayerController->SetViewTarget(Pawn);
			Pawn->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
			PlayerController->SetControlRotation(Rotation);
			if (ACharacter* CaptureCharacter = Cast<ACharacter>(Pawn))
			{
				CaptureCharacter->GetCharacterMovement()->DisableMovement();
			}
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
		DialogueState.Flags.bGuHengDiagnosed = false;
		DialogueState.Flags.bGuHengTreated = false;
		DialogueState.Flags.bGuHengCooperative = false;
		DialogueState.Flags.bMedicalRoomHeated = false;
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
		if (CaptureName.Equals(TEXT("dialogue_gu_diagnosed"))) DialogueState.Flags.bGuHengDiagnosed = true;
		else if (CaptureName.Equals(TEXT("dialogue_gu_treated"))) DialogueState.Flags.bGuHengTreated = true;
		else if (CaptureName.Equals(TEXT("dialogue_gu_cooperative"))) DialogueState.Flags.bGuHengCooperative = true;
		else if (CaptureName.Equals(TEXT("dialogue_ye_heated"))) DialogueState.Flags.bMedicalRoomHeated = true;
		else if (CaptureName.Equals(TEXT("dialogue_ye_treated"))) DialogueState.Flags.bGuHengTreated = true;
		else if (CaptureName.Equals(TEXT("dialogue_gu_unlocked")))
		{
			DialogueState.Flags.bGuHengDiagnosed = true;
			DialogueState.PlayerKnowledge.Add(
				TEXT("FACT_FORCED_RESTART_SUSPICION"),
				EWSKnowledgeLevel::Suspected);
		}
		HUD->SetPresentationCaptureState(DialogueState);
		const FName NPCAction = CaptureName.StartsWith(TEXT("dialogue_ye_"))
			? FName(TEXT("talk_ye_cheng")) : FName(TEXT("talk_gu_heng"));
		HUD->ShowDialogueMenu(NPCAction, true);
		if (CaptureName.Equals(TEXT("dialogue_promise")))
		{
			HUD->ShowDialoguePromiseChoices();
		}
		else if (CaptureName.Equals(TEXT("dialogue_free")) || CaptureName.Equals(TEXT("dialogue_ask_entry")))
		{
			HUD->ShowDialogueFreeTextForCapture();
		}
		else if (CaptureName.Equals(TEXT("dialogue_offline")))
		{
			HUD->SetDialogueIntentStatus(TEXT("离线模式｜当前使用本地意图词典；无法可靠识别时将回到安全轮盘。"), false);
		}
		else if (CaptureName.Equals(TEXT("dialogue_response")) || CaptureName.Equals(TEXT("dialogue_reply")))
		{
			HUD->ShowDialogueReplyForCapture(
				TEXT("顾衡"),
				TEXT("手还不能精细操作。把维修间升温，我就配合修复发电机。"));
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
			TEXT("character_ye_near"), TEXT("character_ye_mid"),
			TEXT("character_gu_front"), TEXT("character_gu_side"), TEXT("character_gu_feet"),
			TEXT("character_ye_front"), TEXT("character_ye_side"), TEXT("character_ye_feet"),
			TEXT("character_ye_back"), TEXT("character_ye_arm")};
		const TArray<FVector> SceneCaptureLocations = {
			FVector(520, 300, 105), FVector(520, 300, 105),
			FVector(520, 300, 105), FVector(860, 720, 115),
			FVector(1000, 350, 120), FVector(620, 300, 105),
			FVector(1600, 300, 105), FVector(-180, 520, 105),
			FVector(120, 1120, 105), FVector(1500, 520, 105),
			FVector(735, 160, 105), FVector(600, 160, 110),
			FVector(120, 690, 105), FVector(120, 560, 110),
			FVector(735, 160, 105), FVector(735, 160, 105), FVector(735, 160, 105),
			FVector(120, 690, 105), FVector(120, 690, 105), FVector(120, 690, 105),
			FVector(120, 690, 105), FVector(120, 690, 105)};
		const TArray<FRotator> SceneCaptureRotations = {
			FRotator(-2, -142, 0), FRotator(-4, -142, 0),
			FRotator(-2, 0, 0), FRotator(-2, 0, 0),
			FRotator(-3, -55, 0), FRotator(-1, -28, 0),
			FRotator(-3, -145, 0), FRotator(-2, 30, 0),
			FRotator(-1, -90, 0), FRotator(-2, 143, 0),
			FRotator(-1, 0, 0), FRotator(-1, 0, 0),
			FRotator(-1, 90, 0), FRotator(-1, 90, 0),
			FRotator(-1, 0, 0), FRotator(-1, 0, 0), FRotator(-1, 0, 0),
			FRotator(-1, 90, 0), FRotator(-1, 90, 0), FRotator(-1, 90, 0),
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
			const FVector InspectionLocation = bEngineer
				? FVector(10000.0f, 10000.0f, 0.0f)
				: FVector(10000.0f, 10600.0f, 0.0f);
			if (!CharacterCaptureGround)
			{
				CharacterCaptureGround = GetWorld()->SpawnActor<AStaticMeshActor>(InspectionLocation, FRotator::ZeroRotator);
				if (CharacterCaptureGround)
				{
					if (UStaticMesh* GroundMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
					{
						CharacterCaptureGround->GetStaticMeshComponent()->SetStaticMesh(GroundMesh);
					}
					if (UMaterialInterface* GridMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")))
					{
						CharacterCaptureGround->GetStaticMeshComponent()->SetMaterial(0, GridMaterial);
					}
					CharacterCaptureGround->SetActorScale3D(FVector(12.0f, 12.0f, 0.1f));
				}
			}
			if (CharacterCaptureGround)
			{
				CharacterCaptureGround->SetActorLocation(InspectionLocation - FVector(0.0f, 0.0f, 5.0f));
			}
			for (int32 GridLine = -6; GridLine <= 6; ++GridLine)
			{
				const float GridOffset = static_cast<float>(GridLine) * 50.0f;
				DrawDebugLine(GetWorld(), InspectionLocation + FVector(GridOffset, -300.0f, 0.5f),
					InspectionLocation + FVector(GridOffset, 300.0f, 0.5f), FColor(42, 73, 98), false, 120.0f, 0, 1.5f);
				DrawDebugLine(GetWorld(), InspectionLocation + FVector(-300.0f, GridOffset, 0.5f),
					InspectionLocation + FVector(300.0f, GridOffset, 0.5f), FColor(42, 73, 98), false, 120.0f, 0, 1.5f);
			}
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
				It->SetActorLocation(InspectionLocation, false, nullptr, ETeleportType::TeleportPhysics);
				It->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
				It->SetCharacterPreviewMood(true);
				const FVector Forward = It->GetActorForwardVector();
				const FVector Right = It->GetActorRightVector();
				FVector ViewDirection = -Right;
				if (CaptureName.EndsWith(TEXT("_side"))) ViewDirection = Forward;
				else if (CaptureName.EndsWith(TEXT("_back"))) ViewDirection = Right;
				else if (CaptureName.EndsWith(TEXT("_arm"))) ViewDirection = (-Right + Forward).GetSafeNormal();
				const bool bMidView = CaptureName.EndsWith(TEXT("_mid"));
				const bool bFeetView = CaptureName.EndsWith(TEXT("_feet"));
				const float Distance = bMidView ? 420.0f : bFeetView ? 280.0f : 240.0f;
				FVector PawnLocation = It->GetActorLocation() + ViewDirection * Distance;
				PawnLocation.Z = 105.0f;
				Pawn->SetActorLocation(PawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
				const FVector CameraLocation = Pawn->GetPawnViewLocation();
				const float LookTargetHeight = bFeetView ? 26.0f : bMidView ? 92.0f : 145.0f;
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
	else if (CaptureName.Equals(TEXT("evidence")) || CaptureName.StartsWith(TEXT("evidence_")))
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
		const int32 EvidenceFilter = CaptureName.Equals(TEXT("evidence_files")) ? 1
			: CaptureName.Equals(TEXT("evidence_items")) ? 2
			: CaptureName.Equals(TEXT("evidence_witnesses")) ? 3
			: CaptureName.Equals(TEXT("evidence_dialogue")) ? 4
			: 0;
		HUD->ShowEvidenceForCapture(EvidenceFilter, CaptureName.Equals(TEXT("evidence_detail")));
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
			: CaptureName.StartsWith(TEXT("lighting_")) ? 3.0f
			: CaptureName.StartsWith(TEXT("scene_")) ? 1.2f
			: 0.45f,
		false);
}

void AWhiteoutGameMode::CapturePresentationFrame()
{
	const FString& CaptureName = PresentationCaptureNames[PresentationCaptureIndex];
	if (CaptureName.Equals(TEXT("focus_npc")))
	{
		if (APlayerController* CaptureController = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (AWhiteoutHUD* CaptureHUD = Cast<AWhiteoutHUD>(CaptureController->GetHUD()))
			{
				for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It)
				{
					if (It->ActionId == TEXT("talk_gu_heng"))
					{
						CaptureHUD->ShowNPCFocusForCapture(It->DisplayName, It->PreviewInteraction());
						const FVector LookTarget = It->GetActorLocation() + FVector(0.0f, 0.0f, 105.0f);
						const FVector CameraLocation = LookTarget - It->GetActorRightVector() * 300.0f;
						ACameraActor* CaptureCamera = GetWorld()->SpawnActor<ACameraActor>(
							CameraLocation, (LookTarget - CameraLocation).Rotation());
						if (CaptureCamera && CaptureCamera->GetCameraComponent())
						{
							CaptureCamera->GetCameraComponent()->SetFieldOfView(55.0f);
							CaptureCamera->SetLifeSpan(2.0f);
							CaptureController->SetViewTarget(CaptureCamera);
						}
						if (CaptureController->PlayerCameraManager)
						{
							CaptureController->PlayerCameraManager->UpdateCamera(0.0f);
						}
						break;
					}
				}
			}
		}
	}
	if (CaptureName.StartsWith(TEXT("scene_")) || CaptureName.StartsWith(TEXT("character_")) || CaptureName.StartsWith(TEXT("lighting_")))
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
		const bool bNPC = CaptureName.Equals(TEXT("focus_npc"));
		const FName TargetAction = bNPC ? FName(TEXT("talk_gu_heng"))
			: CaptureName.Equals(TEXT("focus_blocked")) ? FName(TEXT("send_signal"))
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
						if (bNPC)
						{
							HUD->ShowNPCFocusForCapture(It->DisplayName, FocusPreview);
						}
						else
						{
							HUD->SetInteractionFocus(It->DisplayName, FocusPreview);
						}
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
		|| PresentationCaptureMode.Equals(TEXT("g4systems"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.Equals(TEXT("g4lighting"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.StartsWith(TEXT("focus_"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.StartsWith(TEXT("scene_"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.StartsWith(TEXT("character_"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.StartsWith(TEXT("lighting_"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.Equals(TEXT("hud"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.Equals(TEXT("pause"), ESearchCase::IgnoreCase)
		|| PresentationCaptureMode.Equals(TEXT("evidence"), ESearchCase::IgnoreCase);
	const bool bV07Capture = FParse::Param(FCommandLine::Get(), TEXT("WhiteoutV07Capture"));
	const bool bV06Capture = FParse::Param(FCommandLine::Get(), TEXT("WhiteoutV06Capture"));
	const bool bV04Capture = FParse::Param(FCommandLine::Get(), TEXT("WhiteoutV04Capture"));
	const TCHAR* CaptureFolder = bV07Capture
		? TEXT("../docs/baseline_v0.7")
		: bV06Capture
		? TEXT("../docs/baseline_v0.6")
		: bV04Capture
		? TEXT("../docs/baseline_v0.4")
		: bV03Capture ? TEXT("../docs/baseline_v0.3") : TEXT("../docs/baseline_v0.2");
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

void AWhiteoutGameMode::RunSettingsAudit(const FString& Mode)
{
	UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>();
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation G4 SettingsAudit: subsystem unavailable"));
		FPlatformMisc::RequestExit(false);
		return;
	}
	const bool bWrite = Mode.Equals(TEXT("write"), ESearchCase::IgnoreCase);
	if (bWrite)
	{
		Settings->SetFieldOfView(103.0f, this);
		Settings->SetMasterVolume(0.76f, this);
		Settings->SetAmbienceVolume(0.48f, this);
		Settings->SetEffectsVolume(0.64f, this);
		Settings->SetFeedbackVolume(0.82f, this);
	}
	else
	{
		Settings->Apply(this);
	}
	const float CameraFOV = Cast<AWhiteoutCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))
		? CastChecked<AWhiteoutCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))->FirstPersonCamera->FieldOfView
		: 0.0f;
	const bool bPass = FMath::IsNearlyEqual(Settings->GetFieldOfView(), 103.0f, 0.01f)
		&& FMath::IsNearlyEqual(Settings->GetMasterVolume(), 0.76f, 0.01f)
		&& FMath::IsNearlyEqual(Settings->GetAmbienceVolume(), 0.48f, 0.01f)
		&& FMath::IsNearlyEqual(Settings->GetEffectsVolume(), 0.64f, 0.01f)
		&& FMath::IsNearlyEqual(Settings->GetFeedbackVolume(), 0.82f, 0.01f)
		&& FMath::IsNearlyEqual(CameraFOV, 103.0f, 0.01f);
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../docs/evidence_v0.3"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString OutputPath = Directory / FString::Printf(TEXT("G4_SettingsAudit_%s.json"), bWrite ? TEXT("write") : TEXT("verify"));
	const FString Json = FString::Printf(
		TEXT("{\n  \"schema\": \"whiteout.g4.settings.v1\",\n  \"mode\": \"%s\",\n  \"passed\": %s,\n  \"ini\": \"GameUserSettings.ini\",\n  \"field_of_view\": %.2f,\n  \"camera_field_of_view\": %.2f,\n  \"master_volume\": %.2f,\n  \"ambience_volume\": %.2f,\n  \"effects_volume\": %.2f,\n  \"feedback_volume\": %.2f\n}\n"),
		bWrite ? TEXT("write") : TEXT("verify_after_restart"),
		bPass ? TEXT("true") : TEXT("false"),
		Settings->GetFieldOfView(), CameraFOV,
		Settings->GetMasterVolume(), Settings->GetAmbienceVolume(),
		Settings->GetEffectsVolume(), Settings->GetFeedbackVolume());
	FFileHelper::SaveStringToFile(Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation G4 SettingsAudit: mode=%s pass=%s fov=%.2f output=%s"),
		*Mode, bPass ? TEXT("true") : TEXT("false"), Settings->GetFieldOfView(), *OutputPath);
	FTimerHandle ExitTimer;
	GetWorldTimerManager().SetTimer(ExitTimer, []() { FPlatformMisc::RequestExit(false); }, 0.4f, false);
}

void AWhiteoutGameMode::BeginJumpCapture()
{
	AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!Character || !PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation G4 JumpAudit: player unavailable"));
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
	{
		HUD->DismissOpening();
		HUD->SetInterfaceVisibleForCapture(false);
	}
	Character->SetActorLocation(FVector(600.0f, 400.0f, 96.0f), false, nullptr, ETeleportType::TeleportPhysics);
	PlayerController->SetControlRotation(FRotator(-7.0f, 0.0f, 0.0f));
	Character->GetCharacterMovement()->StopMovementImmediately();
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	JumpCaptureIndex = 0;
	JumpStartZ = Character->GetActorLocation().Z;
	JumpMaxZ = JumpStartZ;
	bJumpOverlapDetected = false;
	JumpHeightSamples.Reset();
	Character->Jump();
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation G4 JumpAudit: started z=%.2f jump_z=%.2f gravity=%.2f"),
		JumpStartZ, Character->GetCharacterMovement()->JumpZVelocity, Character->GetCharacterMovement()->GravityScale);
	FTimerHandle FirstFrameTimer;
	GetWorldTimerManager().SetTimer(FirstFrameTimer, this, &AWhiteoutGameMode::CaptureJumpFrame, 0.05f, false);
}

void AWhiteoutGameMode::CaptureJumpFrame()
{
	AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Character)
	{
		FPlatformMisc::RequestExit(false);
		return;
	}
	const float CurrentZ = Character->GetActorLocation().Z;
	JumpMaxZ = FMath::Max(JumpMaxZ, CurrentZ);
	JumpHeightSamples.Add(CurrentZ - JumpStartZ);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WhiteoutJumpAudit), false, Character);
	const float CapsuleRadius = FMath::Max(1.0f, Character->GetCapsuleComponent()->GetScaledCapsuleRadius() - 1.0f);
	const float CapsuleHalfHeight = FMath::Max(CapsuleRadius, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 1.0f);
	bJumpOverlapDetected |= GetWorld()->OverlapBlockingTestByChannel(
		Character->GetActorLocation(),
		Character->GetActorQuat(),
		ECC_Pawn,
		FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
		QueryParams);
	const FString SequenceDirectory = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / TEXT("../docs/evidence_v0.3/jump_sequence"));
	IFileManager::Get().MakeDirectory(*SequenceDirectory, true);
	const FString ScreenshotPath = SequenceDirectory / FString::Printf(TEXT("Jump_%03d.png"), JumpCaptureIndex);
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false, false, FIntRect(), true);
	const bool bLanded = JumpCaptureIndex >= 6 && Character->GetCharacterMovement()->IsMovingOnGround();
	const bool bTimedOut = JumpCaptureIndex >= 30;
	++JumpCaptureIndex;
	if (!bLanded && !bTimedOut)
	{
		FTimerHandle NextFrameTimer;
		GetWorldTimerManager().SetTimer(NextFrameTimer, this, &AWhiteoutGameMode::CaptureJumpFrame, 0.05f, false);
		return;
	}
	const float JumpHeight = JumpMaxZ - JumpStartZ;
	const float AirTime = JumpCaptureIndex * 0.05f;
	const bool bPass = bLanded && !bJumpOverlapDetected && JumpHeight >= 40.0f && JumpHeight <= 70.0f && AirTime <= 0.85f;
	const FString EvidenceDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../docs/evidence_v0.3"));
	IFileManager::Get().MakeDirectory(*EvidenceDirectory, true);
	const FString OutputPath = EvidenceDirectory / TEXT("G4_JumpAudit.json");
	const FString Json = FString::Printf(
		TEXT("{\n  \"schema\": \"whiteout.g4.jump.v1\",\n  \"passed\": %s,\n  \"landed\": %s,\n  \"blocking_overlap_detected\": %s,\n  \"jump_z_velocity\": %.2f,\n  \"gravity_scale\": %.2f,\n  \"start_z_cm\": %.2f,\n  \"max_z_cm\": %.2f,\n  \"height_cm\": %.2f,\n  \"air_time_seconds\": %.2f,\n  \"captured_frames\": %d,\n  \"ceiling_height_cm\": 365.0,\n  \"capsule_half_height_cm\": %.2f\n}\n"),
		bPass ? TEXT("true") : TEXT("false"), bLanded ? TEXT("true") : TEXT("false"),
		bJumpOverlapDetected ? TEXT("true") : TEXT("false"),
		Character->GetCharacterMovement()->JumpZVelocity,
		Character->GetCharacterMovement()->GravityScale,
		JumpStartZ, JumpMaxZ, JumpHeight, AirTime, JumpCaptureIndex,
		Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FFileHelper::SaveStringToFile(Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation G4 JumpAudit: pass=%s height=%.2fcm airtime=%.2fs frames=%d overlap=%s"),
		bPass ? TEXT("true") : TEXT("false"), JumpHeight, AirTime, JumpCaptureIndex,
		bJumpOverlapDetected ? TEXT("true") : TEXT("false"));
	FTimerHandle ExitTimer;
	GetWorldTimerManager().SetTimer(ExitTimer, []() { FPlatformMisc::RequestExit(false); }, 0.6f, false);
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
	EWSEndingType ExpectedEnding = EWSEndingType::TaskSuccess;
	bool bExpectSignal = true;
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
		bSucceeded &= Commit(TEXT("talk_gu_heng"), [](FWSActionRequest& Request)
		{
			Request.DialogueAct = EWSDialogueAct::Challenge;
		});
		bSucceeded &= Commit(TEXT("dismantle_kitchen_heater"));
		bSucceeded &= Commit(TEXT("heat_repair_room"));
		bSucceeded &= Commit(TEXT("repair_generator"));
		bSucceeded &= Commit(TEXT("calibrate_antenna"));
		bSucceeded &= Commit(TEXT("send_signal"));
	}
	else if (RouteName.Equals(TEXT("quick"), ESearchCase::IgnoreCase))
	{
		ExpectedEnding = EWSEndingType::CostUncontrolled;
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
	else if (RouteName.Equals(TEXT("wait"), ESearchCase::IgnoreCase))
	{
		ExpectedEnding = EWSEndingType::SurvivalWait;
		bExpectSignal = false;
	}
	else if (RouteName.Equals(TEXT("cost"), ESearchCase::IgnoreCase))
	{
		ExpectedEnding = EWSEndingType::CostUncontrolled;
		bExpectSignal = false;
		bSucceeded &= Commit(TEXT("investigate_generator_log"));
		bSucceeded &= Commit(TEXT("forced_self_repair"));
		for (int32 Index = 0; Index < 2; ++Index)
		{
			bSucceeded &= Commit(TEXT("talk_gu_heng"), [](FWSActionRequest& Request)
			{
				Request.DialogueAct = EWSDialogueAct::Challenge;
			});
		}
	}
	else if (RouteName.Equals(TEXT("collapse"), ESearchCase::IgnoreCase))
	{
		ExpectedEnding = EWSEndingType::TotalCollapse;
		bExpectSignal = false;
		bSucceeded &= Commit(TEXT("investigate_generator_log"));
		for (int32 Index = 0; Index < 2; ++Index)
		{
			bSucceeded &= Commit(TEXT("talk_gu_heng"), [](FWSActionRequest& Request)
			{
				Request.DialogueAct = EWSDialogueAct::Challenge;
			});
		}
		bSucceeded &= Commit(TEXT("heat_medical_room"));
		bSucceeded &= Commit(TEXT("heat_repair_room"));
		bSucceeded &= Commit(TEXT("talk_ye_cheng"));
		bSucceeded &= Commit(TEXT("distribute_food"), [](FWSActionRequest& Request)
		{
			Request.FoodForPlayer = 1;
		});
		bSucceeded &= Commit(TEXT("inspect_control_cabinet"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation AutoRoute: unknown route '%s'"), *RouteName);
		return;
	}

	const FWSGameState Results = StateSubsystem->EndGame();
	FString EventLogPath;
	StateSubsystem->ExportEventLog(EventLogPath);
	const bool bOutcomeMatches = bSucceeded
		&& Results.Ending == ExpectedEnding
		&& Results.Tasks.bSignalSent == bExpectSignal;
	const FString Summary = FString::Printf(
		TEXT("route=%s success=%d ending=%s expected=%s signal=%d score=%.2f log=%s"),
		*RouteName,
		bOutcomeMatches,
		*StaticEnum<EWSEndingType>()->GetNameStringByValue(static_cast<int64>(Results.Ending)),
		*StaticEnum<EWSEndingType>()->GetNameStringByValue(static_cast<int64>(ExpectedEnding)),
		Results.Tasks.bSignalSent ? 1 : 0,
		Results.Score.Total,
		*EventLogPath);
	if (bOutcomeMatches)
	{
		UE_LOG(LogTemp, Display, TEXT("WhiteoutStation AutoRoute: %s"), *Summary);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation AutoRoute: %s"), *Summary);
	}
}
