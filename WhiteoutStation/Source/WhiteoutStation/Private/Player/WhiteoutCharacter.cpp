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

	MoveForwardAction->ValueType = EInputActionValueType::Boolean;
	MoveBackwardAction->ValueType = EInputActionValueType::Boolean;
	MoveLeftAction->ValueType = EInputActionValueType::Boolean;
	MoveRightAction->ValueType = EInputActionValueType::Boolean;
	LookAction->ValueType = EInputActionValueType::Axis2D;
	InteractAction->ValueType = EInputActionValueType::Boolean;
	EvidenceAction->ValueType = EInputActionValueType::Boolean;
	RestartAction->ValueType = EInputActionValueType::Boolean;
	SettleAction->ValueType = EInputActionValueType::Boolean;

	RuntimeInputContext->MapKey(MoveForwardAction, EKeys::W);
	RuntimeInputContext->MapKey(MoveBackwardAction, EKeys::S);
	RuntimeInputContext->MapKey(MoveLeftAction, EKeys::A);
	RuntimeInputContext->MapKey(MoveRightAction, EKeys::D);
	RuntimeInputContext->MapKey(LookAction, EKeys::Mouse2D);
	RuntimeInputContext->MapKey(InteractAction, EKeys::F);
	RuntimeInputContext->MapKey(EvidenceAction, EKeys::E);
	RuntimeInputContext->MapKey(RestartAction, EKeys::R);
	RuntimeInputContext->MapKey(SettleAction, EKeys::Enter);
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
			if (const AWSInteractableActor* Interactable = FindLookedAtInteractable())
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
	}
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
		Interactable->Interact(this);
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
				HUD->SetSystemMessage(TEXT("Settlement locked: send the signal, spend all AP, or accept a failed ending."));
			}
		}
		return;
	}
	const FWSGameState Results = StateSubsystem->EndGame();
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->SetSystemMessage(FString::Printf(
				TEXT("RESULT: %s  //  SCORE %.1f (%s)  //  Press R to restart"),
				*StaticEnum<EWSEndingType>()->GetNameStringByValue(static_cast<int64>(Results.Ending)),
				Results.Score.Total,
				*Results.Score.Rating));
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
