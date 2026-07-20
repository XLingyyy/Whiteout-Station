#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "State/WindStationTypes.h"
#include "WSInteractableActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class WHITEOUTSTATION_API AWSInteractableActor : public AActor
{
	GENERATED_BODY()

public:
	AWSInteractableActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FLinearColor AccentColor = FLinearColor(0.15f, 0.55f, 0.78f, 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Configure(FName InActionId, const FText& InDisplayName, FLinearColor InAccentColor);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetInteractionPrompt() const;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	FWSActionResult Interact(
		APawn* InstigatorPawn,
		EWSDialogueAct DialogueAct = EWSDialogueAct::Ask,
		FName PromiseCondition = NAME_None);
};
