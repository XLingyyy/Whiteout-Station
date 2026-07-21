#include "Player/WhiteoutCharacter.h"

#include "Agents/WSAgentGateway.h"
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
#include "Sound/SoundBase.h"
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
	SnowFootstepSound = LoadObject<USoundBase>(
		nullptr,
		TEXT("/Game/WindStation/Audio/Foley/S_FootstepSnow_Original.S_FootstepSnow_Original"));
	MetalFootstepSound = LoadObject<USoundBase>(
		nullptr,
		TEXT("/Game/WindStation/Audio/Foley/S_FootstepMetal_Original.S_FootstepMetal_Original"));
	ConcreteFootstepSound = LoadObject<USoundBase>(
		nullptr,
		TEXT("/Game/WindStation/Audio/Foley/S_FootstepConcrete_Original.S_FootstepConcrete_Original"));
	LastFootstepLocation = GetActorLocation();
	DialogueIntentGateway = NewObject<UWSAgentGateway>(this);
	DialogueIntentGateway->Initialize();
}

void AWhiteoutCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ActiveDialogueTarget)
	{
		if (FocusedInteractable != ActiveDialogueTarget)
		{
			if (FocusedInteractable) FocusedInteractable->SetInteractionFocused(false);
			FocusedInteractable = ActiveDialogueTarget;
			FocusedInteractable->SetInteractionFocused(true);
		}
		return;
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			AWSInteractableActor* Interactable = FindLookedAtInteractable();
			if (FocusedInteractable != Interactable)
			{
				if (FocusedInteractable)
				{
					FocusedInteractable->SetInteractionFocused(false);
				}
				FocusedInteractable = Interactable;
				if (FocusedInteractable)
				{
					FocusedInteractable->SetInteractionFocused(true);
				}
			}
			if (PreviewedInteractable && PreviewedInteractable != Interactable)
			{
				PreviewedInteractable = nullptr;
				bPreviewCanExecute = false;
				HUD->HideActionPreview();
			}
			if (Interactable)
			{
				HUD->SetInteractionFocus(
					Interactable->DisplayName,
					Interactable->PreviewInteraction(),
					Interactable->IsCharacterHotspot());
			}
			else
			{
				HUD->ClearInteractionFocus();
			}
		}
	}
	UpdateFootsteps();
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
		EnhancedInput->BindAction(ContinueAction, ETriggerEvent::Started, this, &AWhiteoutCharacter::ContinueRun);
	}
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
		if (Interactable->IsCharacterHotspot())
		{
			BeginDialogue(Interactable);
			return;
		}
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
			{
				if (PreviewedInteractable == Interactable)
				{
					if (bPreviewCanExecute)
					{
						Interactable->Interact(this);
					}
					else
					{
						HUD->HideActionPreview();
					}
					PreviewedInteractable = nullptr;
					bPreviewCanExecute = false;
					return;
				}

				const FWSActionPreview Preview = Interactable->PreviewInteraction();
				HUD->ShowActionPreview(Interactable->DisplayName, Preview);
				PreviewedInteractable = Interactable;
				bPreviewCanExecute = Preview.bCanExecute;
			}
		}
	}
}

void AWhiteoutCharacter::BeginDialogue(AWSInteractableActor* Interactable)
{
	if (!Interactable || !Interactable->IsCharacterHotspot() || ActiveDialogueTarget)
	{
		return;
	}
	ActiveDialogueTarget = Interactable;
	bDialogueChoiceCommitted = false;
	bDialogueIntentPending = false;
	PreviewedInteractable = nullptr;
	bPreviewCanExecute = false;
	Interactable->SetDialogueLookAtActive(true);
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		const FVector CameraLocation = FirstPersonCamera->GetComponentLocation();
		const FVector TargetLocation = Interactable->GetActorLocation() + FVector(0.0f, 0.0f, 145.0f);
		PlayerController->SetControlRotation((TargetLocation - CameraLocation).Rotation());
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->HideActionPreview();
			HUD->ShowDialogueMenu(Interactable->ActionId, true);
		}
	}
}

void AWhiteoutCharacter::ChooseDialogueAct(const EWSDialogueAct DialogueAct)
{
	if (DialogueAct == EWSDialogueAct::Promise)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
			{
				HUD->ShowDialoguePromiseChoices();
			}
		}
		return;
	}
	CommitDialogueChoice(DialogueAct, NAME_None);
}

void AWhiteoutCharacter::ChooseDialoguePromise(const FName PromiseCondition)
{
	CommitDialogueChoice(EWSDialogueAct::Promise, PromiseCondition);
}

void AWhiteoutCharacter::CommitDialogueChoice(const EWSDialogueAct DialogueAct, const FName PromiseCondition)
{
	if (!ActiveDialogueTarget || bDialogueChoiceCommitted || bDialogueIntentPending)
	{
		return;
	}
	const bool bAllowedAct = DialogueAct == EWSDialogueAct::Ask || DialogueAct == EWSDialogueAct::Challenge
		|| DialogueAct == EWSDialogueAct::Promise || DialogueAct == EWSDialogueAct::Reassure;
	const bool bAllowedPromise = DialogueAct != EWSDialogueAct::Promise
		|| PromiseCondition == TEXT("keep_records") || PromiseCondition == TEXT("reserve_medicine") || PromiseCondition == TEXT("heat_repair_room");
	if (!bAllowedAct || !bAllowedPromise)
	{
		return;
	}
	bDialogueChoiceCommitted = true;
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->SetDialogueIntentStatus(TEXT("交涉已提交，正在组织回应……"), true);
		}
	}
	ActiveDialogueTarget->Interact(this, DialogueAct, PromiseCondition);
}

void AWhiteoutCharacter::SubmitDialogueText(const FString& UserText)
{
	if (!ActiveDialogueTarget || bDialogueChoiceCommitted || bDialogueIntentPending || !DialogueIntentGateway)
	{
		return;
	}
	bDialogueIntentPending = true;
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->SetDialogueIntentStatus(TEXT("正在识别你的交涉意图……"), true);
		}
	}
	TWeakObjectPtr<AWhiteoutCharacter> WeakThis(this);
	DialogueIntentGateway->RequestDialogueIntent(
		UserText,
		true,
		FWSDialogueIntentCallback::CreateLambda(
			[WeakThis](const FWSDialogueIntentResult& Intent)
			{
				if (!WeakThis.IsValid() || !WeakThis->ActiveDialogueTarget)
				{
					return;
				}
				WeakThis->bDialogueIntentPending = false;
				APlayerController* PlayerController = Cast<APlayerController>(WeakThis->Controller);
				AWhiteoutHUD* HUD = PlayerController ? Cast<AWhiteoutHUD>(PlayerController->GetHUD()) : nullptr;
				if (!Intent.bMapped)
				{
					if (HUD)
					{
						HUD->SetDialogueIntentStatus(TEXT("无法可靠识别：已回退到安全轮盘，请直接选择一种交涉方式。"), false);
						HUD->ShowDialogueWheelChoices();
					}
					return;
				}
				if (HUD)
				{
					const FString SourceLabel = Intent.Source == TEXT("online_model") ? TEXT("在线模型") : TEXT("本地意图词典");
					HUD->SetDialogueIntentStatus(FString::Printf(TEXT("%s 已映射到规则内交涉方式（置信度 %.0f%%）。"), *SourceLabel, Intent.Confidence * 100.0f), true);
				}
				WeakThis->CommitDialogueChoice(Intent.DialogueAct, Intent.PromiseCondition);
			}));
}

void AWhiteoutCharacter::CancelDialogue()
{
	if (ActiveDialogueTarget)
	{
		ActiveDialogueTarget->SetDialogueLookAtActive(false);
	}
	ActiveDialogueTarget = nullptr;
	bDialogueChoiceCommitted = false;
	bDialogueIntentPending = false;
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		PlayerController->ResetIgnoreMoveInput();
		PlayerController->ResetIgnoreLookInput();
		PlayerController->SetShowMouseCursor(false);
		PlayerController->SetInputMode(FInputModeGameOnly());
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->ShowDialogueMenu(NAME_None, false);
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

void AWhiteoutCharacter::UpdateFootsteps()
{
	const FVector CurrentLocation = GetActorLocation();
	const float TravelThisFrame = FVector::Dist2D(CurrentLocation, LastFootstepLocation);
	LastFootstepLocation = CurrentLocation;
	if (!GetCharacterMovement()->IsMovingOnGround() || GetVelocity().SizeSquared2D() < FMath::Square(65.0f) || TravelThisFrame > 80.0f)
	{
		if (GetVelocity().SizeSquared2D() < FMath::Square(25.0f))
		{
			FootstepTravel = 0.0f;
		}
		return;
	}
	FootstepTravel += TravelThisFrame;
	if (FootstepTravel < 150.0f)
	{
		return;
	}
	FootstepTravel = 0.0f;
	USoundBase* Footstep = ConcreteFootstepSound;
	if (CurrentLocation.X > 1700.0f)
	{
		Footstep = SnowFootstepSound;
	}
	else if (CurrentLocation.X > 650.0f && CurrentLocation.Y < 520.0f)
	{
		Footstep = MetalFootstepSound;
	}
	if (Footstep)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			Footstep,
			CurrentLocation,
			0.32f,
			FMath::FRandRange(0.93f, 1.07f));
	}
}
