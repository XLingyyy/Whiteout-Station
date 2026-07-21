#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "State/WindStationTypes.h"
#include "WhiteoutCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class AWSInteractableActor;
class USoundBase;

UCLASS()
class WHITEOUTSTATION_API AWhiteoutCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AWhiteoutCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeInputContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveForwardAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveBackwardAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveLeftAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveRightAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> EvidenceAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RestartAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SettleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DialogueModeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ContinueAction;

	int32 DialogueModeIndex = 0;
	bool bDialogueMenuVisible = false;
	bool bPreviewCanExecute = false;

	UPROPERTY(Transient)
	TObjectPtr<AWSInteractableActor> PreviewedInteractable;

	UPROPERTY(Transient)
	TObjectPtr<AWSInteractableActor> FocusedInteractable;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> SnowFootstepSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> MetalFootstepSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> ConcreteFootstepSound;

	FVector LastFootstepLocation = FVector::ZeroVector;
	float FootstepTravel = 0.0f;

	void MoveForward(const FInputActionValue& Value);
	void MoveBackward(const FInputActionValue& Value);
	void MoveLeft(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Interact(const FInputActionValue& Value);
	void ToggleEvidence(const FInputActionValue& Value);
	void RestartRun(const FInputActionValue& Value);
	void Settle(const FInputActionValue& Value);
	void CycleDialogueMode(const FInputActionValue& Value);
	void ContinueRun(const FInputActionValue& Value);
	void SelectDialogue1();
	void SelectDialogue2();
	void SelectDialogue3();
	void SelectDialogue4();
	void SelectDialogue5();
	void SelectDialogue6();
	void SelectDialogueIndex(int32 Index);
	void DismissOpening();
	void TogglePauseMenu();
	EWSDialogueAct SelectedDialogueAct() const;
	FName SelectedPromiseCondition() const;
	AWSInteractableActor* FindLookedAtInteractable() const;
	void UpdateFootsteps();
};
