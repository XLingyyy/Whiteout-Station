#include "World/WSInteractableActor.h"
#include "World/WSLookAtSkeletalMeshComponent.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/GameModeBase.h"
#include "HUD/WhiteoutHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Presentation/WSPresentationData.h"
#include "State/WindStationStateSubsystem.h"
#include "State/WSKnowledgePolicy.h"
#include "UObject/ConstructorHelpers.h"
#include "WorldCollision.h"

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
	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(
		TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->SetBoxExtent(FVector(65.0f, 65.0f, 90.0f));
	InteractionCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(
		ECC_Visibility,
		ECR_Block);
	InteractionCollision->SetGenerateOverlapEvents(false);
	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(SceneRoot);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetVisibility(false);
	HeadMesh->SetCastShadow(true);
	CharacterMesh = CreateDefaultSubobject<UWSLookAtSkeletalMeshComponent>(TEXT("CharacterMesh"));
	CharacterMesh->SetupAttachment(SceneRoot);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CharacterMesh->SetVisibility(false);
	CharacterMesh->SetCastShadow(true);
	InjuryWrap = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InjuryWrap"));
	InjuryWrap->SetupAttachment(SceneRoot);
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

void AWSInteractableActor::BeginPlay()
{
	Super::BeginPlay();

	const bool bShouldInitializeCharacter = ActionId == TEXT("talk_gu_heng") || ActionId == TEXT("talk_ye_cheng");
	if (!bShouldInitializeCharacter || bCharacterPresentation)
	{
		return;
	}

	// Editable level actors already serialize their chosen meshes and materials.
	// Restore only the runtime behavior here so those instance-level visual edits survive PIE.
	bCharacterPresentation = true;
	SetActorTickEnabled(true);
	CaptureHomeTransform();
	RestoreLegacyCharacterMaterials();
	ResolveV10Animations();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			StateSubsystem->OnStateChanged.AddUniqueDynamic(this, &AWSInteractableActor::HandleCharacterStateChanged);
			StateSubsystem->OnActionCommitted.AddUniqueDynamic(this, &AWSInteractableActor::HandleCharacterActionCommitted);
			StateSubsystem->OnDialogueLine.AddUniqueDynamic(this, &AWSInteractableActor::HandleDialogueLine);
			ApplyCharacterState(StateSubsystem->GetStateSnapshot());
			return;
		}
	}
	PlayCharacterAnimation(IdleAnimation);
}

void AWSInteractableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			StateSubsystem->OnStateChanged.RemoveDynamic(this, &AWSInteractableActor::HandleCharacterStateChanged);
			StateSubsystem->OnActionCommitted.RemoveDynamic(this, &AWSInteractableActor::HandleCharacterActionCommitted);
			StateSubsystem->OnDialogueLine.RemoveDynamic(this, &AWSInteractableActor::HandleDialogueLine);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AWSInteractableActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bCharacterPresentation)
	{
		return;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector PlayerOffset = PlayerPawn
		? PlayerPawn->GetActorLocation() - GetActorLocation()
		: FVector::ZeroVector;
	const bool bPlayerInRange = PlayerPawn
		&& !PlayerOffset.IsNearlyZero()
		&& PlayerOffset.SizeSquared2D() <= FMath::Square(
			bDialogueLookAtActive ? 1300.0f : 850.0f);

	if (bMovementActive)
	{
		const FVector CurrentLocation = GetActorLocation();
		const FVector NextLocation = FMath::VInterpConstantTo(CurrentLocation, MovementTarget, DeltaSeconds, 82.0f);
		const FVector TravelDirection = (MovementTarget - CurrentLocation).GetSafeNormal2D();
		const FVector FacingDirection =
			ActiveMovementIntent == EWSNPCMovementIntent::StepBack
				&& bPlayerInRange
			? PlayerOffset.GetSafeNormal2D()
			: TravelDirection;
		if (!FacingDirection.IsNearlyZero())
		{
			const FRotator DesiredRotation = MakeActorRotationFacing(FacingDirection);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRotation, DeltaSeconds, 7.0f));
		}
		if (!IsMovementPathClear(CurrentLocation, NextLocation))
		{
			FinishMovement();
		}
		else
		{
			SetActorLocation(NextLocation, false, nullptr, ETeleportType::None);
			if (FVector::DistSquared2D(NextLocation, MovementTarget) <= FMath::Square(2.0f))
			{
				SetActorLocation(MovementTarget, false, nullptr, ETeleportType::None);
				FinishMovement();
			}
		}
	}
	else
	{
		const FRotator DesiredRotation = bPlayerInRange
			? MakeActorRotationFacing(PlayerOffset)
			: HomeRotation;
		SetActorRotation(FMath::RInterpTo(
			GetActorRotation(),
			DesiredRotation,
			DeltaSeconds,
			bPlayerInRange ? 6.5f : 2.4f));
	}

	if (bReactionActive && GetWorld()->GetTimeSeconds() >= ReactionUntilTime)
	{
		bReactionActive = false;
		if (const UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
		{
			ApplyCharacterState(StateSubsystem->GetStateSnapshot());
		}
	}

	float TargetYaw = 0.0f;
	float TargetPitch = 0.0f;
	if (bPlayerInRange)
	{
		const FName HeadBone = CharacterMesh->GetBoneIndex(TEXT("head")) != INDEX_NONE
			? FName(TEXT("head"))
			: FName(TEXT("J_Bip_C_Head"));
		const FVector HeadLocation = CharacterMesh->GetBoneLocation(HeadBone, EBoneSpaces::WorldSpace);
		const FVector ToPlayer = PlayerPawn->GetPawnViewLocation() - HeadLocation;
		const FRotator TargetRotation = ToPlayer.Rotation();
		TargetYaw = FMath::Clamp(
			FMath::FindDeltaAngleDegrees(
				GetVisualFacingYaw(),
				TargetRotation.Yaw),
			-20.0f,
			20.0f);
		TargetPitch = FMath::Clamp(TargetRotation.Pitch, -12.0f, 15.0f);
	}
	CurrentLookAtYaw = FMath::FInterpTo(CurrentLookAtYaw, TargetYaw, DeltaSeconds, bPlayerInRange ? 4.8f : 3.2f);
	CurrentLookAtPitch = FMath::FInterpTo(CurrentLookAtPitch, TargetPitch, DeltaSeconds, bPlayerInRange ? 4.8f : 3.2f);
	CharacterMesh->SetLookAtAngles(CurrentLookAtYaw, CurrentLookAtPitch);
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
	else if (ActionId == TEXT("send_signal")
		|| ActionId == TEXT("heat_control_room")
		|| ActionId == TEXT("heat_repair_room")
		|| ActionId == TEXT("heat_medical_room")
		|| ActionId == TEXT("heat_kitchen"))
	{
		PresentationMeshPath = TEXT("/Game/WindStation/Art/Environment/Quaternius/Props/Prop_AccessPoint.Prop_AccessPoint");
	}
	else if (ActionId == TEXT("forced_self_repair") || ActionId == TEXT("distribute_food"))
	{
		PresentationMeshPath = TEXT("/Game/WindStation/Art/Environment/Quaternius/Props/Prop_Crate3.Prop_Crate3");
	}
	else if (ActionId == TEXT("treat_gu_heng")
		|| ActionId == TEXT("treat_character"))
	{
		PresentationMeshPath = TEXT("/Game/WindStation/Art/Environment/Quaternius/Interior/NightStand_1.NightStand_1");
	}
	else if (ActionId == TEXT("rest"))
	{
		PresentationMeshPath = TEXT("/Game/WindStation/Art/Environment/Quaternius/Interior/Bed_Single.Bed_Single");
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
		PresentationMeshPath = TEXT("/Game/WindStation/Art/Environment/Quaternius/Props/Prop_Computer.Prop_Computer");
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
		// v0.4: keep the source material intact and use only the CustomDepth outline.
		// FocusOverlayMaterial remains loaded for rollback until the art milestone.
	};
	ApplyFocus(Mesh);
	ApplyFocus(HeadMesh);
	ApplyFocus(CharacterMesh);
	ApplyFocus(InjuryWrap);
}

void AWSInteractableActor::ConfigureCharacterPresentation()
{
	if (CharacterAsset)
	{
		// ===== 数据资产驱动（推荐路径：换 VRM/SK 模型只改数据资产） =====
		if (USkeletalMesh* SkeletalMesh = CharacterAsset->SkeletalMesh.IsNull() ? nullptr : CharacterAsset->SkeletalMesh.LoadSynchronous())
		{
			CharacterMesh->SetSkeletalMesh(SkeletalMesh);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("WhiteoutStation: CharacterAsset %s 的 SkeletalMesh 为空，无法生成角色"),
				*CharacterAsset->GetName());
			return;
		}
		SetActorScale3D(CharacterAsset->ActorScale);

		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Mesh->SetVisibility(false);
		Mesh->SetHiddenInGame(true);
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 86.0f));
		Mesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.72f));
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));

		CharacterMesh->SetVisibility(true);
		CharacterMesh->SetHiddenInGame(false);
		CharacterMesh->SetRelativeLocation(CharacterAsset->MeshLocation);
		CharacterMesh->SetRelativeRotation(CharacterAsset->MeshRotation);
		CharacterMesh->SetRelativeScale3D(CharacterAsset->MeshScale);
		CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

		if (!CharacterAsset->EyeMaterial.IsNull())
		{
			if (UMaterialInterface* EyeMat = CharacterAsset->EyeMaterial.LoadSynchronous())
			{
				const int32 Slot = CharacterMesh->GetMaterialIndex(CharacterAsset->EyeMaterialSlotName);
				if (Slot != INDEX_NONE)
				{
					CharacterMesh->SetMaterial(Slot, EyeMat);
				}
			}
		}

		if (!CharacterAsset->AnimBlueprint.IsNull())
		{
			if (const UAnimBlueprint* AnimBp = CharacterAsset->AnimBlueprint.LoadSynchronous())
			{
				CharacterMesh->SetAnimInstanceClass(AnimBp->GeneratedClass);
			}
		}

		IdleAnimation = CharacterAsset->Animations.Idle.IsNull() ? nullptr : CharacterAsset->Animations.Idle.LoadSynchronous();
		GestureAnimation = CharacterAsset->Animations.Gesture.IsNull() ? nullptr : CharacterAsset->Animations.Gesture.LoadSynchronous();
		GuardedAnimation = CharacterAsset->Animations.Guarded.IsNull() ? nullptr : CharacterAsset->Animations.Guarded.LoadSynchronous();
		WorkAnimation = CharacterAsset->Animations.Work.IsNull() ? nullptr : CharacterAsset->Animations.Work.LoadSynchronous();

		const FWSInjuryWrapConfig& WrapCfg = CharacterAsset->InjuryWrap;
		if (!WrapCfg.Mesh.IsNull())
		{
			if (UStaticMesh* WrapMesh = WrapCfg.Mesh.LoadSynchronous())
			{
				InjuryWrap->SetStaticMesh(WrapMesh);
				InjuryWrap->SetVisibility(false);
				InjuryWrap->SetHiddenInGame(true);
				InjuryWrap->SetRelativeScale3D(WrapCfg.RelativeScale);
				InjuryWrap->SetRelativeRotation(WrapCfg.RelativeRotation);
				if (!WrapCfg.Material.IsNull())
				{
					if (UMaterialInterface* WrapMat = WrapCfg.Material.LoadSynchronous())
					{
						InjuryWrap->SetMaterial(0, WrapMat);
					}
				}
				InjuryWrap->AttachToComponent(
					CharacterMesh,
					FAttachmentTransformRules::KeepRelativeTransform,
					WrapCfg.AttachSocket);
			}
		}
		else
		{
			InjuryWrap->SetVisibility(false);
			InjuryWrap->SetHiddenInGame(true);
		}
	}
	else
	{
		// ===== 旧硬编码路径（v0.4 兼容；未指派 CharacterAsset 时使用） =====
		const bool bEngineer = ActionId == TEXT("talk_gu_heng");
		const TCHAR* RootPath = bEngineer
			? TEXT("/Game/WindStation/Art/Characters/Engineer")
			: TEXT("/Game/WindStation/Art/Characters/Doctor");
		const FString CharacterToken = bEngineer ? TEXT("WS_Engineer") : TEXT("WS_Doctor");
		const FString MeshPath = FString::Printf(TEXT("%s/SK_%s.SK_%s"), RootPath, *CharacterToken, *CharacterToken);
		if (USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath))
		{
			CharacterMesh->SetSkeletalMesh(SkeletalMesh);
			if (UMaterialInterface* EyeMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/WindStation/Art/Materials/M_WS_Eye.M_WS_Eye")))
			{
				const int32 EyeMaterialIndex = CharacterMesh->GetMaterialIndex(TEXT("high-poly"));
				if (EyeMaterialIndex != INDEX_NONE)
				{
					CharacterMesh->SetMaterial(EyeMaterialIndex, EyeMaterial);
				}
			}
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
		InjuryWrap->AttachToComponent(
			CharacterMesh,
			FAttachmentTransformRules::KeepRelativeTransform,
			TEXT("hand_r"));

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
				InjuryWrap->SetVisibility(false);
				InjuryWrap->SetHiddenInGame(true);
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
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			CaptureHomeTransform();
			RestoreLegacyCharacterMaterials();
			ResolveV10Animations();
			StateSubsystem->OnStateChanged.AddUniqueDynamic(this, &AWSInteractableActor::HandleCharacterStateChanged);
			StateSubsystem->OnActionCommitted.AddUniqueDynamic(this, &AWSInteractableActor::HandleCharacterActionCommitted);
			StateSubsystem->OnDialogueLine.AddUniqueDynamic(this, &AWSInteractableActor::HandleDialogueLine);
			ApplyCharacterState(StateSubsystem->GetStateSnapshot());
			return;
		}
	}
	CaptureHomeTransform();
	RestoreLegacyCharacterMaterials();
	ResolveV10Animations();
	PlayCharacterAnimation(IdleAnimation);
}

void AWSInteractableActor::CaptureHomeTransform()
{
	if (bHomeTransformCaptured)
	{
		return;
	}
	HomeLocation = GetActorLocation();
	HomeRotation = GetActorRotation();
	bHomeTransformCaptured = true;
}

void AWSInteractableActor::RestoreLegacyCharacterMaterials()
{
	if (!CharacterMesh)
	{
		return;
	}
	const USkeletalMesh* SkeletalMesh = CharacterMesh->GetSkeletalMeshAsset();
	UMaterialInterface* LegacyEyeMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/WindStation/Art/Materials/M_WS_Eye.M_WS_Eye"));
	if (!SkeletalMesh || !LegacyEyeMaterial)
	{
		return;
	}

	const TArray<FSkeletalMaterial>& ImportedMaterials = SkeletalMesh->GetMaterials();
	const int32 MaterialCount = FMath::Min(CharacterMesh->GetNumMaterials(), ImportedMaterials.Num());
	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		UMaterialInterface* CurrentMaterial = CharacterMesh->GetMaterial(Index);
		UMaterialInterface* ImportedMaterial = ImportedMaterials[Index].MaterialInterface;
		if (CurrentMaterial == LegacyEyeMaterial
			&& ImportedMaterial
			&& ImportedMaterial != LegacyEyeMaterial)
		{
			CharacterMesh->SetMaterial(Index, ImportedMaterial);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("WhiteoutStation v1.0: restored imported character material action=%s slot=%d material=%s"),
				*ActionId.ToString(),
				Index,
				*ImportedMaterial->GetPathName());
		}
	}
}

float AWSInteractableActor::GetVisualFacingYaw() const
{
	const float MeshYaw = CharacterMesh
		? CharacterMesh->GetRelativeRotation().Yaw
		: 0.0f;
	return FRotator::NormalizeAxis(
		GetActorRotation().Yaw + MeshYaw + VisualFacingYawOffsetDegrees);
}

FRotator AWSInteractableActor::MakeActorRotationFacing(
	const FVector& WorldDirection) const
{
	if (WorldDirection.IsNearlyZero())
	{
		return GetActorRotation();
	}
	const float MeshYaw = CharacterMesh
		? CharacterMesh->GetRelativeRotation().Yaw
		: 0.0f;
	return FRotator(
		0.0f,
		WorldDirection.Rotation().Yaw - MeshYaw - VisualFacingYawOffsetDegrees,
		0.0f);
}

void AWSInteractableActor::ResolveV10Animations()
{
	if (!bCharacterPresentation)
	{
		return;
	}
	const bool bGuHeng = ActionId == TEXT("talk_gu_heng");
	const FString Root = bGuHeng
		? TEXT("/Game/WindStation/Art/AnimeNPC/GuHeng/AnimationsV10")
		: TEXT("/Game/WindStation/Art/AnimeNPC/YeChengV10/AnimationsV10");
	const FString Prefix = bGuHeng ? TEXT("AN_GuHeng") : TEXT("AN_YeCheng_V10");
	const auto LoadAnimation = [&Root, &Prefix](const TCHAR* Suffix)
	{
		const FString AssetPath = FString::Printf(
			TEXT("%s/%s_%s.%s_%s"),
			*Root,
			*Prefix,
			Suffix,
			*Prefix,
			Suffix);
		return LoadObject<UAnimSequence>(nullptr, *AssetPath);
	};
	IdleAnimation = LoadAnimation(TEXT("Idle"));
	WalkAnimation = LoadAnimation(TEXT("Walk"));
	AcknowledgeAnimation = LoadAnimation(TEXT("Acknowledge"));
	ConsiderAnimation = LoadAnimation(TEXT("Consider"));
	ReassureAnimation = LoadAnimation(TEXT("Reassure"));
	RejectAnimation = LoadAnimation(TEXT("Reject"));
	AlarmedAnimation = LoadAnimation(TEXT("Alarmed"));
	GestureAnimation = AcknowledgeAnimation;
	GuardedAnimation = RejectAnimation;
	WorkAnimation = ConsiderAnimation;
	if (!IdleAnimation || !WalkAnimation || !AcknowledgeAnimation || !ConsiderAnimation
		|| !ReassureAnimation || !RejectAnimation || !AlarmedAnimation)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("WhiteoutStation v1.0: exact-skeleton animation set is incomplete for %s"),
			*ActionId.ToString());
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

UAnimSequence* AWSInteractableActor::AnimationForReaction(const EWSNPCReaction Reaction) const
{
	switch (Reaction)
	{
	case EWSNPCReaction::Acknowledge:
		return AcknowledgeAnimation ? AcknowledgeAnimation.Get() : IdleAnimation.Get();
	case EWSNPCReaction::Consider:
		return ConsiderAnimation ? ConsiderAnimation.Get() : IdleAnimation.Get();
	case EWSNPCReaction::Reassure:
		return ReassureAnimation ? ReassureAnimation.Get() : IdleAnimation.Get();
	case EWSNPCReaction::Reject:
		return RejectAnimation ? RejectAnimation.Get() : IdleAnimation.Get();
	case EWSNPCReaction::Alarmed:
		return AlarmedAnimation ? AlarmedAnimation.Get() : IdleAnimation.Get();
	default:
		return nullptr;
	}
}

void AWSInteractableActor::PlayReaction(const EWSNPCReaction Reaction)
{
	bMovementActive = false;
	ActiveMovementIntent = EWSNPCMovementIntent::Stay;
	PendingReaction = EWSNPCReaction::Neutral;
	UAnimSequence* Animation = AnimationForReaction(Reaction);
	if (!Animation)
	{
		bReactionActive = false;
		PlayCharacterAnimation(IdleAnimation);
		return;
	}
	bReactionActive = true;
	ReactionUntilTime = GetWorld()->GetTimeSeconds()
		+ FMath::Clamp(Animation->GetPlayLength(), 0.65f, 3.25f);
	PlayCharacterAnimation(Animation, false);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("WhiteoutStation NPCPerformance: action=%s phase=reaction reaction=%s duration=%.3f"),
		*ActionId.ToString(),
		*StaticEnum<EWSNPCReaction>()->GetNameStringByValue(static_cast<int64>(Reaction)),
		Animation->GetPlayLength());
}

bool AWSInteractableActor::TryStartMovement(
	const EWSNPCMovementIntent Intent,
	const EWSNPCReaction FollowupReaction)
{
	if (!bCharacterPresentation || Intent == EWSNPCMovementIntent::Stay || !GetWorld())
	{
		return false;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextMovementAllowedTime)
	{
		return false;
	}
	CaptureHomeTransform();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return false;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector ToPlayer = PlayerPawn->GetActorLocation() - CurrentLocation;
	const FVector DirectionToPlayer = ToPlayer.GetSafeNormal2D();
	FVector Candidate = CurrentLocation;
	switch (Intent)
	{
	case EWSNPCMovementIntent::StepCloser:
	{
		const float PlayerDistance = ToPlayer.Size2D();
		if (PlayerDistance <= 210.0f || DirectionToPlayer.IsNearlyZero())
		{
			return false;
		}
		Candidate += DirectionToPlayer * FMath::Min(85.0f, PlayerDistance - 175.0f);
		break;
	}
	case EWSNPCMovementIntent::StepBack:
		if (DirectionToPlayer.IsNearlyZero())
		{
			return false;
		}
		Candidate -= DirectionToPlayer * 75.0f;
		break;
	case EWSNPCMovementIntent::ReturnToPost:
		Candidate = HomeLocation;
		break;
	default:
		return false;
	}
	Candidate.Z = CurrentLocation.Z;
	const FVector HomeOffset = Candidate - HomeLocation;
	if (HomeOffset.SizeSquared2D() > FMath::Square(185.0f))
	{
		Candidate = HomeLocation + HomeOffset.GetSafeNormal2D() * 185.0f;
		Candidate.Z = CurrentLocation.Z;
	}
	if (FVector::DistSquared2D(CurrentLocation, Candidate) < FMath::Square(8.0f)
		|| FVector::DistSquared2D(PlayerPawn->GetActorLocation(), Candidate) < FMath::Square(145.0f)
		|| !IsMovementPathClear(CurrentLocation, Candidate))
	{
		return false;
	}

	MovementStart = CurrentLocation;
	MovementTarget = Candidate;
	PendingReaction = FollowupReaction;
	ActiveMovementIntent = Intent;
	bMovementActive = true;
	bReactionActive = false;
	NextMovementAllowedTime = Now + 5.5f;
	PlayCharacterAnimation(WalkAnimation ? WalkAnimation.Get() : IdleAnimation.Get(), true);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("WhiteoutStation NPCPerformance: action=%s phase=move_start movement=%s reaction=%s distance=%.2f"),
		*ActionId.ToString(),
		*StaticEnum<EWSNPCMovementIntent>()->GetNameStringByValue(static_cast<int64>(Intent)),
		*StaticEnum<EWSNPCReaction>()->GetNameStringByValue(static_cast<int64>(FollowupReaction)),
		FVector::Dist2D(CurrentLocation, Candidate));
	return true;
}

bool AWSInteractableActor::IsMovementPathClear(const FVector& Start, const FVector& End) const
{
	const UWorld* World = GetWorld();
	if (!World || FVector::DistSquared2D(Start, End) <= FMath::Square(0.5f))
	{
		return true;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WhiteoutNPCMovement), false, this);
	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Params.AddIgnoredActor(PlayerPawn);
	}
	const FVector CapsuleOffset(0.0f, 0.0f, 86.0f);
	return !World->SweepTestByChannel(
		Start + CapsuleOffset,
		End + CapsuleOffset,
		FQuat::Identity,
		ECC_WorldStatic,
		FCollisionShape::MakeCapsule(28.0f, 78.0f),
		Params);
}

void AWSInteractableActor::FinishMovement()
{
	if (!bMovementActive)
	{
		return;
	}
	bMovementActive = false;
	ActiveMovementIntent = EWSNPCMovementIntent::Stay;
	const EWSNPCReaction Reaction = PendingReaction;
	PendingReaction = EWSNPCReaction::Neutral;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("WhiteoutStation NPCPerformance: action=%s phase=move_finish distance=%.2f home_offset=%.2f"),
		*ActionId.ToString(),
		FVector::Dist2D(MovementStart, GetActorLocation()),
		FVector::Dist2D(HomeLocation, GetActorLocation()));
	PlayReaction(Reaction);
}

void AWSInteractableActor::ApplyCharacterState(const FWSGameState& State)
{
	if (!bCharacterPresentation)
	{
		return;
	}
	if (InjuryWrap)
	{
		const bool bShowInjuryWrap = ActionId == TEXT("talk_gu_heng")
			&& FWSKnowledgePolicy::IsGuHengInjuryWrapVisible(State);
		InjuryWrap->SetVisibility(bShowInjuryWrap);
		InjuryWrap->SetHiddenInGame(!bShowInjuryWrap);
	}
	if (bMovementActive || bReactionActive)
	{
		return;
	}
	bReactionActive = false;
	PlayCharacterAnimation(IdleAnimation);
}

void AWSInteractableActor::HandleCharacterStateChanged(const FWSGameState& State)
{
	ApplyCharacterState(State);
}

void AWSInteractableActor::HandleCharacterActionCommitted(const FWSActionResult& Result)
{
	static_cast<void>(Result);
	if (!bCharacterPresentation)
	{
		return;
	}
	bMovementActive = false;
	bReactionActive = false;
	ActiveMovementIntent = EWSNPCMovementIntent::Stay;
	PendingReaction = EWSNPCReaction::Neutral;
	PlayCharacterAnimation(IdleAnimation);
}

void AWSInteractableActor::HandleDialogueLine(const FWSAgentReply& Reply)
{
	if (!bCharacterPresentation)
	{
		return;
	}
	const bool bGuHengRepairFeedback = ActionId == TEXT("talk_gu_heng")
		&& Reply.ActionId == TEXT("repair_generator")
		&& Reply.Speaker == EWSCharacterId::GuHeng;
	const bool bExpectedSpeaker =
		(Reply.ActionId == ActionId && ActionId == TEXT("talk_gu_heng")
			&& Reply.Speaker == EWSCharacterId::GuHeng)
		|| (Reply.ActionId == ActionId && ActionId == TEXT("talk_ye_cheng")
			&& Reply.Speaker == EWSCharacterId::YeCheng)
		|| bGuHengRepairFeedback;
	if (!bExpectedSpeaker)
	{
		return;
	}
	if (!TryStartMovement(Reply.MovementIntent, Reply.Reaction))
	{
		PlayReaction(Reply.Reaction);
	}
}

void AWSInteractableActor::SetCharacterPreviewMood(const bool bHighTrust)
{
	if (!bCharacterPresentation)
	{
		return;
	}
	static_cast<void>(bHighTrust);
	bMovementActive = false;
	bReactionActive = false;
	ActiveMovementIntent = EWSNPCMovementIntent::Stay;
	PendingReaction = EWSNPCReaction::Neutral;
	CurrentLookAtYaw = 0.0f;
	CurrentLookAtPitch = 0.0f;
	CharacterMesh->SetLookAtAngles(0.0f, 0.0f);
	PlayCharacterAnimation(IdleAnimation);
}

void AWSInteractableActor::SetCharacterPreviewPerformance(const FName PerformanceName)
{
	if (!bCharacterPresentation)
	{
		return;
	}
	bMovementActive = false;
	bReactionActive = false;
	ActiveMovementIntent = EWSNPCMovementIntent::Stay;
	PendingReaction = EWSNPCReaction::Neutral;
	CurrentLookAtYaw = 0.0f;
	CurrentLookAtPitch = 0.0f;
	CharacterMesh->SetLookAtAngles(0.0f, 0.0f);
	if (PerformanceName == TEXT("walk"))
	{
		PlayCharacterAnimation(WalkAnimation ? WalkAnimation.Get() : IdleAnimation.Get(), true);
	}
	else if (PerformanceName == TEXT("acknowledge"))
	{
		PlayCharacterAnimation(AcknowledgeAnimation ? AcknowledgeAnimation.Get() : IdleAnimation.Get());
	}
	else if (PerformanceName == TEXT("consider"))
	{
		PlayCharacterAnimation(ConsiderAnimation ? ConsiderAnimation.Get() : IdleAnimation.Get());
	}
	else if (PerformanceName == TEXT("reassure"))
	{
		PlayCharacterAnimation(ReassureAnimation ? ReassureAnimation.Get() : IdleAnimation.Get());
	}
	else if (PerformanceName == TEXT("reject"))
	{
		PlayCharacterAnimation(RejectAnimation ? RejectAnimation.Get() : IdleAnimation.Get());
	}
	else if (PerformanceName == TEXT("alarmed"))
	{
		PlayCharacterAnimation(AlarmedAnimation ? AlarmedAnimation.Get() : IdleAnimation.Get());
	}
	else
	{
		PlayCharacterAnimation(IdleAnimation);
	}
}

void AWSInteractableActor::SetDialogueLookAtActive(const bool bActive)
{
	bDialogueLookAtActive = bCharacterPresentation && bActive;
}

FText AWSInteractableActor::GetInteractionPrompt() const
{
	if (bCharacterPresentation)
	{
		return FText::Format(FText::FromString(TEXT("[F] 开始对话：{0}")), DisplayName);
	}
	return FText::Format(FText::FromString(TEXT("[F] 查看行动：{0}")), DisplayName);
}

FWSActionRequest AWSInteractableActor::BuildActionRequest(
	const EWSDialogueAct DialogueAct,
	const FName PromiseCondition,
	const FString& PlayerSaid,
	const FGuid DialogueSessionId) const
{
	FWSActionRequest Request;
	Request.ActionId = ActionId;
	Request.TransactionId = FGuid::NewGuid();
	Request.DialogueAct = DialogueAct;
	Request.PromiseCondition = PromiseCondition;
	Request.PlayerSaid = PlayerSaid.TrimStartAndEnd().Left(280);
	Request.DialogueSessionId = DialogueSessionId;
	if (ActionId == TEXT("distribute_food"))
	{
		Request.FoodForPlayer = 1;
		Request.FoodForGuHeng = 1;
	}
	if (ActionId == TEXT("treat_gu_heng"))
	{
		Request.TreatmentResource = EWSResourceType::Medicine;
		Request.TreatmentMethod = EWSTreatmentMethod::Full;
		Request.TreatmentTarget = EWSCharacterId::GuHeng;
	}
	else if (ActionId == TEXT("treat_character"))
	{
		Request.TreatmentResource = EWSResourceType::Medicine;
		Request.TreatmentMethod = EWSTreatmentMethod::Bandage;
		Request.TreatmentTarget = EWSCharacterId::Player;
	}
	return Request;
}

FWSActionPreview AWSInteractableActor::PreviewRequest(const FWSActionRequest& Request) const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			FWSActionRequest SanitizedRequest = Request;
			SanitizedRequest.ActionId = ActionId;
			return StateSubsystem->PreviewAction(SanitizedRequest);
		}
	}
	FWSActionPreview Preview;
	Preview.ActionId = ActionId;
	return Preview;
}

FWSActionPreview AWSInteractableActor::PreviewInteraction(
	const EWSDialogueAct DialogueAct,
	const FName PromiseCondition) const
{
	return PreviewRequest(BuildActionRequest(DialogueAct, PromiseCondition));
}

FWSActionResult AWSInteractableActor::Interact(
	APawn* InstigatorPawn,
	const EWSDialogueAct DialogueAct,
	const FName PromiseCondition)
{
	return InteractRequest(InstigatorPawn, BuildActionRequest(DialogueAct, PromiseCondition));
}

FWSActionResult AWSInteractableActor::InteractRequest(APawn* InstigatorPawn, FWSActionRequest Request)
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

	Request.ActionId = ActionId;
	if (!Request.TransactionId.IsValid())
	{
		Request.TransactionId = FGuid::NewGuid();
	}

	const FWSActionPreview Preview = StateSubsystem->PreviewAction(Request);
	FWSActionResult Result;
	if (Preview.bCanExecute)
	{
		Result = StateSubsystem->CommitAction(Request);
	}
	else
	{
		Result.ActionId = ActionId;
		Result.TransactionId = Request.TransactionId;
		Result.DialogueAct = Request.DialogueAct;
		Result.PromiseCondition = Request.PromiseCondition;
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
				Result.bPromiseRecorded);
		}
	}
	return Result;
}
