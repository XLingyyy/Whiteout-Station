#include "Flow/WhiteoutGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HUD/WhiteoutHUD.h"
#include "HUD/WhiteoutHUDWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Player/WhiteoutCharacter.h"
#include "State/WindStationStateSubsystem.h"
#include "Settings/WhiteoutSettingsSubsystem.h"
#include "World/WSInteractableActor.h"
#include "UnrealClient.h"

// Explicit capture fixture; uses normal rule actions in an isolated launch and never edits map assets.
void AWhiteoutGameMode::BeginV17Capture()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	auto* Character = PC ? Cast<AWhiteoutCharacter>(PC->GetPawn()) : nullptr;
	auto* HUD = PC ? Cast<AWhiteoutHUD>(PC->GetHUD()) : nullptr;
	auto* State = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	if (!Character || !HUD || !State) return;
	HUD->DismissOpening();
	HUD->HideActionPreview(); HUD->ShowDialogueMenu(NAME_None, false);
	EWSReasonCode Reason; TArray<FString> Changes;
	State->BeginDayPhase(EWSHeatingZone::MedicalRoom, Reason, Changes);
	FString Mode = TEXT("hud"); FParse::Value(FCommandLine::Get(), TEXT("V17Frame="), Mode);
	const bool bBatch = Mode == TEXT("all");
	if (bBatch) Mode = V17BatchMode.IsEmpty() ? TEXT("hud") : V17BatchMode;
	float TextScale = 1; FParse::Value(FCommandLine::Get(), TEXT("WhiteoutCaptureScale="), TextScale);
	GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>()->SetTextScale(TextScale, this);
	const FName TargetId = Mode.StartsWith(TEXT("dialogue")) || Mode == TEXT("hud") ? FName(TEXT("talk_gu_heng")) : FName(TEXT("inspect_control_cabinet"));
	AWSInteractableActor* Target = nullptr;
	for (TActorIterator<AWSInteractableActor> It(GetWorld()); It; ++It) if (It->ActionId == TargetId) { Target = *It; break; }
	if (!Target) { UE_LOG(LogTemp, Error, TEXT("V17 capture target missing")); return; }
	const FVector Center = Target->IsCharacterHotspot() ? Target->InteractionCollision->Bounds.Origin : Target->Mesh->Bounds.Origin;
	float Yaw = Target->IsCharacterHotspot() ? Target->GetActorRotation().Yaw + 90.f : 180.f;
	float Distance = 235.f, Height = Target->IsCharacterHotspot() ? 0.f : 35.f;
	FParse::Value(FCommandLine::Get(), TEXT("V17CameraYaw="), Yaw);
	FParse::Value(FCommandLine::Get(), TEXT("V17CameraDistance="), Distance);
	FParse::Value(FCommandLine::Get(), TEXT("V17CameraHeight="), Height);
	const FVector Eye = Center + FRotator(0, Yaw, 0).Vector() * Distance + FVector(0, 0, Height);
	UE_LOG(LogTemp, Display, TEXT("V17 frame=%s center=%s eye=%s"), *Mode, *Center.ToString(), *Eye.ToString());
	Character->SetActorLocation(Eye - FVector(0, 0, 64), false, nullptr, ETeleportType::TeleportPhysics);
	Character->GetCharacterMovement()->DisableMovement(); PC->SetViewTarget(Character);
	PC->SetControlRotation((Center - Eye).Rotation());
	FTimerHandle Ready;
	GetWorldTimerManager().SetTimer(Ready, FTimerDelegate::CreateWeakLambda(this, [this, HUD, State, Target, Mode, PC, bBatch]()
	{
		if (Mode == TEXT("probe"))
		{
			for (TObjectIterator<UWhiteoutHUDWidget> It; It; ++It)
				if (It->GetWorld() == GetWorld() && It->IsInViewport()) It->BeginV17RuntimeProbe();
			return;
		}
		if (Mode == TEXT("ap"))
		{
			FWSActionRequest Request; Request.ActionId = Target->ActionId;
			HUD->ShowActionPreview(Target->DisplayName, State->PreviewAction(Request), Request);
		}
		else if (Mode.StartsWith(TEXT("dialogue")))
		{
			if (Mode == TEXT("dialogue_online"))
			{
				FString Error;
				GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>()->SetLLMConfiguration(TEXT("loopback"), TEXT("http://127.0.0.1:8765/v1"), TEXT(""), TEXT("capture-no-send"), true, Error);
			}
			HUD->ShowDialogueMenu(Target->ActionId, true);
			if (Mode == TEXT("dialogue_online")) HUD->ShowDialogueFreeTextForCapture();
		}
		FTimerHandle Capture;
		GetWorldTimerManager().SetTimer(Capture, FTimerDelegate::CreateWeakLambda(this, [this, Mode, PC, bBatch]()
		{
			const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Artifacts/v1.7-evidence/frames"));
			IFileManager::Get().MakeDirectory(*Dir, true);
			for (TObjectIterator<UWhiteoutHUDWidget> It; It; ++It)
				if (It->GetWorld() == GetWorld() && It->IsInViewport()) It->ExportV17Geometry(Dir / (Mode + TEXT(".geometry.json")));
			FScreenshotRequest::RequestScreenshot(Dir / (Mode + TEXT(".png")), true, false, false, FIntRect(), true);
			UE_LOG(LogTemp, Display, TEXT("V17 captured %s; phaseAP=%d; no dialogue request sent"), *Mode, GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>()->GetStateSnapshot().PhaseActionPoints);
			if (bBatch && Mode != TEXT("dialogue_online"))
			{
				V17BatchMode = Mode == TEXT("hud") ? TEXT("interaction") : Mode == TEXT("interaction") ? TEXT("ap") : Mode == TEXT("ap") ? TEXT("dialogue") : TEXT("dialogue_online");
				FTimerHandle Next;
				GetWorldTimerManager().SetTimer(Next, this, &AWhiteoutGameMode::BeginV17Capture, 1.f, false);
				return;
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("WhiteoutAutoExit")))
			{
				FTimerHandle Exit; GetWorldTimerManager().SetTimer(Exit, []() { FPlatformMisc::RequestExit(false); }, 2.f, false);
			}
		}), 2.f, false);
	}), 1.f, false);
}
