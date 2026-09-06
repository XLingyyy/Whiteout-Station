#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dialogue/WSAuthoredDialogueRepository.h"
#include "WSDialoguePanelWidget.generated.h"

class UTextBlock;
class UButton;
class UVerticalBox;
class UScrollBox;
class UMultiLineEditableTextBox;
class UFont;

UCLASS()
class WHITEOUTSTATION_API UWSDialoguePanelWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Build(UFont* Font);
	void Open(FName InAction, EWSDialogueMode InMode);
	void Refresh();
	void FocusInput();
	void ShowReply(const FWSAgentReply& Reply);
	void SetStatus(const FString& Message, bool Busy);
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	UPROPERTY() TObjectPtr<UTextBlock> Header;
	UPROPERTY() TObjectPtr<UTextBlock> Transcript;
	UPROPERTY() TObjectPtr<UTextBlock> Status;
	UPROPERTY() TObjectPtr<UScrollBox> HistoryScroll;
	UPROPERTY() TObjectPtr<UScrollBox> ChoicesScroll;
	UPROPERTY() TObjectPtr<UVerticalBox> ChoicesBox;
	UPROPERTY() TArray<TObjectPtr<UButton>> ChoiceButtons;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> ChoiceLabels;
	UPROPERTY() TObjectPtr<UMultiLineEditableTextBox> Input;
	UPROPERTY() TObjectPtr<UButton> Send;
	UPROPERTY() TObjectPtr<UButton> More;
	UPROPERTY() TObjectPtr<UButton> Offline;
	UPROPERTY() TObjectPtr<UButton> Configure;
	FName Action;
	EWSDialogueMode Mode = EWSDialogueMode::Authored;
	TArray<FWSAuthoredChoice> Choices;
	FString History;
	int32 Page = 0;
	int32 Turns = 0;
	bool bBusy = false;
	void Select(int32 Index);
	void Append(const FString& Text);
	UFUNCTION() void Select0();
	UFUNCTION() void Select1();
	UFUNCTION() void Select2();
	UFUNCTION() void Select3();
	UFUNCTION() void Select4();
	UFUNCTION() void NextPage();
	UFUNCTION() void Submit();
	UFUNCTION() void Leave();
	UFUNCTION() void SwitchOffline();
	UFUNCTION() void OpenConfiguration();
};
