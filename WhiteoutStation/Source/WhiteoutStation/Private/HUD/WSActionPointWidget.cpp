#include "HUD/WSActionPointWidget.h"
#include "Engine/Font.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "HUD/WSUITokens.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UWSActionPointWidget::Build(UFont* Font)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Box;
	const auto Text = [&](int32 Size, const FLinearColor& Color)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
		T->SetFont(FSlateFontInfo(Font, Size)); T->SetColorAndOpacity(Color); T->SetAutoWrapText(true);
		Box->AddChildToVerticalBox(T)->SetPadding(FMargin(0, 0, 0, 8)); return T;
	};
	Phase = Text(24, WSUITokens::V17::Text);
	Number = Text(18, WSUITokens::V17::Attention);
	Segments = WidgetTree->ConstructWidget<USizeBox>(); Segments->SetHeightOverride(26);
	Box->AddChildToVerticalBox(Segments)->SetPadding(FMargin(0, 0, 0, 10));
	Hint = Text(14, WSUITokens::V17::Secondary);
	Track = Text(14, WSUITokens::V17::Secondary);
}

void UWSActionPointWidget::Present(const FWSAPViewModel& Model)
{
	const auto Set = [](UTextBlock* T, const FString& S)
	{
		const FText Value = FText::FromString(S); if (!T->GetText().EqualTo(Value)) T->SetText(Value);
	};
	if (!Model.IsValid() && (View.PhaseCapacity != Model.PhaseCapacity || View.PhaseRemaining != Model.PhaseRemaining))
		UE_LOG(LogTemp, Error, TEXT("Invalid phase AP: remaining=%d capacity=%d"), Model.PhaseRemaining, Model.PhaseCapacity);
	if (View.RunId != Model.RunId || View.PhaseId != Model.PhaseId) SpendCount = 0;
	View = Model;
	const TCHAR* Label = Model.PhaseId == EWSDayPhase::Morning ? TEXT("早晨") : Model.PhaseId == EWSDayPhase::Afternoon ? TEXT("午后")
		: Model.PhaseId == EWSDayPhase::Dusk ? TEXT("黄昏") : TEXT("本轮结束");
	Set(Phase, Label);
	Set(Number, FString::Printf(TEXT("行动力 AP                 %d / %d"), Model.PhaseRemaining, Model.PhaseCapacity));
	FString Copy = !Model.IsValid() ? TEXT("行动力状态异常，暂不可确认")
		: !Model.bPhaseStarted ? TEXT("待选择供暖 · 每格 = 1 AP")
		: Model.PhaseRemaining == 0 ? TEXT("本阶段行动力已用尽")
		: Model.PhaseRemaining == 1 ? TEXT("仅余 1 AP · 执行前看清代价") : TEXT("本阶段行动预算 · 每格 = 1 AP");
	if (Model.QuotedCost.IsSet())
	{
		const int32 Cost = Model.QuotedCost.GetValue();
		Copy = Cost > Model.PhaseRemaining ? FString::Printf(TEXT("需要 %d AP，当前 %d AP"), Cost, Model.PhaseRemaining)
			: FString::Printf(TEXT("预计消耗 %d AP · 确认后扣除"), Cost);
		if (!Model.bCanExecute && !Model.BlockingReason.IsEmpty()) Copy += TEXT("\n") + Model.BlockingReason;
	}
	Set(Hint, Copy);
	Set(Track, FString::Printf(TEXT("%s早晨    %s午后    %s黄昏"), Model.PhaseId == EWSDayPhase::Morning ? TEXT("· ") : TEXT(""),
		Model.PhaseId == EWSDayPhase::Afternoon ? TEXT("· ") : TEXT(""), Model.PhaseId == EWSDayPhase::Dusk ? TEXT("· ") : TEXT("")));
}

void UWSActionPointWidget::AnimateSpend(int32 Cost, bool bReducedMotion)
{
	SpendCount = bReducedMotion ? 0 : Cost;
	SpendStarted = FSlateApplication::Get().GetCurrentTime();
}

int32 UWSActionPointWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, Culling, Elements, Layer, Style, bEnabled);
	if (!Segments || View.PhaseCapacity <= 0) return Layer;
	const FGeometry G = Segments->GetCachedGeometry();
	const FVector2D Size = G.GetLocalSize();
	const int32 Count = FMath::Min(View.PhaseCapacity, 24);
	const float Width = (Size.X - (Count - 1) * 6) / Count;
	if (Width <= 0) return Layer;
	static const FSlateRoundedBoxBrush Rounded(FLinearColor::White, 4.f);
	const FSlateBrush* Brush = &Rounded;
	for (int32 I = 0; I < Count; ++I)
	{
		const float X = I * (Width + 6);
		const EWSAPCell Cell = View.Cell(I);
		const FLinearColor Fill = Cell == EWSAPCell::Available ? WSUITokens::V17::Attention : WSUITokens::V17::SurfaceRaised;
		FSlateDrawElement::MakeBox(Elements, Layer + 1, G.ToPaintGeometry(FVector2D(Width, 24), FSlateLayoutTransform(FVector2D(X, 0))), Brush, ESlateDrawEffect::None, WSUITokens::V17::Stroke);
		FSlateDrawElement::MakeBox(Elements, Layer + 2, G.ToPaintGeometry(FVector2D(Width - 2, 22), FSlateLayoutTransform(FVector2D(X + 1, 1))), Brush, ESlateDrawEffect::None, Fill);
		const float Tail = FMath::Clamp(1.0 - (FSlateApplication::Get().GetCurrentTime() - SpendStarted) / .20, 0.0, 1.0);
		if (Tail > 0 && I >= View.PhaseRemaining && I < View.PhaseRemaining + SpendCount)
		{
			FLinearColor Fade = WSUITokens::V17::Attention; Fade.A = Tail;
			FSlateDrawElement::MakeBox(Elements, Layer + 3, G.ToPaintGeometry(FVector2D(Width - 2, 22), FSlateLayoutTransform(FVector2D(X + 1, 1))), Brush, ESlateDrawEffect::None, Fade);
		}
		if (Cell == EWSAPCell::PreviewSpend)
		{
			for (float T = 3; T < Width - 3; T += 9)
			{
				TArray<FVector2D> Points{FVector2D(X + T, 21), FVector2D(X + FMath::Min(T + 14, Width - 2), 3)};
				FSlateDrawElement::MakeLines(Elements, Layer + 3, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, WSUITokens::V17::Attention, true, 1.5f);
			}
		}
	}
	return Layer + 3;
}
