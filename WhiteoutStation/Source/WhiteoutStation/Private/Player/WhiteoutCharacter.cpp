#include "Player/WhiteoutCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HUD/WhiteoutHUD.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "State/WindStationStateSubsystem.h"
#include "World/WSInteractableActor.h"

AWhiteoutCharacter::AWhiteoutCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 92.0f);
	GetCharacterMovement()->MaxWalkSpeed = 430.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1600.0f;
	bUseControllerRotationYaw = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0, 0, 64));
	FirstPersonCamera->bUsePawnControlRotation = true;

	RuntimeInputContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("RuntimeInputContext"));
	MoveForwardAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveForwardAction"));
	MoveBackwardAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveBackwardAction"));
	MoveLeftAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveLeftAction"));
	MoveRightAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveRightAction"));
	LookAction = CreateDefaultSubobject<UInputAction>(TEXT("LookAction"));
	InteractAction = CreateDefaultSubobject<UInputAction>(TEXT("InteractAction"));
	EvidenceAction = CreateDefaultSubobject<UInputAction>(TEXT("EvidenceAction"));
	RestartAction = CreateDefaultSubobject<UInputAction>(TEXT("RestartAction"));
	SettleAction = CreateDefaultSubobject<UInputAction>(TEXT("SettleAction"));
	DialogueModeAction = CreateDefaultSubobject<UInputAction>(TEXT("DialogueModeAction"));
	ContinueAction = CreateDefaultSubobject<UInputAction>(TEXT("ContinueAction"));

	MoveForwardAction->ValueType = EInputActionValueType::Boolean;
	MoveBackwardAction->ValueType = EInputActionValueType::Boolean;
	MoveLeftAction->ValueType = EInputActionValueType::Boolean;
	MoveRightAction->ValueType = EInputActionValueType::Boolean;
	LookAction->ValueType = EInputActionValueType::Axis2D;
	InteractAction->ValueType = EInputActionValueType::Boolean;
	EvidenceAction->ValueType = EInputActionValueType::Boolean;
	RestartAction->ValueType = EInputActionValueType::Boolean;
	SettleAction->ValueType = EInputActionValueType::Boolean;
	DialogueModeAction->ValueType = EInputActionValueType::Boolean;
	ContinueAction->ValueType = EInputActionValueType::Boolean;

	RuntimeInputContext->MapKey(MoveForwardAction, EKeys::W);
	RuntimeInputContext->MapKey(MoveBackwardAction, EKeys::S);
	RuntimeInputContext->MapKey(MoveLeftAction, EKeys::A);
	RuntimeInputContext->MapKey(MoveRightAction, EKeys::D);
	RuntimeInputContext->MapKey(LookAction, EKeys::Mouse2D);
	RuntimeInputContext->MapKey(InteractAction, EKeys::F);
	RuntimeInputContext->MapKey(EvidenceAction, EKeys::E);
	RuntimeInputContext->MapKey(RestartAction, EKeys::R);
	RuntimeInputContext->MapKey(SettleAction, EKeys::Enter);
	RuntimeInputContext->MapKey(DialogueModeAction, EKeys::Q);
	RuntimeInputContext->MapKey(ContinueAction, EKeys::C);
}

void AWhiteoutCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (const APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				InputSubsystem->AddMappingContext(RuntimeInputContext, 0);
			}
		}
	}
}

void AWhiteoutCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			AWSInteractableActor* Interactable = FindLookedAtInteractable();
			if (PreviewedInteractable && PreviewedInteractable != Interactable)
			{
				PreviewedInteractable = nullptr;
				bPreviewCanExecute = false;
				HUD->HideActionPreview();
			}
			if (Interactable)
			{
				HUD->SetInteractionPrompt(Interactable->GetInteractionPrompt());
			}
			else
			{
				HUD->SetInteractionPrompt(FText::GetEmpty());
			}
		}
	}
}

void AWhiteoutCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &AWhiteoutCharacter::MoveForward);
		EnhancedInput->BindAction(MoveBackwardAction, ETriggerEvent::Triggered, this, &AWhiteoutCharacter::MoveBackward);
		EnhancedInput->BindAction(MoveLeftAction, ETriggerEvent::Triggered, this, &AWhiteoutCharacter::MoveLeft);
		EnhancedInput->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &AWhiteoutCharacter::MoveRight);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWhiteoutCharacter::Look);
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AWhiteoutCharacter::Interact);
		EnhancedInput->BindAction(EvidenceAction, ETriggerEvent::Started, this, &AWhiteoutCharacter::ToggleEvidence);
		EnhancedInput->BindAction(RestartAction, ETriggerEvent::Started, this, &AWhiteoutCharacter::RestartRun);
		EnhancedInput->BindAction(SettleAction, ETriggerEvent::Started, this, &AWhiteoutCharacter::Settle);
		EnhancedInput->BindAction(DialogueModeAction, ETriggerEvent::Started, this, &AWhiteoutCharacter::CycleDialogueMode);
		EnhancedInput->BindAction(ContinueAction, ETriggerEvent::Started, this, &AWhiteoutCharacter::ContinueRun);
	}
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AWhiteoutCharacter::SelectDialogue1);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AWhiteoutCharacter::SelectDialogue2);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AWhiteoutCharacter::SelectDialogue3);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AWhiteoutCharacter::SelectDialogue4);
	PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AWhiteoutCharacter::SelectDialogue5);
	PlayerInputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AWhiteoutCharacter::SelectDialogue6);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AWhiteoutCharacter::DismissOpening);
	FInputKeyBinding& PauseBinding = PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AWhiteoutCharacter::TogglePauseMenu);
	PauseBinding.bExecuteWhenPaused = true;
}

void AWhiteoutCharacter::MoveForward(const FInputActionValue& Value)
{
	if (Value.Get<bool>()) AddMovementInput(GetActorForwardVector(), 1.0f);
}

void AWhiteoutCharacter::MoveBackward(const FInputActionValue& Value)
{
	if (Value.Get<bool>()) AddMovementInput(GetActorForwardVector(), -1.0f);
}

void AWhiteoutCharacter::MoveLeft(const FInputActionValue& Value)
{
	if (Value.Get<bool>()) AddMovementInput(GetActorRightVector(), -1.0f);
}

void AWhiteoutCharacter::MoveRight(const FInputActionValue& Value)
{
	if (Value.Get<bool>()) AddMovementInput(GetActorRightVector(), 1.0f);
}

void AWhiteoutCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void AWhiteoutCharacter::Interact(const FInputActionValue& Value)
{
	if (AWSInteractableActor* Interactable = FindLookedAtInteractable())
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
			{
				if (PreviewedInteractable == Interactable)
				{
					if (bPreviewCanExecute)
					{
						Interactable->Interact(this, SelectedDialogueAct(), SelectedPromiseCondition());
					}
					else
					{
						HUD->HideActionPreview();
					}
					PreviewedInteractable = nullptr;
					bPreviewCanExecute = false;
					return;
				}

				const FWSActionPreview Preview = Interactable->PreviewInteraction(SelectedDialogueAct(), SelectedPromiseCondition());
				HUD->ShowActionPreview(Interactable->DisplayName, Preview);
				PreviewedInteractable = Interactable;
				bPreviewCanExecute = Preview.bCanExecute;
			}
		}
	}
}

void AWhiteoutCharacter::CycleDialogueMode(const FInputActionValue& Value)
{
	bDialogueMenuVisible = !bDialogueMenuVisible;
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->ShowDialogueMenu(DialogueModeIndex, bDialogueMenuVisible);
		}
	}
}

void AWhiteoutCharacter::SelectDialogue1() { SelectDialogueIndex(0); }
void AWhiteoutCharacter::SelectDialogue2() { SelectDialogueIndex(1); }
void AWhiteoutCharacter::SelectDialogue3() { SelectDialogueIndex(2); }
void AWhiteoutCharacter::SelectDialogue4() { SelectDialogueIndex(3); }
void AWhiteoutCharacter::SelectDialogue5() { SelectDialogueIndex(4); }
void AWhiteoutCharacter::SelectDialogue6() { SelectDialogueIndex(5); }

void AWhiteoutCharacter::SelectDialogueIndex(const int32 Index)
{
	DialogueModeIndex = FMath::Clamp(Index, 0, 5);
	bDialogueMenuVisible = true;
	PreviewedInteractable = nullptr;
	bPreviewCanExecute = false;
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->HideActionPreview();
			HUD->ShowDialogueMenu(DialogueModeIndex, true);
		}
	}
}

void AWhiteoutCharacter::DismissOpening()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->DismissOpening();
		}
	}
}

void AWhiteoutCharacter::TogglePauseMenu()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->TogglePauseMenu();
		}
	}
}

void AWhiteoutCharacter::ContinueRun(const FInputActionValue& Value)
{
	UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	if (!StateSubsystem)
	{
		return;
	}
	const bool bLoaded = StateSubsystem->LoadSnapshot();
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->SetSystemMessage(bLoaded ? TEXT("已恢复自动存档") : TEXT("没有找到自动存档"));
		}
	}
}

EWSDialogueAct AWhiteoutCharacter::SelectedDialogueAct() const
{
	if (DialogueModeIndex == 1) return EWSDialogueAct::Challenge;
	if (DialogueModeIndex >= 2 && DialogueModeIndex <= 4) return EWSDialogueAct::Promise;
	if (DialogueModeIndex == 5) return EWSDialogueAct::Reassure;
	return EWSDialogueAct::Ask;
}

FName AWhiteoutCharacter::SelectedPromiseCondition() const
{
	if (DialogueModeIndex == 2) return TEXT("heat_repair_room");
	if (DialogueModeIndex == 3) return TEXT("reserve_medicine");
	if (DialogueModeIndex == 4) return TEXT("keep_records");
	return NAME_None;
}

void AWhiteoutCharacter::ToggleEvidence(const FInputActionValue& Value)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->ToggleEvidence();
		}
	}
}

void AWhiteoutCharacter::RestartRun(const FInputActionValue& Value)
{
	if (UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		StateSubsystem->NewGame();
	}
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), false);
}

void AWhiteoutCharacter::Settle(const FInputActionValue& Value)
{
	UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	if (!StateSubsystem)
	{
		return;
	}
	const FWSGameState Before = StateSubsystem->GetStateSnapshot();
	if (!Before.Tasks.bSignalSent && Before.ActionPoints > 0 && Before.Phase != EWSGamePhase::Ending)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
			{
				HUD->SetSystemMessage(TEXT("现在还不能结算：先发出信号、用完行动力，或接受失败结局。"));
			}
		}
		return;
	}
	const FWSGameState Results = StateSubsystem->EndGame();
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->SetSystemMessage(FString::Printf(TEXT("本轮已结束：总分 %.1f，评级 %s。按 R 重新开始。"), Results.Score.Total, *Results.Score.Rating));
		}
	}
}

AWSInteractableActor* AWhiteoutCharacter::FindLookedAtInteractable() const
{
	if (!FirstPersonCamera || !GetWorld())
	{
		return nullptr;
	}
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector End = Start + FirstPersonCamera->GetForwardVector() * 425.0f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WhiteoutInteraction), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return Cast<AWSInteractableActor>(Hit.GetActor());
	}
	return nullptr;
}
