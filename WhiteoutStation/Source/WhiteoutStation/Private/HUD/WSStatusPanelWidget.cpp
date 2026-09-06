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

void UWSStatusPanelWidget::Build(UFont* Font)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(); Width->SetWidthOverride(320);
	WidgetTree->RootWidget = Width;
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(); Width->SetContent(Stack);
	const auto Text = [&](int32 Size)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(); T->SetFont(FSlateFontInfo(Font, Size));
		T->SetAutoWrapText(true); T->SetColorAndOpacity(FSlateColor(WSUITokens::V15::Text)); return T;
	};
	for (int32 Card = 0; Card < 2; ++Card)
	{
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetBrush(FSlateRoundedBoxBrush(WSUITokens::V15::Surface, WSUITokens::V15::Radius, WSUITokens::V15::Stroke, 1.0f)); Background->SetPadding(FMargin(16));
		Stack->AddChildToVerticalBox(Background)->SetPadding(FMargin(0, Card ? 12 : 0, 0, 0));
		if (Card) TargetCard = Background;
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(); Background->SetContent(Box);
		UTextBlock* Title = Text(20); Titles.Add(Title);
		Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0, 0, 0, 12));
		for (int32 Row = 0; Row < 4; ++Row)
		{
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
			Box->AddChildToVerticalBox(Line)->SetPadding(FMargin(0, 4));
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
			const FLinearColor Color = Field && Field->Severity == EWSStatusSeverity::Critical ? WSUITokens::V15::Critical
				: Field && Field->Severity == EWSStatusSeverity::Attention ? WSUITokens::V15::Attention : WSUITokens::V15::Text;
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
	if (TargetOpacity >= 1.0f || TargetCard->GetVisibility() == ESlateVisibility::Collapsed) return;
	TargetOpacity = ReducedMotion ? 1.0f : FMath::Min(1.0f, TargetOpacity + DeltaTime / WSUITokens::V15::FadeIn);
	TargetCard->SetRenderOpacity(TargetOpacity);
}
