#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "State/WindStationTypes.h"
#include "WSInteractableActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class UMaterialInterface;

UCLASS()
class WHITEOUTSTATION_API AWSInteractableActor : public AActor
{
	GENERATED_BODY()

public:
	AWSInteractableActor();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<USkeletalMeshComponent> CharacterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> InjuryWrap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FLinearColor AccentColor = FLinearColor(0.15f, 0.55f, 0.78f, 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Configure(FName InActionId, const FText& InDisplayName, FLinearColor InAccentColor);

	void SetCharacterPreviewMood(bool bHighTrust);
	void SetInteractionFocused(bool bFocused);
	bool IsCharacterHotspot() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetInteractionPrompt() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FWSActionPreview PreviewInteraction(
		EWSDialogueAct DialogueAct = EWSDialogueAct::Ask,
		FName PromiseCondition = NAME_None) const;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	FWSActionResult Interact(
		APawn* InstigatorPawn,
		EWSDialogueAct DialogueAct = EWSDialogueAct::Ask,
		FName PromiseCondition = NAME_None);

private:
	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY()
	TObjectPtr<UAnimSequence> GestureAnimation;

	UPROPERTY()
	TObjectPtr<UAnimSequence> GuardedAnimation;

	UPROPERTY()
	TObjectPtr<UAnimSequence> WorkAnimation;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FocusOverlayMaterial;

	bool bCharacterPresentation = false;
	bool bReactionActive = false;
	float ReactionUntilTime = 0.0f;

	void ConfigureCharacterPresentation();
	void PlayCharacterAnimation(UAnimSequence* Animation, bool bLoop = true);
	void ApplyCharacterState(const FWSGameState& State);

	UFUNCTION()
	void HandleCharacterStateChanged(const FWSGameState& State);

	UFUNCTION()
	void HandleCharacterActionCommitted(const FWSActionResult& Result);

	FWSActionRequest BuildRequest(EWSDialogueAct DialogueAct, FName PromiseCondition) const;
};
