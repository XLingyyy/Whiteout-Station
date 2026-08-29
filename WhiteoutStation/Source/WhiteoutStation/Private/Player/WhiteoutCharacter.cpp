#include "Player/WhiteoutCharacter.h"

#include "Agents/WSAgentGateway.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HUD/WhiteoutHUD.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Presentation/WSPresentationText.h"
#include "State/WindStationStateSubsystem.h"
#include "Sound/SoundBase.h"
#include "Settings/WhiteoutSettingsSubsystem.h"
#include "World/WSInteractableActor.h"

AWhiteoutCharacter::AWhiteoutCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 92.0f);
	GetCharacterMovement()->MaxWalkSpeed = 430.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1600.0f;
	GetCharacterMovement()->JumpZVelocity = 350.0f;
	GetCharacterMovement()->GravityScale = 1.15f;
	GetCharacterMovement()->AirControl = 0.10f;
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
	CycleOptionAction = CreateDefaultSubobject<UInputAction>(TEXT("CycleOptionAction"));

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
	CycleOptionAction->ValueType = EInputActionValueType::Boolean;

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
	RuntimeInputContext->MapKey(CycleOptionAction, EKeys::Q);
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
	if (UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		Settings->Apply(this);
	}
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
				PreviewActionRequest = FWSActionRequest();
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
		EnhancedInput->BindAction(CycleOptionAction, ETriggerEvent::Started, this, &AWhiteoutCharacter::CycleActionOption);
	}
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AWhiteoutCharacter::HandleJumpPressed);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AWhiteoutCharacter::AdvanceOpening);
	PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &AWhiteoutCharacter::ToggleGuide);
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
						Interactable->InteractRequest(this, PreviewActionRequest);
					}
					else
					{
						HUD->HideActionPreview();
					}
					PreviewedInteractable = nullptr;
					bPreviewCanExecute = false;
					PreviewActionRequest = FWSActionRequest();
					return;
				}

				PreviewActionRequest = Interactable->BuildActionRequest();
				const FWSActionPreview Preview = Interactable->PreviewRequest(PreviewActionRequest);
				HUD->ShowActionPreview(Interactable->DisplayName, Preview, PreviewActionRequest);
				PreviewedInteractable = Interactable;
				bPreviewCanExecute = Preview.bCanExecute;
			}
		}
	}
}

void AWhiteoutCharacter::CycleActionOption(const FInputActionValue& Value)
{
	if (!PreviewedInteractable || ActiveDialogueTarget)
	{
		return;
	}
	if (PreviewActionRequest.ActionId == TEXT("distribute_food"))
	{
		static const int32 FoodOptions[][3] = {
			{1, 0, 0},
			{0, 1, 0},
			{0, 0, 1},
			{1, 1, 0},
			{1, 0, 1},
			{0, 1, 1}};
		int32 CurrentIndex = 0;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(FoodOptions); ++Index)
		{
			if (PreviewActionRequest.FoodForPlayer == FoodOptions[Index][0]
				&& PreviewActionRequest.FoodForGuHeng == FoodOptions[Index][1]
				&& PreviewActionRequest.FoodForYeCheng == FoodOptions[Index][2])
			{
				CurrentIndex = Index;
				break;
			}
		}
		const int32 NextIndex = (CurrentIndex + 1) % UE_ARRAY_COUNT(FoodOptions);
		PreviewActionRequest.FoodForPlayer = FoodOptions[NextIndex][0];
		PreviewActionRequest.FoodForGuHeng = FoodOptions[NextIndex][1];
		PreviewActionRequest.FoodForYeCheng = FoodOptions[NextIndex][2];
		if (NextIndex == 0)
		{
			PreviewActionRequest.bHotMeal =
				!PreviewActionRequest.bHotMeal;
		}
	}
	else if (
		PreviewActionRequest.ActionId == TEXT("treat_gu_heng")
		|| PreviewActionRequest.ActionId == TEXT("treat_character"))
	{
		static const EWSCharacterId Targets[] = {
			EWSCharacterId::Player,
			EWSCharacterId::GuHeng,
			EWSCharacterId::YeCheng};
		static const EWSTreatmentMethod Methods[] = {
			EWSTreatmentMethod::Bandage,
			EWSTreatmentMethod::Full,
			EWSTreatmentMethod::HeatPack};
		int32 TargetIndex = 0;
		int32 MethodIndex = 0;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Targets); ++Index)
		{
			if (PreviewActionRequest.TreatmentTarget == Targets[Index])
			{
				TargetIndex = Index;
				break;
			}
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Methods); ++Index)
		{
			if (PreviewActionRequest.TreatmentMethod == Methods[Index])
			{
				MethodIndex = Index;
				break;
			}
		}
		MethodIndex =
			(MethodIndex + 1) % UE_ARRAY_COUNT(Methods);
		if (MethodIndex == 0)
		{
			TargetIndex =
				(TargetIndex + 1) % UE_ARRAY_COUNT(Targets);
		}
		PreviewActionRequest.TreatmentTarget = Targets[TargetIndex];
		PreviewActionRequest.TreatmentMethod = Methods[MethodIndex];
		PreviewActionRequest.TreatmentResource =
			PreviewActionRequest.TreatmentMethod
				== EWSTreatmentMethod::HeatPack
			? EWSResourceType::HeatPack
			: EWSResourceType::Medicine;
	}
	else if (PreviewActionRequest.ActionId == TEXT("rest"))
	{
		static const EWSCharacterId Targets[] = {
			EWSCharacterId::Player,
			EWSCharacterId::GuHeng,
			EWSCharacterId::YeCheng};
		static const EWSCharacterLocation Locations[] = {
			EWSCharacterLocation::ControlRoom,
			EWSCharacterLocation::RepairRoom,
			EWSCharacterLocation::MedicalRoom,
			EWSCharacterLocation::Kitchen};
		int32 TargetIndex = 0;
		int32 LocationIndex = 0;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Targets); ++Index)
		{
			if (PreviewActionRequest.RestTarget == Targets[Index])
			{
				TargetIndex = Index;
				break;
			}
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Locations); ++Index)
		{
			if (PreviewActionRequest.RestLocation == Locations[Index])
			{
				LocationIndex = Index;
				break;
			}
		}
		LocationIndex =
			(LocationIndex + 1) % UE_ARRAY_COUNT(Locations);
		if (LocationIndex == 0)
		{
			TargetIndex =
				(TargetIndex + 1) % UE_ARRAY_COUNT(Targets);
		}
		PreviewActionRequest.RestTarget = Targets[TargetIndex];
		PreviewActionRequest.RestLocation = Locations[LocationIndex];
	}
	else if (
		PreviewActionRequest.ActionId
			== TEXT("inspect_control_cabinet"))
	{
		PreviewActionRequest.bHasCollaborator =
			!PreviewActionRequest.bHasCollaborator;
		PreviewActionRequest.Collaborator =
			EWSCharacterId::GuHeng;
	}
	else if (
		PreviewActionRequest.ActionId
			== TEXT("dismantle_kitchen_heater"))
	{
		PreviewActionRequest.bHasCollaborator =
			!PreviewActionRequest.bHasCollaborator;
		PreviewActionRequest.Collaborator =
			EWSCharacterId::Player;
	}
	else if (
		PreviewActionRequest.ActionId == TEXT("repair_generator"))
	{
		int32 Mode = 0;
		if (PreviewActionRequest.bForce)
		{
			Mode = 3;
		}
		else if (PreviewActionRequest.bUseRelay)
		{
			Mode = 2;
		}
		else if (PreviewActionRequest.bHasCollaborator)
		{
			Mode = 1;
		}
		Mode = (Mode + 1) % 4;
		PreviewActionRequest.bHasCollaborator = Mode == 1;
		PreviewActionRequest.Collaborator =
			EWSCharacterId::Player;
		PreviewActionRequest.bUseRelay = Mode == 2;
		PreviewActionRequest.bForce = Mode == 3;
	}
	else if (
		PreviewActionRequest.ActionId == TEXT("calibrate_antenna"))
	{
		int32 Mode = PreviewActionRequest.bForce
			? 2
			: PreviewActionRequest.bHasCollaborator ? 1 : 0;
		Mode = (Mode + 1) % 3;
		PreviewActionRequest.bHasCollaborator = Mode == 1;
		PreviewActionRequest.Collaborator =
			EWSCharacterId::YeCheng;
		PreviewActionRequest.bForce = Mode == 2;
	}
	else
	{
		return;
	}
	RefreshActionPreview();
}

void AWhiteoutCharacter::RefreshActionPreview()
{
	if (!PreviewedInteractable)
	{
		return;
	}
	const FWSActionPreview Preview = PreviewedInteractable->PreviewRequest(PreviewActionRequest);
	bPreviewCanExecute = Preview.bCanExecute;
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->ShowActionPreview(PreviewedInteractable->DisplayName, Preview, PreviewActionRequest);
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
	PendingPlayerSaid.Reset();
	PendingSemanticFrame = FWSDialogueSemanticFrame();
	CurrentDialogueTopicActionId = NAME_None;
	ActiveDialogueSessionId = FGuid::NewGuid();
	ActiveDialogueTransactionId.Invalidate();
	PreviewedInteractable = nullptr;
	bPreviewCanExecute = false;
	PreviewActionRequest = FWSActionRequest();
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
	PendingSemanticFrame = FWSDialogueSemanticFrame();
	PendingSemanticFrame.SpeechAct = DialogueAct;
	PendingSemanticFrame.Source = TEXT("dialogue_wheel");
	PendingSemanticFrame.Confidence = 1.0f;
	SubmitDialogueChoice(DialogueAct, NAME_None, FString());
}

void AWhiteoutCharacter::ChooseDialoguePromise(const FName PromiseCondition)
{
	PendingSemanticFrame = FWSDialogueSemanticFrame();
	PendingSemanticFrame.SpeechAct = EWSDialogueAct::Promise;
	PendingSemanticFrame.TargetActionId = PromiseCondition == TEXT("heat_repair_room")
		? FName(TEXT("repair_generator"))
		: NAME_None;
	PendingSemanticFrame.Source = TEXT("dialogue_wheel");
	PendingSemanticFrame.Confidence = 1.0f;
	SubmitDialogueChoice(EWSDialogueAct::Promise, PromiseCondition, FString());
}

FWSActionPreview AWhiteoutCharacter::PreviewActiveDialogue(
	const EWSDialogueAct DialogueAct,
	const FName PromiseCondition) const
{
	if (ActiveDialogueTarget)
	{
		return ActiveDialogueTarget->PreviewInteraction(DialogueAct, PromiseCondition);
	}
	FWSActionPreview Preview;
	Preview.ReasonCode = EWSReasonCode::UnknownAction;
	return Preview;
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
	FWSActionRequest Request = ActiveDialogueTarget->BuildActionRequest(
		DialogueAct,
		PromiseCondition,
		PendingPlayerSaid,
		ActiveDialogueSessionId);
	Request.SemanticFrame = PendingSemanticFrame;
	const FWSActionPreview Preview = ActiveDialogueTarget->PreviewRequest(Request);
	if (!Preview.bCanExecute)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
			{
				HUD->SetDialogueIntentStatus(FString::Printf(
					TEXT("%s %s"),
					*FWSPresentationText::ReasonCause(Preview.ReasonCode).ToString(),
					*FWSPresentationText::ReasonNextStep(Preview.ReasonCode).ToString()), false);
			}
		}
		return;
	}
	bDialogueChoiceCommitted = true;
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->SetDialogueIntentStatus(TEXT("交涉已提交，正在组织回应……按 Esc 或“离开”取消等待。"), true);
		}
	}
	ActiveDialogueTransactionId = Request.TransactionId;
	ActiveDialogueTarget->InteractRequest(this, Request);
	PendingPlayerSaid.Reset();
}

void AWhiteoutCharacter::SubmitDialogueText(const FString& UserText)
{
	if (!ActiveDialogueTarget || bDialogueChoiceCommitted || bDialogueIntentPending)
	{
		return;
	}
	PendingPlayerSaid = UserText.TrimStartAndEnd().Left(280);
	if (PendingPlayerSaid.IsEmpty()
		|| UWSAgentGateway::ContainsAdversarialInstruction(PendingPlayerSaid))
	{
		PendingPlayerSaid.Reset();
		return;
	}
	UWindStationStateSubsystem* StateSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>()
		: nullptr;
	if (!StateSubsystem)
	{
		return;
	}
	bDialogueIntentPending = true;
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->SetDialogueIntentStatus(TEXT("正在理解你的问题……"), true);
		}
	}
	const FGuid ExpectedSessionId = ActiveDialogueSessionId;
	const FName DialogueActionId = ActiveDialogueTarget->ActionId;
	TWeakObjectPtr<AWhiteoutCharacter> WeakThis(this);
	StateSubsystem->RequestDialogueIntent(
		PendingPlayerSaid,
		DialogueActionId,
		CurrentDialogueTopicActionId,
		[WeakThis, ExpectedSessionId](const FWSDialogueIntentResult& Intent)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			AWhiteoutCharacter* Character = WeakThis.Get();
			if (!Character->ActiveDialogueTarget
				|| Character->ActiveDialogueSessionId != ExpectedSessionId)
			{
				return;
			}
			Character->bDialogueIntentPending = false;
			if (!Intent.bMapped)
			{
				if (APlayerController* PlayerController = Cast<APlayerController>(Character->Controller))
				{
					if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
					{
						HUD->SetDialogueIntentStatus(TEXT("这句话的意图不够明确，请换一种问法或使用对话选项。"), false);
					}
				}
				return;
			}
			Character->PendingSemanticFrame = Intent.ToSemanticFrame();
			if (!Intent.TargetActionId.IsNone())
			{
				Character->CurrentDialogueTopicActionId = Intent.TargetActionId;
			}
			Character->CommitDialogueChoice(Intent.DialogueAct, Intent.PromiseCondition);
		});
}

void AWhiteoutCharacter::SubmitDialogueChoice(
	const EWSDialogueAct DialogueAct,
	const FName PromiseCondition,
	const FString& PlayerSaid)
{
	if (!ActiveDialogueTarget || bDialogueChoiceCommitted || bDialogueIntentPending)
	{
		return;
	}
	PendingPlayerSaid = PlayerSaid.TrimStartAndEnd().Left(280);
	if (!PendingPlayerSaid.IsEmpty() && UWSAgentGateway::ContainsAdversarialInstruction(PendingPlayerSaid))
	{
		PendingPlayerSaid.Reset();
		return;
	}
	if (PendingSemanticFrame.Source.IsEmpty())
	{
		PendingSemanticFrame.SpeechAct = DialogueAct;
		PendingSemanticFrame.TargetCharacter = ActiveDialogueTarget->ActionId == TEXT("talk_ye_cheng")
			? EWSCharacterId::YeCheng
			: EWSCharacterId::GuHeng;
		PendingSemanticFrame.Source = TEXT("dialogue_wheel");
		PendingSemanticFrame.Confidence = 1.0f;
	}
	CommitDialogueChoice(DialogueAct, PromiseCondition);
}

void AWhiteoutCharacter::ContinueDialogue()
{
	if (!ActiveDialogueTarget)
	{
		return;
	}
	bDialogueChoiceCommitted = false;
	bDialogueIntentPending = false;
	PendingPlayerSaid.Reset();
	PendingSemanticFrame = FWSDialogueSemanticFrame();
	ActiveDialogueTransactionId.Invalidate();
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->ShowDialogueWheelChoices();
		}
	}
}

void AWhiteoutCharacter::CancelDialogue()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWindStationStateSubsystem* StateSubsystem =
			GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			StateSubsystem->CancelPendingDialogue();
		}
	}
	if (ActiveDialogueTarget)
	{
		ActiveDialogueTarget->SetDialogueLookAtActive(false);
	}
	ActiveDialogueTarget = nullptr;
	ActiveDialogueSessionId.Invalidate();
	ActiveDialogueTransactionId.Invalidate();
	bDialogueChoiceCommitted = false;
	bDialogueIntentPending = false;
	PendingPlayerSaid.Reset();
	PendingSemanticFrame = FWSDialogueSemanticFrame();
	CurrentDialogueTopicActionId = NAME_None;
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

void AWhiteoutCharacter::HandleJumpPressed()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			if (HUD->IsOpeningVisible())
			{
				HUD->AdvanceOpening();
				return;
			}
		}
	}
	Jump();
}

void AWhiteoutCharacter::AdvanceOpening()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->AdvanceOpening();
		}
	}
}

void AWhiteoutCharacter::ToggleGuide()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->ToggleGuide();
		}
	}
}

void AWhiteoutCharacter::TogglePauseMenu()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->HandleBackRequested();
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
	bEarlySettleConfirmationPending = false;
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
	const auto FinishRun = [this, StateSubsystem]()
	{
		const FWSGameState Results = StateSubsystem->EndGame();
		FString EventLogPath;
		StateSubsystem->ExportEventLog(EventLogPath);
		if (APlayerController* PlayerController =
				Cast<APlayerController>(Controller))
		{
			if (AWhiteoutHUD* HUD =
					Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
			{
				HUD->SetSystemMessage(FString::Printf(
					TEXT("本轮已结束：总分 %.1f，评级 %s。按 R 重新开始。"),
					Results.Score.Total,
					*Results.Score.Rating));
			}
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("WhiteoutStation InputSettle: ending=%s score=%.2f log=%s"),
			*StaticEnum<EWSEndingType>()->GetNameStringByValue(
				static_cast<int64>(Results.Ending)),
			Results.Score.Total,
			*EventLogPath);
	};
	if (StateSubsystem->GetRulesEngine().IsV11())
	{
		if (Before.Phase == EWSGamePhase::Results)
		{
			return;
		}
		if (Before.Tasks.bSignalSent
			|| Before.bDayWindowClosed
			|| Before.DayPhase == EWSDayPhase::Complete)
		{
			bEarlySettleConfirmationPending = false;
			FinishRun();
			return;
		}
		if (!Before.bDayPhaseStarted)
		{
			if (APlayerController* PlayerController =
					Cast<APlayerController>(Controller))
			{
				if (AWhiteoutHUD* HUD =
						Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
				{
					HUD->SetSystemMessage(
						TEXT("本阶段尚未开始。请先在控制室、维修间、医务室或厨房选择一个供暖区。"));
				}
			}
			return;
		}
		if (Before.ActionPoints > 0)
		{
			const bool bSameStateWasConfirmed =
				bEarlySettleConfirmationPending
				&& EarlySettleConfirmationAP == Before.ActionPoints
				&& EarlySettleConfirmationTransactionCount
					== Before.CommittedTransactions.Num();
			if (!bSameStateWasConfirmed)
			{
				bEarlySettleConfirmationPending = true;
				EarlySettleConfirmationAP = Before.ActionPoints;
				EarlySettleConfirmationTransactionCount =
					Before.CommittedTransactions.Num();
				if (APlayerController* PlayerController =
						Cast<APlayerController>(Controller))
				{
					if (AWhiteoutHUD* HUD =
							Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
					{
						HUD->SetSystemMessage(FString::Printf(
							TEXT("本阶段仍有 %d AP；再次按 Enter 将结束阶段，未使用的行动力不会结转。"),
							Before.ActionPoints));
					}
				}
				return;
			}
		}
		bEarlySettleConfirmationPending = false;
		EWSReasonCode Reason = EWSReasonCode::PhaseLocked;
		FWSPhaseSummary Summary;
		if (!StateSubsystem->SettleCurrentDayPhase(Reason, Summary))
		{
			if (APlayerController* PlayerController =
					Cast<APlayerController>(Controller))
			{
				if (AWhiteoutHUD* HUD =
						Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
				{
					HUD->SetSystemMessage(
						FWSPresentationText::ReasonCause(Reason).ToString());
				}
			}
			return;
		}
		const FWSGameState After = StateSubsystem->GetStateSnapshot();
		if (After.DayPhase == EWSDayPhase::Complete)
		{
			FinishRun();
			return;
		}
		if (APlayerController* PlayerController =
				Cast<APlayerController>(Controller))
		{
			if (AWhiteoutHUD* HUD =
					Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
			{
				const FString CausalChanges = Summary.Changes.IsEmpty()
					? TEXT("人物状态无额外变化")
					: FString::Join(Summary.Changes, TEXT("；"));
				HUD->SetSystemMessage(FString::Printf(
					TEXT("阶段结算：%s。放弃 %d AP；下一阶段请重新选择供暖区。"),
					*CausalChanges,
					Summary.UnusedAPDiscarded));
			}
		}
		return;
	}
	if (!Before.Tasks.bSignalSent && Before.ActionPoints > 0 && Before.Phase != EWSGamePhase::Ending)
	{
		const bool bSameStateWasConfirmed = bEarlySettleConfirmationPending
			&& EarlySettleConfirmationAP == Before.ActionPoints
			&& EarlySettleConfirmationTransactionCount == Before.CommittedTransactions.Num();
		if (bSameStateWasConfirmed)
		{
			bEarlySettleConfirmationPending = false;
		}
		else
		{
			bEarlySettleConfirmationPending = true;
			EarlySettleConfirmationAP = Before.ActionPoints;
			EarlySettleConfirmationTransactionCount = Before.CommittedTransactions.Num();
			if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
			{
				if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
				{
					HUD->SetSystemMessage(TEXT("尚未发出求救信号。再次按 Enter 将接受失败结局并结束本轮。"));
				}
			}
			return;
		}
	}
	else
	{
		bEarlySettleConfirmationPending = false;
	}
	FinishRun();
}

AWSInteractableActor* AWhiteoutCharacter::FindLookedAtInteractable() const
{
	if (!FirstPersonCamera || !GetWorld())
	{
		return nullptr;
	}
	return FindInteractableFromView(
		GetWorld(),
		FirstPersonCamera->GetComponentLocation(),
		FirstPersonCamera->GetForwardVector(),
		this);
}

AWSInteractableActor* AWhiteoutCharacter::FindInteractableFromView(
	UWorld* World,
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	const AActor* Viewer,
	const float MaxDistance)
{
	if (!World || ViewDirection.IsNearlyZero() || MaxDistance <= 0.0f)
	{
		return nullptr;
	}

	constexpr float InteractionSweepRadius = 22.0f;
	const FVector Forward = ViewDirection.GetSafeNormal();
	const FVector SweepEnd = ViewLocation + Forward * MaxDistance;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams CandidateParams(
		SCENE_QUERY_STAT(WhiteoutInteractionCandidates),
		false,
		Viewer);
	TArray<FHitResult> CandidateHits;
	World->SweepMultiByChannel(
		CandidateHits,
		ViewLocation,
		SweepEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(InteractionSweepRadius),
		CandidateParams);
	TArray<FHitResult> DynamicCandidateHits;
	World->SweepMultiByObjectType(
			DynamicCandidateHits,
			ViewLocation,
			SweepEnd,
			FQuat::Identity,
			ObjectParams,
			FCollisionShape::MakeSphere(InteractionSweepRadius),
			CandidateParams);
	CandidateHits.Append(DynamicCandidateHits);
	if (CandidateHits.IsEmpty())
	{
		return nullptr;
	}

	AWSInteractableActor* BestCandidate = nullptr;
	float BestAngularError = TNumericLimits<float>::Max();
	float BestDistance = TNumericLimits<float>::Max();
	TSet<AWSInteractableActor*> EvaluatedCandidates;
	for (const FHitResult& CandidateHit : CandidateHits)
	{
		AWSInteractableActor* Candidate =
			Cast<AWSInteractableActor>(CandidateHit.GetActor());
		if (!Candidate || EvaluatedCandidates.Contains(Candidate))
		{
			continue;
		}
		EvaluatedCandidates.Add(Candidate);

		FVector AimPoint = CandidateHit.ImpactPoint;
		if (AimPoint.IsNearlyZero())
		{
			AimPoint = CandidateHit.Location;
		}
		if (AimPoint.IsNearlyZero())
		{
			AimPoint = Candidate->InteractionCollision
				? Candidate->InteractionCollision->Bounds.Origin
				: Candidate->GetActorLocation();
		}
		const FVector ToCandidate = AimPoint - ViewLocation;
		const float CandidateDistance = ToCandidate.Size();
		if (CandidateDistance <= KINDA_SMALL_NUMBER
			|| CandidateDistance > MaxDistance + InteractionSweepRadius)
		{
			continue;
		}
		const float ForwardDistance = FVector::DotProduct(ToCandidate, Forward);
		if (ForwardDistance <= 0.0f)
		{
			continue;
		}

		FCollisionQueryParams SightParams(
			SCENE_QUERY_STAT(WhiteoutInteractionSight),
			false,
			Viewer);
		FHitResult SightHit;
		const FVector SightEnd =
			AimPoint + ToCandidate.GetSafeNormal() * 2.0f;
		const bool bSightBlocked = World->LineTraceSingleByChannel(
			SightHit,
			ViewLocation,
			SightEnd,
			ECC_Visibility,
			SightParams);
		if (bSightBlocked && SightHit.GetActor() != Candidate)
		{
			continue;
		}

		const float Alignment = FVector::DotProduct(
			ToCandidate / CandidateDistance,
			Forward);
		const float AngularError = 1.0f - Alignment;
		if (AngularError < BestAngularError - KINDA_SMALL_NUMBER
			|| (FMath::IsNearlyEqual(AngularError, BestAngularError)
				&& CandidateDistance < BestDistance))
		{
			BestCandidate = Candidate;
			BestAngularError = AngularError;
			BestDistance = CandidateDistance;
		}
	}
	return BestCandidate;
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
