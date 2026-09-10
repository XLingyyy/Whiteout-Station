#include "HUD/WSTutorialWidget.h"
#include "Engine/Font.h"
#include "HUD/WSUITokens.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Settings/WhiteoutSettingsSubsystem.h"

UWSTutorialData::UWSTutorialData()
{
	Pages = UWSTutorialWidget::DefaultPages();
}

TArray<FWSTutorialPage> UWSTutorialWidget::DefaultPages()
{
	TArray<FWSTutorialPage> Result;
	const auto Add = [&](const TCHAR* Id, const TCHAR* Title, const TCHAR* Lead, const TCHAR* Body,
		const TCHAR* Hint, const TCHAR* Image, const TCHAR* Role, EWSTutorialMode Mode = EWSTutorialMode::Any)
	{
		FWSTutorialPage P; P.PageId = Id; P.Title = FText::FromString(Title); P.Lead = FText::FromString(Lead);
		P.Body = FText::FromString(Body); P.Hint = FText::FromString(Hint); P.ModeRequirement = Mode; P.ImageRole = Role;
		P.AltText = FText::FromString(FString::Printf(TEXT("%s界面示例，数值以当前状态为准。"), Role));
		P.PrimaryImage = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/WindStation/UI/v17/Tutorial/Textures/%s.%s"), Image, Image)));
		P.CaptureManifestId = Image; Result.Add(P);
	};
	Add(TEXT("T01"), TEXT("先交谈，再做决定"), TEXT("靠近 NPC，按 F 开始交谈。"),
		TEXT("向顾衡了解设备，向叶澄了解人员情况。离线时可以选择话题；在线功能启用后，也可以用自己的话输入问题。"),
		TEXT("交谈不消耗 AP。每名 NPC 每局有 10 轮计数额度，跨阶段保留。"), TEXT("T_Tut01_Dialogue_Offline_A"), TEXT("Dialogue"), EWSTutorialMode::Offline);
	FWSTutorialPage Online = Result[0]; Online.ModeRequirement = EWSTutorialMode::OnlineReady;
	Online.PrimaryImage = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/WindStation/UI/v17/Tutorial/Textures/T_Tut01_Dialogue_Online_A.T_Tut01_Dialogue_Online_A")));
	Online.CaptureManifestId = TEXT("T_Tut01_Dialogue_Online_A"); Result.Add(Online);
	Add(TEXT("T02"), TEXT("观察现场，寻找可交互物品"), TEXT("留意带白色轮廓的设备。"),
		TEXT("靠近后按 F 查看行动，按 Q 切换方案。先读消耗与条件，再按 F 确认；Esc 可以取消。"),
		TEXT("探索现场、查看物品，可以帮助你补全情况，不必只依赖 NPC 的讲述。"), TEXT("T_Tut02_Interactable_A"), TEXT("Interactable"));
	Add(TEXT("T03"), TEXT("每一次行动，都要看清代价"), TEXT("每阶段 4 AP，每格代表 1 AP。"),
		TEXT("早晨、午后、黄昏共 12 AP，未用 AP 不结转。执行前查看预览：有的行动还会消耗物资或体能。"),
		TEXT("斜纹格表示预计消耗；成功执行后才扣除。交谈不消耗 AP。"), TEXT("T_Tut03_AP_A"), TEXT("AP"));
	Result.Last().SecondaryImage = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/WindStation/UI/v17/Tutorial/Textures/T_Tut03_CostPreview_A.T_Tut03_CostPreview_A")));
	Add(TEXT("T04"), TEXT("别忽略自己的状态"), TEXT("体温、体能、伤势、压力都很重要。"),
		TEXT("行动力决定还能做多少事；体能是另一项人物状态。休息、进食与治疗的作用不同，先看行动预览再决定。"),
		TEXT("暖区休息可以回温、恢复体能并缓解压力，但休息不等于治疗伤势。"), TEXT("T_Tut04_PlayerStatus_A"), TEXT("Player"));
	Add(TEXT("T05"), TEXT("也要关注同行者"), TEXT("关注 NPC 时，查看对方的状态卡。"),
		TEXT("体温、体能、伤势与态度，能帮助你安排照护和协作。未知的信息会显示“尚未确认”，不要把它当作正常。"),
		TEXT("修好设备只是任务的一部分；照顾你、顾衡和叶澄同样重要。"), TEXT("T_Tut05_NPCStatus_Unknown_A"), TEXT("NPC"));
	return Result;
}

void UWSTutorialWidget::Build(UFont* Font)
{
	FontFamily = Font; SetIsFocusable(true);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Root;
	const auto Fill = [&](UWidget* W, int32 Z)
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(W); S->SetAnchors(FAnchors(0, 0, 1, 1)); S->SetOffsets(FMargin(0)); S->SetZOrder(Z);
	};
	Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(); Blur->SetBlurStrength(WSUITokens::V17::TutorialBlur);
	Blur->SetLowQualityFallbackBrush(FSlateRoundedBoxBrush(WSUITokens::V17::Surface, 0.f));
	Blur->SetVisibility(ESlateVisibility::HitTestInvisible); Fill(Blur, 0);
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(); Dim->SetBrushColor(FLinearColor(0, 0, 0, .45f)); Fill(Dim, 1);
	Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrush(FSlateRoundedBoxBrush(WSUITokens::V17::SurfaceRaised, 12.f, WSUITokens::V17::Stroke, 1.f));
	Panel->SetPadding(FMargin(32));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel); PanelSlot->SetZOrder(2);
	PanelSlot->SetAnchors(FAnchors(.5f, .5f)); PanelSlot->SetAlignment(FVector2D(.5f));
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(); Panel->SetContent(Box);
	const auto Text = [&](int32 Size, const FLinearColor& Color)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(); T->SetFont(FSlateFontInfo(Font, Size));
		T->SetColorAndOpacity(Color); T->SetAutoWrapText(true); T->SetVisibility(ESlateVisibility::HitTestInvisible); return T;
	};
	PageNumber = Text(16, WSUITokens::V17::Attention); Box->AddChildToVerticalBox(PageNumber)->SetPadding(FMargin(0, 0, 0, 12));
	Title = Text(34, WSUITokens::V17::Text); Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0, 0, 0, 20));
	UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
	Box->AddChildToVerticalBox(Content)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ImageColumn = WidgetTree->ConstructWidget<USizeBox>();
	Content->AddChildToHorizontalBox(ImageColumn)->SetPadding(FMargin(0, 0, 28, 0));
	UVerticalBox* ImageBox = WidgetTree->ConstructWidget<UVerticalBox>(); ImageColumn->SetContent(ImageBox);
	const auto Picture = [&]()
	{
		UImage* I = WidgetTree->ConstructWidget<UImage>(); I->SetVisibility(ESlateVisibility::HitTestInvisible);
		UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(); Scale->SetStretch(EStretch::ScaleToFit); Scale->SetContent(I);
		ImageBox->AddChildToVerticalBox(Scale)->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); return I;
	};
	PrimaryImage = Picture(); SecondaryImage = Picture();
	MissingImage = Text(16, WSUITokens::V17::Secondary); ImageBox->AddChildToVerticalBox(MissingImage);
	UTextBlock* Caption = Text(14, WSUITokens::V17::Secondary); Caption->SetText(FText::FromString(TEXT("界面示例，数值以当前状态为准")));
	ImageBox->AddChildToVerticalBox(Caption)->SetPadding(FMargin(0, 10, 0, 0));
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Content->AddChildToHorizontalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* Copy = WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(Copy);
	Lead = Text(23, WSUITokens::V17::Text); Copy->AddChildToVerticalBox(Lead)->SetPadding(FMargin(0, 0, 0, 20));
	Body = Text(20, WSUITokens::V17::Secondary); Body->SetLineHeightPercentage(1.3f); Copy->AddChildToVerticalBox(Body);
	Hint = Text(18, WSUITokens::V17::Attention); Copy->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0, 24, 0, 0));
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(); Box->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0, 22, 0, 0));
	const auto Button = [&](UHorizontalBox* Row, const TCHAR* Label)
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(); FButtonStyle Style;
		Style.SetNormal(FSlateRoundedBoxBrush(WSUITokens::V17::Surface, 6.f, WSUITokens::V17::Stroke, 1.f));
		Style.SetHovered(FSlateRoundedBoxBrush(WSUITokens::V17::SurfaceRaised, 6.f, WSUITokens::V17::Attention, 1.f));
		Style.SetPressed(FSlateRoundedBoxBrush(WSUITokens::V17::Stroke, 6.f));
		Style.SetDisabled(FSlateRoundedBoxBrush(WSUITokens::V17::Surface, 6.f)); B->SetStyle(Style);
		UTextBlock* T = Text(16, WSUITokens::V17::Text); T->SetText(FText::FromString(Label)); B->SetContent(T);
		Row->AddChildToHorizontalBox(B)->SetPadding(FMargin(0, 0, 8, 0)); return B;
	};
	PreviousButton = Button(Footer, TEXT("上一页")); PreviousButton->OnClicked.AddDynamic(this, &UWSTutorialWidget::Previous);
	Button(Footer, TEXT("跳过教程"))->OnClicked.AddDynamic(this, &UWSTutorialWidget::Skip);
	USizeBox* Gap = WidgetTree->ConstructWidget<USizeBox>(); Footer->AddChildToHorizontalBox(Gap)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UButton* Continue = Button(Footer, TEXT("鼠标左键 · 下一页")); ContinueLabel = Cast<UTextBlock>(Continue->GetContent()); Continue->OnClicked.AddDynamic(this, &UWSTutorialWidget::Next);
	Confirm = WidgetTree->ConstructWidget<UBorder>(); Confirm->SetBrush(FSlateRoundedBoxBrush(WSUITokens::V17::SurfaceRaised, 12.f, WSUITokens::V17::Stroke, 1.f)); Confirm->SetPadding(FMargin(24));
	UCanvasPanelSlot* CS = Root->AddChildToCanvas(Confirm); CS->SetAnchors(FAnchors(.5f)); CS->SetAlignment(FVector2D(.5f)); CS->SetAutoSize(true); CS->SetZOrder(3);
	UVerticalBox* CB = WidgetTree->ConstructWidget<UVerticalBox>(); Confirm->SetContent(CB);
	UTextBlock* Question = Text(22, WSUITokens::V17::Text); Question->SetText(FText::FromString(TEXT("跳过入门教程？\n以后可以在 H 生存手册中重看。"))); CB->AddChildToVerticalBox(Question)->SetPadding(FMargin(0, 0, 0, 20));
	UHorizontalBox* Choices = WidgetTree->ConstructWidget<UHorizontalBox>(); CB->AddChildToVerticalBox(Choices);
	Button(Choices, TEXT("继续阅读"))->OnClicked.AddDynamic(this, &UWSTutorialWidget::ContinueReading);
	Button(Choices, TEXT("确认跳过"))->OnClicked.AddDynamic(this, &UWSTutorialWidget::ConfirmSkip);
	Confirm->SetVisibility(ESlateVisibility::Collapsed);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UWSTutorialWidget::Preload(bool bOnlineReady)
{
	if (bPreloadStarted) return;
	bPreloadStarted = true; const uint32 Expected = ++Generation;
	Data = LoadObject<UWSTutorialData>(nullptr, TEXT("/Game/WindStation/UI/v17/Tutorial/Data/DA_Tutorial_A.DA_Tutorial_A"));
	const TArray<FWSTutorialPage> Source = Data ? Data->Pages : DefaultPages();
	TArray<FSoftObjectPath> Paths;
	for (const FWSTutorialPage& Page : Source)
	{
		if (Page.ModeRequirement == EWSTutorialMode::OnlineReady && !bOnlineReady) continue;
		if (Page.ModeRequirement == EWSTutorialMode::Offline && bOnlineReady) continue;
		Pages.Add(Page);
		if (!Page.PrimaryImage.IsNull()) Paths.AddUnique(Page.PrimaryImage.ToSoftObjectPath());
		if (!Page.SecondaryImage.IsNull()) Paths.AddUnique(Page.SecondaryImage.ToSoftObjectPath());
	}
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateWeakLambda(this, [this, Expected]() { FinishPreload(Expected); }));
	if (!LoadHandle) FinishPreload(Expected);
}

void UWSTutorialWidget::FinishPreload(uint32 Expected)
{
	if (Expected != Generation) return;
	for (const FWSTutorialPage& Page : Pages)
	{
		if (Page.PrimaryImage.Get()) Images.AddUnique(Page.PrimaryImage.Get());
		if (Page.SecondaryImage.Get()) Images.AddUnique(Page.SecondaryImage.Get());
	}
	bImagesReady = true;
}

void UWSTutorialWidget::Open(const FWSTutorialPreference& Preference, bool bReplay)
{
	Flow.Open(Preference, bReplay); bClosePending = bConfirming = false; HeldKeys.Reset();
	Confirm->SetVisibility(ESlateVisibility::Collapsed); Panel->SetIsEnabled(true);
	if (UGameUserSettings* S = UGameUserSettings::GetGameUserSettings()) Blur->SetBlurStrength(S->GetVisualEffectQuality() <= 0 ? 0 : WSUITokens::V17::TutorialBlur);
	bApplicationActive = FSlateApplication::Get().IsActive();
	FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &UWSTutorialWidget::HandleApplicationActivation);
	SetVisibility(ESlateVisibility::Visible); SetKeyboardFocus(); ShowPage();
}

void UWSTutorialWidget::ShowPage()
{
	if (!Pages.IsValidIndex(Flow.GetPage())) return;
	const FWSTutorialPage& P = Pages[Flow.GetPage()];
	Title->SetText(P.Title); Lead->SetText(P.Lead); Body->SetText(P.Body); Hint->SetText(P.Hint);
	PageNumber->SetText(FText::FromString(FString::Printf(TEXT("风雪站 / 生存入门                                      %02d / 05"), Flow.GetPage() + 1)));
	PrimaryImage->SetBrushFromTexture(P.PrimaryImage.Get(), true);
	SecondaryImage->SetBrushFromTexture(P.SecondaryImage.Get(), true);
	SecondaryImage->GetParent()->SetVisibility(P.SecondaryImage.IsNull() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	MissingImage->SetVisibility(P.PrimaryImage.Get() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	MissingImage->SetText(FText::FromString(TEXT("开发占位：实机配图尚未建立\n") + P.AltText.ToString()));
	PreviousButton->SetIsEnabled(Flow.GetPage() > 0);
	ContinueLabel->SetText(FText::FromString(Flow.GetPage() == 4 ? TEXT("鼠标左键 · 开始探索") : TEXT("鼠标左键 · 下一页")));
	OnPageChanged.ExecuteIfBound(P.PageId);
	UE_LOG(LogTemp, Display, TEXT("Tutorial version=1 page=%s replay=%d"), *P.PageId.ToString(), Flow.IsReplay());
}

void UWSTutorialWidget::NativeTick(const FGeometry& G, float DeltaTime)
{
	Super::NativeTick(G, DeltaTime);
	const FVector2D Available = G.GetLocalSize();
	if (Available != LayoutSize && Available.X > 0)
	{
		LayoutSize = Available;
		const FVector2D Size(FMath::Min(1232.0, Available.X * .90), FMath::Min(680.0, Available.Y * .88));
		Cast<UCanvasPanelSlot>(Panel->Slot)->SetSize(Size);
		const bool Compact = Size.Y < 500 || Size.X < 900;
		Panel->SetPadding(FMargin(Compact ? 16 : 32));
		Title->SetFont(FSlateFontInfo(FontFamily, Compact ? 26 : 34));
		ImageColumn->SetWidthOverride(Size.X * (Compact ? .34 : .48));
	}
	if (!bApplicationActive) return;
	const bool Down = !FSlateApplication::Get().GetPressedMouseButtons().IsEmpty() || !HeldKeys.IsEmpty();
	Flow.TickInput(Down, FSlateApplication::Get().GetCurrentTime());
	if (bClosePending)
	{
		if (Down) CloseReleaseFrames = 0;
		else if (++CloseReleaseFrames >= 2)
		{
			const auto Reason = CloseReason; Shutdown(); OnClosed.ExecuteIfBound(Reason);
		}
	}
}

void UWSTutorialWidget::Next()
{
	if (bConfirming || bClosePending || !Flow.IsArmed()) return;
	if (Flow.GetPage() == 4) { RequestClose(EWSTutorialStatus::Completed); return; }
	if (Flow.Navigate(1, FSlateApplication::Get().GetCurrentTime())) ShowPage();
}
void UWSTutorialWidget::Previous()
{
	if (!bConfirming && !bClosePending && Flow.Navigate(-1, FSlateApplication::Get().GetCurrentTime())) ShowPage();
}
void UWSTutorialWidget::Skip() { if (Flow.IsArmed() && !bClosePending) Back(); }
void UWSTutorialWidget::Back()
{
	if (bClosePending) return;
	if (Flow.IsReplay()) { RequestClose(EWSTutorialStatus::InProgress); return; }
	bConfirming = !bConfirming; Panel->SetIsEnabled(!bConfirming);
	Confirm->SetVisibility(bConfirming ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Flow.AwaitRelease(); SetKeyboardFocus();
}
void UWSTutorialWidget::ConfirmSkip() { if (bConfirming && Flow.IsArmed()) RequestClose(EWSTutorialStatus::Skipped); }
void UWSTutorialWidget::ContinueReading() { if (bConfirming) Back(); }
void UWSTutorialWidget::RequestClose(EWSTutorialStatus Reason)
{
	if (bClosePending) return;
	bClosePending = true; CloseReason = Reason; CloseReleaseFrames = 0; Flow.AwaitRelease();
}
void UWSTutorialWidget::HandleApplicationActivation(bool bActive)
{
	bApplicationActive = bActive; HeldKeys.Reset(); Flow.AwaitRelease();
	if (bActive) SetKeyboardFocus();
}
void UWSTutorialWidget::Shutdown()
{
	Flow.Close(); bClosePending = false; HeldKeys.Reset();
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().OnApplicationActivationStateChanged().RemoveAll(this);
	SetVisibility(ESlateVisibility::Collapsed);
}
void UWSTutorialWidget::NativeDestruct()
{
	Shutdown(); ++Generation; if (LoadHandle) LoadHandle->CancelHandle(); LoadHandle.Reset();
	Super::NativeDestruct();
}
FReply UWSTutorialWidget::NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent& E)
{
	if (!PreviousButton->GetIsEnabled() && PreviousButton->GetCachedGeometry().IsUnderLocation(E.GetScreenSpacePosition()))
		return FReply::Handled();
	if (E.GetEffectingButton() == EKeys::LeftMouseButton && !bConfirming && !bClosePending) Flow.Press();
	return FReply::Handled().CaptureMouse(TakeWidget());
}
FReply UWSTutorialWidget::NativeOnMouseButtonUp(const FGeometry&, const FPointerEvent& E)
{
	if (E.GetEffectingButton() == EKeys::LeftMouseButton && Flow.Release(FSlateApplication::Get().GetCurrentTime())) Next();
	return FReply::Handled().ReleaseMouseCapture();
}
FReply UWSTutorialWidget::NativeOnMouseButtonDoubleClick(const FGeometry&, const FPointerEvent&)
{
	Flow.AwaitRelease(); return FReply::Handled();
}
FReply UWSTutorialWidget::NativeOnPreviewKeyDown(const FGeometry& G, const FKeyEvent& E)
{
	if (E.GetKey() == EKeys::Tab) return Super::NativeOnPreviewKeyDown(G, E);
	HeldKeys.Add(E.GetKey());
	if (!E.IsRepeat() && !bClosePending)
	{
		if (E.GetKey() == EKeys::Escape) Back();
		else if (E.GetKey() == EKeys::Left) Previous();
		else if (E.GetKey() == EKeys::Right || E.GetKey() == EKeys::SpaceBar || E.GetKey() == EKeys::Enter) Next();
	}
	return FReply::Handled();
}
FReply UWSTutorialWidget::NativeOnKeyDown(const FGeometry& G, const FKeyEvent& E)
{
	return E.GetKey() == EKeys::Tab ? Super::NativeOnKeyDown(G, E) : FReply::Handled();
}
FReply UWSTutorialWidget::NativeOnKeyUp(const FGeometry&, const FKeyEvent& E)
{
	HeldKeys.Remove(E.GetKey()); return FReply::Handled();
}
