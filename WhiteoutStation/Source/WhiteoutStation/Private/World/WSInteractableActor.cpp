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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	SetActorScale3D(FVector(0.55f, 0.55f, 0.9f));
}

void AWSInteractableActor::Configure(const FName InActionId, const FText& InDisplayName, const FLinearColor InAccentColor)
{
	ActionId = InActionId;
	DisplayName = InDisplayName;
	AccentColor = InAccentColor;
#if WITH_EDITOR
	SetActorLabel(InDisplayName.ToString());
#endif

	if (UMaterialInterface* BaseMaterial = Mesh->GetMaterial(0))
	{
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), AccentColor);
		Mesh->SetMaterial(0, DynamicMaterial);
	}
}

FText AWSInteractableActor::GetInteractionPrompt() const
{
	return FText::Format(FText::FromString(TEXT("[F] {0}")), DisplayName);
}

FWSActionResult AWSInteractableActor::Interact(APawn* InstigatorPawn)
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
