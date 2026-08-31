#include "HUD/WhiteoutHUDWidget.h"

#include "Agents/WSAgentGateway.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
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
#include "HAL/IConsoleManager.h"
#include "HUD/WSUITokens.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputCoreTypes.h"
#include "Presentation/WSPresentationText.h"
#include "Player/WhiteoutCharacter.h"
#include "Settings/WhiteoutSettingsSubsystem.h"
#include "State/WhiteoutRulesEngine.h"
#include "State/WSKnowledgePolicy.h"
#include "State/WindStationStateSubsystem.h"
#include "Styling/CoreStyle.h"

namespace
{
	TAutoConsoleVariable<int32> CVarWhiteoutDialogueDebug(
		TEXT("Whiteout.DialogueDebug"),
		0,
		TEXT("Shows the local dialogue semantic frame and answer source in Development builds."),
		ECVF_Default);

	constexpr int32 InitialActionPoints = 12;
	constexpr int32 PhaseActionPoints = 4;
	constexpr int32 MinutesPerActionPoint = 50;
	constexpr int32 CollectableEvidenceCount = 7;
	constexpr int32 DiscoverableFactCount = 8;

	// 颜色别名 —— 全部来自 WSUITokens 单一可信来源
	const FLinearColor& PanelColor = WSUITokens::Color::SurfacePanel;
	const FLinearColor& DeepPanel = WSUITokens::Color::SurfaceDeep;
	const FLinearColor& Cyan = WSUITokens::Color::AccentInfo;
	const FLinearColor& Amber = WSUITokens::Color::AccentAction;
	const FLinearColor& Danger = WSUITokens::Color::AccentWarning;
	const FLinearColor& Body = WSUITokens::Color::TextPrimary;
	const FLinearColor& Secondary = WSUITokens::Color::TextSecondary;

	FString DayPhaseLabel(const EWSDayPhase DayPhase)
	{
		switch (DayPhase)
		{
		case EWSDayPhase::Morning:
			return TEXT("早晨");
		case EWSDayPhase::Afternoon:
			return TEXT("午后");
		case EWSDayPhase::Dusk:
			return TEXT("黄昏");
		default:
			return TEXT("夜间");
		}
	}

	FString HeatingZoneLabel(const EWSHeatingZone HeatingZone)
	{
		switch (HeatingZone)
		{
		case EWSHeatingZone::RepairRoom:
			return TEXT("维修间");
		case EWSHeatingZone::MedicalRoom:
			return TEXT("医务室");
		case EWSHeatingZone::Kitchen:
			return TEXT("厨房");
		case EWSHeatingZone::ControlRoom:
			return TEXT("控制室");
		default:
			return TEXT("待选择");
		}
	}

	FString InjuryLabel(const EWSInjurySeverity Severity)
	{
		switch (Severity)
		{
		case EWSInjurySeverity::Restricted:
			return TEXT("受限");
		case EWSInjurySeverity::Critical:
			return TEXT("危重");
		default:
			return TEXT("正常");
		}
	}

	FString CharacterShortLabel(const EWSCharacterId CharacterId)
	{
		switch (CharacterId)
		{
		case EWSCharacterId::GuHeng:
			return TEXT("顾衡");
		case EWSCharacterId::YeCheng:
			return TEXT("叶澄");
		default:
			return TEXT("玩家");
		}
	}

	FString CharacterLocationLabel(const EWSCharacterLocation Location)
	{
		switch (Location)
		{
		case EWSCharacterLocation::RepairRoom:
			return TEXT("维修间");
		case EWSCharacterLocation::MedicalRoom:
			return TEXT("医务室");
		case EWSCharacterLocation::Kitchen:
			return TEXT("厨房");
		case EWSCharacterLocation::OutdoorAntenna:
			return TEXT("室外天线");
		default:
			return TEXT("控制室");
		}
	}

	FString LLMProviderDisplayName(const FString& ProviderId)
	{
		for (const FWSLLMProviderPreset& Preset :
			UWSAgentGateway::GetProviderPresets())
		{
			if (Preset.ProviderId.Equals(
					ProviderId,
					ESearchCase::IgnoreCase))
			{
				return Preset.DisplayName;
			}
		}
		return ProviderId.IsEmpty() ? TEXT("未知厂商") : ProviderId;
	}

	FString LLMFallbackReasonLabel(const FString& Reason)
	{
		if (Reason == TEXT("provider_authentication_failed"))
		{
			return TEXT("API Key 验证失败");
		}
		if (Reason == TEXT("provider_insufficient_balance"))
		{
			return TEXT("账户余额不足");
		}
		if (Reason == TEXT("provider_rate_limited"))
		{
			return TEXT("请求频率受限");
		}
		if (Reason == TEXT("provider_overloaded"))
		{
			return TEXT("厂商服务繁忙");
		}
		if (Reason == TEXT("transport_error"))
		{
			return TEXT("网络连接失败");
		}
		if (Reason == TEXT("request_not_started")
			|| Reason == TEXT("retry_manager_unavailable"))
		{
			return TEXT("请求未能启动");
		}
		return TEXT("响应未通过安全校验");
	}

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
	TickOpening(InDeltaTime);
	const bool bReducedMotion = IsReducedMotionEnabled();
	if (ToastRemaining > 0.0f && ToastBorder)
	{
		ToastRemaining = FMath::Max(0.0f, ToastRemaining - InDeltaTime);
		const float Opacity = FMath::Clamp(ToastRemaining * 1.8f, 0.0f, 1.0f);
		ToastBorder->SetRenderOpacity(Opacity);
		if (TopText && !bReducedMotion)
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
		const int32 Stage = bReducedMotion ? 2 : CrisisElapsed < 0.55f ? 0 : CrisisElapsed < 1.45f ? 1 : 2;
		if (Stage != ActiveCrisisStage)
		{
			ApplyCrisisStage(Stage);
		}
		CrisisBorder->SetRenderOpacity(bReducedMotion
			? 1.0f
			: CrisisElapsed < 0.25f
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
		const float EndingDuration = bReducedMotion ? 2.2f : 4.4f;
		EndingCinematicBorder->SetRenderOpacity(bReducedMotion
			? 1.0f
			: EndingElapsed < 0.5f
				? EndingElapsed / 0.5f
				: FMath::Clamp((EndingDuration - EndingElapsed) / 0.8f, 0.0f, 1.0f));
		if (EndingElapsed >= EndingDuration)
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
	if (InKeyEvent.GetKey() == EKeys::H)
	{
		ToggleGuide();
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

	TopPanel = MakeGlassPanel(Canvas, TEXT("TopPanel"), FAnchors(0, 0), FMargin(20, 20, 340, 124), 12.0f, WSUITokens::Color::SurfacePanel);
	SetGlassPanelPadding(TopPanel, FMargin(10, 7));
	TopPanel->SetClipping(EWidgetClipping::ClipToBounds);
	UVerticalBox* TopBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TopBox"));
	TopText = MakeText(TEXT("TopText"), 16, Body, false);
	TopText->SetFont(UIFont(16, true));
	TopStatusText = MakeText(TEXT("TopStatusText"), 15, Body, false);
	TopConditionText = MakeText(TEXT("TopConditionText"), 15, Secondary, false);
	TopBox->AddChildToVerticalBox(TopText);
	TopBox->AddChildToVerticalBox(TopStatusText)->SetPadding(FMargin(0, 2, 0, 0));
	TopBox->AddChildToVerticalBox(TopConditionText)->SetPadding(FMargin(0, 2, 0, 0));
	SetGlassPanelContent(TopPanel, TopBox);

	ObjectivePanel = MakeGlassPanel(Canvas, TEXT("ObjectivePanel"), FAnchors(0, 0), FMargin(20, 156, 340, 420), 12.0f, WSUITokens::Color::SurfacePanel);
	SetGlassPanelPadding(ObjectivePanel, FMargin(12));
	ObjectivePanel->SetClipping(EWidgetClipping::ClipToBounds);
	UVerticalBox* ObjectiveBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ObjectiveBox"));
	ObjectiveText = MakeText(TEXT("ObjectiveText"), 13, Body, true);
	ObjectiveText->SetWrapTextAt(316.0f);
	ObjectiveText->SetLineHeightPercentage(1.25f);
	ObjectiveBox->AddChildToVerticalBox(ObjectiveText);
	TutorialTitleText = MakeText(TEXT("TutorialTitleText"), 13, Cyan, false);
	TutorialTitleText->SetFont(UIFont(13, true));
	TutorialTitleText->SetText(FText::FromString(TEXT("当前建议")));
	TutorialTitleText->SetAutoWrapText(true);
	TutorialTitleText->SetWrapTextAt(316.0f);
	ObjectiveBox->AddChildToVerticalBox(TutorialTitleText)->SetPadding(FMargin(0, 12, 0, 3));
	TutorialText = MakeText(TEXT("TutorialText"), 12, WSUITokens::Color::TextCinematicWarm);
	TutorialText->SetWrapTextAt(316.0f);
	TutorialText->SetLineHeightPercentage(1.2f);
	ObjectiveBox->AddChildToVerticalBox(TutorialText);
	SetGlassPanelContent(ObjectivePanel, ObjectiveBox);

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
		StatusLegend->SetText(FText::FromString(TEXT("温　体　伤　压　备")));
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
		TEXT("ui_help_v06"),
		TEXT("WASD 移动　鼠标观察　Space 跳跃　F 预览/确认　E 证据板　H 生存手册　Enter 结束　Esc 返回")));
	BottomBox->AddChildToVerticalBox(FeedbackText)->SetPadding(FMargin(0, 0, 0, 3));
	BottomBox->AddChildToVerticalBox(PromptText)->SetPadding(FMargin(0, 0, 0, 3));
	BottomBox->AddChildToVerticalBox(HelpText);

	CrosshairText = MakeText(TEXT("CrosshairText"), 14, Body, false);
	CrosshairText->SetText(FText::FromString(TEXT("○")));
	CrosshairText->SetJustification(ETextJustify::Center);
	CrosshairText->SetRenderTranslation(FVector2D(0.0f, -3.0f));
	UCanvasPanelSlot* CrosshairSlot = Canvas->AddChildToCanvas(CrosshairText);
	CrosshairSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	CrosshairSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CrosshairSlot->SetOffsets(FMargin(0.0f, 0.0f, 24.0f, 24.0f));
	FocusBorder = MakePanel(Canvas, TEXT("FocusPanel"), FAnchors(0.5f, 0.5f), FMargin(0, 44, 460, 56), FLinearColor::Transparent);
	if (UCanvasPanelSlot* FocusSlot = Cast<UCanvasPanelSlot>(FocusBorder->Slot))
	{
		FocusSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	}
	FocusBorder->SetPadding(FMargin(0));
	FocusBorder->SetClipping(EWidgetClipping::ClipToBounds);
	UOverlay* FocusOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("FocusOverlay"));
	if (InkBrushTexture)
	{
		UImage* FocusBrush = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FocusBrush"));
		FocusBrush->SetBrushFromTexture(InkBrushTexture, true);
		FocusBrush->SetDesiredSizeOverride(FVector2D(460.0f, 56.0f));
		FocusBrush->SetColorAndOpacity(FLinearColor(0.015f, 0.015f, 0.015f, 0.92f));
		UOverlaySlot* FocusBrushSlot = FocusOverlay->AddChildToOverlay(FocusBrush);
		FocusBrushSlot->SetHorizontalAlignment(HAlign_Fill);
		FocusBrushSlot->SetVerticalAlignment(VAlign_Fill);
	}
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
	UOverlaySlot* FocusContentSlot = FocusOverlay->AddChildToOverlay(FocusLine);
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
	SilenceButton(EvidenceCloseButton);
	EvidenceCloseButton->SetBackgroundColor(WSUITokens::Color::ButtonNormal);
	UTextBlock* EvidenceCloseText = MakeText(TEXT("EvidenceCloseText"), 13, Secondary, false);
	EvidenceCloseText->SetText(FText::FromString(TEXT("✕ 关闭")));
	EvidenceCloseButton->SetContent(EvidenceCloseText);
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
		SilenceButton(FilterButton);
		FilterButton->SetBackgroundColor(FLinearColor::Transparent);
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
	if (UScrollBoxSlot* GridSlot = Cast<UScrollBoxSlot>(EvidenceScroll->AddChild(EvidenceCardGrid)))
	{
		GridSlot->SetHorizontalAlignment(HAlign_Left);
	}
	UVerticalBoxSlot* EvidenceScrollSlot = EvidenceContent->AddChildToVerticalBox(EvidenceScroll);
	EvidenceScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	EvidenceScrollSlot->SetHorizontalAlignment(HAlign_Fill);
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

	GuideBorder = MakePanel(
		Canvas,
		TEXT("GuidePanel"),
		FAnchors(0, 0, 1, 1),
		FMargin(0),
		FLinearColor(0.002f, 0.004f, 0.008f, 0.92f));
	UCanvasPanel* GuideCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GuideCanvas"));
	GuideBorder->SetContent(GuideCanvas);
	UBorder* GuideCard = MakeGlassPanel(
		GuideCanvas,
		TEXT("GuideCard"),
		FAnchors(0.18f, 0.08f, 0.82f, 0.92f),
		FMargin(0),
		18.0f,
		WSUITokens::Color::SurfaceDeep);
	SetGlassPanelPadding(GuideCard, FMargin(22, 18));
	UVerticalBox* GuideBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GuideBox"));
	SetGlassPanelContent(GuideCard, GuideBox);
	UTextBlock* GuideTitle = MakeText(TEXT("GuideTitle"), 25, Cyan, false);
	GuideTitle->SetFont(UIFont(25, true));
	GuideTitle->SetText(FText::FromString(TEXT("风雪站生存手册")));
	GuideBox->AddChildToVerticalBox(GuideTitle)->SetPadding(FMargin(0, 0, 0, 8));
	UTextBlock* GuideIntro = MakeText(TEXT("GuideIntro"), 14, Body);
	GuideIntro->SetLineHeightPercentage(1.28f);
	GuideIntro->SetText(FText::FromString(
		TEXT("阶段与选择\n")
		TEXT("早晨、午后、黄昏各有 4 AP，未使用的 AP 在阶段结算时丢弃。每阶段开始先消耗 1 燃料锁定一个供暖区。\n")
		TEXT("修复发电机并发送信号可以结束本轮；也可保留燃料、照护队员并等待风暴过去。信号质量、人员状态与剩余储备会导向不同结局。\n")
		TEXT("按 F 查看动态 AP、执行者和风险，再按 F 确认；带有多个方案的行动可按 Q 切换。\n\n")
		TEXT("人物状态\n")
		TEXT("体温：6.0 以上温暖，3.5—5.9 寒冷，低于 3.5 失温。\n")
		TEXT("体能：2 充足、1 疲惫、0 耗尽；食物和供暖区休整可以恢复。\n")
		TEXT("伤势：正常、受限、危重。包扎只阻止下一次恶化，完整治疗可移除伤势。\n")
		TEXT("压力：越低越稳定；寒冷、强迫行动和失衡分配会推高压力。\n")
		TEXT("准备度：综合体温、体能、伤势与压力；预览中的动态 AP 会反映这些因素。\n")
		TEXT("信任：影响合作、情报和结算。公平分配、照护和兑现承诺会改变信任。\n\n")
		TEXT("信息与交涉\n")
		TEXT("按 E 查看证据板。对话先选意向，再自由输入；质疑和承诺只会在已有事实或可兑现条件时出现。")));
	UScrollBox* GuideScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("GuideScroll"));
	GuideScroll->AddChild(GuideIntro);
	UVerticalBoxSlot* GuideScrollSlot = GuideBox->AddChildToVerticalBox(GuideScroll);
	GuideScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	GuideContextText = MakeText(TEXT("GuideContextText"), 14, WSUITokens::Color::TextCinematicWarm);
	GuideContextText->SetLineHeightPercentage(1.24f);
	GuideBox->AddChildToVerticalBox(GuideContextText)->SetPadding(FMargin(0, 12, 0, 8));
	UButton* GuideCloseButton = MakeButton(
		GuideBox,
		FText::FromString(TEXT("返回游戏　[H / Esc]")),
		TEXT("GuideCloseButton"));
	GuideCloseButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::CloseGuide);
	GuideBorder->SetVisibility(ESlateVisibility::Collapsed);

	DialogueBorder = MakePanel(Canvas, TEXT("DialoguePanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor::Transparent);
	DialogueBorder->SetPadding(FMargin(0));
	UCanvasPanel* DialogueCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialogueCanvas"));
	DialogueBorder->SetContent(DialogueCanvas);
	UBorder* DialogueBar = MakeGlassPanel(DialogueCanvas, TEXT("DialogueBar"), FAnchors(0.18f, 0.57f, 0.82f, 0.97f), FMargin(0), 18.0f, WSUITokens::Color::SurfaceDialogue);
	SetGlassPanelPadding(DialogueBar, FMargin(14, 10));
	UVerticalBox* DialogueBarBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueBarBox"));
	SetGlassPanelContent(DialogueBar, DialogueBarBox);
	DialogueNameText = MakeText(TEXT("DialogueNameText"), 15, Cyan, false);
	DialogueNameText->SetFont(UIFont(15, true));
	DialogueLineText = MakeText(TEXT("DialogueLineText"), 16, Body);
	DialogueLineText->SetLineHeightPercentage(1.2f);
	DialogueText = DialogueLineText;
	DialogueStatusText = MakeText(TEXT("DialogueStatusText"), 12, Secondary);
	DialogueStatusText->SetText(FText::FromString(TEXT("本地预设")));
	UVerticalBoxSlot* DialogueNameSlot = DialogueBarBox->AddChildToVerticalBox(DialogueNameText);
	DialogueNameSlot->SetPadding(FMargin(0, 0, 0, 3));
	DialogueNameSlot->SetHorizontalAlignment(HAlign_Fill);
	UVerticalBoxSlot* DialogueLineSlot = DialogueBarBox->AddChildToVerticalBox(DialogueLineText);
	DialogueLineSlot->SetPadding(FMargin(0, 0, 0, 7));
	DialogueLineSlot->SetHorizontalAlignment(HAlign_Fill);
	UVerticalBoxSlot* DialogueStatusSlot = DialogueBarBox->AddChildToVerticalBox(DialogueStatusText);
	DialogueStatusSlot->SetPadding(FMargin(0, 0, 0, 6));
	DialogueStatusSlot->SetHorizontalAlignment(HAlign_Fill);

	DialogueConditionBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueConditionCard"));
	DialogueConditionBorder->SetBrushColor(FLinearColor(0.04f, 0.08f, 0.10f, 0.94f));
	DialogueConditionBorder->SetPadding(FMargin(9, 7));
	UVerticalBox* ConditionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueConditionBox"));
	DialogueConditionBorder->SetContent(ConditionBox);
	DialogueConditionTitleText = MakeText(TEXT("DialogueConditionTitle"), 12, Cyan, false);
	DialogueConditionTitleText->SetFont(UIFont(12, true));
	DialogueConditionTitleText->SetText(FText::FromString(TEXT("协作条件 · 发电机维修")));
	ConditionBox->AddChildToVerticalBox(DialogueConditionTitleText)->SetPadding(FMargin(0, 0, 0, 3));
	DialogueConditionBodyText = MakeText(TEXT("DialogueConditionBody"), 12, Body);
	DialogueConditionBodyText->SetLineHeightPercentage(1.05f);
	ConditionBox->AddChildToVerticalBox(DialogueConditionBodyText)->SetPadding(FMargin(0, 0, 0, 4));
	DialogueConditionStatusText = MakeText(TEXT("DialogueConditionStatus"), 11, Secondary, false);
	ConditionBox->AddChildToVerticalBox(DialogueConditionStatusText)->SetPadding(FMargin(0, 0, 0, 4));
	UHorizontalBox* ConditionActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DialogueConditionActions"));
	ConditionBox->AddChildToVerticalBox(ConditionActions);
	auto MakeConditionActionButton = [this, ConditionActions](const FName Name, const FString& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(WSUITokens::Color::ButtonNormal);
		Style.Hovered.TintColor = FSlateColor(WSUITokens::Color::ButtonHover);
		Style.Pressed.TintColor = FSlateColor(WSUITokens::Color::ButtonPressed);
		Button->SetStyle(Style);
		SilenceButton(Button);
		UTextBlock* LabelText = MakeText(
			FName(*(Name.ToString() + TEXT("Label"))),
			11,
			WSUITokens::Color::TextPrimary,
			false);
		LabelText->SetText(FText::FromString(Label));
		LabelText->SetJustification(ETextJustify::Center);
		Button->SetContent(LabelText);
		UHorizontalBoxSlot* Slot = ConditionActions->AddChildToHorizontalBox(Button);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Slot->SetPadding(FMargin(0, 0, 5, 0));
		return Button;
	};
	DialogueConditionPinButton = MakeConditionActionButton(TEXT("DialogueConditionPin"), TEXT("固定到任务栏"));
	DialogueConditionAcceptButton = MakeConditionActionButton(TEXT("DialogueConditionAccept"), TEXT("接受条件"));
	DialogueConditionPinButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::PinDialogueConditions);
	DialogueConditionAcceptButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::AcceptDialogueConditions);
	UVerticalBoxSlot* ConditionCardSlot = DialogueBarBox->AddChildToVerticalBox(DialogueConditionBorder);
	ConditionCardSlot->SetPadding(FMargin(0, 0, 0, 6));
	ConditionCardSlot->SetHorizontalAlignment(HAlign_Fill);
	DialogueConditionBorder->SetVisibility(ESlateVisibility::Collapsed);

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
	UButton* PreventSelfHarmButton = MakeButton(PromiseBox, FText::FromString(TEXT("保留药品｜应对突发状况")), TEXT("PromisePreventSelfHarm"));
	UButton* RepairTogetherButton = MakeButton(PromiseBox, FWSPresentationText::UI(TEXT("dialogue_promise_heat"), TEXT("配合修复｜维修间升温")), TEXT("PromiseRepairTogether"));
	KeepRecordsButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChoosePromiseKeepRecords);
	PreventSelfHarmButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChoosePromisePreventSelfHarm);
	RepairTogetherButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ChoosePromiseRepairTogether);
	DialoguePromiseButtons = {KeepRecordsButton, PreventSelfHarmButton, RepairTogetherButton};
	DialogueBarBox->AddChildToVerticalBox(DialoguePromiseBorder);
	DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);

	DialogueFreeTextBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueFreeTextPanel"));
	DialogueFreeTextBorder->SetBrushColor(FLinearColor::Transparent);
	DialogueFreeTextBorder->SetPadding(FMargin(0));
	UVerticalBox* FreeTextBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueFreeTextBox"));
	DialogueFreeTextBorder->SetContent(FreeTextBox);
	DialogueFreeTextInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("DialogueFreeTextInput"));
	DialogueFreeTextInput->SetHintText(BuildDialogueInputHint(EWSDialogueAct::Ask));
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
	DialogueContinueButton = MakeButton(ReplyBox, FWSPresentationText::UI(TEXT("dlg_continue_button_v04"), TEXT("继续交涉")), TEXT("DialogueContinue"));
	UButton* EndDialogueButton = MakeButton(ReplyBox, FWSPresentationText::UI(TEXT("dlg_end_button_v04"), TEXT("结束对话")), TEXT("DialogueEnd"));
	DialogueContinueButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ContinueDialogue);
	EndDialogueButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::CancelDialogue);
	DialogueBarBox->AddChildToVerticalBox(DialogueReplyBorder);
	DialogueReplyBorder->SetVisibility(ESlateVisibility::Collapsed);

	UBorder* NPCCard = MakeGlassPanel(DialogueCanvas, TEXT("DialogueNPCCard"), FAnchors(0.76f, 0.06f, 0.97f, 0.28f), FMargin(0), 12.0f, FLinearColor(0.020f, 0.020f, 0.020f, 0.94f));
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

	ComponentGalleryBorder = MakePanel(Canvas, TEXT("ComponentGalleryPanel"), FAnchors(0.015f, 0.015f, 0.985f, 0.985f), FMargin(0), FLinearColor(0.004f, 0.004f, 0.004f, 0.998f));
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
		ToastBrush->SetColorAndOpacity(FLinearColor(0.015f, 0.015f, 0.015f, 0.92f));
		ToastOverlay->AddChildToOverlay(ToastBrush);
	}
	ToastText = MakeText(TEXT("ActionToastText"), 17, Body, false);
	ToastText->SetJustification(ETextJustify::Center);
	UOverlaySlot* ToastTextSlot = ToastOverlay->AddChildToOverlay(ToastText);
	ToastTextSlot->SetHorizontalAlignment(HAlign_Center);
	ToastTextSlot->SetVerticalAlignment(VAlign_Center);
	ToastBorder->SetContent(ToastOverlay);
	ToastBorder->SetVisibility(ESlateVisibility::Collapsed);

	EndingCinematicBorder = MakePanel(Canvas, TEXT("EndingCinematicPanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.004f, 0.004f, 0.006f, 0.94f));
	EndingCinematicBorder->SetPadding(FMargin(0));
	{
		UOverlay* EndingOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("EndingOverlay"));
		if (InkBrushTexture)
		{
			UImage* EndingBrush = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EndingBrush"));
			EndingBrush->SetBrushFromTexture(InkBrushTexture, true);
			EndingBrush->SetColorAndOpacity(FLinearColor(0.006f, 0.006f, 0.008f, 0.6f));
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

	OpeningBorder = MakePanel(Canvas, TEXT("OpeningPanel"), FAnchors(0, 0, 1, 1), FMargin(0), FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
	OpeningBorder->SetPadding(FMargin(0));
	{
		UOverlay* OpeningOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("OpeningOverlay"));
		USizeBox* OpeningContentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("OpeningContentSize"));
		OpeningContentSize->SetWidthOverride(980.0f);
		UVerticalBox* OpeningBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OpeningBox"));
		OpeningContentSize->SetContent(OpeningBox);
		OpeningText = MakeText(TEXT("OpeningStoryText"), 28, FLinearColor::White, true);
		OpeningText->SetFont(UIFont(28, false));
		OpeningText->SetJustification(ETextJustify::Center);
		OpeningText->SetLineHeightPercentage(1.35f);
		OpeningBox->AddChildToVerticalBox(OpeningText)->SetPadding(FMargin(0, 0, 0, 20));
		OpeningDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OpeningDivider"));
		OpeningDivider->SetBrushColor(WSUITokens::Color::StrokeDivider);
		USizeBox* OpeningDividerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("OpeningDividerSize"));
		OpeningDividerSize->SetWidthOverride(280.0f);
		OpeningDividerSize->SetHeightOverride(1.0f);
		OpeningDividerSize->SetContent(OpeningDivider);
		UVerticalBoxSlot* OpeningDividerSlot = OpeningBox->AddChildToVerticalBox(OpeningDividerSize);
		OpeningDividerSlot->SetHorizontalAlignment(HAlign_Center);
		OpeningDividerSlot->SetPadding(FMargin(0, 0, 0, 14));
		OpeningDivider->SetVisibility(ESlateVisibility::Collapsed);
		OpeningSubtitleText = MakeText(TEXT("OpeningSubtitleText"), 16, WSUITokens::Color::TextSecondary);
		OpeningSubtitleText->SetJustification(ETextJustify::Center);
		OpeningBox->AddChildToVerticalBox(OpeningSubtitleText)->SetPadding(FMargin(0, 0, 0, 12));
		OpeningFooterText = MakeText(TEXT("OpeningFooterText"), 14, WSUITokens::Color::TextSecondary, false);
		OpeningFooterText->SetJustification(ETextJustify::Center);
		OpeningBox->AddChildToVerticalBox(OpeningFooterText);
		UOverlaySlot* OpeningBoxSlot = OpeningOverlay->AddChildToOverlay(OpeningContentSize);
		OpeningBoxSlot->SetHorizontalAlignment(HAlign_Center);
		OpeningBoxSlot->SetVerticalAlignment(VAlign_Center);
		OpeningBorder->SetContent(OpeningOverlay);
	}
	OpeningLines = BuildOpeningStoryLines();
	OpeningElapsed = 0.0f;
	OpeningPhase = EWSOpeningPhase::FadingInLine;
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
	SilenceButton(PauseCloseButton);
	PauseCloseButton->SetBackgroundColor(WSUITokens::Color::ButtonNormal);
	UTextBlock* PauseCloseText = MakeText(TEXT("PauseCloseText"), 13, Secondary, false);
	PauseCloseText->SetText(FText::FromString(TEXT("✕ 关闭")));
	PauseCloseButton->SetContent(PauseCloseText);
	PauseCloseButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ResumeGame);
	PauseTitleRow->AddChildToHorizontalBox(PauseCloseButton)->SetVerticalAlignment(VAlign_Center);
	PauseBox->AddChildToVerticalBox(PauseTitleRow)->SetPadding(FMargin(0, 0, 0, 7));
	PauseStatusText = MakeText(TEXT("PauseStatus"), 14, Secondary);
	PauseStatusText->SetJustification(ETextJustify::Left);
	PauseBox->AddChildToVerticalBox(PauseStatusText)->SetPadding(FMargin(0, 0, 0, 13));
	UButton* ResumeButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_resume"), TEXT("继续游戏")), TEXT("ResumeButton"));
	PauseDefaultButton = ResumeButton;
	UButton* SaveButton = MakeButton(PauseBox, FText::FromString(TEXT("保存本轮　｜　记录当前状态")), TEXT("SaveButton"));
	LoadGameButton = MakeButton(PauseBox, FText::FromString(TEXT("读取存档　｜　恢复最近记录")), TEXT("LoadButton"));
	UButton* SettingsButton = MakeButton(PauseBox, FText::FromString(TEXT("设置　　　｜　画面、声音与辅助")), TEXT("SettingsButton"));
	UButton* HelpButton = MakeButton(PauseBox, FText::FromString(TEXT("生存手册　｜　目标、状态与操作")), TEXT("HelpButton"));
	UButton* RestartButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_restart"), TEXT("重新开始")), TEXT("RestartButton"));
	UButton* QuitButton = MakeButton(PauseBox, FWSPresentationText::UI(TEXT("ui_quit"), TEXT("退出到桌面")), TEXT("QuitButton"));
	ResumeButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ResumeGame);
	SaveButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::SaveGame);
	LoadGameButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::LoadGame);
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
	PauseHelpText->SetText(FText::FromString(TEXT("WASD 移动　鼠标观察　Space 跳跃\nF 互动 / 对话　E 证据板　Enter 结算阶段　Esc 返回")));
	PauseHelpText->SetJustification(ETextJustify::Center);
	PauseHelpText->SetVisibility(ESlateVisibility::Collapsed);
	PauseBox->AddChildToVerticalBox(PauseHelpText)->SetPadding(FMargin(8, 12, 8, 0));
	PauseBorder->SetVisibility(ESlateVisibility::Collapsed);

	SettingsBorder = MakeGlassPanel(Canvas, TEXT("SettingsPanel"), FAnchors(0.5f, 0.5f), FMargin(-370, -330, 740, 660), 16.0f, WSUITokens::Color::SurfaceDeep);
	SetGlassPanelPadding(SettingsBorder, FMargin(18));
	UVerticalBox* SettingsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsBox"));
	SetGlassPanelContent(SettingsBorder, SettingsBox);
	UTextBlock* SettingsTitle = MakeText(TEXT("SettingsTitle"), 31, Body);
	SettingsTitle->SetText(FText::FromString(TEXT("设置")));
	SettingsTitle->SetJustification(ETextJustify::Center);
	SettingsBox->AddChildToVerticalBox(SettingsTitle)->SetPadding(FMargin(0, 0, 0, 8));
	UTextBlock* SettingsHint = MakeText(TEXT("SettingsHint"), 13, Secondary);
	SettingsHint->SetText(FText::FromString(TEXT("显示与音频实时保存；API Key 只保留到本次运行结束。")));
	SettingsHint->SetJustification(ETextJustify::Center);
	SettingsBox->AddChildToVerticalBox(SettingsHint)->SetPadding(FMargin(0, 0, 0, 10));
	USizeBox* SettingsScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SettingsScrollSize"));
	SettingsScrollSize->SetHeightOverride(495.0f);
	SettingsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SettingsScroll"));
	SettingsScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	UVerticalBox* SettingsContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsContent"));
	SettingsScroll->AddChild(SettingsContent);
	SettingsScrollSize->SetContent(SettingsScroll);
	SettingsBox->AddChildToVerticalBox(SettingsScrollSize);
	auto AddSettingsRow = [this, SettingsContent](const FName Name, const FString& Label, UTextBlock*& OutValueText)
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
		SettingsContent->AddChildToVerticalBox(Row)->SetPadding(FMargin(8, 7));
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
	UTextBlock* TextScaleValue = nullptr;
	TextScaleSlider = AddSettingsRow(TEXT("TextScaleSlider"), TEXT("界面字号"), TextScaleValue);
	TextScaleValueText = TextScaleValue;
	FOVSlider->OnValueChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleFOVChanged);
	MasterVolumeSlider->OnValueChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleMasterVolumeChanged);
	AmbienceVolumeSlider->OnValueChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleAmbienceVolumeChanged);
	EffectsVolumeSlider->OnValueChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleEffectsVolumeChanged);
	FeedbackVolumeSlider->OnValueChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleFeedbackVolumeChanged);
	TextScaleSlider->OnValueChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleTextScaleChanged);
	ReducedMotionButton = MakeButton(
		SettingsContent,
		FText::FromString(TEXT("减少动态效果　｜　关闭")),
		TEXT("ReducedMotionButton"));
	ReducedMotionButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ToggleReducedMotion);
	ReducedMotionValueText = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("ReducedMotionButtonLabel")));
	SettingsContent->AddChildToVerticalBox(MakeText(TEXT("SettingsScope"), 12, Secondary))->SetPadding(FMargin(8, 10, 8, 14));
	if (UTextBlock* ScopeText = Cast<UTextBlock>(SettingsContent->GetChildAt(SettingsContent->GetChildrenCount() - 1)))
	{
		ScopeText->SetText(FText::FromString(TEXT("字号 90%–120%｜减少动态效果会保留逐句推进并缩短淡入淡出")));
		ScopeText->SetJustification(ETextJustify::Center);
	}

	UTextBlock* LLMTitle = MakeText(TEXT("LLMSettingsTitle"), 20, Cyan, false);
	LLMTitle->SetFont(UIFont(20, true));
	LLMTitle->SetText(FText::FromString(TEXT("语言模型（可选）")));
	SettingsContent->AddChildToVerticalBox(LLMTitle)->SetPadding(FMargin(8, 8, 8, 4));
	UTextBlock* LLMIntro = MakeText(TEXT("LLMSettingsIntro"), 12, Secondary);
	LLMIntro->SetText(FText::FromString(TEXT("模型只理解自由文本并组织 NPC 表达；规则、行动点和结局仍由本地系统决定。")));
	SettingsContent->AddChildToVerticalBox(LLMIntro)->SetPadding(FMargin(8, 0, 8, 10));

	auto AddLLMControlRow = [this, SettingsContent](const FName Name, const FString& Label, UWidget* Control)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("Row"))));
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("LabelBox"))));
		LabelBox->SetWidthOverride(150.0f);
		UTextBlock* LabelText = MakeText(FName(*(Name.ToString() + TEXT("Label"))), 15, Body, false);
		LabelText->SetText(FText::FromString(Label));
		LabelBox->SetContent(LabelText);
		Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);
		USizeBox* ControlBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("ControlBox"))));
		ControlBox->SetWidthOverride(510.0f);
		ControlBox->SetMinDesiredHeight(34.0f);
		ControlBox->SetContent(Control);
		Row->AddChildToHorizontalBox(ControlBox)->SetVerticalAlignment(VAlign_Center);
		SettingsContent->AddChildToVerticalBox(Row)->SetPadding(FMargin(8, 5));
	};
	auto ConfigureLLMTextInput = [this](UEditableTextBox* Input)
	{
		FEditableTextBoxStyle InputStyle = Input->GetWidgetStyle();
		InputStyle.SetFont(UIFont(14));
		InputStyle.TextStyle.SetColorAndOpacity(FSlateColor(WSUITokens::Color::TextPrimary));
		InputStyle.BackgroundImageNormal.TintColor = FSlateColor(WSUITokens::Color::SurfaceInput);
		InputStyle.BackgroundImageHovered.TintColor = FSlateColor(WSUITokens::Color::SurfaceInputFocused);
		InputStyle.BackgroundImageFocused.TintColor = FSlateColor(WSUITokens::Color::SurfaceInputFocused);
		InputStyle.ForegroundColor = FSlateColor(WSUITokens::Color::TextPrimary);
		InputStyle.FocusedForegroundColor = FSlateColor(WSUITokens::Color::TextPrimary);
		InputStyle.ReadOnlyForegroundColor = FSlateColor(WSUITokens::Color::TextSecondary);
		InputStyle.BackgroundColor = FSlateColor(WSUITokens::Color::SurfaceInput);
		Input->SetWidgetStyle(InputStyle);
		Input->SetForegroundColor(WSUITokens::Color::TextPrimary);
	};

	LLMProviderCombo = WidgetTree->ConstructWidget<UComboBoxString>(
		UComboBoxString::StaticClass(),
		TEXT("LLMProviderCombo"));
	for (const FWSLLMProviderPreset& Preset : UWSAgentGateway::GetProviderPresets())
	{
		LLMProviderCombo->AddOption(Preset.DisplayName);
	}
	LLMProviderCombo->OnSelectionChanged.AddDynamic(this, &UWhiteoutHUDWidget::HandleLLMProviderChanged);
	AddLLMControlRow(TEXT("LLMProvider"), TEXT("厂商"), LLMProviderCombo);

	LLMBaseUrlInput = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("LLMBaseUrlInput"));
	ConfigureLLMTextInput(LLMBaseUrlInput);
	LLMBaseUrlInput->SetHintText(FText::FromString(TEXT("官方 BaseURL")));
	AddLLMControlRow(TEXT("LLMBaseUrl"), TEXT("BaseURL"), LLMBaseUrlInput);

	LLMApiKeyInput = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("LLMApiKeyInput"));
	ConfigureLLMTextInput(LLMApiKeyInput);
	LLMApiKeyInput->SetIsPassword(true);
	LLMApiKeyInput->SetHintText(FText::FromString(TEXT("仅保存在当前进程内存")));
	AddLLMControlRow(TEXT("LLMApiKey"), TEXT("API Key"), LLMApiKeyInput);

	LLMModelCandidateCombo = WidgetTree->ConstructWidget<UComboBoxString>(
		UComboBoxString::StaticClass(),
		TEXT("LLMModelCandidateCombo"));
	LLMModelCandidateCombo->OnSelectionChanged.AddDynamic(
		this,
		&UWhiteoutHUDWidget::HandleLLMModelCandidateChanged);
	AddLLMControlRow(TEXT("LLMModelCandidate"), TEXT("常用模型"), LLMModelCandidateCombo);

	LLMModelInput = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("LLMModelInput"));
	ConfigureLLMTextInput(LLMModelInput);
	LLMModelInput->SetHintText(FText::FromString(TEXT("可直接填写厂商支持的模型 ID")));
	AddLLMControlRow(TEXT("LLMModel"), TEXT("模型 ID"), LLMModelInput);
	LLMModelHintText = MakeText(TEXT("LLMModelHint"), 11, Secondary);
	SettingsContent->AddChildToVerticalBox(LLMModelHintText)->SetPadding(FMargin(158, 0, 8, 6));

	LLMEnabledButton = MakeButton(
		SettingsContent,
		FText::FromString(TEXT("模型调用　｜　关闭")),
		TEXT("LLMEnabledButton"));
	LLMEnabledButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ToggleLLMEnabled);
	LLMEnabledValueText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("LLMEnabledButtonLabel")));
	LLMApplyButton = MakeButton(
		SettingsContent,
		FText::FromString(TEXT("应用模型设置")),
		TEXT("LLMApplyButton"));
	LLMApplyButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::ApplyLLMSettings);
	LLMStatusText = MakeText(TEXT("LLMStatus"), 12, Secondary);
	LLMStatusText->SetJustification(ETextJustify::Center);
	SettingsContent->AddChildToVerticalBox(LLMStatusText)->SetPadding(FMargin(8, 8, 8, 12));

	UButton* SettingsBackButton = MakeButton(SettingsBox, FText::FromString(TEXT("返回暂停菜单")), TEXT("SettingsBackButton"));
	SettingsBackButton->OnClicked.AddDynamic(this, &UWhiteoutHUDWidget::CloseSettings);
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

void UWhiteoutHUDWidget::SilenceButton(UButton* Button)
{
	if (!Button)
	{
		return;
	}
	FButtonStyle ButtonStyle = Button->GetStyle();
	ButtonStyle.HoveredSlateSound = FSlateSound();
	ButtonStyle.PressedSlateSound = FSlateSound();
	Button->SetStyle(ButtonStyle);
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
	SilenceButton(Button);
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
	SilenceButton(Button);
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
	for (int32 Index = 0; Index < InitialActionPoints; ++Index)
	{
		Cells += Index < Remaining ? TEXT("■ ") : TEXT("□ ");
	}
	return Cells;
}

FString UWhiteoutHUDWidget::ClockForProgress(
	const EWSDayPhase DayPhase,
	const int32 Remaining)
{
	int32 PhaseIndex = 3;
	if (DayPhase == EWSDayPhase::Morning)
	{
		PhaseIndex = 0;
	}
	else if (DayPhase == EWSDayPhase::Afternoon)
	{
		PhaseIndex = 1;
	}
	else if (DayPhase == EWSDayPhase::Dusk)
	{
		PhaseIndex = 2;
	}
	const int32 UsedInPhase = PhaseIndex < 3
		? PhaseActionPoints
			- FMath::Clamp(Remaining, 0, PhaseActionPoints)
		: 0;
	const int32 ElapsedMinutes =
		(PhaseIndex * PhaseActionPoints + UsedInPhase)
		* MinutesPerActionPoint;
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
		|| Layer == EWSUILayer::Guide
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
	ResetMouseToViewportCenter();
}

void UWhiteoutHUDWidget::ResetMouseToViewportCenter()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
		if (ViewportWidth > 0 && ViewportHeight > 0)
		{
			PlayerController->SetMouseLocation(ViewportWidth / 2, ViewportHeight / 2);
		}
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
			return;
		}
	}
}

TArray<FText> UWhiteoutHUDWidget::BuildOpeningStoryLines()
{
	return {
		FText::FromString(TEXT("风雪站是高纬山区的气象与通信中继站。今天，你是三人值班组的负责人。")),
		FText::FromString(TEXT("昨夜，暴风雪切断了外部线路；清晨八点十五分，主电源又在一次异常重启后停机。")),
		FText::FromString(TEXT("备用电池只能撑到傍晚。届时供暖、照明和无线电会一起停止。")),
		FText::FromString(TEXT("顾衡是站里唯一熟悉发电机和天线控制系统的工程师。事故后，他不愿多谈昨夜的操作经过。")),
		FText::FromString(TEXT("修复需要顾衡判断故障、核对现场并配合操作；具体条件只能通过调查和交谈确认。")),
		FText::FromString(TEXT("叶澄是值班医生，负责医疗判断与有限物资。她还在评估三人的状态和供暖顺序。")),
		FText::FromString(TEXT("没有可靠的状态评估和资源安排，任何维修计划都可能把三个人推向更大风险。")),
		FText::FromString(TEXT("两人对责任、风险和资源顺序意见不一。你需要调查现场，再用证据、帮助或承诺争取合作。")),
		FText::FromString(TEXT("今天共有 12 点行动力，分为早晨、午后、黄昏三个阶段，每阶段 4 点。未使用的行动力不会结转。")),
		FText::FromString(TEXT("每次行动都会推进时间并改变状态、信任与物资。风雪敲打站体——你在控制室睁开了眼。"))};
}

FString UWhiteoutHUDWidget::BuildVisibleInjuryLabel(
	const EWSCharacterId CharacterId,
	const FWSGameState& State)
{
	if (CharacterId == EWSCharacterId::GuHeng
		&& !FWSKnowledgePolicy::IsGuHengInjuryVisible(State))
	{
		return TEXT("未确认");
	}
	const FWSCharacterState* Character = State.Characters.Find(CharacterId);
	return Character
		? InjuryLabel(Character->InjurySeverity)
		: TEXT("未知");
}

FString UWhiteoutHUDWidget::BuildVisibleCharacterStatus(
	const EWSCharacterId CharacterId,
	const FWSGameState& State)
{
	const FWSCharacterState* Character = State.Characters.Find(CharacterId);
	if (!Character)
	{
		return TEXT("状态未知");
	}
	const FString Stamina = Character->Stamina >= 2
		? TEXT("充足")
		: Character->Stamina == 1
			? TEXT("吃紧")
			: TEXT("耗尽");
	const FString Pressure = Character->Pressure >= 8.0f
		? TEXT("高度紧绷")
		: Character->Pressure >= 5.0f
			? TEXT("紧张")
			: TEXT("可控");
	FString Result = FString::Printf(
		TEXT("体温 %s｜体能 %s｜伤势 %s｜压力 %s"),
		*FWSPresentationText::ConditionLevel(Character->Temperature).ToString(),
		*Stamina,
		*BuildVisibleInjuryLabel(CharacterId, State),
		*Pressure);
	if (CharacterId != EWSCharacterId::Player)
	{
		Result += TEXT("｜信任 ") + FWSPresentationText::TrustLevel(
			Character->Trust).ToString();
	}
	return Result;
}

FString UWhiteoutHUDWidget::BuildDialogueCardSummary(
	const EWSCharacterId CharacterId,
	const FWSGameState& State)
{
	const FWSCharacterState* Character = State.Characters.Find(CharacterId);
	const FWSCharacterState SafeState = Character ? *Character : FWSCharacterState();
	const FString Relationship = SafeState.Trust >= 6.2f ? TEXT("信任")
		: SafeState.Trust >= 5.0f ? TEXT("可合作")
		: SafeState.Trust >= 4.2f ? TEXT("有所保留") : TEXT("戒备");
	const bool bGuHeng = CharacterId == EWSCharacterId::GuHeng;
	FString Stance;
	if (bGuHeng)
	{
		Stance = State.Flags.bGuHengCooperative ? TEXT("愿意配合维修")
			: State.Flags.bGuHengTreated ? TEXT("等待维修条件")
			: State.Flags.bGuHengDiagnosed ? TEXT("带伤防御")
			: TEXT("警惕并回避具体情况");
	}
	else
	{
		Stance = State.Flags.bGuHengTreated ? TEXT("持续监测人员状态")
			: State.Flags.bMedicalRoomHeated ? TEXT("准备诊疗")
			: TEXT("优先恢复医疗条件");
	}
	const FString Identity = bGuHeng
		? TEXT("顾衡｜工程师")
		: TEXT("叶澄｜医生");
	return FString::Printf(
		TEXT("%s\n\n关系　%s\n立场　%s\n\n状态概览\n%s"),
		*Identity,
		*Relationship,
		*Stance,
		*BuildVisibleCharacterStatus(CharacterId, State));
}

FText UWhiteoutHUDWidget::BuildDialogueInputHint(const EWSDialogueAct DialogueAct)
{
	switch (DialogueAct)
	{
	case EWSDialogueAct::Challenge:
		return FText::FromString(TEXT("例：你前后的说法对不上，请解释清楚。"));
	case EWSDialogueAct::Reassure:
		return FText::FromString(TEXT("例：先稳住，我们一步一步处理。"));
	case EWSDialogueAct::Promise:
		return FText::FromString(TEXT("例：我会保留维修记录，也不会临时改口。"));
	default:
		return FText::FromString(TEXT("例：你现在能确认什么？"));
	}
}

FString UWhiteoutHUDWidget::BuildPhaseSettlementSummary(
	const FWSPhaseSummary&,
	const FWSGameState& State)
{
	return FString::Printf(
		TEXT("供暖与行动影响已结算。\n玩家：%s\n顾衡：%s\n叶澄：%s"),
		*BuildVisibleCharacterStatus(EWSCharacterId::Player, State),
		*BuildVisibleCharacterStatus(EWSCharacterId::GuHeng, State),
		*BuildVisibleCharacterStatus(EWSCharacterId::YeCheng, State));
}

FString UWhiteoutHUDWidget::BuildObjectiveSummary(const FWSGameState& State)
{
	const FString ObjectiveFormat = TEXT(
		"阶段：{0}｜供暖 {1}\n"
		"任务：发电机 {2}/2｜天线 {3}/1｜求救 {4}\n"
		"储备：燃料 {5}｜食品 {6}｜药品 {7}");
	FString Result = FString::Format(
		*ObjectiveFormat,
		{DayPhaseLabel(State.DayPhase),
		 HeatingZoneLabel(State.Heating.CurrentZone),
		 State.Tasks.GeneratorProgress,
		 State.Tasks.AntennaCalibration,
		 FWSPresentationText::UI(
			 State.Tasks.bSignalSent ? TEXT("ui_sent") : TEXT("ui_not_sent"),
			 State.Tasks.bSignalSent ? TEXT("已发送") : TEXT("未发送")).ToString(),
		 State.Resources.Fuel,
		 State.Resources.Food,
		 State.Resources.Medicine});
	TArray<FString> KnownDetails;
	if (FWSKnowledgePolicy::IsRelayRepairRouteVisible(State))
	{
		KnownDetails.Add(FString::Printf(
			TEXT("继电器 %d"),
			State.Resources.ReplacementRelay));
	}
	if (FWSKnowledgePolicy::IsHeatPackOptionVisible(State))
	{
		KnownDetails.Add(FString::Printf(
			TEXT("保温包 %d"),
			State.Resources.HeatPack));
	}
	KnownDetails.Add(FString::Printf(TEXT("证据 %d"), State.Evidence.Num()));
	KnownDetails.Add(State.Flags.bKitchenHeaterIntact ? TEXT("厨房设施 完好") : TEXT("厨房设施 已拆解"));
	return Result + TEXT("\n") + FString::Join(KnownDetails, TEXT("｜"));
}

void UWhiteoutHUDWidget::UpdateFromState(const FWSGameState& State)
{
	const FString PhaseCondition = !State.bDayPhaseStarted
		? TEXT("阶段待开始 ｜ 选择一个供暖区")
		: FString::Printf(
			TEXT("本阶段供暖：%s ｜ Enter 结算阶段"),
			*HeatingZoneLabel(State.Heating.CurrentZone));
	if (TopText) TopText->SetText(FWSPresentationText::UI(TEXT("title"), TEXT("风雪站：断电前夜")));
	if (TopStatusText)
	{
		TopStatusText->SetText(FText::Format(
			FText::FromString(TEXT("{0} ｜ {1} AP {2} / 4 ｜ {3}")),
			FText::FromString(
				ClockForProgress(State.DayPhase, State.ActionPoints)),
			FText::FromString(DayPhaseLabel(State.DayPhase)),
			FText::AsNumber(State.ActionPoints),
			FWSPresentationText::PhaseLabel(State.Phase)));
		TopStatusText->SetColorAndOpacity(FSlateColor(
			State.ActionPoints <= 1 ? Danger : Body));
	}
	if (TopConditionText)
	{
		TopConditionText->SetText(FText::FromString(PhaseCondition));
	}

	ObjectiveText->SetText(FText::FromString(BuildObjectiveSummary(State)));
	if (TutorialTitleText)
	{
		int32 MinimumAP = 0;
		const FString TaskGuide = BuildTaskGuide(State, MinimumAP);
		const FString GuideTitle = State.bDayPhaseStarted
			? FString::Printf(
				TEXT("可选下一步｜本阶段 %d / 4 AP"),
				State.ActionPoints)
			: TEXT("阶段准备｜四个供暖区任选其一");
		TutorialTitleText->SetText(FText::FromString(GuideTitle));
		if (TutorialText)
		{
			TutorialText->SetColorAndOpacity(
				FSlateColor(WSUITokens::Color::TextCinematicWarm));
			TutorialText->SetText(FText::FromString(TaskGuide));
		}
	}
	UpdateGuideContext(State);

	CrewText->SetText(FWSPresentationText::UI(TEXT("ui_crew_header_v03"), TEXT("值班组状态")));
	const TArray<EWSCharacterId> CharacterIds = {EWSCharacterId::Player, EWSCharacterId::GuHeng, EWSCharacterId::YeCheng};
	for (int32 CharacterIndex = 0; CharacterIndex < CharacterIds.Num(); ++CharacterIndex)
	{
		const EWSCharacterId CharacterId = CharacterIds[CharacterIndex];
		if (const FWSCharacterState* Character = State.Characters.Find(CharacterId))
		{
			const bool bInjuryVisible = CharacterId != EWSCharacterId::GuHeng
				|| FWSKnowledgePolicy::IsGuHengInjuryVisible(State);
			if (CrewCardTexts.IsValidIndex(CharacterIndex))
			{
				FString Card = FWSPresentationText::CharacterName(CharacterId).ToString();
				Card += TEXT("\n");
				Card += BuildVisibleCharacterStatus(CharacterId, State);
				CrewCardTexts[CharacterIndex]->SetText(FText::FromString(Card));
			}
			const float InjuryRatio = !bInjuryVisible
				? 1.0f
				:
				Character->InjurySeverity == EWSInjurySeverity::Normal
				? 1.0f
				: Character->InjurySeverity
					== EWSInjurySeverity::Restricted
				? 0.5f
				: 0.0f;
			const float ReadinessRatio = FMath::Min(
				FMath::Min(
					FMath::Clamp(
						Character->Temperature / 6.0f,
						0.0f,
						1.0f),
					FMath::Clamp(
						static_cast<float>(Character->Stamina) / 2.0f,
						0.0f,
						1.0f)),
				FMath::Min(
					InjuryRatio,
					FMath::Clamp(
						1.0f - Character->Pressure / 10.0f,
						0.0f,
						1.0f)));
			const TArray<float> Ratios = {
				Character->Temperature / 10.0f,
				static_cast<float>(Character->Stamina) / 2.0f,
				InjuryRatio,
				1.0f - Character->Pressure / 10.0f,
				ReadinessRatio};
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
				CrewTrustBars[CharacterIndex]->SetPercent(FMath::Clamp(Character->Trust / 10.0f, 0.0f, 1.0f));
			}
		}
	}
	if (PauseStatusText)
	{
		PauseStatusText->SetText(FText::FromString(FString::Printf(
			TEXT("%s　｜　%s AP %d / 4　｜　%s"),
			*ClockForProgress(State.DayPhase, State.ActionPoints),
			*DayPhaseLabel(State.DayPhase),
			State.ActionPoints,
			*FWSPresentationText::PhaseLabel(State.Phase).ToString())));
	}
	if (PauseSituationText)
	{
		PauseSituationText->SetText(FText::FromString(TEXT("当前情况")));
	}
	if (PauseSituationValues.Num() >= 5)
	{
		PauseSituationValues[0]->SetText(FText::FromString(FString::Printf(TEXT("%d / 4"), State.ActionPoints)));
		PauseSituationValues[1]->SetText(FText::FromString(
			ClockForProgress(EWSDayPhase::Complete, 0)));
		PauseSituationValues[2]->SetText(FText::FromString(FString::Printf(TEXT("%d / 2"), State.Tasks.GeneratorProgress)));
		PauseSituationValues[3]->SetText(FText::FromString(FString::Printf(TEXT("%d / 1"), State.Tasks.AntennaCalibration)));
		PauseSituationValues[4]->SetText(FText::FromString(State.Tasks.bSignalSent ? TEXT("已发送") : TEXT("未发送")));
	}
	FeedbackText->SetText(FText::FromString(SystemMessage));
	PromptText->SetText(InteractionPrompt);
	UpdateEvidence(State);
	UpdateResults(State);
}

FString UWhiteoutHUDWidget::BuildTutorialHint(const FWSGameState& State) const
{
	if (State.Phase == EWSGamePhase::Results)
	{
		return TEXT("本轮已经结算。复盘会列出结局、人员、储备与信息责任的实际代价。");
	}
	if (State.Tasks.bSignalSent)
	{
		return TEXT("信号已发送。按 Enter 立即结算，也可先查看当前人员与储备状态。");
	}
	if (!State.bDayPhaseStarted)
	{
		return TEXT("比较四个供暖区：维修、医疗、热餐和记录保护分别支持不同策略。");
	}
	if (State.Tasks.GeneratorProgress < 2)
	{
		return TEXT("可调查事故、照护队员、补给休整或直接推进维修；按 Q 查看行动的替代方案。");
	}
	if (State.Tasks.AntennaCalibration < 1)
	{
		return TEXT("可立刻校准天线，也可先恢复体能、降低风险或让叶澄协助。");
	}
	return TEXT("可现在发送信号结束本轮，也可用剩余阶段改善人员与储备，或保温等待。");
}

FString UWhiteoutHUDWidget::BuildTaskGuide(
	const FWSGameState& State,
	int32& OutMinimumAP) const
{
	OutMinimumAP = 0;
	if (State.Phase == EWSGamePhase::Results)
	{
		return TEXT("本轮已结算。查看复盘后按 R 开始新一轮。");
	}
	if (State.Tasks.bSignalSent)
	{
		return TEXT("求救信号已发送。按 Enter 进入结局复盘。");
	}
	if (!State.bDayPhaseStarted)
	{
		return FString::Printf(
			TEXT(
				"• 维修间：降低精细维修的寒冷代价\n"
				"• 医务室：开放完整治疗\n"
				"• 厨房：可准备热餐并恢复体能\n"
				"• 控制室：保护记录并稳定信息链\n"
				"当前燃料 %d；选择后本阶段锁定。"),
			State.Resources.Fuel);
	}

	TArray<FString> Options;
	if (State.PinnedRequirementActions.Contains(TEXT("repair_generator")))
	{
		const FWSNegotiationOffer* ActiveOffer = State.NegotiationOffers.FindByPredicate(
			[](const FWSNegotiationOffer& Offer)
			{
				return Offer.TargetActionId == TEXT("repair_generator")
					&& Offer.bAccepted
					&& !Offer.bFulfilled
					&& !Offer.bBroken;
			});
		Options.Add(ActiveOffer
			? TEXT("• 已接受：本阶段内完成顾衡的发电机协作条件")
			: TEXT("• 已固定：顾衡提出的发电机协作条件"));
	}
	if (State.Tasks.GeneratorProgress < 2)
	{
		Options.Add(TEXT("• 调查：检查日志或控制柜，确认停机原因和可用线索"));
		Options.Add(TEXT("• 人员：分配食物、休整、包扎或完整治疗"));
		Options.Add(TEXT("• 工程：维修发电机；按 Q 切换当前已知的可行方案"));
	}
	else if (State.Tasks.AntennaCalibration < 1)
	{
		Options.Add(TEXT("• 室外：校准天线；按 Q 切换单独、叶澄协助或强行"));
		Options.Add(TEXT("• 准备：食物、治疗或休整，降低室外行动风险"));
		Options.Add(TEXT("• 保留：提前 Enter 结算，留下资源进入下一阶段"));
	}
	else
	{
		Options.Add(TEXT("• 发信：回控制室立即发送求救信号"));
		Options.Add(TEXT("• 整备：先恢复体能、治疗伤势或平衡食物分配"));
		Options.Add(TEXT("• 等待：保留燃料并推进阶段，接受未知救援结果"));
	}
	if (State.ActionPoints > 0)
	{
		Options.Add(FString::Printf(
			TEXT("• 结束阶段：Enter 放弃剩余 %d AP，进入下一阶段"),
			State.ActionPoints));
	}
	return FString::Join(Options, TEXT("\n"));
}

void UWhiteoutHUDWidget::UpdateGuideContext(const FWSGameState& State)
{
	if (!GuideContextText)
	{
		return;
	}
	auto CharacterLine = [&State](const EWSCharacterId CharacterId)
	{
		const FWSCharacterState* Character = State.Characters.Find(CharacterId);
		if (!Character)
		{
			return FString();
		}
		return FString::Printf(
			TEXT("%s　%s｜位置 %s"),
			*FWSPresentationText::CharacterName(CharacterId).ToString(),
			*BuildVisibleCharacterStatus(CharacterId, State),
			*CharacterLocationLabel(Character->Location));
	};
	GuideContextText->SetText(FText::FromString(FString::Printf(
		TEXT("%s即时读数｜AP %d / 4｜供暖 %s\n%s\n%s\n%s\n\n选择提示：%s"),
		*DayPhaseLabel(State.DayPhase),
		State.ActionPoints,
		*HeatingZoneLabel(State.Heating.CurrentZone),
		*CharacterLine(EWSCharacterId::Player),
		*CharacterLine(EWSCharacterId::GuHeng),
		*CharacterLine(EWSCharacterId::YeCheng),
		*BuildTutorialHint(State))));
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
	DialogueNPCText->SetText(FText::FromString(
		BuildDialogueCardSummary(CharacterId, State)));
	if (DialogueNPCBars.Num() >= 4)
	{
		DialogueNPCBars[0]->SetPercent(ScoreRatio(SafeState.Health, 10.0f));
		DialogueNPCBars[1]->SetPercent(ScoreRatio(SafeState.Temperature, 10.0f));
		DialogueNPCBars[2]->SetPercent(ScoreRatio(SafeState.Pressure, 10.0f));
		DialogueNPCBars[3]->SetPercent(ScoreRatio(SafeState.Trust, 10.0f));
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
		TEXT("现场证据 %02d / %02d　｜　已确认事实 %02d / %02d　｜　待核验 %02d"),
		FMath::Clamp(State.Evidence.Num(), 0, CollectableEvidenceCount),
		CollectableEvidenceCount,
		FMath::Clamp(ConfirmedCount, 0, DiscoverableFactCount),
		DiscoverableFactCount,
		ClaimCount)));

	EvidenceCardGrid->ClearChildren();
	EvidenceCardButtons.Reset();
	EvidenceCardDetailCopies.Reset();
	int32 CardIndex = 0;
	const auto AddCard = [this, &CardIndex](
		const FString& Title,
		const FString& Type,
		const FString& Summary,
		const FLinearColor& TypeColor,
		const TCHAR* IconName,
		const int32 Category)
	{
		if (EvidenceFilterIndex != 0 && Category != EvidenceFilterIndex)
		{
			return;
		}
		UButton* CardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*FString::Printf(TEXT("EvidenceCardButton%d"), CardIndex)));
		SilenceButton(CardButton);
		CardButton->SetBackgroundColor(FLinearColor::Transparent);
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
			Icon->SetColorAndOpacity(TypeColor);
			IconBox->SetContent(Icon);
			CardRow->AddChildToHorizontalBox(IconBox)->SetPadding(FMargin(0, 0, 10, 0));
		}
		UTextBlock* CardCopy = MakeText(FName(*FString::Printf(TEXT("EvidenceCopy%d"), CardIndex)), 13, Body);
		CardCopy->SetWrapTextAt(500.0f);
		CardCopy->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s\n%s"), *Type, *Title, *Summary)));
		UHorizontalBoxSlot* CopySlot = CardRow->AddChildToHorizontalBox(CardCopy);
		CopySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CopySlot->SetVerticalAlignment(VAlign_Center);
		SetGlassPanelContent(Card, CardRow);
		CardButton->SetContent(Card);
		UUniformGridSlot* CardSlot = EvidenceCardGrid->AddChildToUniformGrid(CardButton, CardIndex, 0);
		CardSlot->SetHorizontalAlignment(HAlign_Fill);
		CardSlot->SetVerticalAlignment(VAlign_Fill);
		EvidenceCardButtons.Add(CardButton);
		EvidenceCardDetailCopies.Add(FString::Printf(TEXT("%s｜%s\n%s"), *Title, *Type, *Summary));
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
				FString::Printf(
					TEXT("核验状态：%s。%s"),
					*FWSPresentationText::KnowledgeLevel(Pair.Value).ToString(),
					*FWSPresentationText::FactDescription(Pair.Key).ToString()),
				Amber,
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
				FString::Printf(
					TEXT("已确认。%s"),
					*FWSPresentationText::FactDescription(Pair.Key).ToString()),
				Danger,
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
			TEXT("玩家向顾衡作出的可核验承诺。当前状态：") + FWSPresentationText::UI(StatusKey, StatusFallback).ToString(),
			Promise.bSettled && !Promise.bFulfilled ? Danger : Amber,
			TEXT("I_Evidence_Dialogue"),
			4);
	}
	if (CardIndex == 0)
	{
		const int32 EmptyCategory = EvidenceFilterIndex == 0 ? 0 : EvidenceFilterIndex;
		AddCard(TEXT("尚未取得证据"), TEXT("系统"), TEXT("调查发电机日志、控制柜，或与顾衡、叶澄交谈。"), Body, TEXT("I_Evidence_File"), EmptyCategory);
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
	FString RouteName = TEXT("未成型路线");
	FString RouteCost = TEXT("行动没有形成稳定的任务链，关键资源与人员风险缺少统一取舍。");
	FString RouteAdvice = TEXT("先锁定一条完整路径，再用剩余行动处理人员或储备。");
	if (State.Flags.bSelfRepairUsed)
	{
		RouteName = TEXT("强行自修线");
		RouteCost = TEXT("绕开协作直接抢修，节省交涉步骤，同时让玩家承担更高健康与失温风险。");
		RouteAdvice = TEXT("先取得事故记录或顾衡协作，避免把人员状态当作隐性维修材料。");
	}
	else if (State.Flags.bRelayInstalled
		|| State.ActionCounts.FindRef(TEXT("dismantle_kitchen_heater")) > 0)
	{
		RouteName = TEXT("证据替代线");
		RouteCost = TEXT("利用事故证据和替代继电器推进任务，代价是拆解厨房供暖并削弱长期保温能力。");
		RouteAdvice = TEXT("比较保留厨房供暖与消耗医疗资源的分数差异，验证信息路线的真实代价。");
	}
	else if (State.Flags.bGuHengTreated || State.Flags.bMedicalRoomHeated)
	{
		RouteName = TEXT("医疗协作线");
		RouteCost = TEXT("投入供暖与医疗物资稳定伤员，换取较安全的维修过程与更好的人员状态。");
		RouteAdvice = TEXT("尝试减少一次供暖或治疗开销，同时保持顾衡能够安全参与维修。");
	}
	else if (State.ActionCounts.FindRef(TEXT("repair_generator")) >= 2)
	{
		RouteName = TEXT("直接抢修线");
		RouteCost = TEXT("跳过部分调查与交涉，用两次维修换取速度；信息责任、信任和伤情更难兼顾。");
		RouteAdvice = TEXT("补一次关键调查或有效交涉，观察任务速度与信息、关系评分的交换。");
	}

	FString PromiseSummary = TEXT("本轮未作承诺");
	if (!State.Promises.IsEmpty())
	{
		TArray<FString> PromiseLines;
		for (const FWSPromiseRecord& Promise : State.Promises)
		{
			const FString PromiseLabel = Promise.ConditionId == TEXT("keep_records")
				? TEXT("保留事故记录")
				: Promise.ConditionId == TEXT("reserve_medicine")
					? TEXT("保留医疗物资")
					: TEXT("先为维修间供暖");
			const FString PromiseState = !Promise.bSettled
				? TEXT("待结算")
				: Promise.bFulfilled ? TEXT("已兑现") : TEXT("已违背");
			PromiseLines.Add(FString::Printf(TEXT("%s：%s"), *PromiseLabel, *PromiseState));
		}
		PromiseSummary = FString::Join(PromiseLines, TEXT("｜"));
	}
	if (!State.NegotiationOffers.IsEmpty())
	{
		TArray<FString> OfferLines;
		for (const FWSNegotiationOffer& Offer : State.NegotiationOffers)
		{
			const FString OfferState = Offer.bFulfilled
				? TEXT("已履行")
				: Offer.bBroken ? TEXT("已违约") : TEXT("待完成");
			OfferLines.Add(FString::Printf(TEXT("顾衡维修条件：%s"), *OfferState));
		}
		const FString OfferSummary = FString::Join(OfferLines, TEXT("｜"));
		PromiseSummary = PromiseSummary == TEXT("本轮未作承诺")
			? OfferSummary
			: FString::Printf(TEXT("%s｜%s"), *PromiseSummary, *OfferSummary);
	}
	const FString HeaderFormat = FWSPresentationText::UI(
		TEXT("ui_results_header_format_v06"),
		TEXT("行动复盘\n{0}\n{1}\n\n本轮路线　{8}\n关键代价　{9}\n承诺结算　{10}\n\n总分 {2} / 100　｜　评级 {3}\n\n最终状态\n发电机 {4} / 2　天线 {5} / 1　信号 {6}　剩余行动力 {7}")).ToString();
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
		 State.ActionPoints,
		 RouteName,
		 RouteCost,
		 PromiseSummary})));

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
			*ClockForProgress(Event.DayPhase, Event.APAfter),
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
	ResultsAdviceText->SetText(FText::FromString(FString::Printf(
		TEXT("下一轮建议\n%s\n路线复盘：%s\n\n按 R 开始新一轮　｜　Esc 打开退出菜单"),
		*FWSPresentationText::EndingAdvice(State.Ending).ToString(),
		*RouteAdvice)));
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
	}
	if (CrosshairText)
	{
		CrosshairText->SetText(FText::FromString(TEXT("☝")));
		CrosshairText->SetFont(UIFont(20, true));
		CrosshairText->SetColorAndOpacity(FSlateColor(Amber));
		CrosshairText->SetRenderTranslation(FVector2D(0.0f, -7.0f));
		CrosshairText->SetVisibility(ESlateVisibility::Visible);
	}
	if (FocusBorder && FocusText && FocusAPText && FocusKeyText)
	{
		FocusBorder->SetVisibility(ESlateVisibility::Visible);
		FocusBorder->SetBrushColor(FLinearColor::Transparent);
		FocusText->SetColorAndOpacity(FSlateColor(Body));
		FocusText->SetText(FText::FromString(NewName));
		if (bDialogue)
		{
			FocusAPText->SetVisibility(ESlateVisibility::Collapsed);
			FocusKeyText->SetText(FText::FromString(TEXT("　·　[F] 开始对话")));
		}
		else
		{
			FocusAPText->SetVisibility(ESlateVisibility::Visible);
			FocusAPText->SetText(FText::FromString(FString::Printf(TEXT("　·　%d AP"), Preview.APCost)));
			FocusKeyText->SetText(FText::FromString(TEXT("　·　[F] 查看行动")));
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
		CrosshairText->SetText(FText::FromString(TEXT("○")));
		CrosshairText->SetFont(UIFont(14, false));
		CrosshairText->SetColorAndOpacity(FSlateColor(Body));
		CrosshairText->SetRenderTranslation(FVector2D(0.0f, -3.0f));
		CrosshairText->SetVisibility(CurrentLayer == EWSUILayer::Game || CurrentLayer == EWSUILayer::Preview
			? ESlateVisibility::Visible
			: ESlateVisibility::Hidden);
	}
	if (FocusBorder)
	{
		FocusBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWhiteoutHUDWidget::ShowActionPreview(
	const FText& ActionName,
	const FWSActionPreview& Preview,
	const FWSActionRequest& Request)
{
	SetLayer(EWSUILayer::Preview);
	ShowPanelAnimated(PreviewBorder, true, WSUITokens::Anim::Fast, false);
	PreviewTitleText->SetText(ActionName);
	FString Selection;
	bool bCanCycleOption = false;
	if (Request.ActionId == TEXT("distribute_food"))
	{
		Selection = FString::Printf(
			TEXT("当前方案：%s｜玩家 ×%d　顾衡 ×%d　叶澄 ×%d"),
			Request.bHotMeal ? TEXT("热餐") : TEXT("冷口粮"),
			Request.FoodForPlayer,
			Request.FoodForGuHeng,
			Request.FoodForYeCheng);
		bCanCycleOption = true;
	}
	else if (
		Request.ActionId == TEXT("treat_gu_heng")
		|| Request.ActionId == TEXT("treat_character"))
	{
		const FString Method =
			Request.TreatmentMethod == EWSTreatmentMethod::Bandage
			? TEXT("简单包扎")
			: Request.TreatmentMethod == EWSTreatmentMethod::HeatPack
			? TEXT("保温包临时支撑")
			: TEXT("完整治疗（药品 ×1）");
		Selection = FString::Printf(
			TEXT("当前方案：%s｜目标 %s"),
			*Method,
			*CharacterShortLabel(Request.TreatmentTarget));
		bCanCycleOption = true;
	}
	else if (Request.ActionId == TEXT("rest"))
	{
		Selection = FString::Printf(
			TEXT("当前方案：%s在%s休整"),
			*CharacterShortLabel(Request.RestTarget),
			*CharacterLocationLabel(Request.RestLocation));
		bCanCycleOption = true;
	}
	else if (
		Request.ActionId == TEXT("inspect_control_cabinet")
		|| Request.ActionId == TEXT("dismantle_kitchen_heater"))
	{
		Selection = Request.bHasCollaborator
			? FString::Printf(
				TEXT("当前方案：%s协作"),
				*CharacterShortLabel(Request.Collaborator))
			: TEXT("当前方案：单独执行");
		bCanCycleOption = true;
	}
	else if (Request.ActionId == TEXT("repair_generator"))
	{
		Selection = Request.bForce
			? TEXT("当前方案：强迫推进（关系与人身风险）")
			: Request.bUseRelay
			? TEXT("当前方案：安装替代继电器")
			: Request.bHasCollaborator
			? TEXT("当前方案：玩家协助顾衡")
			: TEXT("当前方案：顾衡常规维修");
		bCanCycleOption = true;
	}
	else if (Request.ActionId == TEXT("calibrate_antenna"))
	{
		Selection = Request.bForce
			? TEXT("当前方案：强行校准")
			: Request.bHasCollaborator
			? TEXT("当前方案：叶澄协助")
			: TEXT("当前方案：玩家单独校准");
		bCanCycleOption = true;
	}
	else if (
		Request.ActionId == TEXT("heat_control_room")
		|| Request.ActionId == TEXT("heat_repair_room")
		|| Request.ActionId == TEXT("heat_medical_room")
		|| Request.ActionId == TEXT("heat_kitchen"))
	{
		Selection =
			TEXT("阶段选择：燃料 ×1；确认后本阶段不可更改");
	}
	const bool bHasSelectableOption = !Selection.IsEmpty();
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
		FString ResourceCost = FWSPresentationText::ActionResourceCost(Preview.ActionId).ToString();
		if (bHasSelectableOption)
		{
			ResourceCost = Selection;
		}
		const FString BodyFormat = FWSPresentationText::UI(
			TEXT("ui_preview_body_format"),
			TEXT("行动成本\n行动力 ×{0}\n\n执行者\n{1}\n\n资源成本\n{2}\n\n可预见风险\n{3}\n\n预期结果\n{4}\n\n当前前置条件满足；确认后立即结算。")).ToString();
		PreviewBodyText->SetText(FText::FromString(FString::Format(
			*BodyFormat,
			{Preview.APCost,
			 FWSPresentationText::ActionExecutor(Preview.ActionId).ToString(),
			 ResourceCost,
			 Risk,
			 Expected})));
		PreviewFooterText->SetText(FText::FromString(
			bCanCycleOption
				? TEXT("[Q] 切换方案　｜　再次按 F 确认执行　｜　移开视线取消")
				: FWSPresentationText::UI(TEXT("ui_preview_footer"), TEXT("再次按 F 确认执行　｜　移开视线取消")).ToString()));
	}
	else
	{
		PreviewTitleText->SetColorAndOpacity(FSlateColor(Danger));
		const FString RejectionFormat = FWSPresentationText::UI(TEXT("ui_rejection_format"), TEXT("现在不能执行\n{0}\n\n怎样改变条件\n{1}")).ToString();
		FString Rejection = FString::Format(
			*RejectionFormat,
			{FWSPresentationText::ReasonCause(Preview.ReasonCode).ToString(), FWSPresentationText::ReasonNextStep(Preview.ReasonCode).ToString()});
		if (bHasSelectableOption)
		{
			Rejection = Selection + TEXT("\n\n") + Rejection;
		}
		PreviewBodyText->SetText(FText::FromString(Rejection));
		PreviewFooterText->SetText(FText::FromString(
			bHasSelectableOption
				? TEXT("[Q] 切换方案　｜　按 F 关闭提示")
				: FWSPresentationText::UI(TEXT("ui_rejection_footer"), TEXT("移开视线或按 F 关闭提示")).ToString()));
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
		ResetMouseToViewportCenter();
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
		ResetMouseToViewportCenter();
	}
}

void UWhiteoutHUDWidget::ShowDialogueMenu(const FName NPCActionId, const bool bVisible)
{
	bDialogueVisible = bVisible;
	ActiveDialogueActionId = bVisible ? NPCActionId : NAME_None;
	HideDialogueConditionCard();
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
		ResetMouseToViewportCenter();
	}
}

void UWhiteoutHUDWidget::ShowDialogueWheelChoices()
{
	HideDialogueConditionCard();
	DialogueStage = EWSDialogueStage::IntentPick;
	if (DialogueWheelPanel) DialogueWheelPanel->SetVisibility(ESlateVisibility::Visible);
	if (DialoguePromiseBorder) DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueFreeTextBorder) DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueReplyBorder) DialogueReplyBorder->SetVisibility(ESlateVisibility::Collapsed);
	RefreshDialogueAvailability();
}

void UWhiteoutHUDWidget::ShowDialoguePromiseChoices()
{
	RefreshDialogueAvailability();
	const bool bHasPromiseChoice = DialoguePromiseButtons.ContainsByPredicate(
		[](const UButton* Button)
		{
			return Button && Button->GetVisibility() == ESlateVisibility::Visible;
		});
	if (!bHasPromiseChoice)
	{
		ShowDialogueWheelChoices();
		return;
	}
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
		DialogueFreeTextInput->SetHintText(BuildDialogueInputHint(DialogueAct));
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
		if (PendingDialogueAct == EWSDialogueAct::Ask && PendingPromiseCondition.IsNone())
		{
			Character->SubmitDialogueText(UserText);
		}
		else
		{
			Character->SubmitDialogueChoice(PendingDialogueAct, PendingPromiseCondition, UserText);
		}
	}
}

void UWhiteoutHUDWidget::RefreshDialogueAvailability()
{
	AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn());
	const bool bLiveDialogue = Character && Character->IsDialogueActive();
	if (!bLiveDialogue && !bPresentationCaptureOverride)
	{
		for (UButton* Button : DialogueIntentButtons)
		{
			if (Button)
			{
				Button->SetIsEnabled(false);
				Button->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		for (UButton* Button : DialoguePromiseButtons)
		{
			if (Button)
			{
				Button->SetIsEnabled(false);
				Button->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		return;
	}
	FWhiteoutRulesEngine CaptureRules;
	if (bPresentationCaptureOverride)
	{
		CaptureRules.SetState(PresentationCaptureState);
	}
	const auto PreviewDialogue = [
		this,
		Character,
		bLiveDialogue,
		&CaptureRules](const EWSDialogueAct Act, const FName Condition = NAME_None)
	{
		if (bLiveDialogue)
		{
			return Character->PreviewActiveDialogue(Act, Condition);
		}
		FWSActionRequest Request;
		Request.ActionId = ActiveDialogueActionId;
		Request.DialogueAct = Act;
		Request.PromiseCondition = Condition;
		return CaptureRules.Preview(Request);
	};
	const TArray<EWSDialogueAct> Acts = {
		EWSDialogueAct::Ask,
		EWSDialogueAct::Challenge,
		EWSDialogueAct::Reassure,
		EWSDialogueAct::Promise};
	TArray<UButton*> AvailableButtons;
	FWSActionPreview FirstPreview;
	for (int32 Index = 0; Index < DialogueIntentButtons.Num() && Index < Acts.Num(); ++Index)
	{
		FWSActionPreview Preview;
		if (Acts[Index] == EWSDialogueAct::Promise)
		{
			for (const FName Condition : {FName(TEXT("keep_records")), FName(TEXT("reserve_medicine")), FName(TEXT("heat_repair_room"))})
			{
				const FWSActionPreview Candidate = PreviewDialogue(Acts[Index], Condition);
				if (Candidate.bCanExecute || Preview.ActionId.IsNone()) Preview = Candidate;
				if (Candidate.bCanExecute) break;
			}
		}
		else
		{
			Preview = PreviewDialogue(Acts[Index]);
		}
		if (Index == 0) FirstPreview = Preview;
		DialogueIntentButtons[Index]->SetIsEnabled(Preview.bCanExecute);
		DialogueIntentButtons[Index]->SetVisibility(
			Preview.bCanExecute ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (Preview.bCanExecute)
		{
			AvailableButtons.Add(DialogueIntentButtons[Index]);
		}
	}
	const TArray<FName> PromiseConditions = {
		TEXT("keep_records"),
		TEXT("reserve_medicine"),
		TEXT("heat_repair_room")};
	for (int32 Index = 0; Index < DialoguePromiseButtons.Num() && Index < PromiseConditions.Num(); ++Index)
	{
		const FWSActionPreview Preview = PreviewDialogue(
			EWSDialogueAct::Promise,
			PromiseConditions[Index]);
		DialoguePromiseButtons[Index]->SetIsEnabled(Preview.bCanExecute);
		DialoguePromiseButtons[Index]->SetVisibility(
			Preview.bCanExecute ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	ReflowDialogueIntentButtons(AvailableButtons);
	if (DialogueStatusText)
	{
		DialogueStatusText->SetText(FText::FromString(FString::Printf(
			TEXT("当前可谈 %d 项｜意向会随证据、伤情、关系和局势变化"),
			AvailableButtons.Num())));
		DialogueStatusText->SetColorAndOpacity(FSlateColor(Secondary));
	}
	if (AvailableButtons.IsEmpty() && DialogueLineText)
	{
		DialogueLineText->SetText(FText::Format(
			FText::FromString(TEXT("{0} {1}")),
			FWSPresentationText::ReasonCause(FirstPreview.ReasonCode),
			FWSPresentationText::ReasonNextStep(FirstPreview.ReasonCode)));
		DialogueLineText->SetColorAndOpacity(FSlateColor(Amber));
	}
}

void UWhiteoutHUDWidget::ReflowDialogueIntentButtons(const TArray<UButton*>& AvailableButtons)
{
	const int32 Count = AvailableButtons.Num();
	if (Count <= 0)
	{
		return;
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UButton* Button = AvailableButtons[Index];
		UCanvasPanelSlot* CanvasSlot = Button ? Cast<UCanvasPanelSlot>(Button->Slot) : nullptr;
		if (!CanvasSlot)
		{
			continue;
		}
		const float CenterX = (static_cast<float>(Index) + 0.5f) / static_cast<float>(Count);
		CanvasSlot->SetAnchors(FAnchors(CenterX, 0.5f));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
		CanvasSlot->SetOffsets(FMargin(-75.0f, -24.0f, 150.0f, 48.0f));
	}
}

void UWhiteoutHUDWidget::ShowDialogueReplyActions()
{
	DialogueStage = EWSDialogueStage::Reply;
	if (DialogueWheelPanel) DialogueWheelPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (DialoguePromiseBorder) DialoguePromiseBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueFreeTextBorder) DialogueFreeTextBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (DialogueReplyBorder) DialogueReplyBorder->SetVisibility(ESlateVisibility::Visible);
	if (DialogueContinueButton) DialogueContinueButton->SetKeyboardFocus();
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

FString UWhiteoutHUDWidget::BuildDialogueStatusSummary(
	const FWSAgentReply& Reply,
	const bool bIncludeDebugDetails)
{
	const TCHAR* MovementLabel = TEXT("原地");
	switch (Reply.MovementIntent)
	{
	case EWSNPCMovementIntent::StepCloser: MovementLabel = TEXT("靠近"); break;
	case EWSNPCMovementIntent::StepBack: MovementLabel = TEXT("后退"); break;
	case EWSNPCMovementIntent::ReturnToPost: MovementLabel = TEXT("归位"); break;
	default: break;
	}
	const TCHAR* ReactionLabel = TEXT("自然");
	switch (Reply.Reaction)
	{
	case EWSNPCReaction::Acknowledge: ReactionLabel = TEXT("回应"); break;
	case EWSNPCReaction::Consider: ReactionLabel = TEXT("思考"); break;
	case EWSNPCReaction::Reassure: ReactionLabel = TEXT("安抚"); break;
	case EWSNPCReaction::Reject: ReactionLabel = TEXT("拒绝"); break;
	case EWSNPCReaction::Alarmed: ReactionLabel = TEXT("警觉"); break;
	default: break;
	}
	FString Status = FString::Printf(
		TEXT("表演：%s · %s"),
		MovementLabel,
		ReactionLabel);
	if (!bIncludeDebugDetails)
	{
		return Status;
	}
	const FString ProviderStatus = Reply.AnswerSource == TEXT("spine_plus_ai")
		? FString::Printf(
			TEXT("%s 人格尾句"),
			*LLMProviderDisplayName(Reply.Provider))
		: Reply.Provider == TEXT("preset")
			? TEXT("本地语义骨架")
			: FString::Printf(
				TEXT("%s 尾句丢弃，保留本地骨架：%s"),
				*LLMProviderDisplayName(Reply.Provider),
				*LLMFallbackReasonLabel(Reply.ValidationReason));
	Status = ProviderStatus + TEXT("　｜　") + Status;
	Status += FString::Printf(
		TEXT("\nSemantic: %s / %s / %s / %.2f　｜　%s　｜　%s"),
		*StaticEnum<EWSDialogueAct>()->GetNameStringByValue(
			static_cast<int64>(Reply.SemanticFrame.SpeechAct)),
		*StaticEnum<EWSDialogueQueryType>()->GetNameStringByValue(
			static_cast<int64>(Reply.SemanticFrame.QueryType)),
		Reply.SemanticFrame.TargetActionId.IsNone()
			? TEXT("none")
			: *Reply.SemanticFrame.TargetActionId.ToString(),
		Reply.SemanticFrame.Confidence,
		Reply.SemanticFrame.Source.IsEmpty()
			? TEXT("unspecified")
			: *Reply.SemanticFrame.Source,
		*Reply.AnswerSource);
	return Status;
}

void UWhiteoutHUDWidget::HandleDialogueLine(const FWSAgentReply& Reply)
{
	if (!bDialogueVisible || Reply.ActionId != ActiveDialogueActionId || !DialogueLineText)
	{
		return;
	}
	if (const AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(GetOwningPlayerPawn()))
	{
		if (Reply.DialogueSessionId != Character->GetActiveDialogueSessionId()
			|| Reply.TransactionId != Character->GetActiveDialogueTransactionId())
		{
			return;
		}
	}
	const FString Speaker = Reply.Speaker == EWSCharacterId::GuHeng ? TEXT("顾衡") : TEXT("叶澄");
	if (DialogueNameText) DialogueNameText->SetText(FText::FromString(Speaker));
	DialogueLineText->SetText(FText::FromString(Reply.Utterance));
	DialogueLineText->SetColorAndOpacity(FSlateColor(Body));
	if (DialogueStatusText)
	{
		bool bIncludeDebugDetails = false;
#if !UE_BUILD_SHIPPING
		bIncludeDebugDetails = CVarWhiteoutDialogueDebug.GetValueOnGameThread() != 0;
#endif
		DialogueStatusText->SetText(FText::FromString(
			BuildDialogueStatusSummary(Reply, bIncludeDebugDetails)));
		DialogueStatusText->SetColorAndOpacity(FSlateColor(!Reply.bFallback ? Cyan : Secondary));
	}
	UpdateDialogueConditionCard(Reply);
	ShowDialogueReplyActions();
}

FString UWhiteoutHUDWidget::BuildDialogueConditionSummary(
	const FWSActionRequirementReport& Report)
{
	TArray<FString> Lines;
	TArray<FString> Universal;
	bool bHasDisclosableUniversal = false;
	for (const FWSRequirementItem& Item : Report.UniversalRequirements)
	{
		if (Item.MechanicalVisibility
			== EWSRequirementMechanicalVisibility::Hidden
			|| Item.PlayerFacingDetail.IsEmpty())
		{
			continue;
		}
		bHasDisclosableUniversal = true;
		if (!Item.bSatisfied)
		{
			Universal.Add(Item.PlayerFacingDetail.ToString());
		}
	}
	if (!Universal.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("共同：%s"), *FString::Join(Universal, TEXT("；"))));
	}
	else if (bHasDisclosableUniversal)
	{
		Lines.Add(TEXT("共同：当前基础协作条件已满足"));
	}
	int32 VisiblePlanIndex = 0;
	for (int32 PlanIndex = 0;
		PlanIndex < Report.AlternativePlans.Num() && VisiblePlanIndex < 2;
		++PlanIndex)
	{
		const FWSRequirementPlan& Plan = Report.AlternativePlans[PlanIndex];
		TArray<FString> Missing;
		bool bHasDisclosableRequirement = false;
		for (const FWSRequirementItem& Item : Plan.Requirements)
		{
			if (Item.MechanicalVisibility
				== EWSRequirementMechanicalVisibility::Hidden
				|| Item.PlayerFacingDetail.IsEmpty())
			{
				continue;
			}
			bHasDisclosableRequirement = true;
			if (!Item.bSatisfied)
			{
				Missing.Add(Item.PlayerFacingDetail.ToString());
			}
		}
		if (!bHasDisclosableRequirement)
		{
			continue;
		}
		Lines.Add(FString::Printf(
			TEXT("路线 %s：%s"),
			VisiblePlanIndex++ == 0 ? TEXT("A") : TEXT("B"),
			Missing.IsEmpty() ? TEXT("当前已满足") : *FString::Join(Missing, TEXT("；"))));
	}
	for (const FWSRequirementItem& Risk : Report.Risks)
	{
		if (Risk.MechanicalVisibility
				== EWSRequirementMechanicalVisibility::Visible
			&& !Risk.PlayerFacingDetail.IsEmpty()
			&& !Risk.bSatisfied)
		{
			Lines.Add(FString::Printf(
				TEXT("风险：%s"),
				*Risk.PlayerFacingDetail.ToString()));
			break;
		}
	}
	return FString::Join(Lines, TEXT("\n"));
}

void UWhiteoutHUDWidget::UpdateDialogueConditionCard(const FWSAgentReply& Reply)
{
	if (!DialogueConditionBorder
		|| Reply.RequirementReport.ActionId != TEXT("repair_generator")
		|| Reply.AnswerContract.QueryType != EWSDialogueQueryType::Requirements)
	{
		HideDialogueConditionCard();
		return;
	}
	ActiveDialogueRequirementReport = Reply.RequirementReport;
	if (DialogueConditionBodyText)
	{
		DialogueConditionBodyText->SetText(FText::FromString(
			BuildDialogueConditionSummary(ActiveDialogueRequirementReport)));
	}
	bool bPinned = false;
	bool bOfferActive = false;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UWindStationStateSubsystem* StateSubsystem =
			GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			const FWSGameState State = StateSubsystem->GetStateSnapshot();
			bPinned = State.PinnedRequirementActions.Contains(TEXT("repair_generator"));
			bOfferActive = State.NegotiationOffers.ContainsByPredicate(
				[](const FWSNegotiationOffer& Offer)
				{
					return Offer.TargetActionId == TEXT("repair_generator")
						&& Offer.bAccepted
						&& !Offer.bFulfilled
						&& !Offer.bBroken;
				});
		}
	}
	if (DialogueConditionPinButton)
	{
		DialogueConditionPinButton->SetIsEnabled(!bPinned);
	}
	if (DialogueConditionAcceptButton)
	{
		DialogueConditionAcceptButton->SetIsEnabled(!bOfferActive);
	}
	if (DialogueConditionStatusText)
	{
		const FString Status = bOfferActive
			? TEXT("已接受 · 本阶段有效 · 实际行动才消耗 AP/资源")
			: bPinned
				? TEXT("已固定到任务栏")
				: TEXT("条件来自当前规则状态，可固定或接受");
		DialogueConditionStatusText->SetText(FText::FromString(Status));
	}
	DialogueConditionBorder->SetVisibility(ESlateVisibility::Visible);
}

void UWhiteoutHUDWidget::HideDialogueConditionCard()
{
	ActiveDialogueRequirementReport = FWSActionRequirementReport();
	if (DialogueConditionBorder)
	{
		DialogueConditionBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWhiteoutHUDWidget::PinDialogueConditions()
{
	if (ActiveDialogueRequirementReport.ActionId != TEXT("repair_generator"))
	{
		return;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWindStationStateSubsystem* StateSubsystem =
			GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			if (StateSubsystem->SetRequirementPinned(TEXT("repair_generator"), true))
			{
				if (DialogueConditionPinButton) DialogueConditionPinButton->SetIsEnabled(false);
				if (DialogueConditionStatusText)
				{
					DialogueConditionStatusText->SetText(FText::FromString(TEXT("已固定到任务栏")));
				}
			}
		}
	}
}

void UWhiteoutHUDWidget::AcceptDialogueConditions()
{
	FString Message;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWindStationStateSubsystem* StateSubsystem =
			GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			const bool bAccepted = StateSubsystem->AcceptLatestNegotiationOffer(Message);
			if (DialogueConditionAcceptButton) DialogueConditionAcceptButton->SetIsEnabled(!bAccepted);
			if (DialogueConditionPinButton && bAccepted) DialogueConditionPinButton->SetIsEnabled(false);
			if (DialogueConditionStatusText)
			{
				DialogueConditionStatusText->SetText(FText::FromString(Message));
				DialogueConditionStatusText->SetColorAndOpacity(FSlateColor(bAccepted ? Cyan : Amber));
			}
		}
	}
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
	ResetPanelVisual(GuideBorder);
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
			ResetMouseToViewportCenter();
		}
	}
	RefreshSettingsUI();
	ShowPanelInstant(PauseBorder, false);
	ShowPanelInstant(SettingsBorder, true);
	SetLayer(EWSUILayer::Settings);
}

void UWhiteoutHUDWidget::ShowLLMSettingsForCapture()
{
	ShowSettingsForCapture();
	if (SettingsScroll)
	{
		SettingsScroll->SetScrollOffset(10000.0f);
	}
}

void UWhiteoutHUDWidget::SetOpeningCaptureStage(const int32 Stage)
{
	if (!OpeningBorder)
	{
		return;
	}
	OpeningBorder->SetVisibility(ESlateVisibility::Visible);
	OpeningBorder->SetRenderOpacity(1.0f);
	OpeningElapsed = 0.0f;
	OpeningPhase = EWSOpeningPhase::AwaitingAdvance;
	ApplyOpeningStage(FMath::Clamp(Stage, 0, FMath::Max(OpeningLines.Num() - 1, 0)));
	if (OpeningText) OpeningText->SetRenderOpacity(1.0f);
	if (OpeningSubtitleText) OpeningSubtitleText->SetRenderOpacity(1.0f);
	if (OpeningFooterText) OpeningFooterText->SetRenderOpacity(1.0f);
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

void UWhiteoutHUDWidget::TickOpening(const float DeltaTime)
{
	if (!IsOpeningVisible() || OpeningPhase == EWSOpeningPhase::Complete)
	{
		return;
	}
	const bool bReducedMotion = IsReducedMotionEnabled();
	const float LineFadeInSeconds = bReducedMotion ? 0.05f : 0.70f;
	const float LineFadeOutSeconds = bReducedMotion ? 0.05f : 0.36f;
	const float BlackRevealSeconds = bReducedMotion ? 0.18f : 1.65f;
	OpeningElapsed += DeltaTime;

	auto SetLineOpacity = [this](const float Opacity)
	{
		if (OpeningText) OpeningText->SetRenderOpacity(Opacity);
		if (OpeningSubtitleText) OpeningSubtitleText->SetRenderOpacity(Opacity);
		if (OpeningFooterText) OpeningFooterText->SetRenderOpacity(Opacity);
	};

	switch (OpeningPhase)
	{
	case EWSOpeningPhase::FadingInLine:
	{
		const float Alpha = FMath::Clamp(OpeningElapsed / LineFadeInSeconds, 0.0f, 1.0f);
		SetLineOpacity(FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f));
		if (Alpha >= 1.0f)
		{
			OpeningPhase = EWSOpeningPhase::AwaitingAdvance;
			OpeningElapsed = 0.0f;
		}
		break;
	}
	case EWSOpeningPhase::AwaitingAdvance:
		if (OpeningFooterText)
		{
			OpeningFooterText->SetRenderOpacity(
				bReducedMotion ? 0.8f : 0.62f + 0.18f * FMath::Sin(OpeningElapsed * 2.6f));
		}
		break;
	case EWSOpeningPhase::FadingOutLine:
	{
		const float Alpha = FMath::Clamp(OpeningElapsed / LineFadeOutSeconds, 0.0f, 1.0f);
		SetLineOpacity(1.0f - FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f));
		if (Alpha >= 1.0f)
		{
			if (ActiveOpeningStage + 1 < OpeningLines.Num())
			{
				OpeningPhase = EWSOpeningPhase::FadingInLine;
				OpeningElapsed = 0.0f;
				ApplyOpeningStage(ActiveOpeningStage + 1);
			}
			else
			{
				if (AWhiteoutGameMode* GameMode = GetWorld()
					? Cast<AWhiteoutGameMode>(GetWorld()->GetAuthGameMode())
					: nullptr)
				{
					GameMode->PrepareOpeningReveal();
				}
				OpeningPhase = EWSOpeningPhase::RevealingStation;
				OpeningElapsed = 0.0f;
			}
		}
		break;
	}
	case EWSOpeningPhase::RevealingStation:
	{
		const float Alpha = FMath::Clamp(OpeningElapsed / BlackRevealSeconds, 0.0f, 1.0f);
		if (OpeningBorder)
		{
			OpeningBorder->SetRenderOpacity(1.0f - FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f));
		}
		if (Alpha >= 1.0f)
		{
			DismissOpening();
		}
		break;
	}
	case EWSOpeningPhase::Complete:
	default:
		break;
	}
}

bool UWhiteoutHUDWidget::AdvanceOpening()
{
	if (!IsOpeningVisible())
	{
		return false;
	}
	if (OpeningPhase == EWSOpeningPhase::FadingInLine)
	{
		OpeningPhase = EWSOpeningPhase::AwaitingAdvance;
		OpeningElapsed = 0.0f;
		if (OpeningText) OpeningText->SetRenderOpacity(1.0f);
		if (OpeningSubtitleText) OpeningSubtitleText->SetRenderOpacity(1.0f);
		if (OpeningFooterText) OpeningFooterText->SetRenderOpacity(0.8f);
	}
	else if (OpeningPhase == EWSOpeningPhase::AwaitingAdvance)
	{
		OpeningPhase = EWSOpeningPhase::FadingOutLine;
		OpeningElapsed = 0.0f;
	}
	return true;
}

bool UWhiteoutHUDWidget::IsOpeningVisible() const
{
	return OpeningBorder
		&& OpeningBorder->GetVisibility() == ESlateVisibility::Visible
		&& OpeningPhase != EWSOpeningPhase::Complete;
}

void UWhiteoutHUDWidget::ApplyOpeningStage(const int32 Stage)
{
	if (!OpeningBorder || !OpeningText)
	{
		return;
	}
	if (OpeningLines.IsEmpty())
	{
		return;
	}
	ActiveOpeningStage = FMath::Clamp(Stage, 0, OpeningLines.Num() - 1);
	OpeningBorder->SetBrushColor(FLinearColor::Black);
	OpeningBorder->SetRenderOpacity(1.0f);
	OpeningText->SetText(OpeningLines[ActiveOpeningStage]);
	OpeningText->SetFont(UIFont(28, false));
	OpeningText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	OpeningText->SetRenderOpacity(0.0f);
	if (OpeningDivider)
	{
		OpeningDivider->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (OpeningSubtitleText)
	{
		OpeningSubtitleText->SetText(FText::FromString(FString::Printf(
			TEXT("%d / %d"),
			ActiveOpeningStage + 1,
			OpeningLines.Num())));
		OpeningSubtitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.75f, 0.80f, 1.0f)));
		OpeningSubtitleText->SetRenderOpacity(0.0f);
	}
	if (OpeningFooterText)
	{
		OpeningFooterText->SetText(FText::FromString(TEXT("空格 / 点击屏幕继续")));
		OpeningFooterText->SetColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.62f, 0.68f, 1.0f)));
		OpeningFooterText->SetVisibility(ESlateVisibility::Visible);
		OpeningFooterText->SetRenderOpacity(0.0f);
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
		CrisisBorder->SetBrushColor(FLinearColor(0.012f, 0.012f, 0.014f, 0.92f));
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
		Key = TEXT("ui_ending_cost_cinematic_v06");
		Fallback = TEXT("代价越过边界\n求救与生存无法同时守住");
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

void UWhiteoutHUDWidget::ShowPanelAnimated(UBorder* Panel, const bool bShow, const float Duration, const bool bScaleWithFade)
{
	if (!Panel)
	{
		return;
	}
	if (IsReducedMotionEnabled())
	{
		ShowPanelInstant(Panel, bShow);
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

void UWhiteoutHUDWidget::DismissOpening()
{
	OpeningPhase = EWSOpeningPhase::Complete;
	if (OpeningBorder)
	{
		OpeningBorder->SetRenderOpacity(1.0f);
		OpeningBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (AWhiteoutGameMode* GameMode = GetWorld() ? Cast<AWhiteoutGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
	{
		GameMode->FinishOpeningPresentation();
	}
}

void UWhiteoutHUDWidget::ToggleGuide()
{
	if (IsOpeningVisible())
	{
		return;
	}
	if (CurrentLayer == EWSUILayer::Guide)
	{
		CloseGuide();
		return;
	}
	if (CurrentLayer != EWSUILayer::Game)
	{
		return;
	}
	HideActionPreview();
	if (GuideBorder)
	{
		ShowPanelAnimated(GuideBorder, true, WSUITokens::Anim::Normal);
	}
	SetLayer(EWSUILayer::Guide);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetShowMouseCursor(true);
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		ResetMouseToViewportCenter();
		SetKeyboardFocus();
	}
}

void UWhiteoutHUDWidget::CloseGuide()
{
	if (GuideBorder)
	{
		ShowPanelAnimated(GuideBorder, false, WSUITokens::Anim::Fast);
	}
	SetLayer(EWSUILayer::Game);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetShowMouseCursor(false);
		PlayerController->SetInputMode(FInputModeGameOnly());
		ResetMouseToViewportCenter();
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
	case EWSUILayer::Guide:
		CloseGuide();
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
	if (IsOpeningVisible())
	{
		return;
	}
	if (SettingsBorder)
	{
		ShowPanelAnimated(SettingsBorder, false, WSUITokens::Anim::Fast);
	}
	if (LoadGameButton && GetGameInstance())
	{
		if (const UWindStationStateSubsystem* StateSubsystem =
			GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
		{
			LoadGameButton->SetIsEnabled(StateSubsystem->HasSnapshot());
		}
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
		ResetMouseToViewportCenter();
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
		ResetMouseToViewportCenter();
	}
}

void UWhiteoutHUDWidget::SaveGame()
{
	if (!GetGameInstance())
	{
		return;
	}
	if (UWindStationStateSubsystem* StateSubsystem =
		GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		const bool bSaved = StateSubsystem->SaveSnapshot();
		SystemMessage = bSaved
			? TEXT("本轮状态已保存。")
			: TEXT("保存失败，请检查存储权限。");
		if (bSaved)
		{
			ResumeGame();
		}
	}
}

void UWhiteoutHUDWidget::LoadGame()
{
	if (!GetGameInstance())
	{
		return;
	}
	if (UWindStationStateSubsystem* StateSubsystem =
		GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		const bool bLoaded = StateSubsystem->LoadSnapshot();
		SystemMessage = bLoaded
			? TEXT("已恢复最近保存的本轮状态。")
			: TEXT("没有可读取的 v1.2 或兼容 v1.1 本轮存档。");
		if (bLoaded)
		{
			ResumeGame();
		}
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
		ResetMouseToViewportCenter();
	}
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
		ResetMouseToViewportCenter();
	}
}

void UWhiteoutHUDWidget::RefreshLLMProviderFields(const FString& ProviderId, const bool bReplaceModel)
{
	if (!LLMModelCandidateCombo)
	{
		return;
	}
	const TArray<FString> Candidates = UWSAgentGateway::GetModelCandidates(ProviderId);
	LLMModelCandidateCombo->ClearOptions();
	for (const FString& Candidate : Candidates)
	{
		LLMModelCandidateCombo->AddOption(Candidate);
	}
	if (!Candidates.IsEmpty())
	{
		LLMModelCandidateCombo->SetSelectedOption(Candidates[0]);
		if (bReplaceModel && LLMModelInput)
		{
			LLMModelInput->SetText(FText::FromString(Candidates[0]));
		}
	}
	if (LLMModelHintText)
	{
		LLMModelHintText->SetText(FText::FromString(
			Candidates.IsEmpty()
				? TEXT("直接填写厂商支持的模型 ID。")
				: FString::Printf(TEXT("候选：%s｜也可手动输入"), *FString::Join(Candidates, TEXT("、")))));
	}
}

void UWhiteoutHUDWidget::HandleLLMProviderChanged(
	const FString SelectedItem,
	const ESelectInfo::Type SelectionType)
{
	if (bUpdatingSettings || SelectionType == ESelectInfo::Direct)
	{
		return;
	}
	for (const FWSLLMProviderPreset& Preset : UWSAgentGateway::GetProviderPresets())
	{
		if (Preset.DisplayName != SelectedItem)
		{
			continue;
		}
		bUpdatingSettings = true;
		if (LLMBaseUrlInput)
		{
			LLMBaseUrlInput->SetText(FText::FromString(Preset.BaseUrl));
		}
		if (LLMApiKeyInput)
		{
			LLMApiKeyInput->SetText(FText::GetEmpty());
			LLMApiKeyInput->SetHintText(FText::FromString(
				TEXT("厂商已切换，请填写对应的 API Key")));
		}
		RefreshLLMProviderFields(Preset.ProviderId, true);
		bUpdatingSettings = false;
		return;
	}
}

void UWhiteoutHUDWidget::HandleLLMModelCandidateChanged(
	const FString SelectedItem,
	const ESelectInfo::Type SelectionType)
{
	if (bUpdatingSettings || SelectionType == ESelectInfo::Direct || !LLMModelInput)
	{
		return;
	}
	LLMModelInput->SetText(FText::FromString(SelectedItem));
}

void UWhiteoutHUDWidget::ToggleLLMEnabled()
{
	bPendingLLMEnabled = !bPendingLLMEnabled;
	if (LLMEnabledValueText)
	{
		LLMEnabledValueText->SetText(FText::FromString(FString::Printf(
			TEXT("模型调用　｜　%s"),
			bPendingLLMEnabled ? TEXT("开启") : TEXT("关闭"))));
	}
	if (LLMStatusText)
	{
		LLMStatusText->SetText(FText::FromString(TEXT("尚未应用。")));
		LLMStatusText->SetColorAndOpacity(FSlateColor(Amber));
	}
}

void UWhiteoutHUDWidget::ApplyLLMSettings()
{
	if (!GetGameInstance() || !LLMProviderCombo || !LLMBaseUrlInput || !LLMModelInput)
	{
		return;
	}
	UWhiteoutSettingsSubsystem* Settings =
		GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>();
	if (!Settings)
	{
		return;
	}
	FString ProviderId;
	const FString SelectedProvider = LLMProviderCombo->GetSelectedOption();
	for (const FWSLLMProviderPreset& Preset : UWSAgentGateway::GetProviderPresets())
	{
		if (Preset.DisplayName == SelectedProvider)
		{
			ProviderId = Preset.ProviderId;
			break;
		}
	}
	FString ApiKey = LLMApiKeyInput ? LLMApiKeyInput->GetText().ToString() : FString();
	if (ApiKey.TrimStartAndEnd().IsEmpty() && Settings->HasSessionLLMApiKey())
	{
		ApiKey = Settings->GetSessionLLMApiKey();
	}
	FString Error;
	if (!Settings->SetLLMConfiguration(
		ProviderId,
		LLMBaseUrlInput->GetText().ToString(),
		ApiKey,
		LLMModelInput->GetText().ToString(),
		bPendingLLMEnabled,
		Error))
	{
		if (LLMStatusText)
		{
			LLMStatusText->SetText(FText::FromString(FString::Printf(TEXT("未应用｜%s"), *Error)));
			LLMStatusText->SetColorAndOpacity(FSlateColor(Danger));
		}
		return;
	}
	if (LLMApiKeyInput)
	{
		LLMApiKeyInput->SetText(FText::GetEmpty());
		LLMApiKeyInput->SetHintText(FText::FromString(
			Settings->HasSessionLLMApiKey()
				? TEXT("本次运行已设置；留空会继续使用")
				: TEXT("仅保存在当前进程内存")));
	}
	if (const UWindStationStateSubsystem* State =
		GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		if (LLMStatusText)
		{
			LLMStatusText->SetText(FText::FromString(State->GetLLMRuntimeStatus()));
			LLMStatusText->SetColorAndOpacity(
				FSlateColor(State->HasLiveLLMProvider() ? Cyan : Amber));
		}
	}
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
	if (TextScaleSlider) TextScaleSlider->SetValue((Settings->GetTextScale() - 0.9f) / 0.3f);
	if (FOVValueText) FOVValueText->SetText(FText::FromString(FString::Printf(TEXT("%d°"), FMath::RoundToInt(Settings->GetFieldOfView()))));
	if (MasterVolumeValueText) MasterVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetMasterVolume() * 100.0f))));
	if (AmbienceVolumeValueText) AmbienceVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetAmbienceVolume() * 100.0f))));
	if (EffectsVolumeValueText) EffectsVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetEffectsVolume() * 100.0f))));
	if (FeedbackVolumeValueText) FeedbackVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetFeedbackVolume() * 100.0f))));
	if (TextScaleValueText) TextScaleValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetTextScale() * 100.0f))));
	if (ReducedMotionValueText)
	{
		ReducedMotionValueText->SetText(FText::FromString(FString::Printf(
			TEXT("减少动态效果　｜　%s"),
			Settings->IsReducedMotionEnabled() ? TEXT("开启") : TEXT("关闭"))));
	}
	bPendingLLMEnabled = Settings->IsLLMEnabled();
	if (LLMProviderCombo)
	{
		for (const FWSLLMProviderPreset& Preset : UWSAgentGateway::GetProviderPresets())
		{
			if (Preset.ProviderId == Settings->GetLLMProviderId())
			{
				LLMProviderCombo->SetSelectedOption(Preset.DisplayName);
				break;
			}
		}
	}
	RefreshLLMProviderFields(Settings->GetLLMProviderId(), false);
	if (LLMBaseUrlInput)
	{
		LLMBaseUrlInput->SetText(FText::FromString(Settings->GetLLMBaseUrl()));
	}
	if (LLMModelInput)
	{
		LLMModelInput->SetText(FText::FromString(Settings->GetLLMModelId()));
	}
	if (LLMModelCandidateCombo
		&& LLMModelCandidateCombo->FindOptionIndex(Settings->GetLLMModelId()) != INDEX_NONE)
	{
		LLMModelCandidateCombo->SetSelectedOption(Settings->GetLLMModelId());
	}
	if (LLMApiKeyInput)
	{
		LLMApiKeyInput->SetText(FText::GetEmpty());
		LLMApiKeyInput->SetHintText(FText::FromString(
			Settings->HasSessionLLMApiKey()
				? TEXT("本次运行已设置；留空会继续使用")
				: TEXT("仅保存在当前进程内存")));
	}
	if (LLMEnabledValueText)
	{
		LLMEnabledValueText->SetText(FText::FromString(FString::Printf(
			TEXT("模型调用　｜　%s"),
			bPendingLLMEnabled ? TEXT("开启") : TEXT("关闭"))));
	}
	if (const UWindStationStateSubsystem* State =
		GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		if (LLMStatusText)
		{
			LLMStatusText->SetText(FText::FromString(State->GetLLMRuntimeStatus()));
			LLMStatusText->SetColorAndOpacity(
				FSlateColor(State->HasLiveLLMProvider() ? Cyan : Amber));
		}
	}
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

void UWhiteoutHUDWidget::HandleTextScaleChanged(const float Value)
{
	if (bUpdatingSettings || !GetGameInstance()) return;
	if (UWhiteoutSettingsSubsystem* Settings =
		GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		Settings->SetTextScale(0.9f + FMath::Clamp(Value, 0.0f, 1.0f) * 0.3f, this);
		if (TextScaleValueText)
		{
			TextScaleValueText->SetText(FText::FromString(FString::Printf(
				TEXT("%d%%"),
				FMath::RoundToInt(Settings->GetTextScale() * 100.0f))));
		}
	}
}

void UWhiteoutHUDWidget::ToggleReducedMotion()
{
	if (!GetGameInstance())
	{
		return;
	}
	if (UWhiteoutSettingsSubsystem* Settings =
		GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		Settings->SetReducedMotionEnabled(!Settings->IsReducedMotionEnabled());
		RefreshSettingsUI();
	}
}

bool UWhiteoutHUDWidget::IsReducedMotionEnabled() const
{
	if (!GetGameInstance())
	{
		return false;
	}
	if (const UWhiteoutSettingsSubsystem* Settings =
		GetGameInstance()->GetSubsystem<UWhiteoutSettingsSubsystem>())
	{
		return Settings->IsReducedMotionEnabled();
	}
	return false;
}

void UWhiteoutHUDWidget::ToggleControls()
{
	ResumeGame();
	ToggleGuide();
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
