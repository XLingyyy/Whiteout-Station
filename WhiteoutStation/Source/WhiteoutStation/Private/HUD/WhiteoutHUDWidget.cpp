#include "HUD/WhiteoutHUDWidget.h"

#include "Agents/WSAgentGateway.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BackgroundBlur.h"
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
#include "Components/Slider.h"
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
#include "HUD/WSUITokens.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputCoreTypes.h"
#include "Presentation/WSPresentationText.h"
#include "Player/WhiteoutCharacter.h"
#include "Settings/WhiteoutSettingsSubsystem.h"
#include "State/WindStationStateSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Sound/SoundBase.h"

namespace
{
	// 颜色别名 —— 全部来自 WSUITokens 单一可信来源
	const FLinearColor& PanelColor = WSUITokens::Color::SurfacePanel;
	const FLinearColor& DeepPanel = WSUITokens::Color::SurfaceDeep;
	const FLinearColor& Cyan = WSUITokens::Color::AccentInfo;
	const FLinearColor& Amber = WSUITokens::Color::AccentAction;
	const FLinearColor& Danger = WSUITokens::Color::AccentWarning;
	const FLinearColor& Body = WSUITokens::Color::TextPrimary;
	const FLinearColor& Secondary = WSUITokens::Color::TextSecondary;

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
	SetIsFocusable(true);
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
	TickPanelAnimations(InDeltaTime);
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

FReply UWhiteoutHUDWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleBackRequested();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::E && CurrentLayer == EWSUILayer::Evidence)
	{
		CloseEvidence();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UWhiteoutHUDWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRoot"));
	WidgetTree->RootWidget = Canvas;

	TopPanel = MakeGlassPanel(Canvas, TEXT("TopPanel"), FAnchors(0, 0), FMargin(20, 20, 340, 96), 12.0f, WSUITokens::Color::SurfacePanel);
	SetGlassPanelPadding(TopPanel, FMargin(10, 7));
	UVerticalBox* TopBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TopBox"));
	TopText = MakeText(TEXT("TopText"), 16, Body, false);
	TopText->SetFont(UIFont(16, true));
	TopStatusText = MakeText(TEXT("TopStatusText"), 15, Body, false);
	TopConditionText = MakeText(TEXT("TopConditionText"), 15, Secondary, false);
	TopBox->AddChildToVerticalBox(TopText);
	TopBox->AddChildToVerticalBox(TopStatusText)->SetPadding(FMargin(0, 2, 0, 0));
	TopBox->AddChildToVerticalBox(TopConditionText)->SetPadding(FMargin(0, 2, 0, 0));
	SetGlassPanelContent(TopPanel, TopBox);

	ObjectivePanel = MakeGlassPanel(Canvas, TEXT("ObjectivePanel"), FAnchors(0, 0), FMargin(20, 128, 300, 342), 12.0f, WSUITokens::Color::SurfacePanel);
	SetGlassPanelPadding(ObjectivePanel, FMargin(12));
	ObjectiveText = MakeText(TEXT("ObjectiveText"), 13, Body, false);
	ObjectiveText->SetLineHeightPercentage(1.25f);
	SetGlassPanelContent(ObjectivePanel, ObjectiveText);

	CrewPanel = MakeGlassPanel(Canvas, TEXT("CrewPanel"), FAnchors(1, 0), FMargin(-300, 20, 280, 520), 12.0f, WSUITokens::Color::SurfacePanel);
	SetGlassPanelPadding(CrewPanel, FMargin(12));
	UVerticalBox* CrewBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CrewBox"));
	SetGlassPanelContent(CrewPanel, CrewBox);
	CrewText = MakeText(TEXT("CrewHeader"), 16, Body);
	CrewText->SetText(FText::FromString(TEXT("值班组状态")));
	CrewBox->AddChildToVerticalBox(CrewText)->SetPadding(FMargin(0, 0, 0, 8));
	const TArray<FLinearColor> StatusColors = {
		WSUITokens::Color::StatusHealth,
		WSUITokens::Color::StatusTemperature,
		WSUITokens::Color::StatusEnergy,
		WSUITokens::Color::StatusHunger,
		WSUITokens::Color::StatusPressure};
	CrewCardTexts.Reset();
	CrewStatusBars.Reset();
	CrewTrustBars.Reset();
	for (int32 CharacterIndex = 0; CharacterIndex < 3; ++CharacterIndex)
	{
		UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("CrewCard%d"), CharacterIndex)));
		if (CharacterIndex == 0 && PlayerPortraitTexture)
		{
			USizeBox* PortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CrewPortraitBox0"));
			PortraitBox->SetWidthOverride(66.0f);
			PortraitBox->SetHeightOverride(88.0f);
			UImage* Portrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CrewPortrait0"));
			Portrait->SetBrushFromTexture(PlayerPortraitTexture, false);
			Portrait->SetDesiredSizeOverride(FVector2D(66.0f, 88.0f));
			PortraitBox->SetContent(Portrait);
			CardRow->AddChildToHorizontalBox(PortraitBox)->SetPadding(FMargin(0, 0, 10, 0));
		}

		UVerticalBox* CardInfo = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*FString::Printf(TEXT("CrewInfo%d"), CharacterIndex)));
		UTextBlock* CardText = MakeText(FName(*FString::Printf(TEXT("CrewText%d"), CharacterIndex)), 13, WSUITokens::Color::TextPrimary);
		CardInfo->AddChildToVerticalBox(CardText)->SetPadding(FMargin(0, 0, 0, 4));
		CrewCardTexts.Add(CardText);
		UTextBlock* StatusLegend = MakeText(FName(*FString::Printf(TEXT("CrewLegend%d"), CharacterIndex)), 10, WSUITokens::Color::TextSecondary, false);
		StatusLegend->SetText(FText::FromString(TEXT("健　温　精　饥　压")));
		CardInfo->AddChildToVerticalBox(StatusLegend)->SetPadding(FMargin(0, 0, 0, 2));
		UHorizontalBox* StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("CrewBars%d"), CharacterIndex)));
		for (int32 StatusIndex = 0; StatusIndex < 5; ++StatusIndex)
		{
			USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("CrewBarBox%d_%d"), CharacterIndex, StatusIndex)));
			BarBox->SetWidthOverride(31.0f);
			BarBox->SetHeightOverride(7.0f);
			UProgressBar* Bar = MakeProgressBar(FName(*FString::Printf(TEXT("CrewBar%d_%d"), CharacterIndex, StatusIndex)), StatusColors[StatusIndex]);
			BarBox->SetContent(Bar);
			StatusRow->AddChildToHorizontalBox(BarBox)->SetPadding(FMargin(0, 0, 3, 0));
			CrewStatusBars.Add(Bar);
		}
		CardInfo->AddChildToVerticalBox(StatusRow)->SetPadding(FMargin(0, 0, 0, 4));
		if (CharacterIndex > 0)
		{
			UTextBlock* TrustLabel = MakeText(FName(*FString::Printf(TEXT("CrewTrustLabel%d"), CharacterIndex)), 10, WSUITokens::Color::TextSecondary, false);
			TrustLabel->SetText(FText::FromString(TEXT("信任")));
			CardInfo->AddChildToVerticalBox(TrustLabel);
			USizeBox* TrustBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("CrewTrustBox%d"), CharacterIndex)));
			TrustBox->SetHeightOverride(6.0f);
			UProgressBar* TrustBar = MakeProgressBar(FName(*FString::Printf(TEXT("CrewTrust%d"), CharacterIndex)), WSUITokens::Color::TrustBar);
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

	BottomPanel = MakeGlassPanel(Canvas, TEXT("BottomPanel"), FAnchors(0.5f, 1.0f), FMargin(-420, -142, 840, 122), 12.0f, WSUITokens::Color::SurfacePanel);
	SetGlassPanelPadding(BottomPanel, FMargin(12, 8));
	UVerticalBox* BottomBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BottomBox"));
	SetGlassPanelContent(BottomPanel, BottomBox);
	FeedbackText = MakeText(TEXT("FeedbackText"), 14, Body);
	PromptText = MakeText(TEXT("PromptText"), 16, Amber);
	UTextBlock* HelpText = MakeText(TEXT("HelpText"), 12, Secondary);
	HelpText->SetText(FWSPresentationText::UI(
		TEXT("ui_help_v04"),
		TEXT("WASD 移动　鼠标观察　Space 跳跃　F 互动/对话　E 证据板　Enter 结束　Esc 返回/暂停")));
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
	FocusBorder = MakePanel(Canvas, TEXT("FocusPanel"), FAnchors(0.5f, 0.5f), FMargin(-270, 42, 540, 78), FLinearColor::Transparent);
	FocusBorder->SetPadding(FMargin(8));
	UOverlay* FocusOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("FocusOverlay"));
	if (InkBrushTexture)
	{
		UImage* FocusBrush = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FocusBrush"));
		FocusBrush->SetBrushFromTexture(InkBrushTexture, true);
		FocusBrush->SetColorAndOpacity(FLinearColor(0.02f, 0.03f, 0.05f, 0.92f));
		FocusOverlay->AddChildToOverlay(FocusBrush);
	}
	UVerticalBox* FocusContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FocusContent"));
	UTextBlock* FocusMarker = MakeText(TEXT("FocusMarker"), 11, Amber, false);
	FocusMarker->SetText(FText::FromString(TEXT("◆")));
	FocusMarker->SetJustification(ETextJustify::Center);
	FocusContent->AddChildToVerticalBox(FocusMarker);
	UHorizontalBox* FocusLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FocusLine"));
	FocusText = MakeText(TEXT("FocusText"), 15, Body, false);
	FocusText->SetJustification(ETextJustify::Center);
	FocusLine->AddChildToHorizontalBox(FocusText)->SetVerticalAlignment(VAlign_Center);
	FocusAPText = MakeText(TEXT("FocusAPText"), 15, Amber, false);
	FocusAPText->SetJustification(ETextJustify::Center);
	FocusLine->AddChildToHorizontalBox(FocusAPText)->SetVerticalAlignment(VAlign_Center);
	FocusKeyText = MakeText(TEXT("FocusKeyText"), 15, Body, false);
	FocusKeyText->SetJustification(ETextJustify::Center);
	FocusLine->AddChildToHorizontalBox(FocusKeyText)->SetVerticalAlignment(VAlign_Center);
	UVerticalBoxSlot* FocusLineSlot = FocusContent->AddChildToVerticalBox(FocusLine);
	FocusLineSlot->SetHorizontalAlignment(HAlign_Center);
	UOverlaySlot* FocusContentSlot = FocusOverlay->AddChildToOverlay(FocusContent);
	FocusContentSlot->SetHorizontalAlignment(HAlign_Center);
	FocusContentSlot->SetVerticalAlignment(VAlign_Center);
	FocusBorder->SetContent(FocusOverlay);
	FocusBorder->SetVisibility(ESlateVisibility::Collapsed);

	PreviewBorder = MakePanel(Canvas, TEXT("PreviewPanel"), FAnchors(0.16f, 0.10f, 0.84f, 0.90f), FMargin(0), WSUITokens::Color::SurfacePreview);
	UVerticalBox* PreviewBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PreviewBox"));
	PreviewBorder->SetContent(PreviewBox);
	PreviewTitleText = MakeText(TEXT("PreviewTitle"), 28, Cyan);
	PreviewBodyText = MakeText(TEXT("PreviewBody"), 18, Body);
	PreviewFooterText = MakeText(TEXT("PreviewFooter"), 18, Amber);
	PreviewBox->AddChildToVerticalBox(PreviewTitleText)->SetPadding(FMargin(0, 0, 0, 14));
	PreviewBox->AddChildToVerticalBox(PreviewBodyText)->SetPadding(FMargin(0, 0, 0, 16));
	PreviewBox->AddChildToVerticalBox(PreviewFooterText);
	PreviewBorder->SetVisibility(ESlateVisibility::Collapsed);

	EvidenceBorder = MakeGlassPanel(Canvas, TEXT("EvidencePanel"), FAnchors(0.12f, 0.08f, 0.88f, 0.92f), FMargin(0), 16.0f, WSUITokens::Color::SurfaceDeep);
	SetGlassPanelPadding(EvidenceBorder, FMargin(18));
	UVerticalBox* EvidenceBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EvidenceBox"));
	UHorizontalBox* EvidenceTitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EvidenceTitleRow"));
	EvidenceTitleText = MakeText(TEXT("EvidenceTitle"), 20, Body);
	EvidenceTitleText->SetFont(UIFont(20, true));
	UHorizontalBoxSlot* EvidenceTitleSlot = EvidenceTitleRow->AddChildToHorizontalBox(EvidenceTitleText);
	EvidenceTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	EvidenceTitleSlot->SetVerticalAlignment(VAlign_Center);
	UButton* EvidenceCloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EvidenceCloseButton"));
	EvidenceCloseButton->SetBackgroundColor(WSUITokens::Color::ButtonNormal);
	UTextBlock* EvidenceCloseText = MakeText(TEXT("EvidenceCloseText"), 13, Secondary, false);
	EvidenceCloseText->SetText(FText::FromString(TEXT("✕ 关闭")));
	EvidenceCloseButton->SetContent(EvidenceCloseText);
	EvidenceCloseButton->OnHovered.AddDynamic(this, &UWhiteoutHUDWidget::PlayHoverSound);
	EvidenceCloseButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::CloseEvidence);
	EvidenceTitleRow->AddChildToHorizontalBox(EvidenceCloseButton)->SetVerticalAlignment(VAlign_Center);
	EvidenceBox->AddChildToVerticalBox(EvidenceTitleRow)->SetPadding(FMargin(0, 0, 0, 12));
	UHorizontalBox* EvidenceMain = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EvidenceMain"));
	USizeBox* FilterSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EvidenceFilterSize"));
	FilterSize->SetWidthOverride(205.0f);
	UBorder* FilterPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EvidenceFilterPanel"));
	FilterPanel->SetBrushColor(WSUITokens::Color::SurfaceFilter);
	FilterPanel->SetPadding(FMargin(14));
	UVerticalBox* FilterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EvidenceFilterBox"));
	EvidenceFilterText = MakeText(TEXT("EvidenceFilters"), 13, Secondary);
	EvidenceFilterText->SetText(FText::FromString(TEXT("类型过滤")));
	FilterBox->AddChildToVerticalBox(EvidenceFilterText)->SetPadding(FMargin(0, 0, 0, 10));
	EvidenceFilterButtons.Reset();
	EvidenceFilterIndicators.Reset();
	EvidenceFilterLabels.Reset();
	EvidenceFilterCounts.Reset();
	const TArray<FString> FilterLabels = {TEXT("全部"), TEXT("文件记录"), TEXT("物品证据"), TEXT("目击信息"), TEXT("对话记录")};
	for (int32 FilterIndex = 0; FilterIndex < FilterLabels.Num(); ++FilterIndex)
	{
		UButton* FilterButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*FString::Printf(TEXT("EvidenceFilterButton%d"), FilterIndex)));
		FilterButton->SetBackgroundColor(FLinearColor::Transparent);
		FilterButton->OnHovered.AddDynamic(this, &UWhiteoutHUDWidget::PlayHoverSound);
		switch (FilterIndex)
		{
		case 0: FilterButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::FilterEvidenceAll); break;
		case 1: FilterButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::FilterEvidenceFiles); break;
		case 2: FilterButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::FilterEvidenceItems); break;
		case 3: FilterButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::FilterEvidenceWitnesses); break;
		default: FilterButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::FilterEvidenceDialogue); break;
		}
		UHorizontalBox* FilterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("EvidenceFilterRow%d"), FilterIndex)));
		USizeBox* IndicatorSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("EvidenceFilterIndicatorSize%d"), FilterIndex)));
		IndicatorSize->SetWidthOverride(2.0f);
		UBorder* Indicator = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("EvidenceFilterIndicator%d"), FilterIndex)));
		Indicator->SetBrushColor(FilterIndex == 0 ? Amber : FLinearColor::Transparent);
		IndicatorSize->SetContent(Indicator);
		FilterRow->AddChildToHorizontalBox(IndicatorSize)->SetPadding(FMargin(0, 2, 10, 2));
		UTextBlock* Label = MakeText(FName(*FString::Printf(TEXT("EvidenceFilterLabel%d"), FilterIndex)), 13, FilterIndex == 0 ? Body : Secondary, false);
		Label->SetText(FText::FromString(FilterLabels[FilterIndex]));
		UHorizontalBoxSlot* LabelSlot = FilterRow->AddChildToHorizontalBox(Label);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Count = MakeText(FName(*FString::Printf(TEXT("EvidenceFilterCount%d"), FilterIndex)), 12, Secondary, false);
		Count->SetText(FText::FromString(TEXT("00")));
		FilterRow->AddChildToHorizontalBox(Count)->SetVerticalAlignment(VAlign_Center);
		FilterButton->SetContent(FilterRow);
		FilterBox->AddChildToVerticalBox(FilterButton)->SetPadding(FMargin(0, 3));
		EvidenceFilterButtons.Add(FilterButton);
		EvidenceFilterIndicators.Add(Indicator);
		EvidenceFilterLabels.Add(Label);
		EvidenceFilterCounts.Add(Count);
	}
	FilterPanel->SetContent(FilterBox);
	FilterSize->SetContent(FilterPanel);
	EvidenceMain->AddChildToHorizontalBox(FilterSize)->SetPadding(FMargin(0, 0, 14, 0));
	UVerticalBox* EvidenceContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EvidenceContent"));
	EvidenceScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("EvidenceScroll"));
	EvidenceCardGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("EvidenceCardGrid"));
	EvidenceCardGrid->SetMinDesiredSlotWidth(300.0f);
	EvidenceScroll->AddChild(EvidenceCardGrid);
	UVerticalBoxSlot* EvidenceScrollSlot = EvidenceContent->AddChildToVerticalBox(EvidenceScroll);
	EvidenceScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UBorder* EvidenceDetailPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EvidenceDetailPanel"));
	EvidenceDetailPanel->SetBrushColor(WSUITokens::Color::SurfaceFilter);
	EvidenceDetailPanel->SetPadding(FMargin(12));
	EvidenceDetailText = MakeText(TEXT("EvidenceDetailText"), 14, Body);
	EvidenceDetailText->SetText(FText::FromString(TEXT("选择一条记录查看细节。")));
	EvidenceDetailPanel->SetContent(EvidenceDetailText);
	EvidenceContent->AddChildToVerticalBox(EvidenceDetailPanel)->SetPadding(FMargin(0, 10, 0, 0));
	UHorizontalBoxSlot* EvidenceCardsSlot = EvidenceMain->AddChildToHorizontalBox(EvidenceContent);
	EvidenceCardsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBoxSlot* EvidenceMainSlot = EvidenceBox->AddChildToVerticalBox(EvidenceMain);
	EvidenceMainSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	EvidenceProgressText = MakeText(TEXT("EvidenceProgress"), 13, Secondary);
	EvidenceBox->AddChildToVerticalBox(EvidenceProgressText)->SetPadding(FMargin(0, 12, 0, 0));
	SetGlassPanelContent(EvidenceBorder, EvidenceBox);
	EvidenceBorder->SetVisibility(ESlateVisibility::Collapsed);

	DialogueBorder = MakePanel(Canvas, TEXT("DialoguePanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor::Transparent);
	DialogueBorder->SetPadding(FMargin(0));
	UCanvasPanel* DialogueCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialogueCanvas"));
	DialogueBorder->SetContent(DialogueCanvas);
	UBorder* DialogueBar = MakeGlassPanel(DialogueCanvas, TEXT("DialogueBar"), FAnchors(0.18f, 0.68f, 0.82f, 0.97f), FMargin(0), 18.0f, WSUITokens::Color::SurfaceDialogue);
	SetGlassPanelPadding(DialogueBar, FMargin(14, 10));
	UVerticalBox* DialogueBarBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueBarBox"));
	SetGlassPanelContent(DialogueBar, DialogueBarBox);
	DialogueNameText = MakeText(TEXT("DialogueNameText"), 15, Cyan, false);
	DialogueNameText->SetFont(UIFont(15, true));
	DialogueLineText = MakeText(TEXT("DialogueLineText"), 16, Body);
	DialogueLineText->SetLineHeightPercentage(1.2f);
	DialogueText = DialogueLineText;
	DialogueStatusText = DialogueLineText;
	UVerticalBoxSlot* DialogueNameSlot = DialogueBarBox->AddChildToVerticalBox(DialogueNameText);
	DialogueNameSlot->SetPadding(FMargin(0, 0, 0, 3));
	DialogueNameSlot->SetHorizontalAlignment(HAlign_Fill);
	UVerticalBoxSlot* DialogueLineSlot = DialogueBarBox->AddChildToVerticalBox(DialogueLineText);
	DialogueLineSlot->SetPadding(FMargin(0, 0, 0, 7));
	DialogueLineSlot->SetHorizontalAlignment(HAlign_Fill);

	DialogueWheelPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialogueWheel"));
	USizeBox* IntentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DialogueIntentSize"));
	IntentSize->SetHeightOverride(52.0f);
	IntentSize->SetContent(DialogueWheelPanel);
	UVerticalBoxSlot* IntentSlot = DialogueBarBox->AddChildToVerticalBox(IntentSize);
	IntentSlot->SetHorizontalAlignment(HAlign_Fill);
	UButton* AskButton = MakeDialogueChoiceButton(DialogueWheelPanel, FWSPresentationText::UI(TEXT("dialogue_ask"), TEXT("询问")), TEXT("I_Dialogue_Inquire"), TEXT("DialogueAsk"), FAnchors(0.125f, 0.5f), FMargin(-70, -24, 140, 48));
	UButton* ChallengeButton = MakeDialogueChoiceButton(DialogueWheelPanel, FWSPresentationText::UI(TEXT("dialogue_challenge"), TEXT("质疑")), TEXT("I_Dialogue_Doubt"), TEXT("DialogueChallenge"), FAnchors(0.375f, 0.5f), FMargin(-70, -24, 140, 48));
	UButton* ReassureButton = MakeDialogueChoiceButton(DialogueWheelPanel, FWSPresentationText::UI(TEXT("dialogue_reassure"), TEXT("安抚")), TEXT("I_Dialogue_Comfort"), TEXT("DialogueReassure"), FAnchors(0.625f, 0.5f), FMargin(-70, -24, 140, 48));
	UButton* PromiseButton = MakeDialogueChoiceButton(DialogueWheelPanel, FWSPresentationText::UI(TEXT("dialogue_promise"), TEXT("承诺")), TEXT("I_Dialogue_Promise"), TEXT("DialoguePromise"), FAnchors(0.875f, 0.5f), FMargin(-70, -24, 140, 48));
	AskButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChooseDialogueAsk);
	ChallengeButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChooseDialogueChallenge);
	ReassureButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChooseDialogueReassure);
	PromiseButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChooseDialoguePromise);
	DialogueIntentButtons = {AskButton, ChallengeButton, ReassureButton, PromiseButton};

	DialoguePromiseBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialoguePromisePanel"));
	DialoguePromiseBorder->SetBrushColor(FLinearColor::Transparent);
	DialoguePromiseBorder->SetPadding(FMargin(0));
	UVerticalBox* PromiseBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialoguePromiseBox"));
	DialoguePromiseBorder->SetContent(PromiseBox);
	UTextBlock* PromiseTitle = MakeText(TEXT("DialoguePromiseTitle"), 13, Amber);
	PromiseTitle->SetText(FWSPresentationText::UI(TEXT("ui_dialogue_promise_title"), TEXT("选择承诺条件，再用自己的话发送")));
	PromiseBox->AddChildToVerticalBox(PromiseTitle)->SetPadding(FMargin(0, 0, 0, 3));
	UButton* KeepRecordsButton = MakeButton(PromiseBox, FWSPresentationText::UI(TEXT("dialogue_promise_records"), TEXT("不弃站｜保存记录")), TEXT("PromiseKeepRecords"));
	UButton* PreventSelfHarmButton = MakeButton(PromiseBox, FWSPresentationText::UI(TEXT("dialogue_promise_medicine"), TEXT("不放任自伤｜保留药品")), TEXT("PromisePreventSelfHarm"));
	UButton* RepairTogetherButton = MakeButton(PromiseBox, FWSPresentationText::UI(TEXT("dialogue_promise_heat"), TEXT("配合修复｜维修间升温")), TEXT("PromiseRepairTogether"));
	KeepRecordsButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChoosePromiseKeepRecords);
	PreventSelfHarmButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChoosePromisePreventSelfHarm);
	RepairTogetherButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChoosePromiseRepairTogether);
	DialogueBarBox->AddChildToVerticalBox(DialoguePromiseBorder);
	DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);

	DialogueFreeTextBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueFreeTextPanel"));
	DialogueFreeTextBorder->SetBrushColor(FLinearColor::Transparent);
	DialogueFreeTextBorder->SetPadding(FMargin(0));
	UVerticalBox* FreeTextBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueFreeTextBox"));
	DialogueFreeTextBorder->SetContent(FreeTextBox);
	DialogueFreeTextInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("DialogueFreeTextInput"));
	DialogueFreeTextInput->SetHintText(FWSPresentationText::UI(TEXT("dlg_hint_ask_v04"), TEXT("例：继电器烧了之后，还有什么能替？")));
	FEditableTextBoxStyle DialogueInputStyle = DialogueFreeTextInput->GetWidgetStyle();
	DialogueInputStyle.SetFont(UIFont(15));
	DialogueInputStyle.BackgroundImageNormal.TintColor = FSlateColor(WSUITokens::Color::SurfaceInput);
	DialogueInputStyle.BackgroundImageHovered.TintColor = FSlateColor(WSUITokens::Color::SurfaceInputFocused);
	DialogueInputStyle.BackgroundImageFocused.TintColor = FSlateColor(WSUITokens::Color::SurfaceInputFocused);
	DialogueInputStyle.ForegroundColor = FSlateColor(WSUITokens::Color::TextPrimary);
	DialogueInputStyle.BackgroundColor = FSlateColor(WSUITokens::Color::SurfaceInput);
	DialogueFreeTextInput->SetWidgetStyle(DialogueInputStyle);
	DialogueFreeTextInput->SetForegroundColor(FLinearColor::White);
	DialogueFreeTextInput->OnTextCommitted.AddDynamic(this, &UWhiteoutHUDWidget::HandleDialogueTextCommitted);
	FreeTextBox->AddChildToVerticalBox(DialogueFreeTextInput)->SetPadding(FMargin(0, 2, 0, 3));
	UButton* SubmitTextButton = MakeButton(FreeTextBox, FWSPresentationText::UI(TEXT("ui_dialogue_submit_v04"), TEXT("发送")), TEXT("DialogueTextSubmit"));
	SubmitTextButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::SubmitDialogueFreeText);
	DialogueBarBox->AddChildToVerticalBox(DialogueFreeTextBorder);
	DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Collapsed);

	DialogueReplyBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueReplyPanel"));
	DialogueReplyBorder->SetBrushColor(FLinearColor::Transparent);
	UVerticalBox* ReplyBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueReplyBox"));
	DialogueReplyBorder->SetContent(ReplyBox);
	UButton* ContinueButton = MakeButton(ReplyBox, FWSPresentationText::UI(TEXT("dlg_continue_button_v04"), TEXT("继续交涉")), TEXT("DialogueContinue"));
	UButton* EndDialogueButton = MakeButton(ReplyBox, FWSPresentationText::UI(TEXT("dlg_end_button_v04"), TEXT("结束对话")), TEXT("DialogueEnd"));
	ContinueButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ContinueDialogue);
	EndDialogueButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::CancelDialogue);
	DialogueBarBox->AddChildToVerticalBox(DialogueReplyBorder);
	DialogueReplyBorder->SetVisibility(ESlateVisibility::Collapsed);

	UBorder* NPCCard = MakeGlassPanel(DialogueCanvas, TEXT("DialogueNPCCard"), FAnchors(0.76f, 0.06f, 0.97f, 0.28f), FMargin(0), 12.0f, FLinearColor(0.035f, 0.071f, 0.102f, 0.94f));
	SetGlassPanelPadding(NPCCard, FMargin(10));
	UVerticalBox* NPCCardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueNPCCardBox"));
	SetGlassPanelContent(NPCCard, NPCCardBox);
	DialogueNPCText = MakeText(TEXT("DialogueNPCText"), 13, Body);
	NPCCardBox->AddChildToVerticalBox(DialogueNPCText)->SetPadding(FMargin(0, 0, 0, 4));
	DialogueNPCBars.Reset();
	const TArray<FLinearColor> DialogueBarColors = {WSUITokens::Color::AccentWarning, WSUITokens::Color::AccentInfo, WSUITokens::Color::AccentAction, FLinearColor(0.58f, 0.76f, 0.92f, 1.0f)};
	for (int32 Index = 0; Index < DialogueBarColors.Num(); ++Index)
	{
		UProgressBar* Bar = MakeProgressBar(FName(*FString::Printf(TEXT("DialogueNPCBar%d"), Index)), DialogueBarColors[Index]);
		NPCCardBox->AddChildToVerticalBox(Bar)->SetPadding(FMargin(0, 1, 0, 2));
		DialogueNPCBars.Add(Bar);
	}
	UButton* DialogueCancelButton = MakeDialogueChoiceButton(DialogueCanvas, FWSPresentationText::UI(TEXT("ui_dialogue_leave_v04"), TEXT("离开")), TEXT(""), TEXT("DialogueCancel"), FAnchors(0.84f, 0.90f, 0.95f, 0.96f), FMargin(0));
	DialogueCancelButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::CancelDialogue);
	DialogueBorder->SetVisibility(ESlateVisibility::Collapsed);

	ResultsBorder = MakePanel(Canvas, TEXT("ResultsPanel"), FAnchors(0, 0, 1, 1), FMargin(0), WSUITokens::Color::SurfaceFullscreen);
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
	const TArray<FLinearColor> ScoreColors = {WSUITokens::Color::AccentInfo, WSUITokens::Color::AccentSuccess, WSUITokens::Color::AccentAction, FLinearColor(0.72f, 0.55f, 1.0f), FLinearColor(0.45f, 0.72f, 1.0f)};
	for (int32 Index = 0; Index < ScoreLabels.Num(); ++Index)
	{
		UTextBlock* ScoreText = MakeText(FName(*FString::Printf(TEXT("ResultScoreText%d"), Index)), 16, WSUITokens::Color::TextPrimary);
		ScoreText->SetText(ScoreLabels[Index]);
		ResultsBox->AddChildToVerticalBox(ScoreText)->SetPadding(FMargin(0, 5, 0, 3));
		UProgressBar* ScoreBar = MakeProgressBar(FName(*FString::Printf(TEXT("ResultScoreBar%d"), Index)), ScoreColors[Index]);
		ScoreBar->SetPercent(0.0f);
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
	UHorizontalBox* GlassSamples = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GalleryGlassSamples"));
	const TArray<FVector2D> GlassSampleSizes = {FVector2D(180, 62), FVector2D(260, 72), FVector2D(340, 82)};
	for (int32 SampleIndex = 0; SampleIndex < GlassSampleSizes.Num(); ++SampleIndex)
	{
		USizeBox* SampleSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("GalleryGlassSize%d"), SampleIndex)));
		SampleSize->SetWidthOverride(GlassSampleSizes[SampleIndex].X);
		SampleSize->SetHeightOverride(GlassSampleSizes[SampleIndex].Y);
		UBorder* SamplePanel = MakeGlassPanel(nullptr, FName(*FString::Printf(TEXT("GalleryGlassPanel%d"), SampleIndex)), FAnchors(), FMargin(), 12.0f + SampleIndex * 2.0f, WSUITokens::Color::SurfacePanel);
		SetGlassPanelPadding(SamplePanel, FMargin(8));
		UTextBlock* SampleText = MakeText(FName(*FString::Printf(TEXT("GalleryGlassText%d"), SampleIndex)), 13, Body, false);
		SampleText->SetText(FText::FromString(FString::Printf(TEXT("毛玻璃 %d｜Blur %.0f"), SampleIndex + 1, 12.0f + SampleIndex * 2.0f)));
		SampleText->SetJustification(ETextJustify::Center);
		SetGlassPanelContent(SamplePanel, SampleText);
		SampleSize->SetContent(SamplePanel);
		GlassSamples->AddChildToHorizontalBox(SampleSize)->SetPadding(FMargin(SampleIndex == 0 ? 0.0f : 10.0f, 0, 0, 0));
	}
	GalleryBox->AddChildToVerticalBox(GlassSamples)->SetPadding(FMargin(0, 0, 0, 12));
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

	ToastBorder = MakePanel(Canvas, TEXT("ActionToast"), FAnchors(0.27f, 0.70f, 0.73f, 0.70f), FMargin(0, 0, 0, 104), FLinearColor::Transparent);
	UOverlay* ToastOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ToastOverlay"));
	if (InkBrushTexture)
	{
		UImage* ToastBrush = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ToastBrush"));
		ToastBrush->SetBrushFromTexture(InkBrushTexture, true);
		ToastBrush->SetColorAndOpacity(FLinearColor(0.02f, 0.03f, 0.05f, 0.92f));
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
	EndingCinematicBorder->SetPadding(FMargin(0));
	{
		UOverlay* EndingOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("EndingOverlay"));
		if (InkBrushTexture)
		{
			UImage* EndingBrush = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EndingBrush"));
			EndingBrush->SetBrushFromTexture(InkBrushTexture, true);
			EndingBrush->SetColorAndOpacity(FLinearColor(0.008f, 0.012f, 0.022f, 0.6f));
			EndingOverlay->AddChildToOverlay(EndingBrush);
		}
		UVerticalBox* EndingBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EndingBox"));
		EndingCinematicText = MakeText(TEXT("EndingCinematicTitleText"), 38, WSUITokens::Color::TextPrimary, false);
		EndingCinematicText->SetJustification(ETextJustify::Center);
		EndingBox->AddChildToVerticalBox(EndingCinematicText)->SetPadding(FMargin(0, 0, 0, 10));
		EndingDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EndingDivider"));
		EndingDivider->SetBrushColor(WSUITokens::Color::StrokeDivider);
		USizeBox* EndingDividerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EndingDividerSize"));
		EndingDividerSize->SetWidthOverride(220.0f);
		EndingDividerSize->SetHeightOverride(1.0f);
		EndingDividerSize->SetContent(EndingDivider);
		UVerticalBoxSlot* EndingDividerSlot = EndingBox->AddChildToVerticalBox(EndingDividerSize);
		EndingDividerSlot->SetHorizontalAlignment(HAlign_Center);
		EndingDividerSlot->SetPadding(FMargin(0, 0, 0, 14));
		EndingSubtitleText = MakeText(TEXT("EndingSubtitleText"), 20, WSUITokens::Color::TextSecondary);
		EndingSubtitleText->SetJustification(ETextJustify::Center);
		EndingBox->AddChildToVerticalBox(EndingSubtitleText);
		UOverlaySlot* EndingBoxSlot = EndingOverlay->AddChildToOverlay(EndingBox);
		EndingBoxSlot->SetHorizontalAlignment(HAlign_Center);
		EndingBoxSlot->SetVerticalAlignment(VAlign_Center);
		EndingCinematicBorder->SetContent(EndingOverlay);
	}
	EndingCinematicBorder->SetVisibility(ESlateVisibility::Collapsed);

	CrisisBorder = MakePanel(Canvas, TEXT("CrisisPanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.23f, 0.005f, 0.003f, 0.72f));
	CrisisText = MakeText(TEXT("CrisisText"), 40, WSUITokens::Color::AccentWarning);
	CrisisText->SetJustification(ETextJustify::Center);
	CrisisBorder->SetContent(CrisisText);
	CrisisBorder->SetVisibility(ESlateVisibility::Collapsed);

	OpeningBorder = MakePanel(Canvas, TEXT("OpeningPanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.003f, 0.010f, 0.020f, 0.96f));
	OpeningBorder->SetPadding(FMargin(0));
	{
		UOverlay* OpeningOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("OpeningOverlay"));
		if (InkBrushTexture)
		{
			UImage* OpeningBrush = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("OpeningBrush"));
			OpeningBrush->SetBrushFromTexture(InkBrushTexture, true);
			OpeningBrush->SetColorAndOpacity(FLinearColor(0.01f, 0.015f, 0.025f, 0.55f));
			OpeningOverlay->AddChildToOverlay(OpeningBrush);
		}
		UVerticalBox* OpeningBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OpeningBox"));
		OpeningText = MakeText(TEXT("OpeningTitleText"), 42, WSUITokens::Color::TextPrimary, false);
		OpeningText->SetJustification(ETextJustify::Center);
		OpeningBox->AddChildToVerticalBox(OpeningText)->SetPadding(FMargin(0, 0, 0, 10));
		OpeningDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OpeningDivider"));
		OpeningDivider->SetBrushColor(WSUITokens::Color::StrokeDivider);
		USizeBox* OpeningDividerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("OpeningDividerSize"));
		OpeningDividerSize->SetWidthOverride(280.0f);
		OpeningDividerSize->SetHeightOverride(1.0f);
		OpeningDividerSize->SetContent(OpeningDivider);
		UVerticalBoxSlot* OpeningDividerSlot = OpeningBox->AddChildToVerticalBox(OpeningDividerSize);
		OpeningDividerSlot->SetHorizontalAlignment(HAlign_Center);
		OpeningDividerSlot->SetPadding(FMargin(0, 0, 0, 14));
		OpeningSubtitleText = MakeText(TEXT("OpeningSubtitleText"), 20, WSUITokens::Color::TextCinematicWarm);
		OpeningSubtitleText->SetJustification(ETextJustify::Center);
		OpeningBox->AddChildToVerticalBox(OpeningSubtitleText)->SetPadding(FMargin(0, 0, 0, 18));
		OpeningFooterText = MakeText(TEXT("OpeningFooterText"), 14, WSUITokens::Color::TextSecondary, false);
		OpeningFooterText->SetJustification(ETextJustify::Center);
		OpeningBox->AddChildToVerticalBox(OpeningFooterText);
		UOverlaySlot* OpeningBoxSlot = OpeningOverlay->AddChildToOverlay(OpeningBox);
		OpeningBoxSlot->SetHorizontalAlignment(HAlign_Center);
		OpeningBoxSlot->SetVerticalAlignment(VAlign_Center);
		OpeningBorder->SetContent(OpeningOverlay);
	}
	ApplyOpeningStage(0);

	PauseBorder = MakeGlassPanel(Canvas, TEXT("PausePanel"), FAnchors(0.5f, 0.5f), FMargin(-280, -350, 560, 700), 16.0f, WSUITokens::Color::SurfaceDeep);
	SetGlassPanelPadding(PauseBorder, FMargin(14));
	UVerticalBox* PauseBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseBox"));
	SetGlassPanelContent(PauseBorder, PauseBox);
	UHorizontalBox* PauseTitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PauseTitleRow"));
	UTextBlock* PauseTitle = MakeText(TEXT("PauseTitle"), 20, Body, false);
	PauseTitle->SetFont(UIFont(20, true));
	PauseTitle->SetText(FText::FromString(TEXT("风雪站：断电前夜")));
	UHorizontalBoxSlot* PauseTitleSlot = PauseTitleRow->AddChildToHorizontalBox(PauseTitle);
	PauseTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	PauseTitleSlot->SetVerticalAlignment(VAlign_Center);
	UButton* PauseCloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PauseCloseButton"));
	PauseCloseButton->SetBackgroundColor(WSUITokens::Color::ButtonNormal);
	UTextBlock* PauseCloseText = MakeText(TEXT("PauseCloseText"), 13, Secondary, false);
	PauseCloseText->SetText(FText::FromString(TEXT("✕ 关闭")));
	PauseCloseButton->SetContent(PauseCloseText);
	PauseCloseButton->OnHovered.AddDynamic(this, &UWhiteoutHUDWidget::PlayHoverSound);
	PauseCloseButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ResumeGame);
	PauseTitleRow->AddChildToHorizontalBox(PauseCloseButton)->SetVerticalAlignment(VAlign_Center);
	PauseBox->AddChildToVerticalBox(PauseTitleRow)->SetPadding(FMargin(0, 0, 0, 7));
	PauseStatusText = MakeText(TEXT("PauseStatus"), 14, Secondary);
	PauseStatusText->SetJustification(ETextJustify::Left);
	PauseBox->AddChildToVerticalBox(PauseStatusText)->SetPadding(FMargin(0, 0, 0, 13));
	UButton* ResumeButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_resume"), TEXT("继续游戏")), TEXT("ResumeButton"));
	PauseDefaultButton = ResumeButton;
	UButton* SaveButton = MakeButton(PauseBox, FText::FromString(TEXT("保存游戏　｜　当前版本不可用")), TEXT("SaveButton"));
	UButton* LoadButton = MakeButton(PauseBox, FText::FromString(TEXT("读取游戏　｜　当前版本不可用")), TEXT("LoadButton"));
	UButton* SettingsButton = MakeButton(PauseBox, FText::FromString(TEXT("设置　　　｜　视野与音量")), TEXT("SettingsButton"));
	UButton* HelpButton = MakeButton(PauseBox, FText::FromString(TEXT("操作说明")), TEXT("HelpButton"));
	UButton* RestartButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_restart"), TEXT("重新开始")), TEXT("RestartButton"));
	UButton* MainMenuButton = MakeButton(PauseBox, FText::FromString(TEXT("返回主菜单｜　当前版本不可用")), TEXT("MainMenuButton"));
	UButton* QuitButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_quit"), TEXT("退出到桌面")), TEXT("QuitButton"));
	SaveButton->SetIsEnabled(false);
	LoadButton->SetIsEnabled(false);
	MainMenuButton->SetIsEnabled(false);
	SaveButton->SetColorAndOpacity(WSUITokens::Color::TextMuted);
	LoadButton->SetColorAndOpacity(WSUITokens::Color::TextMuted);
	MainMenuButton->SetColorAndOpacity(WSUITokens::Color::TextMuted);
	ResumeButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ResumeGame);
	SettingsButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::OpenSettings);
	HelpButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ToggleControls);
	RestartButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::RestartGame);
	QuitButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::QuitGame);
	PauseSituationText = MakeText(TEXT("PauseSituation"), 12, Secondary);
	PauseSituationText->SetText(FText::FromString(TEXT("当前情况")));
	PauseBox->AddChildToVerticalBox(PauseSituationText)->SetPadding(FMargin(8, 11, 8, 5));
	UHorizontalBox* SituationRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PauseSituationRow"));
	PauseSituationValues.Reset();
	const TArray<FString> SituationLabels = {TEXT("行动点"), TEXT("暴雪抵达"), TEXT("修复发电机"), TEXT("校准天线"), TEXT("求救信号")};
	for (int32 SituationIndex = 0; SituationIndex < SituationLabels.Num(); ++SituationIndex)
	{
		USizeBox* SituationSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("PauseSituationSize%d"), SituationIndex)));
		SituationSize->SetWidthOverride(96.0f);
		SituationSize->SetHeightOverride(56.0f);
		UBorder* SituationPanel = MakeGlassPanel(nullptr, FName(*FString::Printf(TEXT("PauseSituationPanel%d"), SituationIndex)), FAnchors(), FMargin(), 12.0f, WSUITokens::Color::SurfacePanel);
		SetGlassPanelPadding(SituationPanel, FMargin(6, 5));
		UVerticalBox* SituationBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*FString::Printf(TEXT("PauseSituationBox%d"), SituationIndex)));
		UTextBlock* SituationLabel = MakeText(FName(*FString::Printf(TEXT("PauseSituationLabel%d"), SituationIndex)), 11, Secondary, false);
		SituationLabel->SetText(FText::FromString(SituationLabels[SituationIndex]));
		SituationLabel->SetJustification(ETextJustify::Center);
		UTextBlock* SituationValue = MakeText(FName(*FString::Printf(TEXT("PauseSituationValue%d"), SituationIndex)), 13, Body, false);
		SituationValue->SetJustification(ETextJustify::Center);
		SituationBox->AddChildToVerticalBox(SituationLabel);
		SituationBox->AddChildToVerticalBox(SituationValue)->SetPadding(FMargin(0, 2, 0, 0));
		SetGlassPanelContent(SituationPanel, SituationBox);
		SituationSize->SetContent(SituationPanel);
		SituationRow->AddChildToHorizontalBox(SituationSize)->SetPadding(FMargin(SituationIndex == 0 ? 0.0f : 6.0f, 0, 0, 0));
		PauseSituationValues.Add(SituationValue);
	}
	PauseBox->AddChildToVerticalBox(SituationRow)->SetPadding(FMargin(0, 0, 0, 4));
	PauseHelpText = MakeText(TEXT("PauseHelp"), 13, Secondary);
	PauseHelpText->SetText(FText::FromString(TEXT("WASD 移动　鼠标观察　Space 跳跃\nF 互动 / 对话　E 证据板　Enter 结束当日　Esc 返回")));
	PauseHelpText->SetJustification(ETextJustify::Center);
	PauseHelpText->SetVisibility(ESlateVisibility::Collapsed);
	PauseBox->AddChildToVerticalBox(PauseHelpText)->SetPadding(FMargin(8, 12, 8, 0));
	PauseBorder->SetVisibility(ESlateVisibility::Collapsed);

	SettingsBorder = MakeGlassPanel(Canvas, TEXT("SettingsPanel"), FAnchors(0.5f, 0.5f), FMargin(-330, -300, 660, 600), 16.0f, WSUITokens::Color::SurfaceDeep);
	SetGlassPanelPadding(SettingsBorder, FMargin(18));
	UVerticalBox* SettingsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsBox"));
	SetGlassPanelContent(SettingsBorder, SettingsBox);
	UTextBlock* SettingsTitle = MakeText(TEXT("SettingsTitle"), 31, Body);
	SettingsTitle->SetText(FText::FromString(TEXT("设置")));
	SettingsTitle->SetJustification(ETextJustify::Center);
	SettingsBox->AddChildToVerticalBox(SettingsTitle)->SetPadding(FMargin(0, 0, 0, 8));
	UTextBlock* SettingsHint = MakeText(TEXT("SettingsHint"), 13, Secondary);
	SettingsHint->SetText(FText::FromString(TEXT("更改会实时生效，并保存在本机。")));
	SettingsHint->SetJustification(ETextJustify::Center);
	SettingsBox->AddChildToVerticalBox(SettingsHint)->SetPadding(FMargin(0, 0, 0, 18));
	auto AddSettingsRow = [this, SettingsBox](const FName Name, const FString& Label, UTextBlock*& OutValueText)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*(Name.ToString() + TEXT("Row"))));
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name.ToString() + TEXT("LabelBox"))));
		LabelBox->SetWidthOverride(135.0f);
		UTextBlock* LabelText = MakeText(FName(*(Name.ToString() + TEXT("Label"))), 16, Body);
		LabelText->SetText(FText::FromString(Label));
		LabelBox->SetContent(LabelText);
		Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);
		USizeBox* SliderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name.ToString() + TEXT("SliderBox"))));
		SliderBox->SetWidthOverride(350.0f);
		USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), Name);
		Slider->SetMinValue(0.0f);
		Slider->SetMaxValue(1.0f);
		Slider->SetStepSize(0.01f);
		Slider->SetSliderBarColor(WSUITokens::Color::SliderBar);
		Slider->SetSliderHandleColor(WSUITokens::Color::SliderHandle);
		SliderBox->SetContent(Slider);
		Row->AddChildToHorizontalBox(SliderBox)->SetVerticalAlignment(VAlign_Center);
		USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name.ToString() + TEXT("ValueBox"))));
		ValueBox->SetWidthOverride(90.0f);
		OutValueText = MakeText(FName(*(Name.ToString() + TEXT("Value"))), 15, Cyan);
		OutValueText->SetJustification(ETextJustify::Right);
		ValueBox->SetContent(OutValueText);
		Row->AddChildToHorizontalBox(ValueBox)->SetVerticalAlignment(VAlign_Center);
		SettingsBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(8, 7));
		return Slider;
	};
	UTextBlock* FOVValue = nullptr;
	FOVSlider = AddSettingsRow(TEXT("FOVSlider"), TEXT("视野角度"), FOVValue);
	FOVValueText = FOVValue;
	UTextBlock* MasterValue = nullptr;
	MasterVolumeSlider = AddSettingsRow(TEXT("MasterVolumeSlider"), TEXT("主音量"), MasterValue);
	MasterVolumeValueText = MasterValue;
	UTextBlock* AmbienceValue = nullptr;
	AmbienceVolumeSlider = AddSettingsRow(TEXT("AmbienceVolumeSlider"), TEXT("氛围音量"), AmbienceValue);
	AmbienceVolumeValueText = AmbienceValue;
	UTextBlock* EffectsValue = nullptr;
	EffectsVolumeSlider = AddSettingsRow(TEXT("EffectsVolumeSlider"), TEXT("效果音量"), EffectsValue);
	EffectsVolumeValueText = EffectsValue;
	UTextBlock* FeedbackValue = nullptr;
	FeedbackVolumeSlider = AddSettingsRow(TEXT("FeedbackVolumeSlider"), TEXT("反馈音量"), FeedbackValue);
	FeedbackVolumeValueText = FeedbackValue;
	FOVSlider->OnValueChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleFOVChanged);
	MasterVolumeSlider->OnValueChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleMasterVolumeChanged);
	AmbienceVolumeSlider->OnValueChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleAmbienceVolumeChanged);
	EffectsVolumeSlider->OnValueChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleEffectsVolumeChanged);
	FeedbackVolumeSlider->OnValueChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleFeedbackVolumeChanged);
	UButton* SettingsBackButton = MakeButton(SettingsBox, FText::FromString(TEXT("返回暂停菜单")), TEXT("SettingsBackButton"));
	SettingsBackButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::CloseSettings);
	SettingsBox->AddChildToVerticalBox(MakeText(TEXT("SettingsScope"), 12, Secondary))->SetPadding(FMargin(8, 10, 8, 0));
	if (UTextBlock* ScopeText = Cast<UTextBlock>(SettingsBox->GetChildAt(SettingsBox->GetChildrenCount() - 1)))
	{
		ScopeText->SetText(FText::FromString(TEXT("效果：脚步 / 事件 / 结局音乐　反馈：界面提示")));
		ScopeText->SetJustification(ETextJustify::Center);
	}
	SettingsBorder->SetVisibility(ESlateVisibility::Collapsed);
	RefreshSettingsUI();
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

UBorder* UWhiteoutHUDWidget::MakeGlassPanel(
	UCanvasPanel* Canvas,
	const FName Name,
	const FAnchors& Anchors,
	const FMargin& Offsets,
	const float BlurStrength,
	const FLinearColor& Tint,
	const bool bHairline)
{
	UBorder* Shell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	Shell->SetBrushColor(bHairline
		? WSUITokens::Color::StrokeHairline
		: FLinearColor::Transparent);
	Shell->SetPadding(FMargin(bHairline ? 1.0f : 0.0f));
	Shell->SetHorizontalAlignment(HAlign_Fill);
	Shell->SetVerticalAlignment(VAlign_Fill);

	UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(
		UBackgroundBlur::StaticClass(), FName(*(Name.ToString() + TEXT("Blur"))));
	Blur->SetBlurStrength(BlurStrength);
	Blur->SetApplyAlphaToBlur(true);
	Blur->SetPadding(FMargin(0));
	Blur->SetHorizontalAlignment(HAlign_Fill);
	Blur->SetVerticalAlignment(VAlign_Fill);
	const float CornerRadius = FMath::Clamp(BlurStrength * 0.72f, WSUITokens::Radius::Small, WSUITokens::Radius::Modal);
	Blur->SetCornerRadius(FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius));
	Shell->SetContent(Blur);

	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), FName(*(Name.ToString() + TEXT("Layers"))));
	UBorder* TintLayer = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName(*(Name.ToString() + TEXT("Tint"))));
	TintLayer->SetBrushColor(Tint);
	UOverlaySlot* TintSlot = Layers->AddChildToOverlay(TintLayer);
	TintSlot->SetHorizontalAlignment(HAlign_Fill);
	TintSlot->SetVerticalAlignment(VAlign_Fill);
	UBorder* ContentSlot = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName(*(Name.ToString() + TEXT("Content"))));
	ContentSlot->SetBrushColor(FLinearColor::Transparent);
	ContentSlot->SetPadding(FMargin(20));
	UOverlaySlot* GlassContentSlot = Layers->AddChildToOverlay(ContentSlot);
	GlassContentSlot->SetHorizontalAlignment(HAlign_Fill);
	GlassContentSlot->SetVerticalAlignment(VAlign_Fill);
	Blur->SetContent(Layers);
	GlassPanelContentSlots.Add(Shell, ContentSlot);

	if (Canvas)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Shell);
		CanvasSlot->SetAnchors(Anchors);
		CanvasSlot->SetOffsets(Offsets);
	}
	return Shell;
}

void UWhiteoutHUDWidget::SetGlassPanelContent(UBorder* Panel, UWidget* Content)
{
	if (TObjectPtr<UBorder>* ContentSlot = GlassPanelContentSlots.Find(Panel))
	{
		if (*ContentSlot)
		{
			(*ContentSlot)->SetContent(Content);
		}
	}
}

void UWhiteoutHUDWidget::SetGlassPanelPadding(UBorder* Panel, const FMargin& ContentPadding)
{
	if (TObjectPtr<UBorder>* ContentSlot = GlassPanelContentSlots.Find(Panel))
	{
		if (*ContentSlot)
		{
			(*ContentSlot)->SetPadding(ContentPadding);
		}
	}
}

UButton* UWhiteoutHUDWidget::MakeButton(UVerticalBox* Box, const FText& Label, const FName Name)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	FButtonStyle ButtonStyle = Button->GetStyle();
	ButtonStyle.Normal.TintColor = FSlateColor(WSUITokens::Color::ButtonNormal);
	ButtonStyle.Hovered.TintColor = FSlateColor(WSUITokens::Color::ButtonHover);
	ButtonStyle.Pressed.TintColor = FSlateColor(WSUITokens::Color::ButtonPressed);
	ButtonStyle.NormalForeground = FSlateColor(WSUITokens::Color::TextSecondary);
	ButtonStyle.HoveredForeground = FSlateColor(WSUITokens::Color::TextPrimary);
	ButtonStyle.PressedForeground = FSlateColor(WSUITokens::Color::TextPrimary);
	ButtonStyle.DisabledForeground = FSlateColor(WSUITokens::Color::TextMuted);
	Button->SetStyle(ButtonStyle);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*(Name.ToString() + TEXT("Row"))));
	const FString IconName = ButtonIconName(Name);
	if (!IconName.IsEmpty())
	{
		const FString IconPath = FString::Printf(TEXT("/Game/WindStation/UI/v03/Icons/%s.%s"), *IconName, *IconName);
		if (UTexture2D* IconTexture = LoadObject<UTexture2D>(nullptr, *IconPath))
		{
			USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name.ToString() + TEXT("IconBox"))));
			IconBox->SetWidthOverride(24.0f);
			IconBox->SetHeightOverride(24.0f);
			UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*(Name.ToString() + TEXT("Icon"))));
			Icon->SetBrushFromTexture(IconTexture, true);
			Icon->SetColorAndOpacity(WSUITokens::Color::TextPrimary);
			IconBox->SetContent(Icon);
			Row->AddChildToHorizontalBox(IconBox)->SetPadding(FMargin(10, 0, 14, 0));
		}
	}
	UTextBlock* LabelText = MakeText(FName(*(Name.ToString() + TEXT("Label"))), 16, WSUITokens::Color::TextPrimary, false);
	LabelText->SetText(Label);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	Button->SetContent(Row);
	Button->OnHovered.AddDynamic(this, &UWhiteoutHUDWidget::PlayHoverSound);
	UVerticalBoxSlot* ButtonSlot = Box->AddChildToVerticalBox(Button);
	ButtonSlot->SetPadding(FMargin(14, 3));
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
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
	FButtonStyle ButtonStyle = Button->GetStyle();
	ButtonStyle.Normal.TintColor = FSlateColor(WSUITokens::Color::DialogueChoiceNormal);
	ButtonStyle.Hovered.TintColor = FSlateColor(WSUITokens::Color::DialogueChoiceHover);
	ButtonStyle.Pressed.TintColor = FSlateColor(WSUITokens::Color::ButtonPressed);
	ButtonStyle.NormalForeground = FSlateColor(WSUITokens::Color::TextSecondary);
	ButtonStyle.HoveredForeground = FSlateColor(WSUITokens::Color::TextPrimary);
	ButtonStyle.PressedForeground = FSlateColor(WSUITokens::Color::TextPrimary);
	Button->SetStyle(ButtonStyle);
	Button->SetBackgroundColor(WSUITokens::Color::DialogueChoiceNormal);
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

UProgressBar* UWhiteoutHUDWidget::MakeProgressBar(const FName Name, const FLinearColor& FillColor, const float Height)
{
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
	Bar->SetPercent(0.5f);
	Bar->SetFillColorAndOpacity(FillColor);
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(WSUITokens::Color::ProgressBarBackground);
	Style.FillImage.TintColor = FSlateColor(FillColor);
	Bar->SetWidgetStyle(Style);
	return Bar;
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

void UWhiteoutHUDWidget::SetBaseHudHidden(const bool bHidden)
{
	const ESlateVisibility PanelVisibility = bHidden ? ESlateVisibility::Hidden : ESlateVisibility::Visible;
	if (TopPanel) TopPanel->SetVisibility(PanelVisibility);
	if (ObjectivePanel) ObjectivePanel->SetVisibility(PanelVisibility);
	if (CrewPanel) CrewPanel->SetVisibility(PanelVisibility);
	if (BottomPanel) BottomPanel->SetVisibility(PanelVisibility);
}

void UWhiteoutHUDWidget::SetLayer(const EWSUILayer Layer)
{
	CurrentLayer = Layer;
	const bool bHideBaseHud = Layer == EWSUILayer::Evidence
		|| Layer == EWSUILayer::Dialogue
		|| Layer == EWSUILayer::Pause
		|| Layer == EWSUILayer::Settings
		|| Layer == EWSUILayer::Results;
	SetBaseHudHidden(bHideBaseHud);
	if (bHideBaseHud)
	{
		ClearInteractionFocus();
	}
	if (CrosshairText)
	{
		CrosshairText->SetVisibility(bHideBaseHud ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
	}
}

void UWhiteoutHUDWidget::SetEvidenceFilter(const int32 FilterIndex)
{
	EvidenceFilterIndex = FMath::Clamp(FilterIndex, 0, 4);
	ShowEvidenceDetail(TEXT("选择一条记录查看细节。"));
	if (bPresentationCaptureOverride)
	{
		UpdateEvidence(PresentationCaptureState);
		return;
	}
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			UpdateEvidence(StateSubsystem->GetStateSnapshot());
		}
	}
}

void UWhiteoutHUDWidget::ShowEvidenceDetail(const FString& DetailCopy)
{
	if (EvidenceDetailText)
	{
		EvidenceDetailText->SetText(FText::FromString(DetailCopy));
	}
}

void UWhiteoutHUDWidget::FilterEvidenceAll() { SetEvidenceFilter(0); }
void UWhiteoutHUDWidget::FilterEvidenceFiles() { SetEvidenceFilter(1); }
void UWhiteoutHUDWidget::FilterEvidenceItems() { SetEvidenceFilter(2); }
void UWhiteoutHUDWidget::FilterEvidenceWitnesses() { SetEvidenceFilter(3); }
void UWhiteoutHUDWidget::FilterEvidenceDialogue() { SetEvidenceFilter(4); }

void UWhiteoutHUDWidget::ShowHoveredEvidenceDetail()
{
	for (int32 Index = 0; Index < EvidenceCardButtons.Num(); ++Index)
	{
		const UButton* Button = EvidenceCardButtons[Index];
		if (Button && (Button->IsHovered() || Button->HasKeyboardFocus()) && EvidenceCardDetailCopies.IsValidIndex(Index))
		{
			ShowEvidenceDetail(EvidenceCardDetailCopies[Index]);
			PlayUISound(UIConfirmSound, 0.52f);
			return;
		}
	}
}

void UWhiteoutHUDWidget::UpdateFromState(const FWSGameState& State)
{
	const FString Crisis = State.bMidCrisisTriggered
		? FWSPresentationText::UI(TEXT("ui_top_l3_crisis_v04"), TEXT("备用电池故障 ｜ 仅保留应急负载")).ToString()
		: FWSPresentationText::UI(TEXT("ui_top_l3_v04"), TEXT("暴风雪逼近 ｜ 电力正在衰减")).ToString();
	if (TopText) TopText->SetText(FWSPresentationText::UI(TEXT("title"), TEXT("风雪站：断电前夜")));
	if (TopStatusText)
	{
		TopStatusText->SetText(FText::Format(
			FWSPresentationText::UI(TEXT("ui_top_l2_v04"), TEXT("{0} ｜ AP {1} / 8 ｜ {2}")),
			FText::FromString(ClockForAP(State.ActionPoints)),
			FText::AsNumber(State.ActionPoints),
			FWSPresentationText::PhaseLabel(State.Phase)));
		TopStatusText->SetColorAndOpacity(FSlateColor(State.ActionPoints <= 4 ? Danger : Body));
	}
	if (TopConditionText) TopConditionText->SetText(FText::FromString(Crisis));

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
		PauseSituationText->SetText(FText::FromString(TEXT("当前情况")));
	}
	if (PauseSituationValues.Num() >= 5)
	{
		PauseSituationValues[0]->SetText(FText::FromString(FString::Printf(TEXT("%d / 8"), State.ActionPoints)));
		PauseSituationValues[1]->SetText(FText::FromString(ClockForAP(0)));
		PauseSituationValues[2]->SetText(FText::FromString(FString::Printf(TEXT("%d / 2"), State.Tasks.GeneratorProgress)));
		PauseSituationValues[3]->SetText(FText::FromString(FString::Printf(TEXT("%d / 1"), State.Tasks.AntennaCalibration)));
		PauseSituationValues[4]->SetText(FText::FromString(State.Tasks.bSignalSent ? TEXT("已发送") : TEXT("未发送")));
	}
	FeedbackText->SetText(FText::FromString(SystemMessage));
	PromptText->SetText(InteractionPrompt);
	UpdateEvidence(State);
	UpdateResults(State);
}

void UWhiteoutHUDWidget::UpdateDialogueCard(const FWSGameState& State)
{
	if (!DialogueNPCText)
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
	}
	else
	{
		Stance = State.Flags.bGuHengTreated ? TEXT("持续监测伤员")
			: State.Flags.bMedicalRoomHeated ? TEXT("准备诊疗")
			: TEXT("优先恢复医疗条件");
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
	EvidenceTitleText->SetText(FText::FromString(FString::Printf(TEXT("证据板　%02d 条记录"), TotalCards)));
	const TArray<int32> FilterCounts = {TotalCards, FileCount, ItemCount, WitnessCount, DialogueCount};
	for (int32 Index = 0; Index < FilterCounts.Num(); ++Index)
	{
		if (EvidenceFilterCounts.IsValidIndex(Index) && EvidenceFilterCounts[Index])
		{
			EvidenceFilterCounts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%02d"), FilterCounts[Index])));
		}
		if (EvidenceFilterIndicators.IsValidIndex(Index) && EvidenceFilterIndicators[Index])
		{
			EvidenceFilterIndicators[Index]->SetBrushColor(Index == EvidenceFilterIndex ? Amber : FLinearColor::Transparent);
		}
		if (EvidenceFilterLabels.IsValidIndex(Index) && EvidenceFilterLabels[Index])
		{
			EvidenceFilterLabels[Index]->SetColorAndOpacity(FSlateColor(Index == EvidenceFilterIndex ? Body : Secondary));
		}
	}
	EvidenceProgressText->SetText(FText::FromString(FString::Printf(
		TEXT("重要性　● 关键　　● 重要　　● 普通　　　　　　　　　　　　　　收集进度 %02d / 18"),
		FMath::Clamp(State.Evidence.Num(), 0, 18))));

	EvidenceCardGrid->ClearChildren();
	EvidenceCardButtons.Reset();
	EvidenceCardDetailCopies.Reset();
	int32 CardIndex = 0;
	const auto AddCard = [this, &CardIndex](
		const FString& Title,
		const FString& Type,
		const FString& Summary,
		const FLinearColor& Importance,
		const FString& ImportanceLabel,
		const TCHAR* IconName,
		const int32 Category)
	{
		if (EvidenceFilterIndex != 0 && Category != EvidenceFilterIndex)
		{
			return;
		}
		UButton* CardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*FString::Printf(TEXT("EvidenceCardButton%d"), CardIndex)));
		CardButton->SetBackgroundColor(FLinearColor::Transparent);
		CardButton->OnHovered.AddDynamic(this, &UWhiteoutHUDWidget::PlayHoverSound);
		CardButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ShowHoveredEvidenceDetail);
		UBorder* Card = MakeGlassPanel(nullptr, FName(*FString::Printf(TEXT("EvidenceCard%d"), CardIndex)), FAnchors(), FMargin(), 12.0f, WSUITokens::Color::SurfacePanel);
		SetGlassPanelPadding(Card, FMargin(12));
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
		SetGlassPanelContent(Card, CardRow);
		CardButton->SetContent(Card);
		UUniformGridSlot* CardSlot = EvidenceCardGrid->AddChildToUniformGrid(CardButton, CardIndex / 2, CardIndex % 2);
		CardSlot->SetHorizontalAlignment(HAlign_Fill);
		CardSlot->SetVerticalAlignment(VAlign_Fill);
		EvidenceCardButtons.Add(CardButton);
		EvidenceCardDetailCopies.Add(FString::Printf(TEXT("%s｜%s｜%s\n%s"), *Title, *Type, *ImportanceLabel, *Summary));
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
			bWitness ? TEXT("关键") : bFile ? TEXT("重要") : TEXT("普通"),
			bFile ? TEXT("I_Evidence_File") : bWitness ? TEXT("I_Evidence_Witness") : TEXT("I_Evidence_Item"),
			bFile ? 1 : bWitness ? 3 : 2);
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
				TEXT("重要"),
				TEXT("I_Evidence_Dialogue"),
				4);
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
				TEXT("关键"),
				TEXT("I_Evidence_Witness"),
				3);
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
			Promise.bSettled && !Promise.bFulfilled ? TEXT("关键") : TEXT("重要"),
			TEXT("I_Evidence_Dialogue"),
			4);
	}
	if (CardIndex == 0)
	{
		const int32 EmptyCategory = EvidenceFilterIndex == 0 ? 0 : EvidenceFilterIndex;
		AddCard(TEXT("尚未取得证据"), TEXT("系统"), TEXT("调查设备、现场物品或与队员交谈。"), Body, TEXT("普通"), TEXT("I_Evidence_File"), EmptyCategory);
	}
	const bool bEvidenceAnimating = ActivePanelAnimations.ContainsByPredicate(
		[this](const FPanelAnimation& A) { return A.Panel.Get() == EvidenceBorder; });
	if (!bEvidenceAnimating)
	{
		EvidenceBorder->SetVisibility(bEvidenceVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UWhiteoutHUDWidget::UpdateResults(const FWSGameState& State)
{
	const bool bResults = State.Phase == EWSGamePhase::Results;
	if (!bResults)
	{
		ResultsBorder->SetVisibility(ESlateVisibility::Collapsed);
		bWasShowingResults = false;
		if (CurrentLayer == EWSUILayer::Results)
		{
			SetLayer(EWSUILayer::Game);
		}
		return;
	}
	if (CurrentLayer != EWSUILayer::Pause && CurrentLayer != EWSUILayer::Settings)
	{
		SetLayer(EWSUILayer::Results);
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
			TEXT("%02d　%s　%s　行动力 %d → %d%s\n"),
			Event.Index,
			*ClockForAP(Event.APAfter),
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
	if (bInteractionFocusCaptureLock && ActionName.ToString() != InteractionFocusCaptureName)
	{
		return;
	}
	if (CurrentLayer != EWSUILayer::Game && CurrentLayer != EWSUILayer::Preview)
	{
		return;
	}
	const FString NewName = ActionName.ToString();
	if (FocusedActionName != NewName)
	{
		FocusedActionName = NewName;
		PlayUISound(UIHoverSound, 0.42f);
	}
	if (CrosshairText)
	{
		CrosshairText->SetText(FText::FromString(TEXT("+")));
		CrosshairText->SetFont(UIFont(28, true));
		CrosshairText->SetColorAndOpacity(FSlateColor(Body));
	}
	if (FocusBorder && FocusText && FocusAPText && FocusKeyText)
	{
		FocusBorder->SetVisibility(ESlateVisibility::Visible);
		FocusBorder->SetBrushColor(FLinearColor::Transparent);
		FocusText->SetColorAndOpacity(FSlateColor(Body));
		FocusText->SetText(FText::FromString(NewName + TEXT("　")));
		if (bDialogue)
		{
			FocusAPText->SetVisibility(ESlateVisibility::Collapsed);
			FocusKeyText->SetText(FText::FromString(TEXT("｜　[F] 开始对话")));
		}
		else
		{
			FocusAPText->SetVisibility(ESlateVisibility::Visible);
			FocusAPText->SetText(FText::FromString(FString::Printf(TEXT("｜　%d AP　"), Preview.APCost)));
			FocusKeyText->SetText(FText::FromString(TEXT("｜　[F] 查看行动")));
		}
	}
}

void UWhiteoutHUDWidget::ClearInteractionFocus()
{
	if (bInteractionFocusCaptureLock)
	{
		return;
	}
	FocusedActionName.Reset();
	if (CrosshairText)
	{
		CrosshairText->SetText(FText::FromString(TEXT("+")));
		CrosshairText->SetFont(UIFont(28, false));
		CrosshairText->SetColorAndOpacity(FSlateColor(Body));
		CrosshairText->SetVisibility(CurrentLayer == EWSUILayer::Game || CurrentLayer == EWSUILayer::Preview
			? ESlateVisibility::Visible
			: ESlateVisibility::Hidden);
	}
	if (FocusBorder)
	{
		FocusBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWhiteoutHUDWidget::ShowActionPreview(const FText& ActionName, const FWSActionPreview& Preview)
{
	SetLayer(EWSUILayer::Preview);
	ShowPanelAnimated(PreviewBorder, true, WSUITokens::Anim::Fast, false);
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
		ShowPanelAnimated(PreviewBorder, false, WSUITokens::Anim::Fast, false);
	}
	if (CurrentLayer == EWSUILayer::Preview)
	{
		SetLayer(EWSUILayer::Game);
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
		ToastBorder->SetBrushColor(FLinearColor::Transparent);
		ToastBorder->SetVisibility(ESlateVisibility::Visible);
		ToastBorder->SetRenderOpacity(1.0f);
		ToastRemaining = 2.7f;
	}
}

void UWhiteoutHUDWidget::ToggleEvidence()
{
	if (bEvidenceVisible)
	{
		CloseEvidence();
		return;
	}
	HideActionPreview();
	if (bDialogueVisible)
	{
		CancelDialogue();
	}
	bEvidenceVisible = true;
	if (EvidenceBorder) ShowPanelAnimated(EvidenceBorder, true, WSUITokens::Anim::Normal);
	SetLayer(EWSUILayer::Evidence);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetShowMouseCursor(true);
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		SetKeyboardFocus();
	}
}

void UWhiteoutHUDWidget::CloseEvidence()
{
	bEvidenceVisible = false;
	if (EvidenceBorder) ShowPanelAnimated(EvidenceBorder, false, WSUITokens::Anim::Fast);
	SetLayer(EWSUILayer::Game);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetShowMouseCursor(false);
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}

void UWhiteoutHUDWidget::ShowDialogueMenu(const FName NPCActionId, const bool bVisible)
{
	bDialogueVisible = bVisible;
	ActiveDialogueActionId = bVisible ? NPCActionId : NAME_None;
	ShowPanelAnimated(DialogueBorder, bVisible, WSUITokens::Anim::Normal);
	if (!bVisible)
	{
		if (CurrentLayer == EWSUILayer::Dialogue)
		{
			SetLayer(EWSUILayer::Game);
		}
		return;
	}
	HideActionPreview();
	bEvidenceVisible = false;
	if (EvidenceBorder) EvidenceBorder->SetVisibility(ESlateVisibility::Collapsed);
	SetLayer(EWSUILayer::Dialogue);
	DialogueStage = EWSDialogueStage::Opening;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			const FWSGameState State = bPresentationCaptureOverride
				? PresentationCaptureState
				: StateSubsystem->GetStateSnapshot();
			const EWSCharacterId CharacterId = NPCActionId == TEXT("talk_ye_cheng")
				? EWSCharacterId::YeCheng
				: EWSCharacterId::GuHeng;
			if (DialogueNameText)
			{
				DialogueNameText->SetText(FWSPresentationText::CharacterName(CharacterId));
			}
			if (DialogueLineText)
			{
				DialogueLineText->SetText(FWSPresentationText::DialogueOpening(CharacterId, State));
				DialogueLineText->SetColorAndOpacity(FSlateColor(Body));
			}
			UpdateDialogueCard(State);
		}
	}
	ShowDialogueWheelChoices();
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
	DialogueStage = EWSDialogueStage::IntentPick;
	if (DialogueWheelPanel) DialogueWheelPanel->SetVisibility(ESlateVisibility::Visible);
	if (DialoguePromiseBorder) DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueFreeTextBorder) DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueReplyBorder) DialogueReplyBorder->SetVisibility(ESlateVisibility::Collapsed);
	RefreshDialogueAvailability();
}

void UWhiteoutHUDWidget::ShowDialoguePromiseChoices()
{
	DialogueStage = EWSDialogueStage::IntentPick;
	if (DialogueWheelPanel) DialogueWheelPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueFreeTextBorder) DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialoguePromiseBorder) DialoguePromiseBorder->SetVisibility(ESlateVisibility::Visible);
	if (DialogueReplyBorder) DialogueReplyBorder->SetVisibility(ESlateVisibility::Collapsed);
}

void UWhiteoutHUDWidget::ShowDialogueFreeTextForCapture()
{
	OpenDialogueFreeText();
}

void UWhiteoutHUDWidget::ShowDialogueReplyForCapture(const FString& Speaker, const FString& Line)
{
	if (DialogueNameText) DialogueNameText->SetText(FText::FromString(Speaker));
	if (DialogueLineText)
	{
		DialogueLineText->SetText(FText::FromString(Line));
		DialogueLineText->SetColorAndOpacity(FSlateColor(Body));
	}
	ShowDialogueReplyActions();
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
		if (DialogueReplyBorder) DialogueReplyBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWhiteoutHUDWidget::ChooseDialogueAsk()
{
	OpenDialogueTextEntry(EWSDialogueAct::Ask);
}

void UWhiteoutHUDWidget::ChooseDialogueChallenge()
{
	OpenDialogueTextEntry(EWSDialogueAct::Challenge);
}

void UWhiteoutHUDWidget::ChooseDialoguePromise()
{
	ShowDialoguePromiseChoices();
}

void UWhiteoutHUDWidget::ChooseDialogueReassure()
{
	OpenDialogueTextEntry(EWSDialogueAct::Reassure);
}

void UWhiteoutHUDWidget::OpenDialogueFreeText()
{
	OpenDialogueTextEntry(EWSDialogueAct::Ask);
}

void UWhiteoutHUDWidget::OpenDialogueTextEntry(
	const EWSDialogueAct DialogueAct,
	const FName PromiseCondition)
{
	DialogueStage = EWSDialogueStage::TextEntry;
	PendingDialogueAct = DialogueAct;
	PendingPromiseCondition = PromiseCondition;
	if (DialogueWheelPanel) DialogueWheelPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (DialoguePromiseBorder) DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueFreeTextBorder) DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Visible);
	if (DialogueReplyBorder) DialogueReplyBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueFreeTextInput)
	{
		FText Hint;
		switch (DialogueAct)
		{
		case EWSDialogueAct::Challenge:
			Hint = FWSPresentationText::UI(TEXT("dlg_hint_challenge_v04"), TEXT("例：保护装置明明被手动绕过，你怎么解释？"));
			break;
		case EWSDialogueAct::Reassure:
			Hint = FWSPresentationText::UI(TEXT("dlg_hint_reassure_v04"), TEXT("例：别怕，先把暖气抢回来，一步步来。"));
			break;
		case EWSDialogueAct::Promise:
			Hint = FWSPresentationText::UI(TEXT("dlg_hint_promise_v04"), TEXT("例：我保证不拆厨房加热器。"));
			break;
		default:
			Hint = FWSPresentationText::UI(TEXT("dlg_hint_ask_v04"), TEXT("例：继电器烧了之后，还有什么能替？"));
			break;
		}
		DialogueFreeTextInput->SetHintText(Hint);
		DialogueFreeTextInput->SetText(FText::GetEmpty());
		DialogueFreeTextInput->SetKeyboardFocus();
	}
}

void UWhiteoutHUDWidget::ChoosePromiseKeepRecords()
{
	OpenDialogueTextEntry(EWSDialogueAct::Promise, TEXT("keep_records"));
}

void UWhiteoutHUDWidget::ChoosePromisePreventSelfHarm()
{
	OpenDialogueTextEntry(EWSDialogueAct::Promise, TEXT("reserve_medicine"));
}

void UWhiteoutHUDWidget::ChoosePromiseRepairTogether()
{
	OpenDialogueTextEntry(EWSDialogueAct::Promise, TEXT("heat_repair_room"));
}

void UWhiteoutHUDWidget::SubmitDialogueFreeText()
{
	if (!DialogueFreeTextInput)
	{
		return;
	}
	const FString UserText = DialogueFreeTextInput->GetText().ToString().TrimStartAndEnd().Left(280);
	if (!UserText.IsEmpty() && UWSAgentGateway::ContainsAdversarialInstruction(UserText))
	{
		SetDialogueIntentStatus(TEXT("这句话不能这么说，换种表达。"), false);
		return;
	}
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		Character->SubmitDialogueChoice(PendingDialogueAct, PendingPromiseCondition, UserText);
	}
}

void UWhiteoutHUDWidget::RefreshDialogueAvailability()
{
	AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn());
	if (!Character || !Character->IsDialogueActive())
	{
		for (UButton* Button : DialogueIntentButtons)
		{
			if (Button) Button->SetIsEnabled(bPresentationCaptureOverride);
		}
		return;
	}
	const TArray<EWSDialogueAct> Acts = {
		EWSDialogueAct::Ask,
		EWSDialogueAct::Challenge,
		EWSDialogueAct::Reassure,
		EWSDialogueAct::Promise};
	bool bAnyAvailable = false;
	FWSActionPreview FirstPreview;
	for (int32 Index = 0; Index < DialogueIntentButtons.Num() && Index < Acts.Num(); ++Index)
	{
		FWSActionPreview Preview;
		if (Acts[Index] == EWSDialogueAct::Promise)
		{
			for (const FName Condition : {FName(TEXT("keep_records")), FName(TEXT("reserve_medicine")), FName(TEXT("heat_repair_room"))})
			{
				const FWSActionPreview Candidate = Character->PreviewActiveDialogue(Acts[Index], Condition);
				if (Candidate.bCanExecute || Preview.ActionId.IsNone()) Preview = Candidate;
				if (Candidate.bCanExecute) break;
			}
		}
		else
		{
			Preview = Character->PreviewActiveDialogue(Acts[Index]);
		}
		if (Index == 0) FirstPreview = Preview;
		DialogueIntentButtons[Index]->SetIsEnabled(Preview.bCanExecute);
		bAnyAvailable |= Preview.bCanExecute;
	}
	if (!bAnyAvailable && DialogueLineText)
	{
		DialogueLineText->SetText(FText::Format(
			FText::FromString(TEXT("{0} {1}")),
			FWSPresentationText::ReasonCause(FirstPreview.ReasonCode),
			FWSPresentationText::ReasonNextStep(FirstPreview.ReasonCode)));
		DialogueLineText->SetColorAndOpacity(FSlateColor(Amber));
	}
}

void UWhiteoutHUDWidget::ShowDialogueReplyActions()
{
	DialogueStage = EWSDialogueStage::Reply;
	if (DialogueWheelPanel) DialogueWheelPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (DialoguePromiseBorder) DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueFreeTextBorder) DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueReplyBorder) DialogueReplyBorder->SetVisibility(ESlateVisibility::Visible);
}

void UWhiteoutHUDWidget::ContinueDialogue()
{
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		Character->ContinueDialogue();
	}
	if (DialogueLineText)
	{
		DialogueLineText->SetText(FWSPresentationText::UI(TEXT("dlg_continue_v04"), TEXT("还要说什么？")));
		DialogueLineText->SetColorAndOpacity(FSlateColor(Body));
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
	if (!bDialogueVisible || Reply.ActionId != ActiveDialogueActionId || !DialogueLineText)
	{
		return;
	}
	const FString Speaker = Reply.Speaker == EWSCharacterId::GuHeng ? TEXT("顾衡") : TEXT("叶澄");
	if (DialogueNameText) DialogueNameText->SetText(FText::FromString(Speaker));
	DialogueLineText->SetText(FText::FromString(Reply.Utterance));
	DialogueLineText->SetColorAndOpacity(FSlateColor(Body));
	ShowDialogueReplyActions();
}

void UWhiteoutHUDWidget::ResetPresentationCapture()
{
	bPresentationCaptureOverride = false;
	bInteractionFocusCaptureLock = false;
	InteractionFocusCaptureName.Reset();
	bEvidenceVisible = false;
	bDialogueVisible = false;
	ActivePanelAnimations.Reset();
	auto ResetPanelVisual = [](UBorder* Panel)
	{
		if (Panel)
		{
			Panel->SetRenderOpacity(1.0f);
			Panel->SetRenderScale(FVector2D(1.0f));
			Panel->SetVisibility(ESlateVisibility::Collapsed);
		}
	};
	ResetPanelVisual(EvidenceBorder);
	ResetPanelVisual(DialogueBorder);
	ResetPanelVisual(ResultsBorder);
	ResetPanelVisual(ComponentGalleryBorder);
	ResetPanelVisual(ToastBorder);
	ResetPanelVisual(CrisisBorder);
	ResetPanelVisual(EndingCinematicBorder);
	ResetPanelVisual(PauseBorder);
	ResetPanelVisual(SettingsBorder);
	if (PauseHelpText) PauseHelpText->SetVisibility(ESlateVisibility::Collapsed);
	bEndingCinematicCapture = false;
	bWasShowingResults = false;
	bEndingResultsRevealed = false;
	ToastRemaining = 0.0f;
	CrisisElapsed = -1.0f;
	EndingElapsed = -1.0f;
	HideActionPreview();
	SetLayer(EWSUILayer::Game);
}

void UWhiteoutHUDWidget::SetPresentationCaptureState(const FWSGameState& State)
{
	PresentationCaptureState = State;
	bPresentationCaptureOverride = true;
	UpdateFromState(PresentationCaptureState);
}

void UWhiteoutHUDWidget::ShowNPCFocusForCapture(const FText& ActionName, const FWSActionPreview& Preview)
{
	bInteractionFocusCaptureLock = false;
	SetLayer(EWSUILayer::Game);
	SetInteractionFocus(ActionName, Preview, true);
	InteractionFocusCaptureName = ActionName.ToString();
	bInteractionFocusCaptureLock = true;
}

void UWhiteoutHUDWidget::ShowEvidenceForCapture(const int32 FilterIndex, const bool bShowFirstDetail)
{
	bEvidenceVisible = true;
	bDialogueVisible = false;
	ShowPanelInstant(PreviewBorder, false);
	ShowPanelInstant(DialogueBorder, false);
	SetLayer(EWSUILayer::Evidence);
	EvidenceFilterIndex = FMath::Clamp(FilterIndex, 0, 4);
	ShowPanelInstant(EvidenceBorder, true);
	if (bPresentationCaptureOverride)
	{
		UpdateEvidence(PresentationCaptureState);
	}
	if (bShowFirstDetail && EvidenceCardDetailCopies.Num() > 0)
	{
		ShowEvidenceDetail(EvidenceCardDetailCopies[0]);
	}
}

void UWhiteoutHUDWidget::ShowComponentGalleryForCapture()
{
	ResetPresentationCapture();
	DismissOpening();
	ShowPanelInstant(ComponentGalleryBorder, true);
}

void UWhiteoutHUDWidget::ShowSettingsForCapture()
{
	if (!IsPauseMenuVisible())
	{
		ShowPanelInstant(PauseBorder, true);
		SetLayer(EWSUILayer::Pause);
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
	RefreshSettingsUI();
	ShowPanelInstant(PauseBorder, false);
	ShowPanelInstant(SettingsBorder, true);
	SetLayer(EWSUILayer::Settings);
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
		SetLayer(EWSUILayer::Results);
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
	auto SplitText = [](const FText& FullText, FText& OutTitle, FText& OutSubtitle, FText& OutFooter)
	{
		TArray<FString> Parts;
		FullText.ToString().ParseIntoArray(Parts, TEXT("\n\n"), false);
		OutTitle = FText::FromString(Parts.Num() > 0 ? Parts[0].TrimStartAndEnd() : TEXT(""));
		OutSubtitle = FText::FromString(Parts.Num() > 1 ? Parts[1].TrimStartAndEnd() : TEXT(""));
		OutFooter = FText::FromString(Parts.Num() > 2 ? Parts[2].TrimStartAndEnd() : TEXT(""));
	};
	auto SetDividerVisible = [&](bool bVisible)
	{
		if (OpeningDivider) OpeningDivider->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};
	auto SetFooterVisible = [&](bool bVisible)
	{
		if (OpeningFooterText) OpeningFooterText->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};
	if (Stage == 0)
	{
		OpeningBorder->SetBrushColor(FLinearColor(0.002f, 0.008f, 0.016f, 0.985f));
		OpeningText->SetFont(UIFont(42, true));
		OpeningText->SetColorAndOpacity(FSlateColor(WSUITokens::Color::TextPrimary));
		FText Title, Subtitle, Footer;
		SplitText(FWSPresentationText::UI(TEXT("ui_opening_title"), TEXT("风雪站：断电前夜\n\n08:15｜海拔 4,126 米｜极夜值班")), Title, Subtitle, Footer);
		OpeningText->SetText(Title);
		if (OpeningSubtitleText)
		{
			OpeningSubtitleText->SetText(Subtitle);
			OpeningSubtitleText->SetFont(UIFont(20, false));
			OpeningSubtitleText->SetColorAndOpacity(FSlateColor(WSUITokens::Color::TextCinematicWarm));
		}
		SetFooterVisible(false);
		SetDividerVisible(true);
	}
	else if (Stage == 1)
	{
		OpeningBorder->SetBrushColor(FLinearColor(0.002f, 0.012f, 0.024f, 0.34f));
		OpeningText->SetFont(UIFont(31, true));
		OpeningText->SetColorAndOpacity(FSlateColor(WSUITokens::Color::TextCinematicWarm));
		FText Title, Subtitle, Footer;
		SplitText(FWSPresentationText::UI(TEXT("ui_opening_establishing"), TEXT("08:15｜暴雪封山\n\n备用电池正在衰减\n\n按空格跳过")), Title, Subtitle, Footer);
		OpeningText->SetText(Title);
		if (OpeningSubtitleText)
		{
			OpeningSubtitleText->SetText(Subtitle);
			OpeningSubtitleText->SetFont(UIFont(20, false));
			OpeningSubtitleText->SetColorAndOpacity(FSlateColor(WSUITokens::Color::TextCinematicWarm));
		}
		if (OpeningFooterText)
		{
			OpeningFooterText->SetText(Footer);
			OpeningFooterText->SetColorAndOpacity(FSlateColor(WSUITokens::Color::TextSecondary));
		}
		SetFooterVisible(true);
		SetDividerVisible(true);
	}
	else if (Stage == 2)
	{
		OpeningBorder->SetBrushColor(FLinearColor(0.003f, 0.010f, 0.020f, 0.88f));
		OpeningText->SetFont(UIFont(29, true));
		OpeningText->SetColorAndOpacity(FSlateColor(WSUITokens::Color::TextPrimary));
		FText Title, Subtitle, Footer;
		SplitText(FWSPresentationText::UI(TEXT("ui_opening_objective"), TEXT("08:15 → 18:15｜最后一轮抢修\n\n① 修复发电机\n② 校准室外天线\n③ 发出求救信号\n\n预算：8 点行动力\n越过中段后，备用电池将发生一次故障\n\n按空格跳过")), Title, Subtitle, Footer);
		OpeningText->SetText(Title);
		if (OpeningSubtitleText)
		{
			OpeningSubtitleText->SetText(Subtitle);
			OpeningSubtitleText->SetFont(UIFont(20, false));
			OpeningSubtitleText->SetColorAndOpacity(FSlateColor(WSUITokens::Color::TextPrimary));
		}
		if (OpeningFooterText)
		{
			OpeningFooterText->SetText(Footer);
			OpeningFooterText->SetColorAndOpacity(FSlateColor(WSUITokens::Color::TextSecondary));
		}
		SetFooterVisible(true);
		SetDividerVisible(true);
	}
	else
	{
		OpeningBorder->SetBrushColor(FLinearColor(0.003f, 0.010f, 0.020f, 0.91f));
		OpeningText->SetFont(UIFont(27, true));
		OpeningText->SetColorAndOpacity(FSlateColor(WSUITokens::Color::AccentInfo));
		FText Title, Subtitle, Footer;
		SplitText(FWSPresentationText::UI(TEXT("ui_opening_controls"), TEXT("每次行动都先预览，再确认\n\nWASD 移动　鼠标观察　Space 跳跃\nF 预览 / 再按 F 确认\nE 证据板　Esc 暂停/退出\n\n控制权交还")), Title, Subtitle, Footer);
		OpeningText->SetText(Title);
		if (OpeningSubtitleText)
		{
			OpeningSubtitleText->SetText(Subtitle);
			OpeningSubtitleText->SetFont(UIFont(20, false));
			OpeningSubtitleText->SetColorAndOpacity(FSlateColor(WSUITokens::Color::TextPrimary));
		}
		if (OpeningFooterText)
		{
			OpeningFooterText->SetText(Footer);
			OpeningFooterText->SetColorAndOpacity(FSlateColor(WSUITokens::Color::AccentInfo));
		}
		SetFooterVisible(true);
		SetDividerVisible(true);
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
		CrisisText->SetText(FWSPresentationText::UI(TEXT("ui_crisis_voltage_drop"), TEXT("13:15｜电压骤降")));
	}
	else if (Stage == 1)
	{
		CrisisBorder->SetBrushColor(FLinearColor(0.015f, 0.018f, 0.028f, 0.92f));
		CrisisText->SetText(FWSPresentationText::UI(TEXT("ui_crisis_battery_offline"), TEXT("13:15｜备用电池离线")));
	}
	else
	{
		CrisisBorder->SetBrushColor(FLinearColor(0.16f, 0.006f, 0.004f, 0.68f));
		CrisisText->SetText(FWSPresentationText::UI(
			TEXT("ui_crisis_emergency_load"),
			TEXT("13:15｜应急负载接管\n剩余行动力进入红线")));
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
	const FText FullText = FWSPresentationText::UI(Key, Fallback);
	const FString FullString = FullText.ToString();
	int32 NewlineIndex = INDEX_NONE;
	if (FullString.FindChar(TEXT('\n'), NewlineIndex))
	{
		EndingCinematicText->SetText(FText::FromString(FullString.Left(NewlineIndex).TrimStartAndEnd()));
		if (EndingSubtitleText)
		{
			EndingSubtitleText->SetText(FText::FromString(FullString.Mid(NewlineIndex + 1).TrimStartAndEnd()));
			EndingSubtitleText->SetColorAndOpacity(FSlateColor(TextColor));
		}
		if (EndingDivider)
		{
			EndingDivider->SetBrushColor(FLinearColor(
				TextColor.R, TextColor.G, TextColor.B, WSUITokens::Color::StrokeDivider.A));
			EndingDivider->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else
	{
		EndingCinematicText->SetText(FullText);
		if (EndingSubtitleText) EndingSubtitleText->SetText(FText::GetEmpty());
		if (EndingDivider) EndingDivider->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWhiteoutHUDWidget::PlayUISound(USoundBase* Sound, const float Volume)
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound, Volume);
	}
}

void UWhiteoutHUDWidget::ShowPanelAnimated(UBorder* Panel, const bool bShow, const float Duration, const bool bScaleWithFade)
{
	if (!Panel)
	{
		return;
	}
	CancelPanelAnimation(Panel);
	if (bScaleWithFade)
	{
		Panel->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
	if (bShow)
	{
		Panel->SetVisibility(ESlateVisibility::Visible);
		FPanelAnimation Anim;
		Anim.Panel = Panel;
		Anim.StartOpacity = 0.0f;
		Anim.TargetOpacity = 1.0f;
		Anim.Elapsed = 0.0f;
		Anim.Duration = FMath::Max(0.01f, Duration);
		Anim.bCollapseOnComplete = false;
		Anim.bScaleWithFade = bScaleWithFade;
		ActivePanelAnimations.Add(Anim);
		Panel->SetRenderOpacity(0.0f);
		if (bScaleWithFade)
		{
			Panel->SetRenderScale(FVector2D(0.97f));
		}
	}
	else
	{
		FPanelAnimation Anim;
		Anim.Panel = Panel;
		Anim.StartOpacity = Panel->GetRenderOpacity();
		Anim.TargetOpacity = 0.0f;
		Anim.Elapsed = 0.0f;
		Anim.Duration = FMath::Max(0.01f, Duration);
		Anim.bCollapseOnComplete = true;
		Anim.bScaleWithFade = bScaleWithFade;
		ActivePanelAnimations.Add(Anim);
	}
}

void UWhiteoutHUDWidget::TickPanelAnimations(const float DeltaTime)
{
	for (int32 i = ActivePanelAnimations.Num() - 1; i >= 0; --i)
	{
		FPanelAnimation& Anim = ActivePanelAnimations[i];
		UBorder* Panel = Anim.Panel.Get();
		if (!Panel)
		{
			ActivePanelAnimations.RemoveAtSwap(i);
			continue;
		}
		Anim.Elapsed += DeltaTime;
		const float T = FMath::Clamp(Anim.Elapsed / Anim.Duration, 0.0f, 1.0f);
		const float EasedT = 1.0f - FMath::Pow(1.0f - T, WSUITokens::Anim::EaseOutExponent);
		const float Opacity = FMath::Lerp(Anim.StartOpacity, Anim.TargetOpacity, EasedT);
		Panel->SetRenderOpacity(Opacity);
		if (Anim.bScaleWithFade)
		{
			const bool bShowing = Anim.TargetOpacity > 0.5f;
			const float Scale = bShowing
				? FMath::Lerp(0.97f, 1.0f, EasedT)
				: FMath::Lerp(1.0f, 0.97f, EasedT);
			Panel->SetRenderScale(FVector2D(Scale));
		}
		if (T >= 1.0f)
		{
			if (Anim.bCollapseOnComplete)
			{
				Panel->SetVisibility(ESlateVisibility::Collapsed);
			}
			if (Anim.bScaleWithFade)
			{
				Panel->SetRenderScale(FVector2D(1.0f));
			}
			Panel->SetRenderOpacity(Anim.TargetOpacity);
			ActivePanelAnimations.RemoveAtSwap(i);
		}
	}
}

void UWhiteoutHUDWidget::CancelPanelAnimation(UBorder* Panel)
{
	if (!Panel)
	{
		return;
	}
	for (int32 i = ActivePanelAnimations.Num() - 1; i >= 0; --i)
	{
		if (ActivePanelAnimations[i].Panel.Get() == Panel)
		{
			ActivePanelAnimations.RemoveAtSwap(i);
		}
	}
}

void UWhiteoutHUDWidget::ShowPanelInstant(UBorder* Panel, const bool bShow)
{
	if (!Panel)
	{
		return;
	}
	CancelPanelAnimation(Panel);
	Panel->SetRenderOpacity(1.0f);
	Panel->SetRenderScale(FVector2D(1.0f));
	Panel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
	return (PauseBorder && PauseBorder->GetVisibility() == ESlateVisibility::Visible)
		|| (SettingsBorder && SettingsBorder->GetVisibility() == ESlateVisibility::Visible);
}

void UWhiteoutHUDWidget::HandleBackRequested()
{
	// v0.4 global back routing (single source of truth):
	// Settings -> CloseSettings (Pause)
	// Pause -> ResumeGame
	// Evidence -> CloseEvidence
	// Dialogue -> AWhiteoutCharacter::CancelDialogue
	// Preview -> HideActionPreview
	// Results -> open Pause
	// Game -> open Pause
	switch (CurrentLayer)
	{
	case EWSUILayer::Settings:
		CloseSettings();
		break;
	case EWSUILayer::Pause:
		ResumeGame();
		break;
	case EWSUILayer::Evidence:
		CloseEvidence();
		break;
	case EWSUILayer::Dialogue:
		CancelDialogue();
		break;
	case EWSUILayer::Preview:
		HideActionPreview();
		break;
	case EWSUILayer::Results:
		TogglePauseMenu();
		break;
	case EWSUILayer::Game:
	default:
		TogglePauseMenu();
		break;
	}
}

void UWhiteoutHUDWidget::TogglePauseMenu()
{
	if (IsPauseMenuVisible())
	{
		ResumeGame();
		return;
	}
	DismissOpening();
	if (SettingsBorder)
	{
		ShowPanelAnimated(SettingsBorder, false, WSUITokens::Anim::Fast);
	}
	ShowPanelAnimated(PauseBorder, true, WSUITokens::Anim::Normal);
	SetLayer(EWSUILayer::Pause);
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
	ShowPanelAnimated(PauseBorder, false, WSUITokens::Anim::Fast);
	if (SettingsBorder)
	{
		ShowPanelAnimated(SettingsBorder, false, WSUITokens::Anim::Fast);
	}
	if (PauseHelpText)
	{
		PauseHelpText->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetLayer(EWSUILayer::Game);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(false);
		PlayerController->bShowMouseCursor = false;
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}

void UWhiteoutHUDWidget::OpenSettings()
{
	RefreshSettingsUI();
	if (PauseBorder)
	{
		ShowPanelAnimated(PauseBorder, false, WSUITokens::Anim::Fast);
	}
	if (SettingsBorder)
	{
		ShowPanelAnimated(SettingsBorder, true, WSUITokens::Anim::Normal);
	}
	SetLayer(EWSUILayer::Settings);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(FOVSlider ? FOVSlider->TakeWidget() : SettingsBorder->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
	PlayUISound(UIConfirmSound, 0.62f);
}

void UWhiteoutHUDWidget::CloseSettings()
{
	if (SettingsBorder)
	{
		ShowPanelAnimated(SettingsBorder, false, WSUITokens::Anim::Fast);
	}
	if (PauseBorder)
	{
		ShowPanelAnimated(PauseBorder, true, WSUITokens::Anim::Fast);
	}
	SetLayer(EWSUILayer::Pause);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(PauseDefaultButton ? PauseDefaultButton->TakeWidget() : PauseBorder->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
	PlayUISound(UIHoverSound, 0.48f);
}

void UWhiteoutHUDWidget::RefreshSettingsUI()
{
	if (!GetGameInstance())
	{
		return;
	}
	const UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>();
	if (!Settings)
	{
		return;
	}
	bUpdatingSettings = true;
	if (FOVSlider) FOVSlider->SetValue((Settings->GetFieldOfView() - 75.0f) / 30.0f);
	if (MasterVolumeSlider) MasterVolumeSlider->SetValue(Settings->GetMasterVolume());
	if (AmbienceVolumeSlider) AmbienceVolumeSlider->SetValue(Settings->GetAmbienceVolume());
	if (EffectsVolumeSlider) EffectsVolumeSlider->SetValue(Settings->GetEffectsVolume());
	if (FeedbackVolumeSlider) FeedbackVolumeSlider->SetValue(Settings->GetFeedbackVolume());
	if (FOVValueText) FOVValueText->SetText(FText::FromString(FString::Printf(TEXT("%d°"), FMath::RoundToInt(Settings->GetFieldOfView()))));
	if (MasterVolumeValueText) MasterVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetMasterVolume() * 100.0f))));
	if (AmbienceVolumeValueText) AmbienceVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetAmbienceVolume() * 100.0f))));
	if (EffectsVolumeValueText) EffectsVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetEffectsVolume() * 100.0f))));
	if (FeedbackVolumeValueText) FeedbackVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetFeedbackVolume() * 100.0f))));
	bUpdatingSettings = false;
}

void UWhiteoutHUDWidget::HandleFOVChanged(const float Value)
{
	if (bUpdatingSettings || !GetGameInstance()) return;
	if (UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		Settings->SetFieldOfView(75.0f + FMath::Clamp(Value, 0.0f, 1.0f) * 30.0f, this);
		if (FOVValueText) FOVValueText->SetText(FText::FromString(FString::Printf(TEXT("%d°"), FMath::RoundToInt(Settings->GetFieldOfView()))));
	}
}

void UWhiteoutHUDWidget::HandleMasterVolumeChanged(const float Value)
{
	if (bUpdatingSettings || !GetGameInstance()) return;
	if (UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		Settings->SetMasterVolume(Value, this);
		if (MasterVolumeValueText) MasterVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetMasterVolume() * 100.0f))));
	}
}

void UWhiteoutHUDWidget::HandleAmbienceVolumeChanged(const float Value)
{
	if (bUpdatingSettings || !GetGameInstance()) return;
	if (UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		Settings->SetAmbienceVolume(Value, this);
		if (AmbienceVolumeValueText) AmbienceVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetAmbienceVolume() * 100.0f))));
	}
}

void UWhiteoutHUDWidget::HandleEffectsVolumeChanged(const float Value)
{
	if (bUpdatingSettings || !GetGameInstance()) return;
	if (UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		Settings->SetEffectsVolume(Value, this);
		if (EffectsVolumeValueText) EffectsVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetEffectsVolume() * 100.0f))));
	}
}

void UWhiteoutHUDWidget::HandleFeedbackVolumeChanged(const float Value)
{
	if (bUpdatingSettings || !GetGameInstance()) return;
	if (UWhiteoutSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		Settings->SetFeedbackVolume(Value, this);
		if (FeedbackVolumeValueText) FeedbackVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetFeedbackVolume() * 100.0f))));
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
