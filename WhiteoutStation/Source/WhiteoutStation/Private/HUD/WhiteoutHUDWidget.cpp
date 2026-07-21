#include "HUD/WhiteoutHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/FontFace.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Flow/WhiteoutGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Presentation/WSPresentationText.h"
#include "Player/WhiteoutCharacter.h"
#include "State/WindStationStateSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Sound/SoundBase.h"

namespace
{
	const FLinearColor PanelColor(0.035f, 0.071f, 0.102f, 0.82f);
	const FLinearColor DeepPanel(0.020f, 0.035f, 0.050f, 0.91f);
	const FLinearColor Cyan(0.49f, 0.71f, 0.84f, 1.0f);
	const FLinearColor Amber(0.949f, 0.549f, 0.157f, 1.0f);
	const FLinearColor Danger(0.851f, 0.329f, 0.302f, 1.0f);
	const FLinearColor Body(0.953f, 0.961f, 0.969f, 1.0f);
	const FLinearColor Secondary(0.72f, 0.76f, 0.79f, 1.0f);

	FString ButtonIconName(const FName ButtonName)
	{
		const TMap<FName, FString> Icons = {
			{TEXT("ResumeButton"), TEXT("I_Menu_Continue")},
			{TEXT("SaveButton"), TEXT("I_Menu_Save")},
			{TEXT("LoadButton"), TEXT("I_Menu_Load")},
			{TEXT("SettingsButton"), TEXT("I_Menu_Settings")},
			{TEXT("HelpButton"), TEXT("I_Menu_Help")},
			{TEXT("RestartButton"), TEXT("I_Menu_Restart")},
			{TEXT("MainMenuButton"), TEXT("I_Menu_Main")},
			{TEXT("QuitButton"), TEXT("I_Menu_Main")}};
		if (const FString* Found = Icons.Find(ButtonName))
		{
			return *Found;
		}
		return FString();
	}
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
	UIHoverSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/WindStation/Audio/UI/S_UIHover_Original.S_UIHover_Original"));
	UIConfirmSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/WindStation/Audio/UI/S_UIConfirm_Original.S_UIConfirm_Original"));
	UIRejectSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/WindStation/Audio/UI/S_UIReject_Original.S_UIReject_Original"));
	UIPromiseSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/WindStation/Audio/UI/S_UIPromise_Original.S_UIPromise_Original"));
	InkBrushTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/WindStation/UI/v03/Textures/T_UI_InkBrush.T_UI_InkBrush"));
	PlayerPortraitTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/WindStation/UI/v03/Portraits/P_PlayerSilhouette.P_PlayerSilhouette"));
	GuHengPortraitTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/WindStation/UI/v03/Portraits/P_GuHeng.P_GuHeng"));
	YeChengPortraitTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/WindStation/UI/v03/Portraits/P_YeCheng.P_YeCheng"));
	SystemMessage = FWSPresentationText::UI(TEXT("ui_initial_message"), TEXT("靠近带有白色轮廓的设备，按 F 查看行动。")).ToString();
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
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			StateSubsystem->OnDialogueLine.AddUniqueDynamic(this, &UWhiteoutHUDWidget::HandleDialogueLine);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: native UMG widget added to viewport"));
}

void UWhiteoutHUDWidget::NativeDestruct()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			StateSubsystem->OnDialogueLine.RemoveDynamic(this, &UWhiteoutHUDWidget::HandleDialogueLine);
		}
	}
	Super::NativeDestruct();
}

void UWhiteoutHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	OpeningElapsed += InDeltaTime;
	if (OpeningBorder && OpeningBorder->GetVisibility() == ESlateVisibility::Visible)
	{
		const int32 Stage = OpeningElapsed < 2.2f ? 0 : OpeningElapsed < 6.8f ? 1 : OpeningElapsed < 10.8f ? 2 : 3;
		if (Stage != ActiveOpeningStage)
		{
			ApplyOpeningStage(Stage);
		}
		if (OpeningElapsed > 14.0f)
		{
			DismissOpening();
		}
	}
	if (ToastRemaining > 0.0f && ToastBorder)
	{
		ToastRemaining = FMath::Max(0.0f, ToastRemaining - InDeltaTime);
		const float Opacity = FMath::Clamp(ToastRemaining * 1.8f, 0.0f, 1.0f);
		ToastBorder->SetRenderOpacity(Opacity);
		if (TopText)
		{
			const float Pulse = 1.0f + 0.035f * FMath::Sin(ToastRemaining * 12.0f) * Opacity;
			TopText->SetRenderScale(FVector2D(Pulse));
		}
		if (ToastRemaining <= 0.0f)
		{
			ToastBorder->SetVisibility(ESlateVisibility::Collapsed);
			if (TopText) TopText->SetRenderScale(FVector2D(1.0f));
		}
	}
	if (CrisisElapsed >= 0.0f && CrisisBorder)
	{
		CrisisElapsed += InDeltaTime;
		const int32 Stage = CrisisElapsed < 0.55f ? 0 : CrisisElapsed < 1.45f ? 1 : 2;
		if (Stage != ActiveCrisisStage)
		{
			ApplyCrisisStage(Stage);
		}
		CrisisBorder->SetRenderOpacity(CrisisElapsed < 0.25f
			? CrisisElapsed / 0.25f
			: FMath::Clamp((3.8f - CrisisElapsed) / 0.55f, 0.0f, 1.0f));
		if (CrisisElapsed >= 3.8f)
		{
			CrisisElapsed = -1.0f;
			CrisisBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (EndingElapsed >= 0.0f && EndingCinematicBorder)
	{
		EndingElapsed += InDeltaTime;
		EndingCinematicBorder->SetRenderOpacity(EndingElapsed < 0.5f
			? EndingElapsed / 0.5f
			: FMath::Clamp((4.4f - EndingElapsed) / 0.8f, 0.0f, 1.0f));
		if (EndingElapsed >= 4.4f)
		{
			EndingElapsed = -1.0f;
			bEndingResultsRevealed = true;
			EndingCinematicBorder->SetVisibility(ESlateVisibility::Collapsed);
			if (ResultsBorder) ResultsBorder->SetVisibility(ESlateVisibility::Visible);
		}
	}
	if (bPresentationCaptureOverride)
	{
		UpdateFromState(PresentationCaptureState);
		if (bDialogueVisible)
		{
			UpdateDialogueCard(PresentationCaptureState);
		}
	}
	else if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			const FWSGameState Snapshot = StateSubsystem->GetStateSnapshot();
			UpdateFromState(Snapshot);
			if (bDialogueVisible)
			{
				UpdateDialogueCard(Snapshot);
			}
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

	UBorder* TopPanel = MakePanel(Canvas, TEXT("TopPanel"), FAnchors(0, 0), FMargin(20, 20, 340, 122), PanelColor);
	TopText = MakeText(TEXT("TopText"), 16, Body);
	TopPanel->SetContent(TopText);

	UBorder* ObjectivePanel = MakePanel(Canvas, TEXT("ObjectivePanel"), FAnchors(0, 0), FMargin(20, 154, 308, 342), PanelColor);
	ObjectiveText = MakeText(TEXT("ObjectiveText"), 15, Body);
	ObjectivePanel->SetContent(ObjectiveText);

	UBorder* CrewPanel = MakePanel(Canvas, TEXT("CrewPanel"), FAnchors(1, 0), FMargin(-352, 20, 332, 520), PanelColor);
	UVerticalBox* CrewBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CrewBox"));
	CrewPanel->SetContent(CrewBox);
	CrewText = MakeText(TEXT("CrewHeader"), 16, Body);
	CrewText->SetText(FText::FromString(TEXT("值班组状态")));
	CrewBox->AddChildToVerticalBox(CrewText)->SetPadding(FMargin(0, 0, 0, 8));
	const TArray<UTexture2D*> Portraits = {PlayerPortraitTexture, GuHengPortraitTexture, YeChengPortraitTexture};
	const TArray<FLinearColor> StatusColors = {
		Danger,
		Cyan,
		Amber,
		FLinearColor(0.83f, 0.70f, 0.38f, 1.0f),
		FLinearColor(0.72f, 0.50f, 0.78f, 1.0f)};
	CrewCardTexts.Reset();
	CrewStatusBars.Reset();
	CrewTrustBars.Reset();
	for (int32 CharacterIndex = 0; CharacterIndex < 3; ++CharacterIndex)
	{
		UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("CrewCard%d"), CharacterIndex)));
		USizeBox* PortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("CrewPortraitBox%d"), CharacterIndex)));
		PortraitBox->SetWidthOverride(68.0f);
		PortraitBox->SetHeightOverride(86.0f);
		UImage* Portrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*FString::Printf(TEXT("CrewPortrait%d"), CharacterIndex)));
		if (Portraits.IsValidIndex(CharacterIndex) && Portraits[CharacterIndex])
		{
			Portrait->SetBrushFromTexture(Portraits[CharacterIndex], true);
		}
		PortraitBox->SetContent(Portrait);
		CardRow->AddChildToHorizontalBox(PortraitBox)->SetPadding(FMargin(0, 0, 10, 0));

		UVerticalBox* CardInfo = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*FString::Printf(TEXT("CrewInfo%d"), CharacterIndex)));
		UTextBlock* CardText = MakeText(FName(*FString::Printf(TEXT("CrewText%d"), CharacterIndex)), 13, Body);
		CardInfo->AddChildToVerticalBox(CardText)->SetPadding(FMargin(0, 0, 0, 4));
		CrewCardTexts.Add(CardText);
		UTextBlock* StatusLegend = MakeText(FName(*FString::Printf(TEXT("CrewLegend%d"), CharacterIndex)), 10, Secondary, false);
		StatusLegend->SetText(FText::FromString(TEXT("健　温　精　饥　压")));
		CardInfo->AddChildToVerticalBox(StatusLegend)->SetPadding(FMargin(0, 0, 0, 2));
		UHorizontalBox* StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("CrewBars%d"), CharacterIndex)));
		for (int32 StatusIndex = 0; StatusIndex < 5; ++StatusIndex)
		{
			USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("CrewBarBox%d_%d"), CharacterIndex, StatusIndex)));
			BarBox->SetWidthOverride(31.0f);
			BarBox->SetHeightOverride(7.0f);
			UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(*FString::Printf(TEXT("CrewBar%d_%d"), CharacterIndex, StatusIndex)));
			Bar->SetPercent(0.5f);
			Bar->SetFillColorAndOpacity(StatusColors[StatusIndex]);
			BarBox->SetContent(Bar);
			StatusRow->AddChildToHorizontalBox(BarBox)->SetPadding(FMargin(0, 0, 3, 0));
			CrewStatusBars.Add(Bar);
		}
		CardInfo->AddChildToVerticalBox(StatusRow)->SetPadding(FMargin(0, 0, 0, 4));
		if (CharacterIndex > 0)
		{
			UTextBlock* TrustLabel = MakeText(FName(*FString::Printf(TEXT("CrewTrustLabel%d"), CharacterIndex)), 10, Secondary, false);
			TrustLabel->SetText(FText::FromString(TEXT("信任")));
			CardInfo->AddChildToVerticalBox(TrustLabel);
			USizeBox* TrustBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("CrewTrustBox%d"), CharacterIndex)));
			TrustBox->SetHeightOverride(6.0f);
			UProgressBar* TrustBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(*FString::Printf(TEXT("CrewTrust%d"), CharacterIndex)));
			TrustBar->SetFillColorAndOpacity(Cyan);
			TrustBox->SetContent(TrustBar);
			CardInfo->AddChildToVerticalBox(TrustBox);
			CrewTrustBars.Add(TrustBar);
		}
		else
		{
			CrewTrustBars.Add(nullptr);
		}
		UHorizontalBoxSlot* InfoSlot = CardRow->AddChildToHorizontalBox(CardInfo);
		InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CrewBox->AddChildToVerticalBox(CardRow)->SetPadding(FMargin(0, 0, 0, 10));
	}

	UBorder* BottomPanel = MakePanel(Canvas, TEXT("BottomPanel"), FAnchors(0.5f, 1.0f), FMargin(-420, -142, 840, 122), FLinearColor(0.035f, 0.071f, 0.102f, 0.72f));
	UVerticalBox* BottomBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BottomBox"));
	BottomPanel->SetContent(BottomBox);
	FeedbackText = MakeText(TEXT("FeedbackText"), 14, Body);
	PromptText = MakeText(TEXT("PromptText"), 16, Amber);
	UTextBlock* HelpText = MakeText(TEXT("HelpText"), 12, Secondary);
	HelpText->SetText(FWSPresentationText::UI(
		TEXT("ui_help_v03"),
		TEXT("WASD 移动　鼠标观察　Space 跳跃　F 互动/对话　E 证据板　Enter 结束　Esc 暂停")));
	BottomBox->AddChildToVerticalBox(FeedbackText)->SetPadding(FMargin(0, 0, 0, 3));
	BottomBox->AddChildToVerticalBox(PromptText)->SetPadding(FMargin(0, 0, 0, 3));
	BottomBox->AddChildToVerticalBox(HelpText);

	CrosshairText = MakeText(TEXT("CrosshairText"), 28, Body, false);
	CrosshairText->SetText(FText::FromString(TEXT("+")));
	CrosshairText->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* CrosshairSlot = Canvas->AddChildToCanvas(CrosshairText);
	CrosshairSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	CrosshairSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CrosshairSlot->SetOffsets(FMargin(-22.0f, -22.0f, 44.0f, 44.0f));
	FocusBorder = MakePanel(Canvas, TEXT("FocusPanel"), FAnchors(0.5f, 0.5f), FMargin(-270, 42, 540, 78), FLinearColor(0.0f, 0.0f, 0.0f, 0.34f));
	FocusBorder->SetPadding(FMargin(8));
	UOverlay* FocusOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("FocusOverlay"));
	if (InkBrushTexture)
	{
		UImage* FocusBrush = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FocusBrush"));
		FocusBrush->SetBrushFromTexture(InkBrushTexture, true);
		FocusBrush->SetColorAndOpacity(FLinearColor(Amber.R, Amber.G, Amber.B, 0.76f));
		FocusOverlay->AddChildToOverlay(FocusBrush);
	}
	FocusText = MakeText(TEXT("FocusText"), 15, Body, false);
	FocusText->SetJustification(ETextJustify::Center);
	UOverlaySlot* FocusTextSlot = FocusOverlay->AddChildToOverlay(FocusText);
	FocusTextSlot->SetHorizontalAlignment(HAlign_Center);
	FocusTextSlot->SetVerticalAlignment(VAlign_Center);
	FocusBorder->SetContent(FocusOverlay);
	FocusBorder->SetVisibility(ESlateVisibility::Collapsed);

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

	EvidenceBorder = MakePanel(Canvas, TEXT("EvidencePanel"), FAnchors(0.12f, 0.08f, 0.88f, 0.92f), FMargin(0), FLinearColor(0.006f, 0.018f, 0.033f, 0.985f));
	UVerticalBox* EvidenceBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EvidenceBox"));
	EvidenceTitleText = MakeText(TEXT("EvidenceTitle"), 27, Body);
	EvidenceBox->AddChildToVerticalBox(EvidenceTitleText)->SetPadding(FMargin(0, 0, 0, 12));
	UHorizontalBox* EvidenceMain = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EvidenceMain"));
	USizeBox* FilterSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EvidenceFilterSize"));
	FilterSize->SetWidthOverride(205.0f);
	UBorder* FilterPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EvidenceFilterPanel"));
	FilterPanel->SetBrushColor(FLinearColor(0.025f, 0.050f, 0.070f, 0.78f));
	FilterPanel->SetPadding(FMargin(14));
	EvidenceFilterText = MakeText(TEXT("EvidenceFilters"), 14, Secondary);
	FilterPanel->SetContent(EvidenceFilterText);
	FilterSize->SetContent(FilterPanel);
	EvidenceMain->AddChildToHorizontalBox(FilterSize)->SetPadding(FMargin(0, 0, 14, 0));
	EvidenceScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("EvidenceScroll"));
	EvidenceCardGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("EvidenceCardGrid"));
	EvidenceCardGrid->SetMinDesiredSlotWidth(300.0f);
	EvidenceScroll->AddChild(EvidenceCardGrid);
	UHorizontalBoxSlot* EvidenceCardsSlot = EvidenceMain->AddChildToHorizontalBox(EvidenceScroll);
	EvidenceCardsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBoxSlot* EvidenceMainSlot = EvidenceBox->AddChildToVerticalBox(EvidenceMain);
	EvidenceMainSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	EvidenceProgressText = MakeText(TEXT("EvidenceProgress"), 13, Secondary);
	EvidenceBox->AddChildToVerticalBox(EvidenceProgressText)->SetPadding(FMargin(0, 12, 0, 0));
	EvidenceBorder->SetContent(EvidenceBox);
	EvidenceBorder->SetVisibility(ESlateVisibility::Collapsed);

	DialogueBorder = MakePanel(Canvas, TEXT("DialoguePanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.004f, 0.014f, 0.026f, 0.97f));
	DialogueBorder->SetPadding(FMargin(0));
	UCanvasPanel* DialogueCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialogueCanvas"));
	DialogueBorder->SetContent(DialogueCanvas);
	DialogueText = MakeText(TEXT("DialogueTitle"), 27, Body, false);
	DialogueText->SetText(FWSPresentationText::UI(TEXT("ui_dialogue_title"), TEXT("交涉方式｜先选意图，再由规则结算")));
	DialogueText->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* DialogueTitleSlot = DialogueCanvas->AddChildToCanvas(DialogueText);
	DialogueTitleSlot->SetAnchors(FAnchors(0.10f, 0.055f, 0.68f, 0.13f));
	DialogueTitleSlot->SetOffsets(FMargin(0));

	DialogueWheelPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialogueWheel"));
	UCanvasPanelSlot* WheelSlot = DialogueCanvas->AddChildToCanvas(DialogueWheelPanel);
	WheelSlot->SetAnchors(FAnchors(0.05f, 0.14f, 0.69f, 0.83f));
	WheelSlot->SetOffsets(FMargin(0));
	UBorder* WheelCenter = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueWheelCenter"));
	WheelCenter->SetBrushColor(FLinearColor(0.08f, 0.16f, 0.21f, 0.94f));
	WheelCenter->SetPadding(FMargin(10));
	UTextBlock* WheelCenterText = MakeText(TEXT("DialogueWheelCenterText"), 19, Cyan, false);
	WheelCenterText->SetText(FWSPresentationText::UI(TEXT("ui_dialogue_wheel_center"), TEXT("交涉\n方式")));
	WheelCenterText->SetJustification(ETextJustify::Center);
	WheelCenter->SetContent(WheelCenterText);
	UCanvasPanelSlot* WheelCenterSlot = DialogueWheelPanel->AddChildToCanvas(WheelCenter);
	WheelCenterSlot->SetAnchors(FAnchors(0.50f, 0.50f));
	WheelCenterSlot->SetAlignment(FVector2D(0.5f));
	WheelCenterSlot->SetOffsets(FMargin(-61, -61, 122, 122));
	UButton* AskButton = MakeDialogueChoiceButton(DialogueWheelPanel, FWSPresentationText::UI(TEXT("dialogue_ask"), TEXT("询问")), TEXT("I_Dialogue_Inquire"), TEXT("DialogueAsk"), FAnchors(0.50f, 0.18f), FMargin(-82, -43, 164, 86));
	UButton* ChallengeButton = MakeDialogueChoiceButton(DialogueWheelPanel, FWSPresentationText::UI(TEXT("dialogue_challenge"), TEXT("质疑")), TEXT("I_Dialogue_Doubt"), TEXT("DialogueChallenge"), FAnchors(0.79f, 0.38f), FMargin(-82, -43, 164, 86));
	UButton* PromiseButton = MakeDialogueChoiceButton(DialogueWheelPanel, FWSPresentationText::UI(TEXT("dialogue_promise"), TEXT("承诺")), TEXT("I_Dialogue_Promise"), TEXT("DialoguePromise"), FAnchors(0.68f, 0.76f), FMargin(-82, -43, 164, 86));
	UButton* ReassureButton = MakeDialogueChoiceButton(DialogueWheelPanel, FWSPresentationText::UI(TEXT("dialogue_reassure"), TEXT("安抚")), TEXT("I_Dialogue_Comfort"), TEXT("DialogueReassure"), FAnchors(0.32f, 0.76f), FMargin(-82, -43, 164, 86));
	UButton* FreeTextButton = MakeDialogueChoiceButton(DialogueWheelPanel, FWSPresentationText::UI(TEXT("dialogue_free_text"), TEXT("自由输入")), TEXT("I_Dialogue_FreeText"), TEXT("DialogueFreeText"), FAnchors(0.21f, 0.38f), FMargin(-82, -43, 164, 86));
	AskButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChooseDialogueAsk);
	ChallengeButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChooseDialogueChallenge);
	PromiseButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChooseDialoguePromise);
	ReassureButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChooseDialogueReassure);
	FreeTextButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::OpenDialogueFreeText);

	DialoguePromiseBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialoguePromisePanel"));
	DialoguePromiseBorder->SetBrushColor(FLinearColor(0.025f, 0.060f, 0.086f, 0.98f));
	DialoguePromiseBorder->SetPadding(FMargin(24));
	UVerticalBox* PromiseBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialoguePromiseBox"));
	DialoguePromiseBorder->SetContent(PromiseBox);
	UTextBlock* PromiseTitle = MakeText(TEXT("DialoguePromiseTitle"), 23, Amber);
	PromiseTitle->SetText(FWSPresentationText::UI(TEXT("ui_dialogue_promise_title"), TEXT("选择可由现有规则记录的承诺")));
	PromiseBox->AddChildToVerticalBox(PromiseTitle)->SetPadding(FMargin(0, 0, 0, 14));
	UButton* KeepRecordsButton = MakeButton(PromiseBox, FWSPresentationText::UI(TEXT("dialogue_promise_records"), TEXT("不弃站｜保存记录")), TEXT("PromiseKeepRecords"));
	UButton* PreventSelfHarmButton = MakeButton(PromiseBox, FWSPresentationText::UI(TEXT("dialogue_promise_medicine"), TEXT("不放任自伤｜保留药品")), TEXT("PromisePreventSelfHarm"));
	UButton* RepairTogetherButton = MakeButton(PromiseBox, FWSPresentationText::UI(TEXT("dialogue_promise_heat"), TEXT("配合修复｜维修间升温")), TEXT("PromiseRepairTogether"));
	UButton* PromiseBackButton = MakeButton(PromiseBox, FWSPresentationText::UI(TEXT("ui_dialogue_back"), TEXT("返回轮盘")), TEXT("PromiseBack"));
	KeepRecordsButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChoosePromiseKeepRecords);
	PreventSelfHarmButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChoosePromisePreventSelfHarm);
	RepairTogetherButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChoosePromiseRepairTogether);
	PromiseBackButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ShowDialogueWheelChoices);
	UCanvasPanelSlot* PromisePanelSlot = DialogueCanvas->AddChildToCanvas(DialoguePromiseBorder);
	PromisePanelSlot->SetAnchors(FAnchors(0.12f, 0.22f, 0.66f, 0.74f));
	PromisePanelSlot->SetOffsets(FMargin(0));
	DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);

	DialogueFreeTextBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueFreeTextPanel"));
	DialogueFreeTextBorder->SetBrushColor(FLinearColor(0.025f, 0.060f, 0.086f, 0.98f));
	DialogueFreeTextBorder->SetPadding(FMargin(24));
	UVerticalBox* FreeTextBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueFreeTextBox"));
	DialogueFreeTextBorder->SetContent(FreeTextBox);
	UTextBlock* FreeTextTitle = MakeText(TEXT("DialogueFreeTextTitle"), 23, Amber);
	FreeTextTitle->SetText(FWSPresentationText::UI(TEXT("ui_dialogue_free_title"), TEXT("用自己的话交涉")));
	FreeTextBox->AddChildToVerticalBox(FreeTextTitle)->SetPadding(FMargin(0, 0, 0, 8));
	UTextBlock* FreeTextHelp = MakeText(TEXT("DialogueFreeTextHelp"), 14, Secondary);
	FreeTextHelp->SetText(FWSPresentationText::UI(TEXT("ui_dialogue_free_help"), TEXT("系统只识别询问 / 质疑 / 承诺 / 安抚，不执行文本中的状态或规则指令。")));
	FreeTextBox->AddChildToVerticalBox(FreeTextHelp)->SetPadding(FMargin(0, 0, 0, 14));
	DialogueFreeTextInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("DialogueFreeTextInput"));
	DialogueFreeTextInput->SetHintText(FWSPresentationText::UI(TEXT("ui_dialogue_free_hint"), TEXT("例如：我保证会和你一起修好发电机")));
	FEditableTextBoxStyle DialogueInputStyle = DialogueFreeTextInput->GetWidgetStyle();
	DialogueInputStyle.SetFont(UIFont(18));
	DialogueFreeTextInput->SetWidgetStyle(DialogueInputStyle);
	DialogueFreeTextInput->SetForegroundColor(FLinearColor::White);
	DialogueFreeTextInput->OnTextCommitted.AddDynamic(this, &UWhiteoutHUDWidget::HandleDialogueTextCommitted);
	FreeTextBox->AddChildToVerticalBox(DialogueFreeTextInput)->SetPadding(FMargin(12, 5, 12, 14));
	UButton* SubmitTextButton = MakeButton(FreeTextBox, FWSPresentationText::UI(TEXT("ui_dialogue_submit"), TEXT("识别并提交")), TEXT("DialogueTextSubmit"));
	UButton* FreeTextBackButton = MakeButton(FreeTextBox, FWSPresentationText::UI(TEXT("ui_dialogue_back"), TEXT("返回轮盘")), TEXT("DialogueTextBack"));
	SubmitTextButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::SubmitDialogueFreeText);
	FreeTextBackButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ShowDialogueWheelChoices);
	UCanvasPanelSlot* FreeTextPanelSlot = DialogueCanvas->AddChildToCanvas(DialogueFreeTextBorder);
	FreeTextPanelSlot->SetAnchors(FAnchors(0.10f, 0.25f, 0.68f, 0.70f));
	FreeTextPanelSlot->SetOffsets(FMargin(0));
	DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Collapsed);

	UBorder* NPCCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueNPCCard"));
	NPCCard->SetBrushColor(FLinearColor(0.025f, 0.052f, 0.073f, 0.97f));
	NPCCard->SetPadding(FMargin(18));
	UVerticalBox* NPCCardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueNPCCardBox"));
	NPCCard->SetContent(NPCCardBox);
	USizeBox* NPCPortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DialogueNPCPortraitBox"));
	NPCPortraitBox->SetHeightOverride(250.0f);
	DialogueNPCPortrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DialogueNPCPortrait"));
	NPCPortraitBox->SetContent(DialogueNPCPortrait);
	NPCCardBox->AddChildToVerticalBox(NPCPortraitBox)->SetPadding(FMargin(0, 0, 0, 12));
	DialogueNPCText = MakeText(TEXT("DialogueNPCText"), 15, Body);
	NPCCardBox->AddChildToVerticalBox(DialogueNPCText)->SetPadding(FMargin(0, 0, 0, 12));
	DialogueNPCBars.Reset();
	const TArray<FLinearColor> DialogueBarColors = {Danger, Cyan, Amber, FLinearColor(0.58f, 0.76f, 0.92f, 1.0f)};
	for (int32 Index = 0; Index < DialogueBarColors.Num(); ++Index)
	{
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(*FString::Printf(TEXT("DialogueNPCBar%d"), Index)));
		Bar->SetFillColorAndOpacity(DialogueBarColors[Index]);
		NPCCardBox->AddChildToVerticalBox(Bar)->SetPadding(FMargin(0, 3, 0, 5));
		DialogueNPCBars.Add(Bar);
	}
	UCanvasPanelSlot* NPCCardSlot = DialogueCanvas->AddChildToCanvas(NPCCard);
	NPCCardSlot->SetAnchors(FAnchors(0.72f, 0.12f, 0.95f, 0.86f));
	NPCCardSlot->SetOffsets(FMargin(0));

	DialogueStatusText = MakeText(TEXT("DialogueStatusText"), 16, Secondary);
	DialogueStatusText->SetText(FWSPresentationText::UI(TEXT("ui_dialogue_footer"), TEXT("选择交涉方式；自由输入会自动降级，规则结果始终确定。")));
	DialogueStatusText->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* DialogueStatusSlot = DialogueCanvas->AddChildToCanvas(DialogueStatusText);
	DialogueStatusSlot->SetAnchors(FAnchors(0.08f, 0.86f, 0.68f, 0.94f));
	DialogueStatusSlot->SetOffsets(FMargin(0));
	UButton* DialogueCancelButton = MakeDialogueChoiceButton(DialogueCanvas, FWSPresentationText::UI(TEXT("ui_dialogue_cancel"), TEXT("取消 / 返回现场")), TEXT(""), TEXT("DialogueCancel"), FAnchors(0.74f, 0.89f, 0.94f, 0.96f), FMargin(0));
	DialogueCancelButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::CancelDialogue);
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

	ToastBorder = MakePanel(Canvas, TEXT("ActionToast"), FAnchors(0.27f, 0.70f, 0.73f, 0.70f), FMargin(0, 0, 0, 104), FLinearColor(0.0f, 0.0f, 0.0f, 0.30f));
	UOverlay* ToastOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ToastOverlay"));
	if (InkBrushTexture)
	{
		UImage* ToastBrush = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ToastBrush"));
		ToastBrush->SetBrushFromTexture(InkBrushTexture, true);
		ToastBrush->SetColorAndOpacity(FLinearColor(Amber.R, Amber.G, Amber.B, 0.72f));
		ToastOverlay->AddChildToOverlay(ToastBrush);
	}
	ToastText = MakeText(TEXT("ActionToastText"), 17, Body, false);
	ToastText->SetJustification(ETextJustify::Center);
	UOverlaySlot* ToastTextSlot = ToastOverlay->AddChildToOverlay(ToastText);
	ToastTextSlot->SetHorizontalAlignment(HAlign_Center);
	ToastTextSlot->SetVerticalAlignment(VAlign_Center);
	ToastBorder->SetContent(ToastOverlay);
	ToastBorder->SetVisibility(ESlateVisibility::Collapsed);

	EndingCinematicBorder = MakePanel(Canvas, TEXT("EndingCinematicPanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.004f, 0.012f, 0.022f, 0.94f));
	EndingCinematicText = MakeText(TEXT("EndingCinematicText"), 38, Body);
	EndingCinematicText->SetJustification(ETextJustify::Center);
	EndingCinematicBorder->SetContent(EndingCinematicText);
	EndingCinematicBorder->SetVisibility(ESlateVisibility::Collapsed);

	CrisisBorder = MakePanel(Canvas, TEXT("CrisisPanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.23f, 0.005f, 0.003f, 0.72f));
	CrisisText = MakeText(TEXT("CrisisText"), 40, Danger);
	CrisisText->SetJustification(ETextJustify::Center);
	CrisisBorder->SetContent(CrisisText);
	CrisisBorder->SetVisibility(ESlateVisibility::Collapsed);

	OpeningBorder = MakePanel(Canvas, TEXT("OpeningPanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.003f, 0.010f, 0.020f, 0.96f));
	OpeningText = MakeText(TEXT("OpeningText"), 31, Body);
	OpeningText->SetJustification(ETextJustify::Center);
	OpeningBorder->SetContent(OpeningText);
	ApplyOpeningStage(0);

	PauseBorder = MakePanel(Canvas, TEXT("PausePanel"), FAnchors(0.5f, 0.5f), FMargin(-310, -350, 620, 700), FLinearColor(0.004f, 0.014f, 0.026f, 0.985f));
	UVerticalBox* PauseBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseBox"));
	PauseBorder->SetContent(PauseBox);
	UTextBlock* PauseTitle = MakeText(TEXT("PauseTitle"), 31, Body);
	PauseTitle->SetText(FWSPresentationText::UI(TEXT("ui_pause"), TEXT("行动暂停")));
	PauseTitle->SetJustification(ETextJustify::Center);
	PauseBox->AddChildToVerticalBox(PauseTitle)->SetPadding(FMargin(0, 0, 0, 7));
	PauseStatusText = MakeText(TEXT("PauseStatus"), 14, Secondary);
	PauseStatusText->SetJustification(ETextJustify::Center);
	PauseBox->AddChildToVerticalBox(PauseStatusText)->SetPadding(FMargin(0, 0, 0, 13));
	UButton* ResumeButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_resume"), TEXT("继续游戏")), TEXT("ResumeButton"));
	PauseDefaultButton = ResumeButton;
	UButton* SaveButton = MakeButton(PauseBox, FText::FromString(TEXT("保存游戏　｜　当前版本不可用")), TEXT("SaveButton"));
	UButton* LoadButton = MakeButton(PauseBox, FText::FromString(TEXT("读取游戏　｜　当前版本不可用")), TEXT("LoadButton"));
	UButton* SettingsButton = MakeButton(PauseBox, FText::FromString(TEXT("设置　　　｜　当前版本不可用")), TEXT("SettingsButton"));
	UButton* HelpButton = MakeButton(PauseBox, FText::FromString(TEXT("操作说明")), TEXT("HelpButton"));
	UButton* RestartButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_restart"), TEXT("重新开始")), TEXT("RestartButton"));
	UButton* MainMenuButton = MakeButton(PauseBox, FText::FromString(TEXT("返回主菜单｜　当前版本不可用")), TEXT("MainMenuButton"));
	UButton* QuitButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_quit"), TEXT("退出到桌面")), TEXT("QuitButton"));
	SaveButton->SetIsEnabled(false);
	LoadButton->SetIsEnabled(false);
	SettingsButton->SetIsEnabled(false);
	MainMenuButton->SetIsEnabled(false);
	ResumeButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ResumeGame);
	HelpButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ToggleControls);
	RestartButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::RestartGame);
	QuitButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::QuitGame);
	PauseSituationText = MakeText(TEXT("PauseSituation"), 12, Secondary);
	PauseSituationText->SetJustification(ETextJustify::Center);
	PauseBox->AddChildToVerticalBox(PauseSituationText)->SetPadding(FMargin(8, 11, 8, 0));
	PauseHelpText = MakeText(TEXT("PauseHelp"), 13, Secondary);
	PauseHelpText->SetText(FText::FromString(TEXT("WASD 移动　鼠标观察　Space 跳跃\nF 互动 / 对话　E 证据板　Enter 结束当日　Esc 返回")));
	PauseHelpText->SetJustification(ETextJustify::Center);
	PauseHelpText->SetVisibility(ESlateVisibility::Collapsed);
	PauseBox->AddChildToVerticalBox(PauseHelpText)->SetPadding(FMargin(8, 12, 8, 0));
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
	Button->SetBackgroundColor(FLinearColor(0.055f, 0.10f, 0.14f, 0.88f));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*(Name.ToString() + TEXT("Row"))));
	const FString IconName = ButtonIconName(Name);
	if (!IconName.IsEmpty())
	{
		const FString IconPath = FString::Printf(TEXT("/Game/WindStation/UI/v03/Icons/%s.%s"), *IconName, *IconName);
		if (UTexture2D* IconTexture = LoadObject<UTexture2D>(nullptr, *IconPath))
		{
			USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name.ToString() + TEXT("IconBox"))));
			IconBox->SetWidthOverride(28.0f);
			IconBox->SetHeightOverride(28.0f);
			UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*(Name.ToString() + TEXT("Icon"))));
			Icon->SetBrushFromTexture(IconTexture, true);
			Icon->SetColorAndOpacity(Body);
			IconBox->SetContent(Icon);
			Row->AddChildToHorizontalBox(IconBox)->SetPadding(FMargin(10, 0, 14, 0));
		}
	}
	UTextBlock* LabelText = MakeText(FName(*(Name.ToString() + TEXT("Label"))), 18, Body);
	LabelText->SetText(Label);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	Button->SetContent(Row);
	Button->OnHovered.AddDynamic(this, &UWhiteoutHUDWidget::PlayHoverSound);
	Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(14, 4));
	return Button;
}

UButton* UWhiteoutHUDWidget::MakeDialogueChoiceButton(
	UCanvasPanel* Canvas,
	const FText& Label,
	const FString& IconName,
	const FName Name,
	const FAnchors& Anchors,
	const FMargin& Offsets)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Button->SetBackgroundColor(FLinearColor(0.045f, 0.105f, 0.145f, 0.96f));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*(Name.ToString() + TEXT("Row"))));
	if (!IconName.IsEmpty())
	{
		const FString IconPath = FString::Printf(TEXT("/Game/WindStation/UI/v03/Icons/%s.%s"), *IconName, *IconName);
		if (UTexture2D* IconTexture = LoadObject<UTexture2D>(nullptr, *IconPath))
		{
			USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name.ToString() + TEXT("IconBox"))));
			IconBox->SetWidthOverride(42.0f);
			IconBox->SetHeightOverride(42.0f);
			UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*(Name.ToString() + TEXT("Icon"))));
			Icon->SetBrushFromTexture(IconTexture, true);
			Icon->SetColorAndOpacity(Amber);
			IconBox->SetContent(Icon);
			Row->AddChildToHorizontalBox(IconBox)->SetPadding(FMargin(7, 0, 8, 0));
		}
	}
	UTextBlock* LabelText = MakeText(FName(*(Name.ToString() + TEXT("Label"))), 16, Body, false);
	LabelText->SetText(Label);
	LabelText->SetJustification(ETextJustify::Center);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	Button->SetContent(Row);
	Button->OnHovered.AddDynamic(this, &UWhiteoutHUDWidget::PlayHoverSound);
	UCanvasPanelSlot* CanvasButtonSlot = Canvas->AddChildToCanvas(Button);
	CanvasButtonSlot->SetAnchors(Anchors);
	CanvasButtonSlot->SetOffsets(Offsets);
	if (Anchors.Minimum == Anchors.Maximum)
	{
		CanvasButtonSlot->SetAlignment(FVector2D(0.0f));
	}
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

FString UWhiteoutHUDWidget::ClockForAP(const int32 Remaining)
{
	const int32 ElapsedMinutes = (8 - FMath::Clamp(Remaining, 0, 8)) * 75;
	const int32 TotalMinutes = 8 * 60 + 15 + ElapsedMinutes;
	return FString::Printf(TEXT("%02d:%02d"), (TotalMinutes / 60) % 24, TotalMinutes % 60);
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
		TEXT("ui_top_format_v03"),
		TEXT("风雪站：断电前夜\n{0}　｜　AP {1} / 8\n{2}　·　{3}")).ToString();
	TopText->SetText(FText::FromString(FString::Format(
		*TopFormat,
		{ClockForAP(State.ActionPoints), State.ActionPoints, FWSPresentationText::PhaseLabel(State.Phase).ToString(), Crisis})));
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

	CrewText->SetText(FWSPresentationText::UI(TEXT("ui_crew_header_v03"), TEXT("值班组状态")));
	const TArray<EWSCharacterId> CharacterIds = {EWSCharacterId::Player, EWSCharacterId::GuHeng, EWSCharacterId::YeCheng};
	for (int32 CharacterIndex = 0; CharacterIndex < CharacterIds.Num(); ++CharacterIndex)
	{
		const EWSCharacterId CharacterId = CharacterIds[CharacterIndex];
		if (const FWSCharacterState* Character = State.Characters.Find(CharacterId))
		{
			if (CrewCardTexts.IsValidIndex(CharacterIndex))
			{
				FString Card = FWSPresentationText::CharacterName(CharacterId).ToString();
				Card += TEXT("\n");
				Card += FString::Printf(TEXT("健康 %s　体温 %s"),
					*FWSPresentationText::ConditionLevel(Character->Health).ToString(),
					*FWSPresentationText::ConditionLevel(Character->Temperature).ToString());
				if (CharacterId != EWSCharacterId::Player)
				{
					Card += TEXT("\n信任 ") + FWSPresentationText::TrustLevel(Character->Trust).ToString();
				}
				CrewCardTexts[CharacterIndex]->SetText(FText::FromString(Card));
			}
			const TArray<float> Ratios = {
				Character->Health / 100.0f,
				Character->Temperature / 100.0f,
				1.0f - Character->Fatigue / 100.0f,
				1.0f - Character->Hunger / 100.0f,
				1.0f - Character->Pressure / 100.0f};
			for (int32 StatusIndex = 0; StatusIndex < Ratios.Num(); ++StatusIndex)
			{
				const int32 FlatIndex = CharacterIndex * Ratios.Num() + StatusIndex;
				if (CrewStatusBars.IsValidIndex(FlatIndex))
				{
					CrewStatusBars[FlatIndex]->SetPercent(FMath::Clamp(Ratios[StatusIndex], 0.0f, 1.0f));
				}
			}
			if (CrewTrustBars.IsValidIndex(CharacterIndex) && CrewTrustBars[CharacterIndex])
			{
				CrewTrustBars[CharacterIndex]->SetPercent(FMath::Clamp((Character->Trust + 100.0f) / 200.0f, 0.0f, 1.0f));
			}
		}
	}
	if (PauseStatusText)
	{
		PauseStatusText->SetText(FText::FromString(FString::Printf(TEXT("%s　｜　AP %d / 8　｜　%s"),
			*ClockForAP(State.ActionPoints), State.ActionPoints, *FWSPresentationText::PhaseLabel(State.Phase).ToString())));
	}
	if (PauseSituationText)
	{
		PauseSituationText->SetText(FText::FromString(FString::Printf(
			TEXT("当前情况｜AP %d / 8　暴雪抵达 %s　目标：发电机 %d/2 · 天线 %d/1 · 信号 %s"),
			State.ActionPoints,
			*ClockForAP(0),
			State.Tasks.GeneratorProgress,
			State.Tasks.AntennaCalibration,
			State.Tasks.bSignalSent ? TEXT("已发送") : TEXT("未发送"))));
	}
	FeedbackText->SetText(FText::FromString(SystemMessage));
	PromptText->SetText(InteractionPrompt);
	UpdateEvidence(State);
	UpdateResults(State);
}

void UWhiteoutHUDWidget::UpdateDialogueCard(const FWSGameState& State)
{
	if (!DialogueNPCText || !DialogueNPCPortrait)
	{
		return;
	}
	const bool bGuHeng = ActiveDialogueActionId == TEXT("talk_gu_heng");
	const EWSCharacterId CharacterId = bGuHeng ? EWSCharacterId::GuHeng : EWSCharacterId::YeCheng;
	const FWSCharacterState* Character = State.Characters.Find(CharacterId);
	const FWSCharacterState SafeState = Character ? *Character : FWSCharacterState();
	const FString Relationship = SafeState.Trust >= 12.0f ? TEXT("信任")
		: SafeState.Trust >= 0.0f ? TEXT("可合作")
		: SafeState.Trust >= -8.0f ? TEXT("有所保留") : TEXT("戒备");
	FString Stance;
	if (bGuHeng)
	{
		Stance = State.Flags.bGuHengCooperative ? TEXT("愿意配合维修")
			: State.Flags.bGuHengTreated ? TEXT("等待维修条件")
			: State.Flags.bGuHengDiagnosed ? TEXT("带伤防御")
			: TEXT("警惕并回避伤情");
		DialogueNPCPortrait->SetBrushFromTexture(GuHengPortraitTexture, true);
	}
	else
	{
		Stance = State.Flags.bGuHengTreated ? TEXT("持续监测伤员")
			: State.Flags.bMedicalRoomHeated ? TEXT("准备诊疗")
			: TEXT("优先恢复医疗条件");
		DialogueNPCPortrait->SetBrushFromTexture(YeChengPortraitTexture, true);
	}
	const FString Identity = bGuHeng
		? FWSPresentationText::UI(TEXT("character_gu_heng"), TEXT("顾衡｜工程师｜41 岁")).ToString()
		: FWSPresentationText::UI(TEXT("character_ye_cheng"), TEXT("叶澄｜医生｜31 岁")).ToString();
	DialogueNPCText->SetText(FText::FromString(FString::Printf(
		TEXT("%s\n\n关系　%s（信任 %.0f）\n立场　%s\n\n状态概览\n健康 %.0f　体温 %.0f\n压力 %.0f　信任 %.0f"),
		*Identity, *Relationship, SafeState.Trust, *Stance,
		SafeState.Health, SafeState.Temperature, SafeState.Pressure, SafeState.Trust)));
	if (DialogueNPCBars.Num() >= 4)
	{
		DialogueNPCBars[0]->SetPercent(ScoreRatio(SafeState.Health, 100.0f));
		DialogueNPCBars[1]->SetPercent(ScoreRatio(SafeState.Temperature, 100.0f));
		DialogueNPCBars[2]->SetPercent(ScoreRatio(SafeState.Pressure, 100.0f));
		DialogueNPCBars[3]->SetPercent(FMath::Clamp((SafeState.Trust + 20.0f) / 40.0f, 0.0f, 1.0f));
	}
}

void UWhiteoutHUDWidget::UpdateEvidence(const FWSGameState& State)
{
	if (!EvidenceTitleText || !EvidenceFilterText || !EvidenceCardGrid || !EvidenceProgressText)
	{
		return;
	}
	int32 ClaimCount = 0;
	int32 ConfirmedCount = 0;
	for (const TPair<FName, EWSKnowledgeLevel>& Pair : State.PlayerKnowledge)
	{
		ClaimCount += Pair.Value == EWSKnowledgeLevel::Claimed || Pair.Value == EWSKnowledgeLevel::Suspected ? 1 : 0;
		ConfirmedCount += Pair.Value == EWSKnowledgeLevel::Confirmed ? 1 : 0;
	}
	int32 FileCount = 0;
	int32 ItemCount = 0;
	int32 WitnessCount = ConfirmedCount;
	for (const FName EvidenceId : State.Evidence)
	{
		const FString Id = EvidenceId.ToString();
		if (Id.Contains(TEXT("log")) || Id.Contains(TEXT("records")))
		{
			++FileCount;
		}
		else if (Id.Contains(TEXT("diagnosis")))
		{
			++WitnessCount;
		}
		else
		{
			++ItemCount;
		}
	}
	const int32 DialogueCount = ClaimCount + State.Promises.Num();
	const int32 TotalCards = FileCount + ItemCount + WitnessCount + DialogueCount;
	EvidenceTitleText->SetText(FText::FromString(FString::Printf(TEXT("证据板　%02d 条记录　　　　　　　　　[E] 关闭"), TotalCards)));
	EvidenceFilterText->SetText(FText::FromString(FString::Printf(
		TEXT("类型过滤\n\n● 全部　　　　 %02d\n\n　文件记录　　 %02d\n\n　物品证据　　 %02d\n\n　目击信息　　 %02d\n\n　对话记录　　 %02d"),
		TotalCards, FileCount, ItemCount, WitnessCount, DialogueCount)));
	EvidenceProgressText->SetText(FText::FromString(FString::Printf(
		TEXT("重要性　● 关键　　● 重要　　● 普通　　　　　　　　　　　　　　收集进度 %02d / 18"),
		FMath::Clamp(State.Evidence.Num(), 0, 18))));

	EvidenceCardGrid->ClearChildren();
	int32 CardIndex = 0;
	const auto AddCard = [this, &CardIndex](
		const FString& Title,
		const FString& Type,
		const FString& Summary,
		const FLinearColor& Importance,
		const TCHAR* IconName)
	{
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("EvidenceCard%d"), CardIndex)));
		Card->SetBrushColor(FLinearColor(0.030f, 0.060f, 0.082f, 0.82f));
		Card->SetPadding(FMargin(14));
		UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("EvidenceCardRow%d"), CardIndex)));
		const FString IconPath = FString::Printf(TEXT("/Game/WindStation/UI/v03/Icons/%s.%s"), IconName, IconName);
		if (UTexture2D* IconTexture = LoadObject<UTexture2D>(nullptr, *IconPath))
		{
			USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("EvidenceIconBox%d"), CardIndex)));
			IconBox->SetWidthOverride(38.0f);
			IconBox->SetHeightOverride(38.0f);
			UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*FString::Printf(TEXT("EvidenceIcon%d"), CardIndex)));
			Icon->SetBrushFromTexture(IconTexture, true);
			Icon->SetColorAndOpacity(Importance);
			IconBox->SetContent(Icon);
			CardRow->AddChildToHorizontalBox(IconBox)->SetPadding(FMargin(0, 0, 10, 0));
		}
		UTextBlock* CardCopy = MakeText(FName(*FString::Printf(TEXT("EvidenceCopy%d"), CardIndex)), 13, Body);
		CardCopy->SetText(FText::FromString(FString::Printf(TEXT("%s　●\n%s\n%s"), *Type, *Title, *Summary)));
		UHorizontalBoxSlot* CopySlot = CardRow->AddChildToHorizontalBox(CardCopy);
		CopySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CopySlot->SetVerticalAlignment(VAlign_Center);
		Card->SetContent(CardRow);
		UUniformGridSlot* CardSlot = EvidenceCardGrid->AddChildToUniformGrid(Card, CardIndex / 2, CardIndex % 2);
		CardSlot->SetHorizontalAlignment(HAlign_Fill);
		CardSlot->SetVerticalAlignment(VAlign_Fill);
		++CardIndex;
	};

	for (const FName EvidenceId : State.Evidence)
	{
		const FString Id = EvidenceId.ToString();
		const bool bFile = Id.Contains(TEXT("log")) || Id.Contains(TEXT("records"));
		const bool bWitness = Id.Contains(TEXT("diagnosis"));
		const FString Label = FWSPresentationText::EvidenceLabel(EvidenceId).ToString();
		int32 Separator = INDEX_NONE;
		const bool bHasSeparator = Label.FindChar(TEXT('：'), Separator);
		const FString Title = bHasSeparator ? Label.Left(Separator) : Label;
		const FString Summary = bHasSeparator ? Label.Mid(Separator + 1).TrimStart() : TEXT("已收录，可用于行动判断。");
		AddCard(
			Title,
			bFile ? TEXT("文件记录") : bWitness ? TEXT("目击信息") : TEXT("物品证据"),
			Summary,
			bFile ? Amber : bWitness ? Danger : Body,
			bFile ? TEXT("I_Evidence_File") : bWitness ? TEXT("I_Evidence_Witness") : TEXT("I_Evidence_Item"));
	}
	for (const TPair<FName, EWSKnowledgeLevel>& Pair : State.PlayerKnowledge)
	{
		if (Pair.Value == EWSKnowledgeLevel::Claimed || Pair.Value == EWSKnowledgeLevel::Suspected)
		{
			AddCard(
				FWSPresentationText::FactLabel(Pair.Key).ToString(),
				TEXT("对话记录"),
				TEXT("待交叉核验｜") + FWSPresentationText::KnowledgeLevel(Pair.Value).ToString(),
				Amber,
				TEXT("I_Evidence_Dialogue"));
		}
	}
	for (const TPair<FName, EWSKnowledgeLevel>& Pair : State.PlayerKnowledge)
	{
		if (Pair.Value == EWSKnowledgeLevel::Confirmed)
		{
			AddCard(
				FWSPresentationText::FactLabel(Pair.Key).ToString(),
				TEXT("目击信息"),
				TEXT("交叉核验完成，可用于行动判断。"),
				Danger,
				TEXT("I_Evidence_Witness"));
		}
	}
	for (const FWSPromiseRecord& Promise : State.Promises)
	{
		const FName StatusKey = !Promise.bSettled ? TEXT("ui_promise_pending") : Promise.bFulfilled ? TEXT("ui_promise_fulfilled") : TEXT("ui_promise_broken");
		const TCHAR* StatusFallback = !Promise.bSettled ? TEXT("进行中") : Promise.bFulfilled ? TEXT("已兑现") : TEXT("已违背");
		AddCard(
			FWSPresentationText::PromiseLabel(Promise.ConditionId).ToString(),
			TEXT("对话记录"),
			TEXT("承诺状态｜") + FWSPresentationText::UI(StatusKey, StatusFallback).ToString(),
			Promise.bSettled && !Promise.bFulfilled ? Danger : Amber,
			TEXT("I_Evidence_Dialogue"));
	}
	if (CardIndex == 0)
	{
		AddCard(TEXT("尚未取得证据"), TEXT("系统"), TEXT("调查设备、现场物品或与队员交谈。"), Body, TEXT("I_Evidence_File"));
	}
	EvidenceBorder->SetVisibility(bEvidenceVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UWhiteoutHUDWidget::UpdateResults(const FWSGameState& State)
{
	const bool bResults = State.Phase == EWSGamePhase::Results;
	if (!bResults)
	{
		ResultsBorder->SetVisibility(ESlateVisibility::Collapsed);
		bWasShowingResults = false;
		return;
	}
	if (bPresentationCaptureOverride && !bEndingCinematicCapture)
	{
		bEndingResultsRevealed = true;
		ResultsBorder->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		if (!bWasShowingResults)
		{
			BeginEndingCinematic(State.Ending);
		}
		ResultsBorder->SetVisibility(bEndingResultsRevealed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	bWasShowingResults = true;
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

void UWhiteoutHUDWidget::SetInteractionFocus(const FText& ActionName, const FWSActionPreview& Preview, const bool bDialogue)
{
	const FString NewName = ActionName.ToString();
	if (FocusedActionName != NewName)
	{
		FocusedActionName = NewName;
		PlayUISound(UIHoverSound, 0.42f);
	}
	if (CrosshairText)
	{
		CrosshairText->SetText(FText::FromString(TEXT("◆")));
		CrosshairText->SetColorAndOpacity(FSlateColor(Preview.bCanExecute ? Amber : Danger));
	}
	if (FocusBorder && FocusText)
	{
		FocusBorder->SetVisibility(ESlateVisibility::Visible);
		FocusBorder->SetBrushColor(Preview.bCanExecute
			? FLinearColor(0.0f, 0.0f, 0.0f, 0.28f)
			: FLinearColor(0.12f, 0.01f, 0.008f, 0.72f));
		FocusText->SetColorAndOpacity(FSlateColor(Preview.bCanExecute ? Body : Danger));
		if (bDialogue)
		{
			const FString DialogueFormat = FWSPresentationText::UI(
				TEXT("ui_focus_dialogue_format"),
				TEXT("{0}　｜　[F] 开始对话")).ToString();
			FocusText->SetText(FText::FromString(FString::Format(*DialogueFormat, {NewName})));
		}
		else
		{
			const FString Format = FWSPresentationText::UI(
				TEXT("ui_focus_format"),
				TEXT("{0}　｜　{1} AP　｜　[F] 查看行动")).ToString();
			FocusText->SetText(FText::FromString(FString::Format(*Format, {NewName, Preview.APCost})));
		}
	}
}

void UWhiteoutHUDWidget::ClearInteractionFocus()
{
	FocusedActionName.Reset();
	if (CrosshairText)
	{
		CrosshairText->SetText(FText::FromString(TEXT("+")));
		CrosshairText->SetColorAndOpacity(FSlateColor(Body));
	}
	if (FocusBorder)
	{
		FocusBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
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
		PlayUISound(UIRejectSound, 0.72f);
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

void UWhiteoutHUDWidget::SetActionFeedback(
	const FText& ActionName,
	const FWSActionResult& Result,
	const FWSActionPreview& Preview,
	const bool bPromiseCreated)
{
	HideActionPreview();
	if (Result.bCommitted)
	{
		PlayUISound(bPromiseCreated ? UIPromiseSound.Get() : UIConfirmSound.Get(), 0.84f);
		const FString CommittedFormat = FWSPresentationText::UI(TEXT("ui_feedback_committed"), TEXT("已执行：{0}　｜　行动力 {1} → {2}")).ToString();
		SystemMessage = FString::Format(*CommittedFormat, {ActionName.ToString(), Result.APBefore, Result.APAfter});
		if (bPromiseCreated)
		{
			SystemMessage = FWSPresentationText::UI(TEXT("ui_promise_recorded"), TEXT("承诺已记录　｜　")).ToString() + SystemMessage;
		}
		if (Result.bCrisisTriggered)
		{
			SystemMessage += FWSPresentationText::UI(TEXT("ui_feedback_crisis"), TEXT("　｜　备用电池电压崩溃，应急灯已接管")).ToString();
			CrisisElapsed = 0.0f;
			ActiveCrisisStage = INDEX_NONE;
			if (CrisisBorder)
			{
				CrisisBorder->SetVisibility(ESlateVisibility::Visible);
				CrisisBorder->SetRenderOpacity(0.0f);
			}
			ApplyCrisisStage(0);
		}
	}
	else
	{
		PlayUISound(UIRejectSound, 0.80f);
		const FString RejectedFormat = FWSPresentationText::UI(TEXT("ui_feedback_rejected"), TEXT("未执行：{0}　｜　{1}　｜　{2}")).ToString();
		SystemMessage = FString::Format(
			*RejectedFormat,
			{ActionName.ToString(), FWSPresentationText::ReasonCause(Result.ReasonCode).ToString(), FWSPresentationText::ReasonNextStep(Result.ReasonCode).ToString()});
	}
	if (ToastBorder && ToastText)
	{
		ToastText->SetText(FText::FromString(SystemMessage));
		ToastText->SetColorAndOpacity(FSlateColor(Result.bCommitted ? Body : FLinearColor(1.0f, 0.72f, 0.62f, 1.0f)));
		ToastBorder->SetBrushColor(Result.bCommitted
			? FLinearColor(0.0f, 0.0f, 0.0f, 0.18f)
			: FLinearColor(0.10f, 0.0f, 0.0f, 0.34f));
		ToastBorder->SetVisibility(ESlateVisibility::Visible);
		ToastBorder->SetRenderOpacity(1.0f);
		ToastRemaining = 2.7f;
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

void UWhiteoutHUDWidget::ShowDialogueMenu(const FName NPCActionId, const bool bVisible)
{
	bDialogueVisible = bVisible;
	ActiveDialogueActionId = bVisible ? NPCActionId : NAME_None;
	DialogueBorder->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		return;
	}
	HideActionPreview();
	bEvidenceVisible = false;
	ShowDialogueWheelChoices();
	DialogueStatusText->SetText(FWSPresentationText::UI(TEXT("ui_dialogue_footer"), TEXT("选择交涉方式；自由输入会自动降级，规则结果始终确定。")));
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			UpdateDialogueCard(StateSubsystem->GetStateSnapshot());
		}
	}
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetShowMouseCursor(true);
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(DialogueWheelPanel->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
}

void UWhiteoutHUDWidget::ShowDialogueWheelChoices()
{
	if (DialogueWheelPanel) DialogueWheelPanel->SetVisibility(ESlateVisibility::Visible);
	if (DialoguePromiseBorder) DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueFreeTextBorder) DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Collapsed);
}

void UWhiteoutHUDWidget::ShowDialoguePromiseChoices()
{
	if (DialogueWheelPanel) DialogueWheelPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueFreeTextBorder) DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialoguePromiseBorder) DialoguePromiseBorder->SetVisibility(ESlateVisibility::Visible);
}

void UWhiteoutHUDWidget::ShowDialogueFreeTextForCapture()
{
	OpenDialogueFreeText();
}

void UWhiteoutHUDWidget::SetDialogueIntentStatus(const FString& Message, const bool bProcessing)
{
	if (DialogueStatusText)
	{
		DialogueStatusText->SetText(FText::FromString(Message));
		DialogueStatusText->SetColorAndOpacity(FSlateColor(bProcessing ? Amber : Secondary));
	}
	if (bProcessing)
	{
		if (DialogueWheelPanel) DialogueWheelPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (DialoguePromiseBorder) DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);
		if (DialogueFreeTextBorder) DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWhiteoutHUDWidget::ChooseDialogueAsk()
{
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		Character->ChooseDialogueAct(EWSDialogueAct::Ask);
	}
}

void UWhiteoutHUDWidget::ChooseDialogueChallenge()
{
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		Character->ChooseDialogueAct(EWSDialogueAct::Challenge);
	}
}

void UWhiteoutHUDWidget::ChooseDialoguePromise()
{
	ShowDialoguePromiseChoices();
}

void UWhiteoutHUDWidget::ChooseDialogueReassure()
{
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		Character->ChooseDialogueAct(EWSDialogueAct::Reassure);
	}
}

void UWhiteoutHUDWidget::OpenDialogueFreeText()
{
	if (DialogueWheelPanel) DialogueWheelPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (DialoguePromiseBorder) DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueFreeTextBorder) DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Visible);
	if (DialogueFreeTextInput)
	{
		DialogueFreeTextInput->SetText(FText::GetEmpty());
		DialogueFreeTextInput->SetKeyboardFocus();
	}
	if (DialogueStatusText)
	{
		DialogueStatusText->SetText(FWSPresentationText::UI(TEXT("ui_dialogue_offline"), TEXT("在线模型不可用时会自动使用本地词典；仍不确定时回到轮盘。")));
	}
}

void UWhiteoutHUDWidget::ChoosePromiseKeepRecords()
{
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		Character->ChooseDialoguePromise(TEXT("keep_records"));
	}
}

void UWhiteoutHUDWidget::ChoosePromisePreventSelfHarm()
{
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		Character->ChooseDialoguePromise(TEXT("reserve_medicine"));
	}
}

void UWhiteoutHUDWidget::ChoosePromiseRepairTogether()
{
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		Character->ChooseDialoguePromise(TEXT("heat_repair_room"));
	}
}

void UWhiteoutHUDWidget::SubmitDialogueFreeText()
{
	if (!DialogueFreeTextInput)
	{
		return;
	}
	const FString UserText = DialogueFreeTextInput->GetText().ToString().TrimStartAndEnd();
	if (UserText.IsEmpty())
	{
		SetDialogueIntentStatus(TEXT("请输入一句完整的交涉内容。"), false);
		return;
	}
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		Character->SubmitDialogueText(UserText);
	}
}

void UWhiteoutHUDWidget::CancelDialogue()
{
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		Character->CancelDialogue();
	}
}

void UWhiteoutHUDWidget::HandleDialogueTextCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		SubmitDialogueFreeText();
	}
}

void UWhiteoutHUDWidget::HandleDialogueLine(const FWSAgentReply& Reply)
{
	if (!bDialogueVisible || Reply.ActionId != ActiveDialogueActionId || !DialogueStatusText)
	{
		return;
	}
	if (DialogueWheelPanel) DialogueWheelPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (DialoguePromiseBorder) DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueFreeTextBorder) DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Collapsed);
	const FString Speaker = Reply.Speaker == EWSCharacterId::GuHeng ? TEXT("顾衡") : TEXT("叶澄");
	const FString Provider = Reply.bFallback ? TEXT("本地确定性表达") : TEXT("在线表达（已校验）");
	DialogueStatusText->SetText(FText::FromString(FString::Printf(
		TEXT("%s：%s\n\n%s｜点击右下角返回现场"),
		*Speaker,
		*Reply.Utterance,
		*Provider)));
	DialogueStatusText->SetColorAndOpacity(FSlateColor(Body));
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
	if (ToastBorder) ToastBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (CrisisBorder) CrisisBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (EndingCinematicBorder) EndingCinematicBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (PauseBorder) PauseBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (PauseHelpText) PauseHelpText->SetVisibility(ESlateVisibility::Collapsed);
	bEndingCinematicCapture = false;
	bWasShowingResults = false;
	bEndingResultsRevealed = false;
	ToastRemaining = 0.0f;
	CrisisElapsed = -1.0f;
	EndingElapsed = -1.0f;
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

void UWhiteoutHUDWidget::SetOpeningCaptureStage(const int32 Stage)
{
	if (!OpeningBorder)
	{
		return;
	}
	OpeningBorder->SetVisibility(ESlateVisibility::Visible);
	OpeningElapsed = Stage == 0 ? 0.8f : Stage == 1 ? 3.6f : Stage == 2 ? 8.0f : 11.6f;
	ApplyOpeningStage(FMath::Clamp(Stage, 0, 3));
}

void UWhiteoutHUDWidget::SetCrisisCaptureStage(const int32 Stage)
{
	if (!CrisisBorder)
	{
		return;
	}
	// Capture stages are held instead of flowing into the next beat during the
	// screenshot settle delay. Runtime crisis playback still uses CrisisElapsed.
	CrisisElapsed = -1.0f;
	CrisisBorder->SetVisibility(ESlateVisibility::Visible);
	CrisisBorder->SetRenderOpacity(1.0f);
	ApplyCrisisStage(FMath::Clamp(Stage, 0, 2));
}

void UWhiteoutHUDWidget::SetEndingCaptureStage(const EWSEndingType Ending, const bool bShowResults)
{
	ActiveEnding = Ending;
	bEndingCinematicCapture = !bShowResults;
	if (bShowResults)
	{
		EndingElapsed = -1.0f;
		bWasShowingResults = true;
		bEndingResultsRevealed = true;
		if (EndingCinematicBorder) EndingCinematicBorder->SetVisibility(ESlateVisibility::Collapsed);
		if (ResultsBorder) ResultsBorder->SetVisibility(ESlateVisibility::Visible);
		return;
	}
	bEndingResultsRevealed = false;
	ApplyEndingCinematic(Ending);
	EndingElapsed = 1.25f;
	if (EndingCinematicBorder)
	{
		EndingCinematicBorder->SetVisibility(ESlateVisibility::Visible);
		EndingCinematicBorder->SetRenderOpacity(1.0f);
	}
	if (ResultsBorder) ResultsBorder->SetVisibility(ESlateVisibility::Collapsed);
}

void UWhiteoutHUDWidget::SetSystemMessage(const FString& Message)
{
	SystemMessage = Message;
}

void UWhiteoutHUDWidget::ApplyOpeningStage(const int32 Stage)
{
	if (!OpeningBorder || !OpeningText)
	{
		return;
	}
	ActiveOpeningStage = Stage;
	if (Stage == 0)
	{
		OpeningBorder->SetBrushColor(FLinearColor(0.002f, 0.008f, 0.016f, 0.985f));
		OpeningText->SetFont(UIFont(42, true));
		OpeningText->SetColorAndOpacity(FSlateColor(Body));
		OpeningText->SetText(FWSPresentationText::UI(
			TEXT("ui_opening_title"),
			TEXT("风雪站：断电前夜\n\n海拔 4,126 米｜极夜值班")));
	}
	else if (Stage == 1)
	{
		OpeningBorder->SetBrushColor(FLinearColor(0.002f, 0.012f, 0.024f, 0.34f));
		OpeningText->SetFont(UIFont(31, true));
		OpeningText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.92f, 1.0f, 1.0f)));
		const FString EstablishingCopy = FWSPresentationText::UI(
			TEXT("ui_opening_establishing"),
			TEXT("暴雪封山\n备用电池正在衰减\n\n按空格跳过")).ToString();
		OpeningText->SetText(FText::FromString(TEXT("\n\n") + EstablishingCopy));
	}
	else if (Stage == 2)
	{
		OpeningBorder->SetBrushColor(FLinearColor(0.003f, 0.010f, 0.020f, 0.88f));
		OpeningText->SetFont(UIFont(29, true));
		OpeningText->SetColorAndOpacity(FSlateColor(Body));
		OpeningText->SetText(FWSPresentationText::UI(
			TEXT("ui_opening_objective"),
			TEXT("最后一轮抢修\n\n① 修复发电机\n② 校准室外天线\n③ 发出求救信号\n\n预算：8 点行动力\n越过中段后，备用电池将发生一次故障\n\n按空格跳过")));
	}
	else
	{
		OpeningBorder->SetBrushColor(FLinearColor(0.003f, 0.010f, 0.020f, 0.91f));
		OpeningText->SetFont(UIFont(27, true));
		OpeningText->SetColorAndOpacity(FSlateColor(Cyan));
		OpeningText->SetText(FWSPresentationText::UI(
			TEXT("ui_opening_controls"),
			TEXT("每次行动都先预览，再确认\n\nWASD 移动　鼠标观察\nF 预览 / 再按 F 确认\nE 证据板　Q 对话方式　Esc 暂停/退出\n\n控制权交还")));
	}
}

void UWhiteoutHUDWidget::ApplyCrisisStage(const int32 Stage)
{
	if (!CrisisBorder || !CrisisText)
	{
		return;
	}
	ActiveCrisisStage = Stage;
	if (Stage == 0)
	{
		CrisisBorder->SetBrushColor(FLinearColor(0.30f, 0.006f, 0.002f, 0.72f));
		CrisisText->SetText(FWSPresentationText::UI(TEXT("ui_crisis_voltage_drop"), TEXT("电压骤降")));
	}
	else if (Stage == 1)
	{
		CrisisBorder->SetBrushColor(FLinearColor(0.015f, 0.018f, 0.028f, 0.92f));
		CrisisText->SetText(FWSPresentationText::UI(TEXT("ui_crisis_battery_offline"), TEXT("备用电池离线")));
	}
	else
	{
		CrisisBorder->SetBrushColor(FLinearColor(0.16f, 0.006f, 0.004f, 0.68f));
		CrisisText->SetText(FWSPresentationText::UI(
			TEXT("ui_crisis_emergency_load"),
			TEXT("应急负载接管\n剩余行动力进入红线")));
	}
}

void UWhiteoutHUDWidget::BeginEndingCinematic(const EWSEndingType Ending)
{
	ActiveEnding = Ending;
	bEndingResultsRevealed = false;
	EndingElapsed = 0.0f;
	ApplyEndingCinematic(Ending);
	if (EndingCinematicBorder)
	{
		EndingCinematicBorder->SetVisibility(ESlateVisibility::Visible);
		EndingCinematicBorder->SetRenderOpacity(0.0f);
	}
}

void UWhiteoutHUDWidget::ApplyEndingCinematic(const EWSEndingType Ending)
{
	if (!EndingCinematicBorder || !EndingCinematicText)
	{
		return;
	}
	FName Key(TEXT("ui_ending_collapse_cinematic"));
	const TCHAR* Fallback = TEXT("气象站失守\n电力与体温一同沉入黑暗");
	FLinearColor Panel(0.003f, 0.004f, 0.008f, 0.985f);
	FLinearColor TextColor(0.70f, 0.74f, 0.82f, 1.0f);
	if (Ending == EWSEndingType::TaskSuccess)
	{
		Key = TEXT("ui_ending_success_cinematic");
		Fallback = TEXT("信号穿过风雪\n远方电台传来回应");
		Panel = FLinearColor(0.015f, 0.07f, 0.085f, 0.93f);
		TextColor = FLinearColor(0.82f, 0.96f, 1.0f, 1.0f);
	}
	else if (Ending == EWSEndingType::SurvivalWait)
	{
		Key = TEXT("ui_ending_survival_cinematic");
		Fallback = TEXT("等待天明\n风雪仍在敲打外墙");
		Panel = FLinearColor(0.006f, 0.025f, 0.07f, 0.95f);
		TextColor = FLinearColor(0.68f, 0.82f, 1.0f, 1.0f);
	}
	else if (Ending == EWSEndingType::CostUncontrolled)
	{
		Key = TEXT("ui_ending_cost_cinematic");
		Fallback = TEXT("信号已经发出\n但代价越过了安全边界");
		Panel = FLinearColor(0.13f, 0.018f, 0.006f, 0.95f);
		TextColor = FLinearColor(1.0f, 0.68f, 0.42f, 1.0f);
	}
	EndingCinematicBorder->SetBrushColor(Panel);
	EndingCinematicText->SetColorAndOpacity(FSlateColor(TextColor));
	EndingCinematicText->SetText(FWSPresentationText::UI(Key, Fallback));
}

void UWhiteoutHUDWidget::PlayUISound(USoundBase* Sound, const float Volume)
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound, Volume);
	}
}

void UWhiteoutHUDWidget::PlayHoverSound()
{
	PlayUISound(UIHoverSound, 0.48f);
}

void UWhiteoutHUDWidget::DismissOpening()
{
	if (OpeningBorder)
	{
		OpeningBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (AWhiteoutGameMode* GameMode = GetWorld() ? Cast<AWhiteoutGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
	{
		GameMode->FinishOpeningPresentation();
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
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(PauseDefaultButton ? PauseDefaultButton->TakeWidget() : PauseBorder->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
}

void UWhiteoutHUDWidget::ResumeGame()
{
	PauseBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (PauseHelpText)
	{
		PauseHelpText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(false);
		PlayerController->bShowMouseCursor = false;
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}

void UWhiteoutHUDWidget::ToggleControls()
{
	if (!PauseHelpText)
	{
		return;
	}
	const bool bShow = PauseHelpText->GetVisibility() != ESlateVisibility::Visible;
	PauseHelpText->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	PlayUISound(bShow ? UIConfirmSound : UIHoverSound, 0.55f);
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
