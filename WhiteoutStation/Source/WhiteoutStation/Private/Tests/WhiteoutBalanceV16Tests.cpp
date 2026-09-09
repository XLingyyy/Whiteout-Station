#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "State/WhiteoutRulesEngine.h"
#include "Save/WindStationSaveGame.h"
#include "Kismet/GameplayStatics.h"

namespace WSBalanceTests
{
	FWhiteoutRulesEngine Load(FAutomationTestBase& Test, EWSHeatingZone Zone)
	{
		FWhiteoutRulesEngine Engine; FString Error;
		Test.TestTrue(TEXT("Load rules"), Engine.LoadConfig(FPaths::ProjectContentDir() / TEXT("Rules/WhiteoutStationRules.v1.6.json"), Error));
		EWSReasonCode Reason; TArray<FString> Changes;
		Test.TestTrue(TEXT("Begin phase"), Engine.BeginDayPhase(Zone, Reason, Changes));
		return Engine;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWSBalanceV16Preparation, "Whiteout.V16.Balance.PreparationPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWSBalanceV16Preparation::RunTest(const FString& Parameters)
{
	auto Engine = WSBalanceTests::Load(*this, EWSHeatingZone::RepairRoom);
	FWSActionRequest Inspect; Inspect.ActionId = TEXT("inspect_control_cabinet"); Inspect.TransactionId = FGuid::NewGuid();
	TestEqual(TEXT("Solo cost"), Engine.Preview(Inspect).APCost, 1);
	Inspect.bHasCollaborator = true; Inspect.Collaborator = EWSCharacterId::GuHeng;
	TestEqual(TEXT("Initial coordination cost"), Engine.Preview(Inspect).APCost, 2);
	TestFalse(TEXT("Quote grants no preparation"), Engine.GetState().bRepairPreparationAvailable);
	TestTrue(TEXT("Joint inspection"), Engine.Commit(Inspect).bCommitted);
	TestTrue(TEXT("Preparation granted"), Engine.GetState().bRepairPreparationAvailable);
	TestFalse(TEXT("Repeated transaction rejected"), Engine.Commit(Inspect).bCommitted);
	Inspect.TransactionId = FGuid::NewGuid();
	TestFalse(TEXT("Repeated cabinet rejected"), Engine.Commit(Inspect).bCommitted);
	TestFalse(TEXT("Professional record does not confirm responsibility"), Engine.GetState().PlayerKnowledge.FindRef(TEXT("FACT_FORCED_RESTART_SUSPICION")) == EWSKnowledgeLevel::Confirmed);
	FWSActionRequest Repair; Repair.ActionId = TEXT("repair_generator"); Repair.bHasCollaborator = true;
	TestFalse(TEXT("Unprepared stamina blocks repair"), Engine.Commit(Repair).bCommitted);
	TestTrue(TEXT("Failure preserves preparation"), Engine.GetState().bRepairPreparationAvailable);
	auto* Save = NewObject<UWindStationSaveGame>(); Save->State = Engine.GetState();
	TArray<uint8> Bytes;
	TestTrue(TEXT("Serialize actual save"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	auto* Loaded = Cast<UWindStationSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Deserialize save"), Loaded)) return false;
	Engine.SetState(Loaded->State);
	TestTrue(TEXT("Save preserves preparation"), Engine.GetState().bRepairPreparationAvailable);
	EWSReasonCode Reason; FWSPhaseSummary Summary; TArray<FString> Changes;
	TestTrue(TEXT("Settle stage"), Engine.SettleDayPhase(Reason, Summary));
	TestTrue(TEXT("Preparation survives stage"), Engine.GetState().bRepairPreparationAvailable);
	Engine.BeginDayPhase(EWSHeatingZone::RepairRoom, Reason, Changes);
	auto& Gu = Engine.GetMutableStateForTesting().Characters.FindChecked(EWSCharacterId::GuHeng);
	Gu.Stamina = 2; Gu.Trust = 6;
	Repair.bHasCollaborator = false;
	const auto DiscountQuote = Engine.Preview(Repair);
	TestEqual(TEXT("Restricted hand normal 2 AP becomes 1 AP"), DiscountQuote.APCost, 1);
	TestEqual(TEXT("Discount still consumes stamina"), DiscountQuote.Costs.Stamina.FindRef(EWSCharacterId::GuHeng), 1);
	TestTrue(TEXT("Discount repair commits"), Engine.Commit(Repair).bCommitted);
	TestFalse(TEXT("Successful repair consumes preparation"), Engine.GetState().bRepairPreparationAvailable);
	TestEqual(TEXT("Injury still worsens"), Gu.InjuryWorseningMarks, 1);
	Engine = WSBalanceTests::Load(*this, EWSHeatingZone::RepairRoom);
	auto& State = Engine.GetMutableStateForTesting();
	State.bRepairPreparationAvailable = true;
	auto& HealthyGu = State.Characters.FindChecked(EWSCharacterId::GuHeng);
	HealthyGu.Stamina = 2; HealthyGu.Trust = 6; HealthyGu.InjurySeverity = EWSInjurySeverity::Normal;
	TestEqual(TEXT("One AP waiver branch"), Engine.Preview(Repair).APCost, 1);
	TestEqual(TEXT("Quote waives stamina"), Engine.Preview(Repair).Costs.Stamina.Num(), 0);
	TestTrue(TEXT("Waiver commits"), Engine.Commit(Repair).bCommitted);
	TestEqual(TEXT("Actual stamina preserved"), HealthyGu.Stamina, 2);
	TestTrue(TEXT("Event records preparation consumption"), State.EventLog.Last().bRepairPreparationConsumed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWSBalanceV16Scores, "Whiteout.V16.Balance.ScoreAndCare", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWSBalanceV16Scores::RunTest(const FString& Parameters)
{
	auto Engine = WSBalanceTests::Load(*this, EWSHeatingZone::Kitchen);
	auto& State = Engine.GetMutableStateForTesting();
	State.Tasks.GeneratorProgress = 2; State.Tasks.AntennaCalibration = 1; State.Tasks.bSignalSent = true;
	State.Flags.bRecordsPreserved = true;
	for (int32 I = 0; I < 6; ++I) State.PlayerKnowledge.Add(FName(*FString::Printf(TEXT("score_fact_%d"), I)), EWSKnowledgeLevel::Confirmed);
	for (auto& Pair : State.Characters)
	{
		Pair.Value.Temperature = 7; Pair.Value.Stamina = 2; Pair.Value.InjurySeverity = EWSInjurySeverity::Normal; Pair.Value.Pressure = 0; Pair.Value.Trust = 10;
	}
	State.Resources.Fuel = 1; State.Resources.Food = State.Resources.Medicine = State.Resources.HeatPack = 0;
	TestEqual(TEXT("Fully cared crew reaches 100 with no needless stockpile"), Engine.CalculateScore().Total, 100.0f);
	auto& Player = State.Characters.FindChecked(EWSCharacterId::Player);
	Player.Temperature = 5.2f;
	const float ColdScore = Engine.CalculateScore().Total;
	Player.Temperature = 5.7f;
	TestTrue(TEXT("Warming within same band earns score"), Engine.CalculateScore().Total > ColdScore);
	Player.Temperature = 7;
	Player.InjurySeverity = EWSInjurySeverity::Restricted;
	State.Resources.Medicine = 1;
	const float Hoarded = Engine.CalculateScore().Total;
	Player.InjurySeverity = EWSInjurySeverity::Normal; State.Resources.Medicine = 0;
	TestTrue(TEXT("Treatment beats hoarding medicine"), Engine.CalculateScore().Total > Hoarded);
	State.Tasks.bSignalSent = false;
	TestEqual(TEXT("Signal cap agrees with total"), Engine.CalculateScore().Total, 69.9f);
	TestEqual(TEXT("Signal cap grade"), Engine.CalculateScore().Rating, FString(TEXT("C")));
	State.Tasks.bSignalSent = true; Player.InjurySeverity = EWSInjurySeverity::Critical;
	TestEqual(TEXT("Critical cap agrees with total"), Engine.CalculateScore().Total, 79.9f);
	TestEqual(TEXT("Critical cap grade"), Engine.CalculateScore().Rating, FString(TEXT("B")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWSBalanceV16Recovery, "Whiteout.V16.Balance.RecoveryAndAllocation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWSBalanceV16Recovery::RunTest(const FString& Parameters)
{
	FWhiteoutRulesEngine Engine;
	FString Error;
	if (!TestTrue(TEXT("Load v1.6"), Engine.LoadConfig(FPaths::ProjectContentDir() / TEXT("Rules/WhiteoutStationRules.v1.6.json"), Error))) return false;
	EWSReasonCode Reason;
	TArray<FString> Changes;
	Engine.BeginDayPhase(EWSHeatingZone::MedicalRoom, Reason, Changes);
	auto& State = Engine.GetMutableStateForTesting();
	auto& Ye = State.Characters.FindChecked(EWSCharacterId::YeCheng);
	Ye.Temperature = 5.2f;
	FWSActionRequest Rest;
	Rest.ActionId = TEXT("rest"); Rest.RestTarget = EWSCharacterId::YeCheng; Rest.RestLocation = EWSCharacterLocation::MedicalRoom;
	TestTrue(TEXT("Warm rest commits"), Engine.Commit(Rest).bCommitted);
	TestEqual(TEXT("Warm rest immediately restores temperature"), Ye.Temperature, 6.2f);
	TestEqual(TEXT("Warm rest restores stamina"), Ye.Stamina, 2);
	TestEqual(TEXT("Warm rest also reduces pressure"), Ye.Pressure, 4.4f);
	TestTrue(TEXT("Full stamina still allows rest"), Engine.Commit(Rest).bCommitted);
	TestEqual(TEXT("Full stamina still warms"), Ye.Temperature, 7.2f);
	FWSActionRequest Food;
	Food.ActionId = TEXT("distribute_food"); Food.FoodForPlayer = Food.FoodForGuHeng = Food.FoodForYeCheng = 1;
	State.Characters.FindChecked(EWSCharacterId::Player).Stamina = 0;
	TestTrue(TEXT("Exhausted player can distribute to all three"), Engine.Preview(Food).bCanExecute);
	TestEqual(TEXT("Distribution costs exactly one AP"), Engine.Preview(Food).APCost, 1);
	TestTrue(TEXT("Three-person allocation commits"), Engine.Commit(Food).bCommitted);
	TestEqual(TEXT("Three starting rations consumed"), State.Resources.Food, 0);
	return true;
}
#endif
