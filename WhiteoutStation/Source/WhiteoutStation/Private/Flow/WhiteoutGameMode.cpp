#include "Flow/WhiteoutGameMode.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "HUD/WhiteoutHUD.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Player/WhiteoutCharacter.h"
#include "Player/WhiteoutPlayerController.h"
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
		StateSubsystem->NewGame();
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
