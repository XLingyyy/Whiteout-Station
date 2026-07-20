#include "Flow/WhiteoutGameMode.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "HUD/WhiteoutHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Player/WhiteoutCharacter.h"
#include "Player/WhiteoutPlayerController.h"
#include "Sound/SoundBase.h"
#include "State/WindStationStateSubsystem.h"
#include "World/WhiteoutStationBuilder.h"
#include "TimerManager.h"
#include "UnrealClient.h"

AWhiteoutGameMode::AWhiteoutGameMode()
{
	DefaultPawnClass = AWhiteoutCharacter::StaticClass();
	PlayerControllerClass = AWhiteoutPlayerController::StaticClass();
	HUDClass = AWhiteoutHUD::StaticClass();
}

void AWhiteoutGameMode::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation: starting playable v0.1 flow"));
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
	if (USoundBase* WindSound = LoadObject<USoundBase>(
		nullptr,
		TEXT("/Game/WindStation/Audio/Ambience/S_WindStrong_CC0.S_WindStrong_CC0")))
	{
		WindAmbience = UGameplayStatics::SpawnSound2D(this, WindSound, 0.16f, 0.94f, 0.0f, nullptr, false, false);
	}

	FString AutoRoute;
	if (FParse::Value(FCommandLine::Get(), TEXT("WhiteoutAutoRoute="), AutoRoute))
	{
		RunAutomationRoute(AutoRoute);
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
