#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "State/WindStationTypes.h"
#include "WSInteractableActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UWSLookAtSkeletalMeshComponent;
class UAnimSequence;
class UMaterialInterface;
class UWSCharacterAssetData;

UCLASS()
class WHITEOUTSTATION_API AWSInteractableActor : public AActor
{
	GENERATED_BODY()

public:
	AWSInteractableActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UBoxComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UWSLookAtSkeletalMeshComponent> CharacterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> InjuryWrap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FLinearColor AccentColor = FLinearColor(0.15f, 0.55f, 0.78f, 1.0f);

	// 角色资源表。留空则回退到旧硬编码路径（兼容现有 v0.4 行为）。
	// 新建 DA_WS_GuHeng / DA_WS_YeCheng 数据资产，填入网格/动画/材质后指到这里，
	// 之后换 VRM/SK 模型就只改数据资产，不用碰 C++。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Character", meta = (DisplayName = "Character Asset"))
	TObjectPtr<UWSCharacterAssetData> CharacterAsset;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Configure(FName InActionId, const FText& InDisplayName, FLinearColor InAccentColor);

	void SetCharacterPreviewMood(bool bHighTrust);
	void SetCharacterPreviewPerformance(FName PerformanceName);
	void SetDialogueLookAtActive(bool bActive);
	void SetInteractionFocused(bool bFocused);
	bool IsCharacterHotspot() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetInteractionPrompt() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FWSActionPreview PreviewInteraction(
		EWSDialogueAct DialogueAct = EWSDialogueAct::Ask,
		FName PromiseCondition = NAME_None) const;

	FWSActionRequest BuildActionRequest(
		EWSDialogueAct DialogueAct = EWSDialogueAct::Ask,
		FName PromiseCondition = NAME_None,
		const FString& PlayerSaid = FString(),
		FGuid DialogueSessionId = FGuid()) const;
	FWSActionPreview PreviewRequest(const FWSActionRequest& Request) const;
	FWSActionResult InteractRequest(APawn* InstigatorPawn, FWSActionRequest Request);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	FWSActionResult Interact(
		APawn* InstigatorPawn,
		EWSDialogueAct DialogueAct = EWSDialogueAct::Ask,
		FName PromiseCondition = NAME_None);

private:
	UPROPERTY(EditAnywhere, Category = "Interaction|Character")
	TObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY(EditAnywhere, Category = "Interaction|Character")
	TObjectPtr<UAnimSequence> GestureAnimation;

	UPROPERTY(EditAnywhere, Category = "Interaction|Character")
	TObjectPtr<UAnimSequence> GuardedAnimation;

	UPROPERTY(EditAnywhere, Category = "Interaction|Character")
	TObjectPtr<UAnimSequence> WorkAnimation;

	UPROPERTY(EditAnywhere, Category = "Interaction|Character|v0.9")
	TObjectPtr<UAnimSequence> WalkAnimation;

	UPROPERTY(EditAnywhere, Category = "Interaction|Character|v0.9")
	TObjectPtr<UAnimSequence> AcknowledgeAnimation;

	UPROPERTY(EditAnywhere, Category = "Interaction|Character|v0.9")
	TObjectPtr<UAnimSequence> ConsiderAnimation;

	UPROPERTY(EditAnywhere, Category = "Interaction|Character|v0.9")
	TObjectPtr<UAnimSequence> ReassureAnimation;

	UPROPERTY(EditAnywhere, Category = "Interaction|Character|v0.9")
	TObjectPtr<UAnimSequence> RejectAnimation;

	UPROPERTY(EditAnywhere, Category = "Interaction|Character|v0.9")
	TObjectPtr<UAnimSequence> AlarmedAnimation;

	// Current VRM imports visually face local +Y. Keep this editable so a
	// future model with a different forward axis needs no runtime code change.
	UPROPERTY(EditAnywhere, Category = "Interaction|Character|v0.9")
	float VisualFacingYawOffsetDegrees = 90.0f;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FocusOverlayMaterial;

	bool bCharacterPresentation = false;
	bool bReactionActive = false;
	bool bMovementActive = false;
	bool bDialogueLookAtActive = false;
	bool bHomeTransformCaptured = false;
	float ReactionUntilTime = 0.0f;
	float NextMovementAllowedTime = 0.0f;
	float CurrentLookAtYaw = 0.0f;
	float CurrentLookAtPitch = 0.0f;
	FVector HomeLocation = FVector::ZeroVector;
	FVector MovementStart = FVector::ZeroVector;
	FVector MovementTarget = FVector::ZeroVector;
	FRotator HomeRotation = FRotator::ZeroRotator;
	EWSNPCReaction PendingReaction = EWSNPCReaction::Neutral;
	EWSNPCMovementIntent ActiveMovementIntent = EWSNPCMovementIntent::Stay;

	void ConfigureCharacterPresentation();
	void CaptureHomeTransform();
	void RestoreLegacyCharacterMaterials();
	void ResolveV09Animations();
	float GetVisualFacingYaw() const;
	FRotator MakeActorRotationFacing(const FVector& WorldDirection) const;
	void PlayCharacterAnimation(UAnimSequence* Animation, bool bLoop = true);
	UAnimSequence* AnimationForReaction(EWSNPCReaction Reaction) const;
	void PlayReaction(EWSNPCReaction Reaction);
	bool TryStartMovement(EWSNPCMovementIntent Intent, EWSNPCReaction FollowupReaction);
	bool IsMovementPathClear(const FVector& Start, const FVector& End) const;
	void FinishMovement();
	void ApplyCharacterState(const FWSGameState& State);

	UFUNCTION()
	void HandleCharacterStateChanged(const FWSGameState& State);

	UFUNCTION()
	void HandleCharacterActionCommitted(const FWSActionResult& Result);

	UFUNCTION()
	void HandleDialogueLine(const FWSAgentReply& Reply);

};
