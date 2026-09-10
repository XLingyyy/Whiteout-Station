#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/WSAPViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutAPWidgetTest, "WhiteoutStation.V17.AP.PhaseBudgetAndQuote", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWhiteoutAPWidgetTest::RunTest(const FString&)
{
	FWSAPViewModel V; V.PhaseCapacity = 4;
	for (int32 Remaining = 0; Remaining <= 4; ++Remaining)
	{
		V.PhaseRemaining = Remaining;
		int32 Available = 0; for (int32 I = 0; I < 4; ++I) Available += V.Cell(I) == EWSAPCell::Available;
		TestEqual(TEXT("4/3/2/1/0 maps directly to phase AP"), Available, Remaining);
	}
	V.PhaseRemaining = 3; V.QuotedCost = 2;
	TestTrue(TEXT("Leftmost available cell retained"), V.Cell(0) == EWSAPCell::Available);
	TestTrue(TEXT("Two AP quote marks first spent preview"), V.Cell(1) == EWSAPCell::PreviewSpend);
	TestTrue(TEXT("Two AP quote marks second spent preview"), V.Cell(2) == EWSAPCell::PreviewSpend);
	TestTrue(TEXT("Already spent cell is not quoted"), V.Cell(3) == EWSAPCell::Spent);
	TestEqual(TEXT("Preview leaves authoritative number unchanged"), V.PhaseRemaining, 3);
	V.QuotedCost = 0; TestTrue(TEXT("Free conversation never shades AP"), V.Cell(2) == EWSAPCell::Available);
	V.QuotedCost = 5; TestEqual(TEXT("Over-budget quote is not truncated"), V.QuotedCost.GetValue(), 5);
	V.QuotedCost.Reset(); TestTrue(TEXT("Cancel restores all available cells"), V.Cell(2) == EWSAPCell::Available);
	V.PhaseRemaining = 12; TestFalse(TEXT("Total budget in phase field is rejected"), V.IsValid());
	return true;
}
#endif
