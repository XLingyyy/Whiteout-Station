#include "World/WSInteractableActor.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Components/SceneComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameModeBase.h"
#include "HUD/WhiteoutHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "State/WindStationStateSubsystem.h"
#include "UObject/ConstructorHelpers.h"

AWSInteractableActor::AWSInteractableActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetGenerateOverlapEvents(false);
	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(SceneRoot);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetVisibility(false);
	HeadMesh->SetCastShadow(true);
	CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
	CharacterMesh->SetupAttachment(SceneRoot);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CharacterMesh->SetVisibility(false);
	CharacterMesh->SetCastShadow(true);
	InjuryWrap = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InjuryWrap"));
	InjuryWrap->SetupAttachment(CharacterMesh, TEXT("hand_r"));
	InjuryWrap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InjuryWrap->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		HeadMesh->SetStaticMesh(SphereMesh.Object);
	}
	SetActorScale3D(FVector(0.55f, 0.55f, 0.9f));
}

void AWSInteractableActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bCharacterPresentation)
	{
		return;
	}

	if (bReactionActive && GetWorld()->GetTimeSeconds() >= ReactionUntilTime)
	{
		bReactionActive = false;
		if (const UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
		{
			ApplyCharacterState(StateSubsystem->GetStateSnapshot());
		}
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn || FVector::DistSquared2D(PlayerPawn->GetActorLocation(), GetActorLocation()) > FMath::Square(850.0f))
	{
		return;
	}
	const FVector ToPlayer = PlayerPawn->GetActorLocation() - GetActorLocation();
	const float DesiredYaw = ToPlayer.Rotation().Yaw;
	const FRotator CurrentRotation = GetActorRotation();
	SetActorRotation(FRotator(
		0.0f,
		FMath::FInterpTo(CurrentRotation.Yaw, DesiredYaw, DeltaSeconds, 3.4f),
		0.0f));
}

void AWSInteractableActor::Configure(const FName InActionId, const FText& InDisplayName, const FLinearColor InAccentColor)
{
	ActionId = InActionId;
	DisplayName = InDisplayName;
	AccentColor = InAccentColor;
	FocusOverlayMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/WindStation/Art/Materials/M_WS_InteractionOverlay.M_WS_InteractionOverlay"));
	if (!FocusOverlayMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation v0.2: interaction overlay material is missing"));
	}
	const bool bCharacter = ActionId == TEXT("talk_gu_heng") || ActionId == TEXT("talk_ye_cheng");
	bCharacterPresentation = bCharacter;
	SetActorTickEnabled(bCharacter);
	Mesh->SetVisibility(true);
	HeadMesh->SetVisibility(false);
	CharacterMesh->SetVisibility(false);
	InjuryWrap->SetVisibility(false);
	const TCHAR* PresentationMeshPath = nullptr;
	if (ActionId == TEXT("investigate_generator_log") || ActionId == TEXT("inspect_control_cabinet"))
	{
		PresentationMeshPath = TEXT("/Game/WindStation/Art/Environment/Quaternius/Props/Prop_Computer.Prop_Computer");
	}
	else if (ActionId == TEXT("send_signal") || ActionId == TEXT("heat_repair_room") || ActionId == TEXT("heat_medical_room"))
	{
		PresentationMeshPath = TEXT("/Game/WindStation/Art/Environment/Quaternius/Props/Prop_AccessPoint.Prop_AccessPoint");
	}
	else if (ActionId == TEXT("forced_self_repair") || ActionId == TEXT("distribute_food") || ActionId == TEXT("treat_gu_heng"))
	{
		PresentationMeshPath = TEXT("/Game/WindStation/Art/Environment/Quaternius/Props/Prop_Crate3.Prop_Crate3");
	}
	else if (ActionId == TEXT("dismantle_kitchen_heater"))
	{
		PresentationMeshPath = TEXT("/Game/WindStation/Art/Environment/Quaternius/Props/Prop_Barrel_Large.Prop_Barrel_Large");
	}
	else if (ActionId == TEXT("repair_generator"))
	{
		PresentationMeshPath = TEXT("/Game/WindStation/Art/Environment/Quaternius/Columns/Column_Pipes.Column_Pipes");
	}
	else if (ActionId == TEXT("calibrate_antenna"))
	{
		PresentationMeshPath = TEXT("/Game/WindStation/Art/Environment/Quaternius/Columns/Column_MetalSupport.Column_MetalSupport");
	}
	if (PresentationMeshPath)
	{
		if (UStaticMesh* PresentationMesh = LoadObject<UStaticMesh>(nullptr, PresentationMeshPath))
		{
			Mesh->SetStaticMesh(PresentationMesh);
		}
	}
	if (bCharacter)
	{
		ConfigureCharacterPresentation();
	}
#if WITH_EDITOR
	SetActorLabel(InDisplayName.ToString());
#endif

	const bool bIndustrialSurface = ActionId == TEXT("inspect_control_cabinet")
		|| ActionId == TEXT("repair_generator") || ActionId == TEXT("forced_self_repair")
		|| ActionId == TEXT("dismantle_kitchen_heater") || ActionId == TEXT("calibrate_antenna");
	if (!bCharacter)
	{
		const TCHAR* SurfaceMaterialPath = bIndustrialSurface
			? TEXT("/Game/WindStation/Art/Materials/M_WS_RustedMetal.M_WS_RustedMetal")
			: TEXT("/Game/WindStation/Art/Materials/M_WS_PaintedMetal.M_WS_PaintedMetal");
		if (UMaterialInterface* SurfaceMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			SurfaceMaterialPath))
		{
			for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
			{
				Mesh->SetMaterial(Index, SurfaceMaterial);
			}
			return;
		}
	}
	if (UMaterialInterface* BaseMaterial = Mesh->GetMaterial(0); !bCharacter && BaseMaterial)
	{
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), AccentColor);
		Mesh->SetMaterial(0, DynamicMaterial);
	}
}

bool AWSInteractableActor::IsCharacterHotspot() const
{
	return bCharacterPresentation;
}

void AWSInteractableActor::SetInteractionFocused(const bool bFocused)
{
	const auto ApplyFocus = [this, bFocused](UMeshComponent* Component)
	{
		if (!Component)
		{
			return;
		}
		Component->SetRenderCustomDepth(bFocused);
		Component->SetCustomDepthStencilValue(241);
		Component->SetOverlayMaterial(bFocused ? FocusOverlayMaterial.Get() : nullptr);
	};
	ApplyFocus(Mesh);
	ApplyFocus(HeadMesh);
	ApplyFocus(CharacterMesh);
	ApplyFocus(InjuryWrap);
}

void AWSInteractableActor::ConfigureCharacterPresentation()
{
	const bool bEngineer = ActionId == TEXT("talk_gu_heng");
	const TCHAR* RootPath = bEngineer
		? TEXT("/Game/WindStation/Art/Characters/Engineer")
		: TEXT("/Game/WindStation/Art/Characters/Doctor");
	const FString CharacterToken = bEngineer ? TEXT("WS_Engineer") : TEXT("WS_Doctor");
	const FString MeshPath = FString::Printf(TEXT("%s/SK_%s.SK_%s"), RootPath, *CharacterToken, *CharacterToken);
	if (USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath))
	{
		CharacterMesh->SetSkeletalMesh(SkeletalMesh);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation v0.2: missing NPC skeletal mesh %s"), *MeshPath);
		return;
	}

	SetActorScale3D(FVector::OneVector);
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Mesh->SetVisibility(false);
	Mesh->SetHiddenInGame(true);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 86.0f));
	Mesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.72f));
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	CharacterMesh->SetVisibility(true);
	CharacterMesh->SetHiddenInGame(false);
	CharacterMesh->SetRelativeScale3D(FVector(0.1f));
	CharacterMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	CharacterMesh->SetRelativeLocation(bEngineer
		? FVector(0.0f, -11.6f, 2.7f)
		: FVector(0.0f, -7.1f, 1.6f));
	CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	const FString BlueprintPath = FString::Printf(TEXT("%s/ABP_%s.ABP_%s"), RootPath, *CharacterToken, *CharacterToken);
	if (const UAnimBlueprint* AnimationBlueprint = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath))
	{
		CharacterMesh->SetAnimInstanceClass(AnimationBlueprint->GeneratedClass);
	}

	const auto LoadAnimation = [RootPath, CharacterToken](const TCHAR* Suffix)
	{
		const FString Path = FString::Printf(
			TEXT("%s/AN_%s_%s.AN_%s_%s"),
			RootPath, *CharacterToken, Suffix, *CharacterToken, Suffix);
		return LoadObject<UAnimSequence>(nullptr, *Path);
	};
	IdleAnimation = LoadAnimation(TEXT("Idle"));
	GestureAnimation = LoadAnimation(TEXT("Gesture"));
	GuardedAnimation = LoadAnimation(TEXT("Guarded"));
	WorkAnimation = LoadAnimation(TEXT("Work"));

	if (bEngineer)
	{
		if (UStaticMesh* WrapMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
		{
			InjuryWrap->SetStaticMesh(WrapMesh);
			InjuryWrap->SetVisibility(true);
			InjuryWrap->SetHiddenInGame(false);
			// Imported MakeHuman bones carry a 100x local scale; compensate so
			// the primitive reads as a hand-sized emergency wrap, not a prop.
			InjuryWrap->SetRelativeScale3D(FVector(0.006f, 0.006f, 0.0024f));
			InjuryWrap->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
			if (UMaterialInterface* WrapMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/WindStation/Art/Materials/M_WS_Snow.M_WS_Snow")))
			{
				InjuryWrap->SetMaterial(0, WrapMaterial);
			}
		}
	}

	if (UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		StateSubsystem->OnStateChanged.AddUniqueDynamic(this, &AWSInteractableActor::HandleCharacterStateChanged);
		StateSubsystem->OnActionCommitted.AddUniqueDynamic(this, &AWSInteractableActor::HandleCharacterActionCommitted);
		ApplyCharacterState(StateSubsystem->GetStateSnapshot());
	}
	else
	{
		PlayCharacterAnimation(IdleAnimation);
	}
}

void AWSInteractableActor::PlayCharacterAnimation(UAnimSequence* Animation, const bool bLoop)
{
	if (!CharacterMesh || !Animation)
	{
		return;
	}
	CharacterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	CharacterMesh->PlayAnimation(Animation, bLoop);
}

void AWSInteractableActor::ApplyCharacterState(const FWSGameState& State)
{
	if (!bCharacterPresentation || bReactionActive)
	{
		return;
	}
	const EWSCharacterId CharacterId = ActionId == TEXT("talk_gu_heng")
		? EWSCharacterId::GuHeng
		: EWSCharacterId::YeCheng;
	const FWSCharacterState* Character = State.Characters.Find(CharacterId);
	const float Trust = Character ? Character->Trust : 0.0f;
	if (ActionId == TEXT("talk_gu_heng"))
	{
		if (State.Tasks.GeneratorProgress > 0 && !State.Tasks.bSignalSent)
		{
			PlayCharacterAnimation(WorkAnimation);
		}
		else if (State.Flags.bGuHengTreated && Trust >= 0.0f)
		{
			PlayCharacterAnimation(GestureAnimation);
		}
		else if (!State.Flags.bGuHengTreated || Trust < -5.0f)
		{
			PlayCharacterAnimation(GuardedAnimation);
		}
		else
		{
			PlayCharacterAnimation(IdleAnimation);
		}
	}
	else
	{
		PlayCharacterAnimation(Trust < 0.0f ? GuardedAnimation : IdleAnimation);
	}
}

void AWSInteractableActor::HandleCharacterStateChanged(const FWSGameState& State)
{
	ApplyCharacterState(State);
}

void AWSInteractableActor::HandleCharacterActionCommitted(const FWSActionResult& Result)
{
	const bool bMyDialogue = (ActionId == TEXT("talk_gu_heng") && Result.ActionId == TEXT("talk_gu_heng"))
		|| (ActionId == TEXT("talk_ye_cheng") && Result.ActionId == TEXT("talk_ye_cheng"));
	const bool bEngineerWork = ActionId == TEXT("talk_gu_heng") && Result.ActionId == TEXT("repair_generator");
	const bool bEngineerTreatment = ActionId == TEXT("talk_gu_heng") && Result.ActionId == TEXT("treat_gu_heng");
	if (!Result.bCommitted || (!bMyDialogue && !bEngineerWork && !bEngineerTreatment))
	{
		return;
	}
	bReactionActive = true;
	ReactionUntilTime = GetWorld()->GetTimeSeconds() + (bEngineerWork ? 3.2f : 2.1f);
	PlayCharacterAnimation(bEngineerWork ? WorkAnimation : GestureAnimation);
}

void AWSInteractableActor::SetCharacterPreviewMood(const bool bHighTrust)
{
	if (!bCharacterPresentation)
	{
		return;
	}
	bReactionActive = true;
	ReactionUntilTime = TNumericLimits<float>::Max();
	PlayCharacterAnimation(bHighTrust ? GestureAnimation : GuardedAnimation);
}

FText AWSInteractableActor::GetInteractionPrompt() const
{
	return FText::Format(FText::FromString(TEXT("[F] 查看行动：{0}")), DisplayName);
}

FWSActionRequest AWSInteractableActor::BuildRequest(
	const EWSDialogueAct DialogueAct,
	const FName PromiseCondition) const
{
	FWSActionRequest Request;
	Request.ActionId = ActionId;
	Request.TransactionId = FGuid::NewGuid();
	Request.DialogueAct = DialogueAct;
	Request.PromiseCondition = PromiseCondition;
	if (ActionId == TEXT("distribute_food"))
	{
		Request.FoodForPlayer = 1;
		Request.FoodForGuHeng = 1;
	}
	if (ActionId == TEXT("treat_gu_heng"))
	{
		Request.TreatmentResource = EWSResourceType::Medicine;
	}
	return Request;
}

FWSActionPreview AWSInteractableActor::PreviewInteraction(
	const EWSDialogueAct DialogueAct,
	const FName PromiseCondition) const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			return StateSubsystem->PreviewAction(BuildRequest(DialogueAct, PromiseCondition));
		}
	}
	FWSActionPreview Preview;
	Preview.ActionId = ActionId;
	return Preview;
}

FWSActionResult AWSInteractableActor::Interact(
	APawn* InstigatorPawn,
	const EWSDialogueAct DialogueAct,
	const FName PromiseCondition)
{
	FWSActionResult Empty;
	if (!InstigatorPawn || !GetGameInstance())
	{
		return Empty;
	}
	UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	if (!StateSubsystem)
	{
		return Empty;
	}

	const FWSActionRequest Request = BuildRequest(DialogueAct, PromiseCondition);

	const FWSActionPreview Preview = StateSubsystem->PreviewAction(Request);
	FWSActionResult Result;
	if (Preview.bCanExecute)
	{
		Result = StateSubsystem->CommitAction(Request);
	}
	else
	{
		Result.ActionId = ActionId;
		Result.ReasonCode = Preview.ReasonCode;
		Result.APBefore = StateSubsystem->GetStateSnapshot().ActionPoints;
		Result.APAfter = Result.APBefore;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(InstigatorPawn->GetController()))
	{
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PlayerController->GetHUD()))
		{
			HUD->SetActionFeedback(
				DisplayName,
				Result,
				Preview,
				DialogueAct == EWSDialogueAct::Promise && Result.bCommitted);
		}
	}
	return Result;
}
