#include "World/WSInteractableActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameModeBase.h"
#include "HUD/WhiteoutHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "State/WindStationStateSubsystem.h"
#include "UObject/ConstructorHelpers.h"

AWSInteractableActor::AWSInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetGenerateOverlapEvents(false);
	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(Mesh);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetVisibility(false);
	HeadMesh->SetCastShadow(true);

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

void AWSInteractableActor::Configure(const FName InActionId, const FText& InDisplayName, const FLinearColor InAccentColor)
{
	ActionId = InActionId;
	DisplayName = InDisplayName;
	AccentColor = InAccentColor;
	const bool bCharacter = ActionId == TEXT("talk_gu_heng") || ActionId == TEXT("talk_ye_cheng");
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
		if (UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
		{
			Mesh->SetStaticMesh(CylinderMesh);
		}
		HeadMesh->SetVisibility(true);
		HeadMesh->SetRelativeLocation(FVector(0, 0, 62.0f));
		HeadMesh->SetRelativeScale3D(FVector(0.72f, 0.72f, 0.18f));
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
	if (UMaterialInterface* BaseMaterial = Mesh->GetMaterial(0))
	{
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), AccentColor);
		Mesh->SetMaterial(0, DynamicMaterial);
		if (bCharacter)
		{
			UMaterialInstanceDynamic* HeadMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			HeadMaterial->SetVectorParameterValue(TEXT("Color"), AccentColor * 0.72f);
			HeadMesh->SetMaterial(0, HeadMaterial);
		}
	}
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
			HUD->SetActionFeedback(DisplayName, Result, Preview);
		}
	}
	return Result;
}
