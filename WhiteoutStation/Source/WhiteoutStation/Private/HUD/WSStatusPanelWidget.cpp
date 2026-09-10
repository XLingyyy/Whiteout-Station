#include "HUD/WSStatusPanelWidget.h"
#include "HUD/WSUITokens.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Font.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Rendering/DrawElements.h"

void UWSStatusPanelWidget::Build(UFont* Font)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WhiteoutV15StatusBuild);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(); Width->SetWidthOverride(320);
	WidgetTree->RootWidget = Width;
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(); Width->SetContent(Stack);
	const auto Text = [&](int32 Size)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(); T->SetFont(FSlateFontInfo(Font, Size));
		T->SetAutoWrapText(true); T->SetColorAndOpacity(FSlateColor(WSUITokens::V17::Text)); return T;
	};
	for (int32 Card = 0; Card < 2; ++Card)
	{
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetBrush(FSlateRoundedBoxBrush(WSUITokens::V17::Surface, WSUITokens::V17::Radius, WSUITokens::V17::Stroke, 1.0f)); Background->SetPadding(FMargin(16));
		Stack->AddChildToVerticalBox(Background)->SetPadding(FMargin(0, Card ? 12 : 0, 0, 0));
		if (Card) TargetCard = Background;
		else PlayerCard = Background;
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(); Background->SetContent(Box);
		UTextBlock* Title = Text(20); Titles.Add(Title);
		Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0, 0, 0, 12));
		for (int32 Row = 0; Row < 4; ++Row)
		{
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
			Box->AddChildToVerticalBox(Line)->SetPadding(FMargin(0, 8));
			USizeBox* Icon = WidgetTree->ConstructWidget<USizeBox>(); Icon->SetWidthOverride(22); Icon->SetHeightOverride(22); Icons.Add(Icon);
			Line->AddChildToHorizontalBox(Icon)->SetPadding(FMargin(0, 0, 10, 0));
			UTextBlock* Label = Text(16); Labels.Add(Label);
			Line->AddChildToHorizontalBox(Label)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			UTextBlock* Value = Text(16); Values.Add(Value); Value->SetJustification(ETextJustify::Right);
			Line->AddChildToHorizontalBox(Value)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		if (Card)
		{
			DiagnosisSource = Text(13);
			DiagnosisSource->SetText(FText::FromString(TEXT("伤势来源：已确认诊断")));
			Box->AddChildToVerticalBox(DiagnosisSource)->SetPadding(FMargin(0, 8, 0, 0));
			DiagnosisSource->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	TargetCard->SetVisibility(ESlateVisibility::Collapsed);
}

FGeometry UWSStatusPanelWidget::CardGeometry(bool bTarget) const
{
	return (bTarget ? TargetCard : PlayerCard)->GetCachedGeometry();
}

int32 UWSStatusPanelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Culling,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	Layer = Super::NativePaint(Args, G, Culling, Elements, Layer, Style, bEnabled);
	for (int32 I = 0; I < Icons.Num(); ++I)
	{
		if (I >= 4 && TargetCard->GetVisibility() == ESlateVisibility::Collapsed) continue;
		TArray<FVector2D> Points;
		switch (I % 4)
		{
		case 0: Points = {{8, 3}, {12, 3}, {12, 13}, {15, 16}, {15, 19}, {10, 21}, {5, 19}, {5, 16}, {8, 13}, {8, 3}}; break;
		case 1: Points = {{1, 12}, {5, 12}, {8, 5}, {12, 18}, {16, 10}, {21, 10}}; break;
		case 2: Points = {{4, 3}, {8, 3}, {20, 15}, {20, 19}, {16, 19}, {4, 7}, {4, 3}, {18, 17}}; break;
		default: Points = I >= 4 ? TArray<FVector2D>{{3, 5}, {19, 5}, {19, 16}, {8, 16}, {3, 20}, {3, 5}}
			: TArray<FVector2D>{{3, 18}, {2, 12}, {5, 6}, {11, 3}, {17, 6}, {20, 12}, {19, 18}, {11, 12}, {15, 7}}; break;
		}
		FLinearColor Color = WSUITokens::V17::Secondary; Color.A *= I >= 4 ? TargetOpacity : 1.f;
		FSlateDrawElement::MakeLines(Elements, Layer + 1, Icons[I]->GetCachedGeometry().ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, 1.5f);
	}
	return Layer + 1;
}

void UWSStatusPanelWidget::Present(const FWSStatusCardViewModel& Player, const TOptional<FWSStatusCardViewModel>& Target)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WhiteoutV15StatusPresent);
	if (Target.IsSet() && (TargetCard->GetVisibility() == ESlateVisibility::Collapsed
		|| !Titles[1]->GetText().EqualTo(FText::FromString(Target->Title))))
	{
		TargetOpacity = 0.0f;
		TargetCard->SetRenderOpacity(0.0f);
	}
	for (int32 Card = 0; Card < 2; ++Card)
	{
		const FWSStatusCardViewModel* Model = Card ? (Target.IsSet() ? &Target.GetValue() : nullptr) : &Player;
		const FText Title = Model ? FText::FromString(Model->Title) : FText::GetEmpty();
		if (!Titles[Card]->GetText().EqualTo(Title)) Titles[Card]->SetText(Title);
		for (int32 Row = 0; Row < 4; ++Row)
		{
			const int32 Index = Card * 4 + Row;
			const FWSStatusField* Field = Model && Model->Fields.IsValidIndex(Row) ? &Model->Fields[Row] : nullptr;
			const FText Label = Field ? FText::FromString(Field->Label) : FText::GetEmpty();
			const FText Value = Field ? FText::FromString(Field->DisplayValue) : FText::GetEmpty();
			if (!Labels[Index]->GetText().EqualTo(Label)) Labels[Index]->SetText(Label);
			if (!Values[Index]->GetText().EqualTo(Value)) Values[Index]->SetText(Value);
			const FLinearColor Color = Field && Field->Visibility == EWSStatusVisibility::Unknown ? WSUITokens::V17::Secondary
				: Field && Field->Severity == EWSStatusSeverity::Critical ? WSUITokens::V17::Critical
				: Field && Field->Severity == EWSStatusSeverity::Attention ? WSUITokens::V17::Attention : WSUITokens::V17::Text;
			if (Values[Index]->GetColorAndOpacity() != FSlateColor(Color)) Values[Index]->SetColorAndOpacity(FSlateColor(Color));
		}
	}
	TargetCard->SetVisibility(Target.IsSet() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	DiagnosisSource->SetVisibility(Target.IsSet() && Target->Fields.ContainsByPredicate(
		[](const FWSStatusField& Field) { return Field.Source == EWSStatusSource::Diagnosis; })
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UWSStatusPanelWidget::AdvanceAnimation(float DeltaTime, bool ReducedMotion)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WhiteoutV15StatusAnimation);
	if (TargetOpacity >= 1.0f || TargetCard->GetVisibility() == ESlateVisibility::Collapsed) return;
	TargetOpacity = ReducedMotion ? 1.0f : FMath::Min(1.0f, TargetOpacity + DeltaTime / WSUITokens::V17::FadeIn);
	TargetCard->SetRenderOpacity(TargetOpacity);
}
