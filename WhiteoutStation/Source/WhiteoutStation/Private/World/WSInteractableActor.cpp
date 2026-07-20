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
		|| ActionId == TEXT("dismantle_kitchen_heater") || ActionId == TEXT("calibrate_antenna")
		|| ActionId == TEXT("send_signal");
	if (bIndustrialSurface)
	{
		if (UMaterialInterface* SurfaceMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/WindStation/Art/Materials/M_WS_RustedMetal.M_WS_RustedMetal")))
		{
			Mesh->SetMaterial(0, SurfaceMaterial);
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
	return FText::Format(FText::FromString(TEXT("[F] {0}")), DisplayName);
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
