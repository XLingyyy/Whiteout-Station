#include "HUD/WhiteoutHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/FontFace.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Presentation/WSPresentationText.h"
#include "State/WindStationStateSubsystem.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor PanelColor(0.008f, 0.018f, 0.032f, 0.94f);
	const FLinearColor Cyan(0.26f, 0.80f, 1.0f, 1.0f);
	const FLinearColor Amber(1.0f, 0.68f, 0.20f, 1.0f);
	const FLinearColor Danger(1.0f, 0.21f, 0.10f, 1.0f);
	const FLinearColor Body(0.84f, 0.89f, 0.94f, 1.0f);
}

void UWhiteoutHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	InitializeUIFontFamily();
	BuildWidgetTree();
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: native UMG widget tree initialized"));
}

void UWhiteoutHUDWidget::InitializeUIFontFamily()
{
	UFontFace* RegularFace = LoadObject<UFontFace>(
		nullptr,
		TEXT("/Game/WindStation/UI/Fonts/FF_NotoSansSC_Regular.FF_NotoSansSC_Regular"));
	UFontFace* BoldFace = LoadObject<UFontFace>(
		nullptr,
		TEXT("/Game/WindStation/UI/Fonts/FF_NotoSansSC_Bold.FF_NotoSansSC_Bold"));
	if (!RegularFace || !BoldFace)
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation v0.2: Noto Sans SC font faces are missing"));
		return;
	}
	UIFontFamily = NewObject<UFont>(this, TEXT("WhiteoutRuntimeUIFont"));
	UIFontFamily->FontCacheType = EFontCacheType::Runtime;
	FCompositeFont& CompositeFont = UIFontFamily->GetMutableInternalCompositeFont();
	CompositeFont.DefaultTypeface.Fonts.Reset();
	FTypefaceEntry& RegularEntry = CompositeFont.DefaultTypeface.Fonts.Emplace_GetRef(TEXT("Regular"));
	RegularEntry.Font = FFontData(RegularFace);
	FTypefaceEntry& BoldEntry = CompositeFont.DefaultTypeface.Fonts.Emplace_GetRef(TEXT("Bold"));
	BoldEntry.Font = FFontData(BoldFace);
#if WITH_EDITORONLY_DATA
	CompositeFont.MakeDirty();
#endif
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: Noto Sans SC runtime font family initialized"));
}

void UWhiteoutHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: native UMG widget added to viewport"));
}

void UWhiteoutHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	OpeningElapsed += InDeltaTime;
	if (OpeningBorder && OpeningElapsed > 11.0f)
	{
		OpeningBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			UpdateFromState(StateSubsystem->GetStateSnapshot());
		}
	}
}

void UWhiteoutHUDWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRoot"));
	WidgetTree->RootWidget = Canvas;

	UBorder* TopPanel = MakePanel(Canvas, TEXT("TopPanel"), FAnchors(0, 0, 1, 0), FMargin(22, 18, 22, 96), PanelColor);
	TopText = MakeText(TEXT("TopText"), 21, Body);
	TopPanel->SetContent(TopText);

	UBorder* ObjectivePanel = MakePanel(Canvas, TEXT("ObjectivePanel"), FAnchors(0, 0, 0, 1), FMargin(22, 132, 330, 220), PanelColor);
	ObjectiveText = MakeText(TEXT("ObjectiveText"), 17, Body);
	ObjectivePanel->SetContent(ObjectiveText);

	UBorder* CrewPanel = MakePanel(Canvas, TEXT("CrewPanel"), FAnchors(1, 0, 1, 1), FMargin(-372, 132, 350, 220), PanelColor);
	CrewText = MakeText(TEXT("CrewText"), 16, Body);
	CrewPanel->SetContent(CrewText);

	UBorder* BottomPanel = MakePanel(Canvas, TEXT("BottomPanel"), FAnchors(0, 1, 1, 1), FMargin(22, -188, 22, 166), PanelColor);
	UVerticalBox* BottomBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BottomBox"));
	BottomPanel->SetContent(BottomBox);
	FeedbackText = MakeText(TEXT("FeedbackText"), 20, Body);
	PromptText = MakeText(TEXT("PromptText"), 23, Cyan);
	UTextBlock* HelpText = MakeText(TEXT("HelpText"), 16, FLinearColor(0.56f, 0.66f, 0.75f, 1));
	HelpText->SetText(FText::FromString(TEXT("WASD 移动　鼠标观察　F 预览/确认　Q 对话方式　1–6 选择　E 证据板　Enter 结束　R 重开　Esc 暂停/退出")));
	BottomBox->AddChildToVerticalBox(FeedbackText)->SetPadding(FMargin(0, 0, 0, 8));
	BottomBox->AddChildToVerticalBox(PromptText)->SetPadding(FMargin(0, 0, 0, 8));
	BottomBox->AddChildToVerticalBox(HelpText);

	PreviewBorder = MakePanel(Canvas, TEXT("PreviewPanel"), FAnchors(0.5f, 0.5f), FMargin(-380, -230, 760, 460), FLinearColor(0.008f, 0.025f, 0.045f, 0.985f));
	UVerticalBox* PreviewBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PreviewBox"));
	PreviewBorder->SetContent(PreviewBox);
	PreviewTitleText = MakeText(TEXT("PreviewTitle"), 32, Cyan);
	PreviewBodyText = MakeText(TEXT("PreviewBody"), 22, Body);
	PreviewFooterText = MakeText(TEXT("PreviewFooter"), 20, Amber);
	PreviewBox->AddChildToVerticalBox(PreviewTitleText)->SetPadding(FMargin(0, 0, 0, 24));
	PreviewBox->AddChildToVerticalBox(PreviewBodyText)->SetPadding(FMargin(0, 0, 0, 28));
	PreviewBox->AddChildToVerticalBox(PreviewFooterText);
	PreviewBorder->SetVisibility(ESlateVisibility::Collapsed);

	EvidenceBorder = MakePanel(Canvas, TEXT("EvidencePanel"), FAnchors(0.5f, 0.5f), FMargin(-520, -340, 1040, 680), FLinearColor(0.006f, 0.018f, 0.033f, 0.99f));
	EvidenceText = MakeText(TEXT("EvidenceText"), 19, Body);
	EvidenceBorder->SetContent(EvidenceText);
	EvidenceBorder->SetVisibility(ESlateVisibility::Collapsed);

	DialogueBorder = MakePanel(Canvas, TEXT("DialoguePanel"), FAnchors(0.5f, 0.5f), FMargin(-390, -270, 780, 540), FLinearColor(0.010f, 0.023f, 0.040f, 0.99f));
	DialogueText = MakeText(TEXT("DialogueText"), 22, Body);
	DialogueBorder->SetContent(DialogueText);
	DialogueBorder->SetVisibility(ESlateVisibility::Collapsed);

	ResultsBorder = MakePanel(Canvas, TEXT("ResultsPanel"), FAnchors(0.5f, 0.5f), FMargin(-540, -350, 1080, 700), FLinearColor(0.004f, 0.014f, 0.026f, 0.995f));
	ResultsText = MakeText(TEXT("ResultsText"), 19, Body);
	ResultsBorder->SetContent(ResultsText);
	ResultsBorder->SetVisibility(ESlateVisibility::Collapsed);

	OpeningBorder = MakePanel(Canvas, TEXT("OpeningPanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.003f, 0.010f, 0.020f, 0.94f));
	UTextBlock* OpeningText = MakeText(TEXT("OpeningText"), 31, Body);
	OpeningText->SetJustification(ETextJustify::Center);
	OpeningText->SetText(FText::FromString(
		TEXT("风雪站：断电前夜\n\n暴风雪正在封锁山区，备用电池只够支撑最后一轮抢修。\n修复发电机，校准室外天线，并把求救信号送出去。\n\n你有 8 点行动力。跨过中段后，备用电池会发生一次故障。\n每个付费行动都会先显示成本、条件与影响；再次按 F 才会确认。\n\nWASD 移动 · 鼠标观察 · F 交互 · E 查看证据 · Esc 暂停/退出\n\n按空格跳过")));
	OpeningBorder->SetContent(OpeningText);

	PauseBorder = MakePanel(Canvas, TEXT("PausePanel"), FAnchors(0.5f, 0.5f), FMargin(-310, -255, 620, 510), FLinearColor(0.004f, 0.014f, 0.026f, 0.995f));
	UVerticalBox* PauseBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseBox"));
	PauseBorder->SetContent(PauseBox);
	UTextBlock* PauseTitle = MakeText(TEXT("PauseTitle"), 36, Cyan);
	PauseTitle->SetText(FText::FromString(TEXT("行动暂停")));
	PauseTitle->SetJustification(ETextJustify::Center);
	PauseBox->AddChildToVerticalBox(PauseTitle)->SetPadding(FMargin(0, 0, 0, 30));
	UButton* ResumeButton = MakeButton(PauseBox, FText::FromString(TEXT("继续游戏")), TEXT("ResumeButton"));
	UButton* RestartButton = MakeButton(PauseBox, FText::FromString(TEXT("重新开始")), TEXT("RestartButton"));
	UButton* QuitButton = MakeButton(PauseBox, FText::FromString(TEXT("退出到桌面")), TEXT("QuitButton"));
	ResumeButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ResumeGame);
	RestartButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::RestartGame);
	QuitButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::QuitGame);
	PauseBorder->SetVisibility(ESlateVisibility::Collapsed);
}

UTextBlock* UWhiteoutHUDWidget::MakeText(const FName Name, const int32 Size, const FLinearColor& Color, const bool bWrap)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	TextBlock->SetFont(UIFont(Size));
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetAutoWrapText(bWrap);
	TextBlock->SetLineHeightPercentage(1.18f);
	return TextBlock;
}

UBorder* UWhiteoutHUDWidget::MakePanel(
	UCanvasPanel* Canvas,
	const FName Name,
	const FAnchors& Anchors,
	const FMargin& Offsets,
	const FLinearColor& Color)
{
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	Border->SetBrushColor(Color);
	Border->SetPadding(FMargin(20));
	UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Border);
	CanvasSlot->SetAnchors(Anchors);
	CanvasSlot->SetOffsets(Offsets);
	return Border;
}

UButton* UWhiteoutHUDWidget::MakeButton(UVerticalBox* Box, const FText& Label, const FName Name)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Button->SetBackgroundColor(FLinearColor(0.06f, 0.16f, 0.23f, 1.0f));
	UTextBlock* LabelText = MakeText(FName(*(Name.ToString() + TEXT("Label"))), 24, Body);
	LabelText->SetText(Label);
	LabelText->SetJustification(ETextJustify::Center);
	Button->SetContent(LabelText);
	Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(16, 9));
	return Button;
}

FSlateFontInfo UWhiteoutHUDWidget::UIFont(const int32 Size, const bool bBold) const
{
	if (UIFontFamily)
	{
		return FSlateFontInfo(UIFontFamily, Size, bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
	}
	UE_LOG(LogTemp, Warning, TEXT("WhiteoutStation v0.2: UI font family is unavailable"));
	return FSlateFontInfo(FCoreStyle::GetDefaultFont(), Size);
}

FString UWhiteoutHUDWidget::APCells(const int32 Remaining)
{
	FString Cells;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Cells += Index < Remaining ? TEXT("■ ") : TEXT("□ ");
	}
	return Cells;
}

void UWhiteoutHUDWidget::UpdateFromState(const FWSGameState& State)
{
	const FString Crisis = State.bMidCrisisTriggered ? TEXT("备用电池故障｜仅保留应急负载") : TEXT("暴风雪逼近｜电力正在衰减");
	TopText->SetText(FText::FromString(FString::Printf(
		TEXT("风雪站：断电前夜\n行动力 %s  %d / 8　｜　阶段：%s　｜　%s"),
		*APCells(State.ActionPoints),
		State.ActionPoints,
		*FWSPresentationText::PhaseLabel(State.Phase).ToString(),
		*Crisis)));
	TopText->SetColorAndOpacity(FSlateColor(State.ActionPoints <= 4 ? Danger : Body));

	ObjectiveText->SetText(FText::FromString(FString::Printf(
		TEXT("首要目标\n修复发电机　%d / 2\n校准天线　　%d / 1\n求救信号　　%s\n\n现有储备\n燃料 %d　食品 %d　药品 %d\n替代继电器 %d　保温包 %s\n证据 %d 条　厨房供暖 %s"),
		State.Tasks.GeneratorProgress,
		State.Tasks.AntennaCalibration,
		State.Tasks.bSignalSent ? TEXT("已发送") : TEXT("未发送"),
		State.Resources.Fuel,
		State.Resources.Food,
		State.Resources.Medicine,
		State.Resources.ReplacementRelay,
		State.Flags.bHeatPackRevealed ? TEXT("已发现") : TEXT("未知"),
		State.Evidence.Num(),
		State.Flags.bKitchenHeaterIntact ? TEXT("完好") : TEXT("已拆解"))));

	FString Crew = TEXT("队员状态｜仅显示等级\n");
	for (const EWSCharacterId CharacterId : {EWSCharacterId::Player, EWSCharacterId::GuHeng, EWSCharacterId::YeCheng})
	{
		if (const FWSCharacterState* Character = State.Characters.Find(CharacterId))
		{
			Crew += FString::Printf(
				TEXT("\n%s\n健康 %s　体温 %s　精力 %s"),
				*FWSPresentationText::CharacterName(CharacterId).ToString(),
				*FWSPresentationText::ConditionLevel(Character->Health).ToString(),
				*FWSPresentationText::ConditionLevel(Character->Temperature).ToString(),
				*FWSPresentationText::ConditionLevel(Character->Fatigue).ToString());
			if (CharacterId != EWSCharacterId::Player)
			{
				Crew += FString::Printf(TEXT("　信任 %s"), *FWSPresentationText::TrustLevel(Character->Trust).ToString());
			}
			Crew += TEXT("\n");
		}
	}
	CrewText->SetText(FText::FromString(Crew));
	FeedbackText->SetText(FText::FromString(SystemMessage));
	PromptText->SetText(InteractionPrompt);
	UpdateEvidence(State);
	UpdateResults(State);
}

void UWhiteoutHUDWidget::UpdateEvidence(const FWSGameState& State)
{
	if (!EvidenceText)
	{
		return;
	}
	FString Copy = TEXT("证据板　　　　　　　　　　　　　　　　　　　按 E 关闭\n\n一、已取得的现场证据\n");
	if (State.Evidence.IsEmpty())
	{
		Copy += TEXT("　尚未取得证据。\n");
	}
	for (const FName EvidenceId : State.Evidence)
	{
		Copy += TEXT("　● ") + FWSPresentationText::EvidenceLabel(EvidenceId).ToString() + TEXT("\n");
	}
	Copy += TEXT("\n二、事实判断\n");
	for (const TPair<FName, EWSKnowledgeLevel>& Pair : State.PlayerKnowledge)
	{
		if (Pair.Value != EWSKnowledgeLevel::Unknown)
		{
			Copy += FString::Printf(
				TEXT("　%s　｜　%s\n"),
				*FWSPresentationText::FactLabel(Pair.Key).ToString(),
				*FWSPresentationText::KnowledgeLevel(Pair.Value).ToString());
		}
	}
	Copy += TEXT("\n三、已记录的承诺\n");
	if (State.Promises.IsEmpty())
	{
		Copy += TEXT("　当前没有承诺。\n");
	}
	for (const FWSPromiseRecord& Promise : State.Promises)
	{
		const TCHAR* Status = !Promise.bSettled ? TEXT("待兑现") : Promise.bFulfilled ? TEXT("已兑现") : TEXT("已违背");
		Copy += FString::Printf(TEXT("　%s　｜　%s\n"), *FWSPresentationText::PromiseLabel(Promise.ConditionId).ToString(), Status);
	}
	EvidenceText->SetText(FText::FromString(Copy));
	EvidenceBorder->SetVisibility(bEvidenceVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UWhiteoutHUDWidget::UpdateResults(const FWSGameState& State)
{
	const bool bResults = State.Phase == EWSGamePhase::Results;
	ResultsBorder->SetVisibility(bResults ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bResults)
	{
		return;
	}
	FString Timeline;
	const int32 FirstEvent = FMath::Max(0, State.EventLog.Num() - 7);
	for (int32 Index = FirstEvent; Index < State.EventLog.Num(); ++Index)
	{
		const FWSEventRecord& Event = State.EventLog[Index];
		Timeline += FString::Printf(
			TEXT("%02d　%s　行动力 %d → %d%s\n"),
			Event.Index,
			*FWSPresentationText::ActionLabel(Event.ActionId).ToString(),
			Event.APBefore,
			Event.APAfter,
			Event.bCrisisTriggered ? TEXT("　［备用电池故障］") : TEXT(""));
	}
	ResultsText->SetText(FText::FromString(FString::Printf(
		TEXT("行动复盘\n%s\n%s\n\n总分 %.1f / 100　｜　评级 %s\n任务质量 %.1f / 30　人员状态 %.1f / 30　有效储备 %.1f / 20\n社会稳定 %.1f / 12　信息责任 %.1f / 8\n\n最终状态\n发电机 %d / 2　天线 %d / 1　信号 %s　剩余行动力 %d\n\n决策时间线\n%s\n按 R 开始新一轮　｜　Esc 打开退出菜单"),
		*FWSPresentationText::EndingTitle(State.Ending).ToString(),
		*FWSPresentationText::EndingSummary(State.Ending).ToString(),
		State.Score.Total,
		*State.Score.Rating,
		State.Score.TaskQuality,
		State.Score.People,
		State.Score.EffectiveReserves,
		State.Score.SocialStability,
		State.Score.InformationResponsibility,
		State.Tasks.GeneratorProgress,
		State.Tasks.AntennaCalibration,
		State.Tasks.bSignalSent ? TEXT("已发送") : TEXT("失败"),
		State.ActionPoints,
		*Timeline)));
}

void UWhiteoutHUDWidget::SetInteractionPrompt(const FText& Prompt)
{
	InteractionPrompt = Prompt;
}

void UWhiteoutHUDWidget::ShowActionPreview(const FText& ActionName, const FWSActionPreview& Preview)
{
	PreviewBorder->SetVisibility(ESlateVisibility::Visible);
	PreviewTitleText->SetText(ActionName);
	if (Preview.bCanExecute)
	{
		PreviewTitleText->SetColorAndOpacity(FSlateColor(Cyan));
		PreviewBodyText->SetText(FText::FromString(FString::Printf(
			TEXT("行动成本\n消耗 %d 点行动力\n\n预期影响\n%s\n\n规则确认\n当前前置条件满足。提交后将立即结算环境与人物状态。"),
			Preview.APCost,
			*FWSPresentationText::ActionImpact(Preview.ActionId).ToString())));
		PreviewFooterText->SetText(FText::FromString(TEXT("再次按 F 确认执行　｜　移开视线取消")));
	}
	else
	{
		PreviewTitleText->SetColorAndOpacity(FSlateColor(Danger));
		PreviewBodyText->SetText(FText::FromString(FString::Printf(
			TEXT("现在不能执行\n%s\n\n怎样改变条件\n%s"),
			*FWSPresentationText::ReasonCause(Preview.ReasonCode).ToString(),
			*FWSPresentationText::ReasonNextStep(Preview.ReasonCode).ToString())));
		PreviewFooterText->SetText(FText::FromString(TEXT("移开视线或按 F 关闭提示")));
	}
}

void UWhiteoutHUDWidget::HideActionPreview()
{
	if (PreviewBorder)
	{
		PreviewBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWhiteoutHUDWidget::SetActionFeedback(const FText& ActionName, const FWSActionResult& Result, const FWSActionPreview& Preview)
{
	HideActionPreview();
	if (Result.bCommitted)
	{
		SystemMessage = FString::Printf(TEXT("已执行：%s　｜　行动力 %d → %d"), *ActionName.ToString(), Result.APBefore, Result.APAfter);
		if (Result.bCrisisTriggered)
		{
			SystemMessage += TEXT("　｜　备用电池电压崩溃，应急灯已接管");
		}
	}
	else
	{
		SystemMessage = FString::Printf(
			TEXT("未执行：%s　｜　%s　｜　%s"),
			*ActionName.ToString(),
			*FWSPresentationText::ReasonCause(Result.ReasonCode).ToString(),
			*FWSPresentationText::ReasonNextStep(Result.ReasonCode).ToString());
	}
}

void UWhiteoutHUDWidget::ToggleEvidence()
{
	bEvidenceVisible = !bEvidenceVisible;
	if (bEvidenceVisible)
	{
		HideActionPreview();
		DialogueBorder->SetVisibility(ESlateVisibility::Collapsed);
		bDialogueVisible = false;
	}
}

void UWhiteoutHUDWidget::ShowDialogueMenu(const int32 SelectedIndex, const bool bVisible)
{
	bDialogueVisible = bVisible;
	DialogueBorder->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		return;
	}
	HideActionPreview();
	bEvidenceVisible = false;
	const TArray<FString> Options = {
		TEXT("询问　｜　获取对方愿意公开的信息"),
		TEXT("质疑　｜　用现有证据追问矛盾"),
		TEXT("承诺：恢复维修间供暖"),
		TEXT("承诺：为顾衡保留药品"),
		TEXT("承诺：保存完整维修记录"),
		TEXT("安抚　｜　降低压力，争取合作")};
	FString Copy = TEXT("选择对话方式　　　　　　　　　　　　　按 Q 关闭\n\n");
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		Copy += FString::Printf(TEXT("%s %d　%s\n\n"), Index == SelectedIndex ? TEXT("▶") : TEXT("　"), Index + 1, *Options[Index]);
	}
	Copy += TEXT("选择后看向顾衡或叶澄，按 F 查看这次交谈的成本与影响。所有选项均有明确意图，不会暗中循环。 ");
	DialogueText->SetText(FText::FromString(Copy));
}

void UWhiteoutHUDWidget::SetSystemMessage(const FString& Message)
{
	SystemMessage = Message;
}

void UWhiteoutHUDWidget::DismissOpening()
{
	if (OpeningBorder)
	{
		OpeningBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UWhiteoutHUDWidget::IsPauseMenuVisible() const
{
	return PauseBorder && PauseBorder->GetVisibility() == ESlateVisibility::Visible;
}

void UWhiteoutHUDWidget::TogglePauseMenu()
{
	if (IsPauseMenuVisible())
	{
		ResumeGame();
		return;
	}
	DismissOpening();
	PauseBorder->SetVisibility(ESlateVisibility::Visible);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(true);
		PlayerController->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(PauseBorder->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
}

void UWhiteoutHUDWidget::ResumeGame()
{
	PauseBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(false);
		PlayerController->bShowMouseCursor = false;
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}

void UWhiteoutHUDWidget::RestartGame()
{
	if (UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		StateSubsystem->NewGame();
	}
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(false);
	}
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), false);
}

void UWhiteoutHUDWidget::QuitGame()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
	}
}
