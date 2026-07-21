#include "HUD/WhiteoutHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
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
	UIStringTableAsset = LoadObject<UObject>(
		nullptr,
		TEXT("/Game/WindStation/UI/ST_WhiteoutStation_zh.ST_WhiteoutStation_zh"));
	if (!UIStringTableAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation v0.2: Chinese StringTable asset is missing"));
	}
	InitializeUIFontFamily();
	SystemMessage = FWSPresentationText::UI(TEXT("ui_initial_message"), TEXT("靠近带有蓝色轮廓的设备，按 F 查看行动。")).ToString();
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
	if (bPresentationCaptureOverride)
	{
		UpdateFromState(PresentationCaptureState);
	}
	else if (const UGameInstance* GameInstance = GetGameInstance())
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

	UBorder* TopPanel = MakePanel(Canvas, TEXT("TopPanel"), FAnchors(0, 0, 1, 0), FMargin(22, 18, 22, 116), PanelColor);
	TopText = MakeText(TEXT("TopText"), 21, Body);
	TopPanel->SetContent(TopText);

	UBorder* ObjectivePanel = MakePanel(Canvas, TEXT("ObjectivePanel"), FAnchors(0, 0, 0, 1), FMargin(22, 124, 330, 190), PanelColor);
	ObjectiveText = MakeText(TEXT("ObjectiveText"), 17, Body);
	ObjectivePanel->SetContent(ObjectiveText);

	UBorder* CrewPanel = MakePanel(Canvas, TEXT("CrewPanel"), FAnchors(1, 0, 1, 1), FMargin(-372, 124, 350, 190), PanelColor);
	CrewText = MakeText(TEXT("CrewText"), 16, Body);
	CrewPanel->SetContent(CrewText);

	UBorder* BottomPanel = MakePanel(Canvas, TEXT("BottomPanel"), FAnchors(0, 1, 1, 1), FMargin(22, -188, 22, 166), PanelColor);
	UVerticalBox* BottomBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BottomBox"));
	BottomPanel->SetContent(BottomBox);
	FeedbackText = MakeText(TEXT("FeedbackText"), 20, Body);
	PromptText = MakeText(TEXT("PromptText"), 23, Cyan);
	UTextBlock* HelpText = MakeText(TEXT("HelpText"), 16, FLinearColor(0.56f, 0.66f, 0.75f, 1));
	HelpText->SetText(FWSPresentationText::UI(
		TEXT("ui_help"),
		TEXT("WASD 移动　鼠标观察　F 预览/确认　Q 对话方式　1–6 选择　E 证据板　Enter 结束　R 重开　Esc 暂停/退出")));
	BottomBox->AddChildToVerticalBox(FeedbackText)->SetPadding(FMargin(0, 0, 0, 8));
	BottomBox->AddChildToVerticalBox(PromptText)->SetPadding(FMargin(0, 0, 0, 8));
	BottomBox->AddChildToVerticalBox(HelpText);

	PreviewBorder = MakePanel(Canvas, TEXT("PreviewPanel"), FAnchors(0.16f, 0.10f, 0.84f, 0.90f), FMargin(0), FLinearColor(0.008f, 0.025f, 0.045f, 0.985f));
	UVerticalBox* PreviewBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PreviewBox"));
	PreviewBorder->SetContent(PreviewBox);
	PreviewTitleText = MakeText(TEXT("PreviewTitle"), 28, Cyan);
	PreviewBodyText = MakeText(TEXT("PreviewBody"), 18, Body);
	PreviewFooterText = MakeText(TEXT("PreviewFooter"), 18, Amber);
	PreviewBox->AddChildToVerticalBox(PreviewTitleText)->SetPadding(FMargin(0, 0, 0, 14));
	PreviewBox->AddChildToVerticalBox(PreviewBodyText)->SetPadding(FMargin(0, 0, 0, 16));
	PreviewBox->AddChildToVerticalBox(PreviewFooterText);
	PreviewBorder->SetVisibility(ESlateVisibility::Collapsed);

	EvidenceBorder = MakePanel(Canvas, TEXT("EvidencePanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.006f, 0.018f, 0.033f, 1.0f));
	EvidenceScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("EvidenceScroll"));
	EvidenceText = MakeText(TEXT("EvidenceText"), 18, Body);
	EvidenceScroll->AddChild(EvidenceText);
	EvidenceBorder->SetContent(EvidenceScroll);
	EvidenceBorder->SetVisibility(ESlateVisibility::Collapsed);

	DialogueBorder = MakePanel(Canvas, TEXT("DialoguePanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.010f, 0.023f, 0.040f, 1.0f));
	DialogueText = MakeText(TEXT("DialogueText"), 19, Body);
	DialogueBorder->SetContent(DialogueText);
	DialogueBorder->SetVisibility(ESlateVisibility::Collapsed);

	ResultsBorder = MakePanel(Canvas, TEXT("ResultsPanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.004f, 0.014f, 0.026f, 1.0f));
	ResultsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ResultsScroll"));
	UVerticalBox* ResultsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResultsBox"));
	ResultsScroll->AddChild(ResultsBox);
	ResultsText = MakeText(TEXT("ResultsText"), 18, Body);
	ResultsBox->AddChildToVerticalBox(ResultsText)->SetPadding(FMargin(0, 0, 0, 16));
	const TArray<FText> ScoreLabels = {
		FWSPresentationText::UI(TEXT("score_task"), TEXT("任务质量")),
		FWSPresentationText::UI(TEXT("score_people"), TEXT("人员状态")),
		FWSPresentationText::UI(TEXT("score_reserves"), TEXT("有效储备")),
		FWSPresentationText::UI(TEXT("score_social"), TEXT("社会稳定")),
		FWSPresentationText::UI(TEXT("score_information"), TEXT("信息责任"))};
	const TArray<FLinearColor> ScoreColors = {Cyan, FLinearColor(0.40f, 0.88f, 0.62f), Amber, FLinearColor(0.72f, 0.55f, 1.0f), FLinearColor(0.45f, 0.72f, 1.0f)};
	for (int32 Index = 0; Index < ScoreLabels.Num(); ++Index)
	{
		UTextBlock* ScoreText = MakeText(FName(*FString::Printf(TEXT("ResultScoreText%d"), Index)), 16, Body);
		ScoreText->SetText(ScoreLabels[Index]);
		ResultsBox->AddChildToVerticalBox(ScoreText)->SetPadding(FMargin(0, 5, 0, 3));
		UProgressBar* ScoreBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(*FString::Printf(TEXT("ResultScoreBar%d"), Index)));
		ScoreBar->SetPercent(0.0f);
		ScoreBar->SetFillColorAndOpacity(ScoreColors[Index]);
		ResultsBox->AddChildToVerticalBox(ScoreBar)->SetPadding(FMargin(0, 0, 0, 7));
		ResultScoreTexts.Add(ScoreText);
		ResultScoreBars.Add(ScoreBar);
	}
	ResultsCrewText = MakeText(TEXT("ResultsCrewText"), 16, Body);
	ResultsBox->AddChildToVerticalBox(ResultsCrewText)->SetPadding(FMargin(0, 15, 0, 12));
	ResultsTimelineText = MakeText(TEXT("ResultsTimelineText"), 16, Body);
	ResultsBox->AddChildToVerticalBox(ResultsTimelineText)->SetPadding(FMargin(0, 8, 0, 14));
	ResultsAdviceText = MakeText(TEXT("ResultsAdviceText"), 17, Amber);
	ResultsBox->AddChildToVerticalBox(ResultsAdviceText)->SetPadding(FMargin(0, 8, 0, 24));
	ResultsBorder->SetContent(ResultsScroll);
	ResultsBorder->SetVisibility(ESlateVisibility::Collapsed);

	ComponentGalleryBorder = MakePanel(Canvas, TEXT("ComponentGalleryPanel"), FAnchors(0.015f, 0.015f, 0.985f, 0.985f), FMargin(0), FLinearColor(0.004f, 0.014f, 0.026f, 0.998f));
	UVerticalBox* GalleryBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ComponentGalleryBox"));
	ComponentGalleryBorder->SetContent(GalleryBox);
	UTextBlock* GalleryTitle = MakeText(TEXT("GalleryTitle"), 30, Cyan);
	GalleryTitle->SetText(FWSPresentationText::UI(TEXT("ui_gallery_title"), TEXT("界面控件测试页｜Dev_TestMap")));
	GalleryBox->AddChildToVerticalBox(GalleryTitle)->SetPadding(FMargin(0, 0, 0, 18));
	UTextBlock* GalleryType = MakeText(TEXT("GalleryTypography"), 18, Body);
	GalleryType->SetText(FWSPresentationText::UI(TEXT("ui_gallery_typography"), TEXT("标题 30　正文 18　辅助 15｜Noto Sans SC Regular / Bold")));
	GalleryBox->AddChildToVerticalBox(GalleryType)->SetPadding(FMargin(0, 0, 0, 16));
	UTextBlock* GalleryLabels = MakeText(TEXT("GalleryLabels"), 17, Amber);
	GalleryLabels->SetText(FWSPresentationText::UI(TEXT("ui_gallery_labels"), TEXT("状态标签　［可执行］　［条件不足］　［危机］　［已完成］")));
	GalleryBox->AddChildToVerticalBox(GalleryLabels)->SetPadding(FMargin(0, 0, 0, 14));
	const TArray<float> GalleryProgress = {0.82f, 0.52f, 0.24f};
	const TArray<FLinearColor> GalleryColors = {Cyan, Amber, Danger};
	for (int32 Index = 0; Index < GalleryProgress.Num(); ++Index)
	{
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(*FString::Printf(TEXT("GalleryProgress%d"), Index)));
		Bar->SetPercent(GalleryProgress[Index]);
		Bar->SetFillColorAndOpacity(GalleryColors[Index]);
		GalleryBox->AddChildToVerticalBox(Bar)->SetPadding(FMargin(0, 4, 0, 10));
	}
	UTextBlock* GalleryButtonsLabel = MakeText(TEXT("GalleryButtonsLabel"), 17, Body);
	GalleryButtonsLabel->SetText(FWSPresentationText::UI(TEXT("ui_gallery_buttons"), TEXT("按钮状态｜默认 / 聚焦 / 危险操作")));
	GalleryBox->AddChildToVerticalBox(GalleryButtonsLabel)->SetPadding(FMargin(0, 8, 0, 4));
	MakeButton(GalleryBox, FWSPresentationText::UI(TEXT("ui_gallery_primary"), TEXT("确认行动")), TEXT("GalleryPrimaryButton"));
	MakeButton(GalleryBox, FWSPresentationText::UI(TEXT("ui_gallery_secondary"), TEXT("查看证据")), TEXT("GallerySecondaryButton"));
	UBorder* GalleryToast = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GalleryToast"));
	GalleryToast->SetBrushColor(FLinearColor(0.05f, 0.20f, 0.28f, 1.0f));
	GalleryToast->SetPadding(FMargin(14));
	UTextBlock* GalleryToastText = MakeText(TEXT("GalleryToastText"), 17, Body);
	GalleryToastText->SetText(FWSPresentationText::UI(TEXT("ui_gallery_toast"), TEXT("行动已记录｜行动力 6 → 5｜证据板新增 1 条")));
	GalleryToast->SetContent(GalleryToastText);
	GalleryBox->AddChildToVerticalBox(GalleryToast)->SetPadding(FMargin(0, 14, 0, 0));
	ComponentGalleryBorder->SetVisibility(ESlateVisibility::Collapsed);

	OpeningBorder = MakePanel(Canvas, TEXT("OpeningPanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.003f, 0.010f, 0.020f, 0.94f));
	UTextBlock* OpeningText = MakeText(TEXT("OpeningText"), 31, Body);
	OpeningText->SetJustification(ETextJustify::Center);
	OpeningText->SetText(FWSPresentationText::UI(
		TEXT("ui_opening"),
		TEXT("风雪站：断电前夜\n\n暴风雪正在封锁山区，备用电池只够支撑最后一轮抢修。\n修复发电机，校准室外天线，并把求救信号送出去。\n\n你有 8 点行动力。跨过中段后，备用电池会发生一次故障。\n每个付费行动都会先显示成本、条件与影响；再次按 F 才会确认。\n\nWASD 移动 · 鼠标观察 · F 交互 · E 查看证据 · Esc 暂停/退出\n\n按空格跳过")));
	OpeningBorder->SetContent(OpeningText);

	PauseBorder = MakePanel(Canvas, TEXT("PausePanel"), FAnchors(0.5f, 0.5f), FMargin(-310, -255, 620, 510), FLinearColor(0.004f, 0.014f, 0.026f, 0.995f));
	UVerticalBox* PauseBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseBox"));
	PauseBorder->SetContent(PauseBox);
	UTextBlock* PauseTitle = MakeText(TEXT("PauseTitle"), 36, Cyan);
	PauseTitle->SetText(FWSPresentationText::UI(TEXT("ui_pause"), TEXT("行动暂停")));
	PauseTitle->SetJustification(ETextJustify::Center);
	PauseBox->AddChildToVerticalBox(PauseTitle)->SetPadding(FMargin(0, 0, 0, 30));
	UButton* ResumeButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_resume"), TEXT("继续游戏")), TEXT("ResumeButton"));
	UButton* RestartButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_restart"), TEXT("重新开始")), TEXT("RestartButton"));
	UButton* QuitButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_quit"), TEXT("退出到桌面")), TEXT("QuitButton"));
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

float UWhiteoutHUDWidget::ScoreRatio(const float Value, const float Maximum)
{
	return Maximum > 0.0f ? FMath::Clamp(Value / Maximum, 0.0f, 1.0f) : 0.0f;
}

void UWhiteoutHUDWidget::UpdateFromState(const FWSGameState& State)
{
	const FString Crisis = State.bMidCrisisTriggered
		? FWSPresentationText::UI(TEXT("ui_crisis_triggered"), TEXT("备用电池故障｜仅保留应急负载")).ToString()
		: FWSPresentationText::UI(TEXT("ui_crisis_normal"), TEXT("暴风雪逼近｜电力正在衰减")).ToString();
	const FString TopFormat = FWSPresentationText::UI(
		TEXT("ui_top_format"),
		TEXT("风雪站：断电前夜\n行动力 {0}  {1} / 8　｜　阶段：{2}　｜　{3}")).ToString();
	TopText->SetText(FText::FromString(FString::Format(
		*TopFormat,
		{APCells(State.ActionPoints), State.ActionPoints, FWSPresentationText::PhaseLabel(State.Phase).ToString(), Crisis})));
	TopText->SetColorAndOpacity(FSlateColor(State.ActionPoints <= 4 ? Danger : Body));

	const FString ObjectiveFormat = FWSPresentationText::UI(
		TEXT("ui_objective_format"),
		TEXT("首要目标\n修复发电机　{0} / 2\n校准天线　　{1} / 1\n求救信号　　{2}\n\n现有储备\n燃料 {3}　食品 {4}　药品 {5}\n替代继电器 {6}　保温包 {7}\n证据 {8} 条　厨房供暖 {9}")).ToString();
	ObjectiveText->SetText(FText::FromString(FString::Format(
		*ObjectiveFormat,
		{State.Tasks.GeneratorProgress,
		 State.Tasks.AntennaCalibration,
		 FWSPresentationText::UI(State.Tasks.bSignalSent ? TEXT("ui_sent") : TEXT("ui_not_sent"), State.Tasks.bSignalSent ? TEXT("已发送") : TEXT("未发送")).ToString(),
		 State.Resources.Fuel,
		 State.Resources.Food,
		 State.Resources.Medicine,
		 State.Resources.ReplacementRelay,
		 FWSPresentationText::UI(State.Flags.bHeatPackRevealed ? TEXT("ui_discovered") : TEXT("ui_unknown"), State.Flags.bHeatPackRevealed ? TEXT("已发现") : TEXT("未知")).ToString(),
		 State.Evidence.Num(),
		 FWSPresentationText::UI(State.Flags.bKitchenHeaterIntact ? TEXT("ui_intact") : TEXT("ui_dismantled"), State.Flags.bKitchenHeaterIntact ? TEXT("完好") : TEXT("已拆解")).ToString()})));

	FString Crew = FWSPresentationText::UI(TEXT("ui_crew_header"), TEXT("队员状态｜仅显示等级\n")).ToString();
	const FString CrewLineFormat = FWSPresentationText::UI(TEXT("ui_crew_line"), TEXT("\n{0}\n健康 {1}　体温 {2}　精力 {3}")).ToString();
	const FString CrewTrustFormat = FWSPresentationText::UI(TEXT("ui_crew_trust"), TEXT("　信任 {0}")).ToString();
	for (const EWSCharacterId CharacterId : {EWSCharacterId::Player, EWSCharacterId::GuHeng, EWSCharacterId::YeCheng})
	{
		if (const FWSCharacterState* Character = State.Characters.Find(CharacterId))
		{
			Crew += FString::Format(
				*CrewLineFormat,
				{FWSPresentationText::CharacterName(CharacterId).ToString(),
				 FWSPresentationText::ConditionLevel(Character->Health).ToString(),
				 FWSPresentationText::ConditionLevel(Character->Temperature).ToString(),
				 FWSPresentationText::ConditionLevel(Character->Fatigue).ToString()});
			if (CharacterId != EWSCharacterId::Player)
			{
				Crew += FString::Format(*CrewTrustFormat, {FWSPresentationText::TrustLevel(Character->Trust).ToString()});
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
	FString Copy = FWSPresentationText::UI(TEXT("ui_evidence_title"), TEXT("证据板　　　　　　　　　　　　　　　　　　　按 E 关闭\n\n")).ToString();
	Copy += FWSPresentationText::UI(TEXT("ui_evidence_system"), TEXT("一、系统证据｜设备记录与现场物证\n")).ToString();
	if (State.Evidence.IsEmpty())
	{
		Copy += FWSPresentationText::UI(TEXT("ui_evidence_empty"), TEXT("　尚未取得证据。\n")).ToString();
	}
	for (const FName EvidenceId : State.Evidence)
	{
		Copy += TEXT("　● ") + FWSPresentationText::EvidenceLabel(EvidenceId).ToString() + TEXT("\n");
	}
	Copy += FWSPresentationText::UI(TEXT("ui_evidence_claims"), TEXT("\n二、角色说法｜尚待交叉核验\n")).ToString();
	bool bHasClaims = false;
	for (const TPair<FName, EWSKnowledgeLevel>& Pair : State.PlayerKnowledge)
	{
		if (Pair.Value == EWSKnowledgeLevel::Claimed || Pair.Value == EWSKnowledgeLevel::Suspected)
		{
			bHasClaims = true;
			Copy += FString::Printf(
				TEXT("　%s　｜　%s\n"),
				*FWSPresentationText::FactLabel(Pair.Key).ToString(),
				*FWSPresentationText::KnowledgeLevel(Pair.Value).ToString());
		}
	}
	if (!bHasClaims)
	{
		Copy += FWSPresentationText::UI(TEXT("ui_claims_empty"), TEXT("　当前没有待核验说法。\n")).ToString();
	}
	Copy += FWSPresentationText::UI(TEXT("ui_evidence_confirmed"), TEXT("\n三、已证实事实｜可用于行动判断\n")).ToString();
	bool bHasConfirmed = false;
	for (const TPair<FName, EWSKnowledgeLevel>& Pair : State.PlayerKnowledge)
	{
		if (Pair.Value == EWSKnowledgeLevel::Confirmed)
		{
			bHasConfirmed = true;
			Copy += FString::Printf(TEXT("　%s　｜　%s\n"), *FWSPresentationText::FactLabel(Pair.Key).ToString(), *FWSPresentationText::KnowledgeLevel(Pair.Value).ToString());
		}
	}
	if (!bHasConfirmed)
	{
		Copy += FWSPresentationText::UI(TEXT("ui_confirmed_empty"), TEXT("　尚无已证实事实。\n")).ToString();
	}
	Copy += FWSPresentationText::UI(TEXT("ui_evidence_promises"), TEXT("\n四、承诺追踪｜兑现状态\n")).ToString();
	if (State.Promises.IsEmpty())
	{
		Copy += FWSPresentationText::UI(TEXT("ui_promises_empty"), TEXT("　当前没有承诺。\n")).ToString();
	}
	for (const FWSPromiseRecord& Promise : State.Promises)
	{
		const FName StatusKey = !Promise.bSettled ? TEXT("ui_promise_pending") : Promise.bFulfilled ? TEXT("ui_promise_fulfilled") : TEXT("ui_promise_broken");
		const TCHAR* StatusFallback = !Promise.bSettled ? TEXT("进行中") : Promise.bFulfilled ? TEXT("已兑现") : TEXT("已违背");
		Copy += FString::Printf(TEXT("　%s　｜　%s\n"), *FWSPresentationText::PromiseLabel(Promise.ConditionId).ToString(), *FWSPresentationText::UI(StatusKey, StatusFallback).ToString());
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
	const FString HeaderFormat = FWSPresentationText::UI(
		TEXT("ui_results_header_format"),
		TEXT("行动复盘\n{0}\n{1}\n\n总分 {2} / 100　｜　评级 {3}\n\n最终状态\n发电机 {4} / 2　天线 {5} / 1　信号 {6}　剩余行动力 {7}")).ToString();
	const FString TotalScore = FString::Printf(TEXT("%.1f"), State.Score.Total);
	ResultsText->SetText(FText::FromString(FString::Format(
		*HeaderFormat,
		{FWSPresentationText::EndingTitle(State.Ending).ToString(),
		 FWSPresentationText::EndingSummary(State.Ending).ToString(),
		 TotalScore,
		 State.Score.Rating,
		 State.Tasks.GeneratorProgress,
		 State.Tasks.AntennaCalibration,
		 FWSPresentationText::UI(State.Tasks.bSignalSent ? TEXT("ui_sent") : TEXT("ui_failed"), State.Tasks.bSignalSent ? TEXT("已发送") : TEXT("失败")).ToString(),
		 State.ActionPoints})));

	const TArray<float> Values = {State.Score.TaskQuality, State.Score.People, State.Score.EffectiveReserves, State.Score.SocialStability, State.Score.InformationResponsibility};
	const TArray<float> Maximums = {30.0f, 30.0f, 20.0f, 12.0f, 8.0f};
	const TArray<FName> ScoreIds = {TEXT("task"), TEXT("people"), TEXT("reserves"), TEXT("social"), TEXT("information")};
	const TArray<FText> ScoreLabels = {
		FWSPresentationText::UI(TEXT("score_task"), TEXT("任务质量")),
		FWSPresentationText::UI(TEXT("score_people"), TEXT("人员状态")),
		FWSPresentationText::UI(TEXT("score_reserves"), TEXT("有效储备")),
		FWSPresentationText::UI(TEXT("score_social"), TEXT("社会稳定")),
		FWSPresentationText::UI(TEXT("score_information"), TEXT("信息责任"))};
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		if (ResultScoreBars.IsValidIndex(Index))
		{
			ResultScoreBars[Index]->SetPercent(ScoreRatio(Values[Index], Maximums[Index]));
		}
		if (ResultScoreTexts.IsValidIndex(Index))
		{
			ResultScoreTexts[Index]->SetText(FText::FromString(FString::Printf(
				TEXT("%s　%.1f / %.0f　｜　%s"),
				*ScoreLabels[Index].ToString(), Values[Index], Maximums[Index], *FWSPresentationText::ScoreAttribution(ScoreIds[Index]).ToString())));
		}
	}

	FString Crew = FWSPresentationText::UI(TEXT("ui_results_crew_header"), TEXT("三人最终状态\n")).ToString();
	for (const EWSCharacterId CharacterId : {EWSCharacterId::Player, EWSCharacterId::GuHeng, EWSCharacterId::YeCheng})
	{
		if (const FWSCharacterState* Character = State.Characters.Find(CharacterId))
		{
			Crew += FString::Printf(
				TEXT("　%s　健康 %s｜体温 %s｜精力 %s"),
				*FWSPresentationText::CharacterName(CharacterId).ToString(),
				*FWSPresentationText::ConditionLevel(Character->Health).ToString(),
				*FWSPresentationText::ConditionLevel(Character->Temperature).ToString(),
				*FWSPresentationText::ConditionLevel(Character->Fatigue).ToString());
			if (CharacterId != EWSCharacterId::Player)
			{
				Crew += FString::Printf(TEXT("｜信任 %s"), *FWSPresentationText::TrustLevel(Character->Trust).ToString());
			}
			Crew += TEXT("\n");
		}
	}
	ResultsCrewText->SetText(FText::FromString(Crew));

	FString Timeline = FWSPresentationText::UI(TEXT("ui_results_timeline_header"), TEXT("完整因果时间线\n")).ToString();
	for (int32 Index = 0; Index < State.EventLog.Num(); ++Index)
	{
		const FWSEventRecord& Event = State.EventLog[Index];
		Timeline += FString::Printf(
			TEXT("%02d　%s　行动力 %d → %d%s\n"),
			Event.Index,
			*FWSPresentationText::ActionLabel(Event.ActionId).ToString(),
			Event.APBefore,
			Event.APAfter,
			Event.bCrisisTriggered ? *FWSPresentationText::UI(TEXT("ui_crisis_tag"), TEXT("　［备用电池故障］")).ToString() : TEXT(""));
	}
	if (State.EventLog.IsEmpty())
	{
		Timeline += FWSPresentationText::UI(TEXT("ui_timeline_empty"), TEXT("　没有已提交的行动。\n")).ToString();
	}
	ResultsTimelineText->SetText(FText::FromString(Timeline));
	ResultsAdviceText->SetText(FText::Format(
		FWSPresentationText::UI(TEXT("ui_results_advice_format"), TEXT("下一轮建议\n{0}\n\n按 R 开始新一轮　｜　Esc 打开退出菜单")),
		FWSPresentationText::EndingAdvice(State.Ending)));
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
		const FString Risk = Preview.RiskText.IsEmpty()
			? FWSPresentationText::UI(TEXT("ui_risk_none"), TEXT("未发现额外风险。")).ToString()
			: Preview.RiskText.ToString();
		FString Expected = Preview.PreviewText.ToString();
		if (!Expected.IsEmpty())
		{
			Expected += TEXT("\n");
		}
		Expected += FWSPresentationText::ActionImpact(Preview.ActionId).ToString();
		const FString BodyFormat = FWSPresentationText::UI(
			TEXT("ui_preview_body_format"),
			TEXT("行动成本\n行动力 ×{0}\n\n执行者\n{1}\n\n资源成本\n{2}\n\n可预见风险\n{3}\n\n预期结果\n{4}\n\n当前前置条件满足；确认后立即结算。")).ToString();
		PreviewBodyText->SetText(FText::FromString(FString::Format(
			*BodyFormat,
			{Preview.APCost,
			 FWSPresentationText::ActionExecutor(Preview.ActionId).ToString(),
			 FWSPresentationText::ActionResourceCost(Preview.ActionId).ToString(),
			 Risk,
			 Expected})));
		PreviewFooterText->SetText(FWSPresentationText::UI(TEXT("ui_preview_footer"), TEXT("再次按 F 确认执行　｜　移开视线取消")));
	}
	else
	{
		PreviewTitleText->SetColorAndOpacity(FSlateColor(Danger));
		const FString RejectionFormat = FWSPresentationText::UI(TEXT("ui_rejection_format"), TEXT("现在不能执行\n{0}\n\n怎样改变条件\n{1}")).ToString();
		PreviewBodyText->SetText(FText::FromString(FString::Format(
			*RejectionFormat,
			{FWSPresentationText::ReasonCause(Preview.ReasonCode).ToString(), FWSPresentationText::ReasonNextStep(Preview.ReasonCode).ToString()})));
		PreviewFooterText->SetText(FWSPresentationText::UI(TEXT("ui_rejection_footer"), TEXT("移开视线或按 F 关闭提示")));
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
		const FString CommittedFormat = FWSPresentationText::UI(TEXT("ui_feedback_committed"), TEXT("已执行：{0}　｜　行动力 {1} → {2}")).ToString();
		SystemMessage = FString::Format(*CommittedFormat, {ActionName.ToString(), Result.APBefore, Result.APAfter});
		if (Result.bCrisisTriggered)
		{
			SystemMessage += FWSPresentationText::UI(TEXT("ui_feedback_crisis"), TEXT("　｜　备用电池电压崩溃，应急灯已接管")).ToString();
		}
	}
	else
	{
		const FString RejectedFormat = FWSPresentationText::UI(TEXT("ui_feedback_rejected"), TEXT("未执行：{0}　｜　{1}　｜　{2}")).ToString();
		SystemMessage = FString::Format(
			*RejectedFormat,
			{ActionName.ToString(), FWSPresentationText::ReasonCause(Result.ReasonCode).ToString(), FWSPresentationText::ReasonNextStep(Result.ReasonCode).ToString()});
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
		FWSPresentationText::UI(TEXT("dialogue_ask"), TEXT("询问　｜　获取对方愿意公开的信息")).ToString(),
		FWSPresentationText::UI(TEXT("dialogue_challenge"), TEXT("质疑　｜　用现有证据追问矛盾")).ToString(),
		FWSPresentationText::UI(TEXT("dialogue_promise_heat"), TEXT("承诺：恢复维修间供暖")).ToString(),
		FWSPresentationText::UI(TEXT("dialogue_promise_medicine"), TEXT("承诺：为顾衡保留药品")).ToString(),
		FWSPresentationText::UI(TEXT("dialogue_promise_records"), TEXT("承诺：保存完整维修记录")).ToString(),
		FWSPresentationText::UI(TEXT("dialogue_reassure"), TEXT("安抚　｜　降低压力，争取合作")).ToString()};
	FString Copy = FWSPresentationText::UI(TEXT("ui_dialogue_title"), TEXT("选择对话方式　　　　　　　　　　　　　按 Q 关闭\n\n")).ToString();
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		Copy += FString::Printf(TEXT("%s %d　%s\n\n"), Index == SelectedIndex ? TEXT("▶") : TEXT("　"), Index + 1, *Options[Index]);
	}
	Copy += FWSPresentationText::UI(TEXT("ui_dialogue_footer"), TEXT("选择后看向顾衡或叶澄，按 F 查看这次交谈的成本与影响。所有选项均有明确意图，不会暗中循环。 ")).ToString();
	DialogueText->SetText(FText::FromString(Copy));
}

void UWhiteoutHUDWidget::ResetPresentationCapture()
{
	bPresentationCaptureOverride = false;
	bEvidenceVisible = false;
	bDialogueVisible = false;
	if (EvidenceBorder) EvidenceBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueBorder) DialogueBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (ResultsBorder) ResultsBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (ComponentGalleryBorder) ComponentGalleryBorder->SetVisibility(ESlateVisibility::Collapsed);
	HideActionPreview();
}

void UWhiteoutHUDWidget::SetPresentationCaptureState(const FWSGameState& State)
{
	PresentationCaptureState = State;
	bPresentationCaptureOverride = true;
	UpdateFromState(PresentationCaptureState);
}

void UWhiteoutHUDWidget::ShowEvidenceForCapture()
{
	bEvidenceVisible = true;
	bDialogueVisible = false;
	HideActionPreview();
	if (DialogueBorder) DialogueBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (bPresentationCaptureOverride)
	{
		UpdateEvidence(PresentationCaptureState);
	}
}

void UWhiteoutHUDWidget::ShowComponentGalleryForCapture()
{
	ResetPresentationCapture();
	DismissOpening();
	if (ComponentGalleryBorder)
	{
		ComponentGalleryBorder->SetVisibility(ESlateVisibility::Visible);
	}
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
