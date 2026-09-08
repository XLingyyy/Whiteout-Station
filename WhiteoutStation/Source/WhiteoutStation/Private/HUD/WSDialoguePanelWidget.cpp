#include "HUD/WSDialoguePanelWidget.h"
#include "HUD/WhiteoutHUD.h"
#include "HUD/WSUITokens.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Font.h"
#include "Components/Border.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Player/WhiteoutCharacter.h"
#include "State/WindStationStateSubsystem.h"

void UWSDialoguePanelWidget::Build(UFont* Font)
{
	SetIsFocusable(true);
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrush(FSlateRoundedBoxBrush(WSUITokens::V15::Surface, WSUITokens::V15::Radius, WSUITokens::V15::Stroke, 1.0f));
	Background->SetPadding(FMargin(16));
	WidgetTree->RootWidget = Background;
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Background->SetContent(Box);
	const auto Text = [&](int32 Size)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
		T->SetFont(FSlateFontInfo(Font, Size));
		T->SetColorAndOpacity(FSlateColor(WSUITokens::V15::Text));
		T->SetAutoWrapText(true);
		return T;
	};
	const auto StyleButton = [](UButton* B)
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateColorBrush(FLinearColor(0.07f, 0.11f, 0.14f)));
		Style.SetHovered(FSlateColorBrush(FLinearColor(0.12f, 0.21f, 0.25f)));
		Style.SetPressed(FSlateColorBrush(FLinearColor(0.04f, 0.09f, 0.11f)));
		Style.SetDisabled(FSlateColorBrush(FLinearColor(0.06f, 0.07f, 0.08f)));
		Style.SetNormalPadding(FMargin(12, 6)); Style.SetPressedPadding(FMargin(12, 6));
		B->SetStyle(Style);
	};
	Header = Text(20);
	Box->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 12));
	HistoryScroll = WidgetTree->ConstructWidget<UScrollBox>();
	Transcript = Text(16);
	Transcript->SetLineHeightPercentage(1.2f);
	HistoryScroll->AddChild(Transcript);
	Box->AddChildToVerticalBox(HistoryScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ChoicesBox = WidgetTree->ConstructWidget<UVerticalBox>();
	ChoicesScroll = WidgetTree->ConstructWidget<UScrollBox>();
	ChoicesScroll->AddChild(ChoicesBox);
	Box->AddChildToVerticalBox(ChoicesScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	for (int32 I = 0; I < 5; ++I)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		StyleButton(Button);
		UTextBlock* Label = Text(16);
		USizeBox* Minimum = WidgetTree->ConstructWidget<USizeBox>();
		Minimum->SetMinDesiredHeight(44);
		Minimum->SetContent(Label);
		Button->SetContent(Minimum);
		CastChecked<UButtonSlot>(Minimum->Slot)->SetHorizontalAlignment(HAlign_Fill);
		CastChecked<UButtonSlot>(Minimum->Slot)->SetVerticalAlignment(VAlign_Center);
		ChoicesBox->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 3));
		ChoiceButtons.Add(Button); ChoiceLabels.Add(Label);
	}
	ChoiceButtons[0]->OnClicked.AddDynamic(this, &UWSDialoguePanelWidget::Select0);
	ChoiceButtons[1]->OnClicked.AddDynamic(this, &UWSDialoguePanelWidget::Select1);
	ChoiceButtons[2]->OnClicked.AddDynamic(this, &UWSDialoguePanelWidget::Select2);
	ChoiceButtons[3]->OnClicked.AddDynamic(this, &UWSDialoguePanelWidget::Select3);
	ChoiceButtons[4]->OnClicked.AddDynamic(this, &UWSDialoguePanelWidget::Select4);
	Input = WidgetTree->ConstructWidget<UMultiLineEditableTextBox>();
	FTextBlockStyle InputStyle;
	InputStyle.SetFont(FSlateFontInfo(Font, 16));
	InputStyle.SetColorAndOpacity(FSlateColor(FLinearColor(0.91f, 0.93f, 0.95f)));
	Input->WidgetStyle.SetBackgroundImageNormal(FSlateColorBrush(FLinearColor(0.035f, 0.05f, 0.065f)));
	Input->WidgetStyle.SetBackgroundImageHovered(FSlateColorBrush(FLinearColor(0.045f, 0.065f, 0.08f)));
	Input->WidgetStyle.SetBackgroundImageFocused(FSlateColorBrush(FLinearColor(0.055f, 0.085f, 0.105f)));
	Input->WidgetStyle.SetBackgroundImageReadOnly(FSlateColorBrush(FLinearColor(0.035f, 0.04f, 0.05f)));
	Input->WidgetStyle.SetForegroundColor(FLinearColor(0.91f, 0.93f, 0.95f));
	Input->WidgetStyle.SetPadding(FMargin(10));
	Input->SetTextStyle(InputStyle);
	Input->SetHintText(BuildInputHint());
	USizeBox* InputArea = WidgetTree->ConstructWidget<USizeBox>();
	InputArea->SetHeightOverride(96); InputArea->SetContent(Input);
	Box->AddChildToVerticalBox(InputArea)->SetPadding(FMargin(0, 8));
	InputContainer = InputArea;
	Status = Text(14);
	Box->AddChildToVerticalBox(Status)->SetPadding(FMargin(0, 8));
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
	Box->AddChildToVerticalBox(Actions);
	const auto Button = [&](const TCHAR* Label)
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(); StyleButton(B);
		UTextBlock* T = Text(16); T->SetAutoWrapText(false); T->SetText(FText::FromString(Label));
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetMinDesiredHeight(40); Size->SetContent(T); B->SetContent(Size);
		Actions->AddChildToHorizontalBox(B)->SetPadding(FMargin(0, 0, 16, 0)); return B;
	};
	Send = Button(TEXT("发送")); Send->OnClicked.AddDynamic(this, &UWSDialoguePanelWidget::Submit);
	More = Button(TEXT("更多话题")); More->OnClicked.AddDynamic(this, &UWSDialoguePanelWidget::NextPage);
	Offline = Button(TEXT("切换离线对话")); Offline->OnClicked.AddDynamic(this, &UWSDialoguePanelWidget::SwitchOffline);
	Configure = Button(TEXT("配置 AI")); Configure->OnClicked.AddDynamic(this, &UWSDialoguePanelWidget::OpenConfiguration);
	Button(TEXT("离开 [Esc]"))->OnClicked.AddDynamic(this, &UWSDialoguePanelWidget::Leave);
}

void UWSDialoguePanelWidget::Open(FName InAction, EWSDialogueMode InMode)
{
	Action = InAction; Mode = InMode; Page = 0; Turns = 0; History.Reset(); bBusy = false; bPendingConfirmation = false;
	SessionId.Invalidate(); DisplayedEntryCount = INDEX_NONE;
	SetDesiredFocusWidget(Mode == EWSDialogueMode::Online ? Input.Get() : nullptr);
	Input->SetText(FText::GetEmpty());
	const FString Name = Action == TEXT("talk_ye_cheng") ? TEXT("叶澄 · 医生") : TEXT("顾衡 · 工程师");
	const FString ModeText = Mode == EWSDialogueMode::Authored ? TEXT("离线 · 剧本对话")
		: Mode == EWSDialogueMode::Online ? TEXT("在线 · 自由交涉") : TEXT("AI 配置不可用");
	Header->SetText(FText::FromString(Name + TEXT("    ") + ModeText));
	Append(Action == TEXT("talk_ye_cheng") ? TEXT("叶澄：你想说什么？") : TEXT("顾衡：说吧，我听着。"));
	Refresh();
	SetStatus(Mode == EWSDialogueMode::InvalidConfiguration
		? TEXT("AI 配置不可用。可以配置 AI，或切换离线剧本对话。") : TEXT("首次实质回复消耗 1 AP，本次最多 3 轮。"), false);
	if (Mode == EWSDialogueMode::Online) Input->SetKeyboardFocus();
	else SetKeyboardFocus();
}

void UWSDialoguePanelWidget::Refresh()
{
	if (const auto* State = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		const auto Entries = State->GetConversationHistory(Action);
		if (DisplayedEntryCount != Entries.Num())
		{
			DisplayedEntryCount = Entries.Num();
			if (!Entries.IsEmpty())
			{
				History.Reset();
				const FString Name = Action == TEXT("talk_ye_cheng") ? TEXT("叶澄：") : TEXT("顾衡：");
				for (const auto& Entry : Entries)
				{
					if (!History.IsEmpty()) History += TEXT("\n\n");
					History += TEXT("我：") + Entry.PlayerLine + TEXT("\n\n") + Name + Entry.NpcLine;
				}
				Transcript->SetText(FText::FromString(History)); HistoryScroll->ScrollToEnd();
			}
		}
	}
	if (SessionId.IsValid())
		if (const auto* State = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
			if (const auto* Session = State->GetDialogueSessionState(SessionId))
				bPendingConfirmation = Session->PendingCommitment.IsSet();
	const bool Available = Turns < 3 || (Mode == EWSDialogueMode::Online && bPendingConfirmation);
	ChoicesScroll->SetVisibility(Mode == EWSDialogueMode::Authored && Available ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Offline->SetVisibility(Mode != EWSDialogueMode::Authored ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Offline->SetIsEnabled(!bBusy);
	Configure->SetVisibility(Mode == EWSDialogueMode::InvalidConfiguration ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (UWindStationStateSubsystem* State = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
		Choices = State->GetAuthoredDialogueChoices(Action);
	if (Page * PageSize() >= Choices.Num()) Page = 0;
	for (int32 I = 0; I < ChoiceButtons.Num(); ++I)
	{
		const int32 Index = Page * PageSize() + I;
		const bool Visible = Mode == EWSDialogueMode::Authored && Available && I < PageSize() && Choices.IsValidIndex(Index);
		ChoiceButtons[I]->SetVisibility(Visible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ChoiceButtons[I]->SetIsEnabled(!bBusy);
		ChoiceLabels[I]->SetText(Visible ? FText::FromString(Choices[Index].Text) : FText::GetEmpty());
	}
	InputContainer->SetVisibility(Mode == EWSDialogueMode::Online && Available ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Input->SetIsReadOnly(bBusy);
	Send->SetVisibility(InputContainer->GetVisibility()); Send->SetIsEnabled(!bBusy);
	More->SetVisibility(Mode == EWSDialogueMode::Authored && Available && Choices.Num() > 5 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	More->SetIsEnabled(!bBusy);
}

void UWSDialoguePanelWidget::Append(const FString& Text)
{
	if (!History.IsEmpty()) History += TEXT("\n\n");
	History += Text; Transcript->SetText(FText::FromString(History)); HistoryScroll->ScrollToEnd();
}

FText UWSDialoguePanelWidget::BuildInputHint()
{
	return FText::FromString(TEXT("直接说出你想问或协商的事；回车换行，点击发送"));
}

void UWSDialoguePanelWidget::ShowReply(const FWSAgentReply& Reply)
{
	Turns = Reply.DialogueTurnIndex; bBusy = false; bPendingConfirmation = Reply.bPendingConfirmation;
	SessionId = Reply.DialogueSessionId;
	Append((Reply.Speaker == EWSCharacterId::YeCheng ? FString(TEXT("叶澄：")) : FString(TEXT("顾衡："))) + Reply.Utterance);
	SetStatus(Turns >= 3 ? (bPendingConfirmation ? TEXT("三轮已结束，仅可确认或取消待确认事项，不另扣 AP。") : TEXT("本次私聊已结束，请离开。"))
		: FString::Printf(TEXT("本次私聊 1 AP · 本次剩余 %d 轮"), 3 - Turns), false);
	if (Reply.AnswerSource == TEXT("authored_recovery_v15"))
		Status->SetText(FText::FromString(Status->GetText().ToString() + TEXT(" · 本次使用固定台词")));
	if (Turns < 3 || bPendingConfirmation)
		if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn())) Character->ContinueDialogue();
	Refresh();
}

void UWSDialoguePanelWidget::SetStatus(const FString& Message, bool Busy)
{
	bBusy = Busy; Status->SetText(FText::FromString(Message)); Refresh();
}

void UWSDialoguePanelWidget::Select(int32 Index)
{
	Index += Page * PageSize();
	if (Mode != EWSDialogueMode::Authored || bBusy || Turns >= 3 || !Choices.IsValidIndex(Index)) return;
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		const FWSAuthoredChoice Choice = Choices[Index];
		Append(TEXT("我：") + Choice.Text);
		Character->SubmitAuthoredChoice(Choice.ChoiceId);
	}
}
void UWSDialoguePanelWidget::Select0() { Select(0); }
void UWSDialoguePanelWidget::Select1() { Select(1); }
void UWSDialoguePanelWidget::Select2() { Select(2); }
void UWSDialoguePanelWidget::Select3() { Select(3); }
void UWSDialoguePanelWidget::Select4() { Select(4); }
void UWSDialoguePanelWidget::NextPage() { Page = (Page + 1) % FMath::Max(1, FMath::DivideAndRoundUp(Choices.Num(), PageSize())); Refresh(); }
void UWSDialoguePanelWidget::Submit()
{
	if (Mode != EWSDialogueMode::Online || bBusy || (Turns >= 3 && !bPendingConfirmation)) return;
	const FString Text = Input->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty()) return;
	if (Text.Len() > 480) { SetStatus(TEXT("内容过长，请缩短至 480 字以内。"), false); return; }
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		Append(TEXT("我：") + Text); Input->SetText(FText::GetEmpty()); Character->SubmitDialogueText(Text);
	}
}
void UWSDialoguePanelWidget::Leave()
{
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn())) Character->CancelDialogue();
}
FReply UWSDialoguePanelWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape) { Leave(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry, Event);
}

void UWSDialoguePanelWidget::FocusInput()
{
	if (Mode == EWSDialogueMode::Online && (Turns < 3 || bPendingConfirmation)) Input->SetKeyboardFocus();
	else SetKeyboardFocus();
}
void UWSDialoguePanelWidget::SwitchOffline()
{
	if (bBusy) return;
	Mode = EWSDialogueMode::Authored; Page = 0;
	SetDesiredFocusWidget(static_cast<UWidget*>(nullptr));
	Header->SetText(FText::FromString((Action == TEXT("talk_ye_cheng") ? FString(TEXT("叶澄 · 医生")) : FString(TEXT("顾衡 · 工程师"))) + TEXT("    离线 · 剧本对话")));
	SetStatus(TEXT("已切换离线剧本对话；本次已用轮次和 AP 保留。"), false);
	FocusInput();
}
void UWSDialoguePanelWidget::OpenConfiguration()
{
	Leave();
	if (APlayerController* PC = GetOwningPlayer())
		if (AWhiteoutHUD* HUD = Cast<AWhiteoutHUD>(PC->GetHUD())) HUD->OpenDialogueSettings();
}

int32 UWSDialoguePanelWidget::PageSize() const
{
	const int32 Pages = FMath::Max(1, FMath::DivideAndRoundUp(Choices.Num(), 5));
	return FMath::Max(1, FMath::DivideAndRoundUp(Choices.Num(), Pages));
}
