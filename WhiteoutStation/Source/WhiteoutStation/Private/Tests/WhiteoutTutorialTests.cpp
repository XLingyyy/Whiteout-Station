#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Flow/WSTutorialFlow.h"
#include "HUD/WSTutorialWidget.h"
#include "State/WindStationStateSubsystem.h"
#include "State/WhiteoutRulesEngine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutTutorialFlowTest, "WhiteoutStation.V17.Tutorial.ReadingAndInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWhiteoutTutorialFlowTest::RunTest(const FString&)
{
	FWSTutorialPreference P;
	TestTrue(TEXT("Fresh install queues first tutorial"), FWSTutorialFlow::NeedsAutoShow(P, false));
	TestFalse(TEXT("Loaded progress is never interrupted"), FWSTutorialFlow::NeedsAutoShow(P, true));
	P.Status = EWSTutorialStatus::Completed;
	TestFalse(TEXT("Completed tutorial is not repeated"), FWSTutorialFlow::NeedsAutoShow(P, false));
	P.Status = EWSTutorialStatus::Skipped;
	TestFalse(TEXT("Skipped tutorial is not repeated"), FWSTutorialFlow::NeedsAutoShow(P, false));
	P.Status = EWSTutorialStatus::InProgress; P.LastPageId = TEXT("T03");
	FWSTutorialFlow F; F.Open(P, false);
	TestEqual(TEXT("Resume stable page ID"), F.GetPage(), 2);
	TestFalse(TEXT("Opening click cannot arm tutorial"), F.Press());
	F.TickInput(true, 1); F.TickInput(false, 2);
	TestFalse(TEXT("One release frame cannot arm"), F.IsArmed());
	F.TickInput(false, 3); TestTrue(TEXT("Released and settled input arms"), F.Press());
	TestTrue(TEXT("Fresh release accepted"), F.Release(3)); TestTrue(TEXT("One page advance"), F.Navigate(1, 3));
	TestFalse(TEXT("Duplicate release discarded"), F.Release(3));
	TestFalse(TEXT("Transition cannot queue another page"), F.Navigate(1, 3.01));
	F.TickInput(false, 3.2); F.TickInput(false, 3.3);
	TestTrue(TEXT("Next fresh click accepted"), F.Press()); F.AwaitRelease();
	TestFalse(TEXT("Alt-tab cancels incomplete click"), F.Release(4));
	F.Close(); F.Close(); TestFalse(TEXT("Idempotent close"), F.IsActive());
	F.Open(P, true); TestEqual(TEXT("Replay starts at first page"), F.GetPage(), 0);
	TestEqual(TEXT("Replay never mutates persistent preference"), P.LastPageId, FName(TEXT("T03")));
	F.TickInput(false, 5); F.TickInput(false, 6); TestFalse(TEXT("First page cannot go back"), F.Navigate(-1, 6));
	for (bool Online : {false, true})
	{
		TSet<FName> Ids;
		for (const auto& Page : UWSTutorialWidget::DefaultPages())
		{
			if (Page.ModeRequirement == EWSTutorialMode::OnlineReady && !Online) continue;
			if (Page.ModeRequirement == EWSTutorialMode::Offline && Online) continue;
			TestFalse(TEXT("No duplicate page per mode"), Ids.Contains(Page.PageId)); Ids.Add(Page.PageId);
			TestFalse(TEXT("Every page has scalable body text"), Page.Body.IsEmpty());
		}
		TestEqual(TEXT("Both modes provide exactly five pages"), Ids.Num(), 5);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutV17SaveMigrationTest, "WhiteoutStation.V17.Save.PreservesV16State", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWhiteoutV17SaveMigrationTest::RunTest(const FString&)
{
	FWhiteoutRulesEngine Rules; Rules.Reset();
	FWSGameState Before = Rules.GetState(); Before.PhaseActionPoints = 2; Before.ActionPoints = 6;
	for (const FString Version : {FString(TEXT("1.6.0")), FString(TEXT("1.7.0"))})
	{
		const auto After = UWindStationStateSubsystem::MigrateSaveStateForV13(Before, Version, Rules.GetConfig().SchemaVersion, Rules.GetConfig().RulesVersion);
		TestEqual(TEXT("No phase AP grant on migration"), After.PhaseActionPoints, Before.PhaseActionPoints);
		TestEqual(TEXT("No total AP grant on migration"), After.ActionPoints, Before.ActionPoints);
		TestEqual(TEXT("Run identity preserved"), After.RunId, Before.RunId);
	}
	return true;
}
#endif
