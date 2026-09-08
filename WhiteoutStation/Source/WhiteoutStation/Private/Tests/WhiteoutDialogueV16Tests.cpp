#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "State/WindStationStateSubsystem.h"
#include "Agents/WSAgentGateway.h"
#include "Dialogue/WSConversationValidator.h"
#include "State/WSKnowledgePolicy.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutV16QuotaTest,
	"WhiteoutStation.Dialogue.V16.Quota.PersistenceAndZeroAP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV16QuotaTest::RunTest(const FString& Parameters)
{
	UGameInstance* Game = NewObject<UGameInstance>(); Game->AddToRoot(); Game->Init();
	auto* State = Game->GetSubsystem<UWindStationStateSubsystem>();
	if (!TestNotNull(TEXT("Subsystem"), State)) { Game->Shutdown(); Game->RemoveFromRoot(); return false; }
	const FString Slot = TEXT("Whiteout_V16_Quota_") + FGuid::NewGuid().ToString();
	State->SetAutomationSaveSlot(Slot); State->NewGame();
	EWSReasonCode Reason; TArray<FString> Changes;
	State->BeginDayPhase(EWSHeatingZone::MedicalRoom, Reason, Changes);
	State->SetDialogueRealizeTestHook([](const FWSPreparedDialogue& P, FWSDialogueRealizeTestCallback Callback) { Callback(P.LocalFallback); });
	const int32 AP = State->GetStateSnapshot().PhaseActionPoints;
	const float Trust = State->GetStateSnapshot().Characters.FindChecked(EWSCharacterId::GuHeng).Trust;
	for (int32 I = 0; I < 10; ++I)
	{
		const FGuid Session = FGuid::NewGuid();
		TestTrue(FString::Printf(TEXT("Turn %d commits across windows"), I + 1),
			State->SubmitAuthoredDialogueChoice(TEXT("talk_gu_heng"), TEXT("gu_person"), Session).bCommitted);
		State->EndDialogueSession(Session);
	}
	TestEqual(TEXT("All ten conversations cost zero AP"), State->GetStateSnapshot().PhaseActionPoints, AP);
	TestEqual(TEXT("Ordinary repeat questions do not farm trust"), State->GetStateSnapshot().Characters.FindChecked(EWSCharacterId::GuHeng).Trust, Trust);
	TestTrue(TEXT("Save quota"), State->SaveSnapshot());
	TestTrue(TEXT("Restore quota"), State->LoadSnapshot());
	TestFalse(TEXT("Eleventh turn rejected after load"), State->SubmitAuthoredDialogueChoice(TEXT("talk_gu_heng"), TEXT("gu_person"), FGuid::NewGuid()).bCommitted);
	TestTrue(TEXT("Other NPC has independent quota"), State->SubmitAuthoredDialogueChoice(TEXT("talk_ye_cheng"), TEXT("ye_person"), FGuid::NewGuid()).bCommitted);
	TestEqual(TEXT("Ten successful exchanges preserved"), State->GetConversationHistory(TEXT("talk_gu_heng")).Num(), 10);
	UGameplayStatics::DeleteGameInSlot(Slot, 0); State->SetDialogueRealizeTestHook({});
	Game->Shutdown(); Game->RemoveFromRoot(); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutV16NaturalContractTest,
	"WhiteoutStation.Dialogue.V16.Natural.PermissionAndVerification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV16NaturalContractTest::RunTest(const FString& Parameters)
{
	FWSPreparedDialogue Prepared;
	Prepared.bNaturalV16 = true; Prepared.TransactionId = FGuid::NewGuid();
	Prepared.OriginalRequest.ActionId = TEXT("talk_ye_cheng");
	Prepared.OriginalRequest.TransactionId = Prepared.TransactionId;
	Prepared.OriginalRequest.DialogueSessionId = FGuid::NewGuid();
	Prepared.OriginalRequest.SemanticFrame.TopicId = TEXT("medical");
	Prepared.AnswerGoalIds = {TEXT("goal_0")};
	Prepared.LocalFallback.TransactionId = Prepared.TransactionId;
	Prepared.LocalFallback.ActionId = Prepared.OriginalRequest.ActionId;
	Prepared.LocalFallback.DialogueSessionId = Prepared.OriginalRequest.DialogueSessionId;
	Prepared.LocalFallback.Speaker = EWSCharacterId::YeCheng;
	FWSRoleplayKnowledgeItem Fact; Fact.KnowledgeId = TEXT("hand"); Fact.GameFactId = TEXT("FACT_HAND_INJURY"); Fact.bCreatesGameFact = true;
	Prepared.NaturalFacts.Add(Fact.KnowledgeId, Fact); Prepared.AllowedFactIds = {Fact.GameFactId};
	const FString Json = TEXT("{\"npc_line\":\"没有，他的手还没治疗。\",\"addressed_goal_ids\":[\"goal_0\"],\"referenced_fact_ids\":[\"hand\"],\"action_proposal_ids\":[],\"emotion\":\"clinical\",\"reaction_action\":\"consider\"}");
	FWSDialogueOutcome Outcome; FString Error;
	TestTrue(TEXT("Complete natural sentence parses"), FWSConversationValidator::ParseReply(Json, Prepared, Outcome, Error));
	TestEqual(TEXT("No diagnostic string is appended"), Outcome.FinalReply.Utterance, FString(TEXT("没有，他的手还没治疗。")));
	TestTrue(TEXT("Medical sentence requires whole-text verification"), FWSConversationValidator::NeedsSemanticCheck(Prepared, Outcome));
	TestFalse(TEXT("Writer references alone never authorize commit"), FWSConversationValidator::ValidateCommitted(Prepared, Outcome, Error));
	TestEqual(TEXT("No disclosure before verification"), Outcome.DisclosedFactIds.Num(), 0);
	TestFalse(TEXT("Verifier rejection cannot commit"), FWSConversationValidator::ApplyVerdict(TEXT("{\"safe\":false,\"issues\":[\"wrong_patient\"],\"expressed_fact_ids\":[],\"addressed_goal_ids\":[],\"corrects_entry_id\":\"\"}"), Prepared, Outcome, Error));
	TestTrue(TEXT("Independent verdict accepted"), FWSConversationValidator::ApplyVerdict(TEXT("{\"safe\":true,\"issues\":[],\"expressed_fact_ids\":[\"hand\"],\"addressed_goal_ids\":[\"goal_0\"],\"corrects_entry_id\":\"\"}"), Prepared, Outcome, Error));
	TestTrue(TEXT("Verified final text commits"), FWSConversationValidator::ValidateCommitted(Prepared, Outcome, Error));
	TestTrue(TEXT("Only actually expressed fact disclosed"), Outcome.DisclosedFactIds.Contains(Fact.GameFactId));
	TestFalse(TEXT("Unknown fact rejected"), FWSConversationValidator::ParseReply(Json.Replace(TEXT("\"hand\""), TEXT("\"secret\"")), Prepared, Outcome, Error));
	FWSGameState State; FWSCharacterState Gu; Gu.InjurySeverity = EWSInjurySeverity::Restricted;
	State.Characters.Add(EWSCharacterId::GuHeng, Gu);
	TestEqual(TEXT("Untreated state"), FWSKnowledgePolicy::CharacterState(EWSCharacterId::GuHeng, State, true).TreatmentStatus, FString(TEXT("not_started")));
	State.Characters[EWSCharacterId::GuHeng].TemporarySupportUses = 1;
	State.Characters[EWSCharacterId::GuHeng].TemporarySupportPhase = State.DayPhase;
	TestEqual(TEXT("Support is separate from treatment"), FWSKnowledgePolicy::CharacterState(EWSCharacterId::GuHeng, State, true).TreatmentStatus, FString(TEXT("temporary_support_only")));
	State.Flags.bGuHengTreated = true;
	TestEqual(TEXT("Completed treatment outranks old support"), FWSKnowledgePolicy::CharacterState(EWSCharacterId::GuHeng, State, true).TreatmentStatus, FString(TEXT("completed")));
	TestFalse(TEXT("Hidden injury remains unknown"), FWSKnowledgePolicy::CharacterState(EWSCharacterId::GuHeng, State, false).bInjuryKnown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutV16MigrationTest,
	"WhiteoutStation.Dialogue.V16.Save.MigrationAndZeroAPPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV16MigrationTest::RunTest(const FString& Parameters)
{
	FWSGameState Old; Old.PhaseActionPoints = 0; Old.ActionPoints = 0;
	FWSConversationEntry Entry; Entry.EntryId = FGuid::NewGuid(); Entry.SpeakerId = TEXT("gu_heng"); Entry.bCommitted = true;
	Old.ConversationHistory = {Entry, Entry};
	const auto Migrated = UWindStationStateSubsystem::MigrateSaveStateForV13(Old, TEXT("1.5.0"), 7, TEXT("1.6.0"));
	TestEqual(TEXT("Migrate actual unique exchanges"), Migrated.DialogueLedger.FindChecked(TEXT("gu_heng")).UsedTurns, 1);
	TestEqual(TEXT("No legacy AP refund"), Migrated.PhaseActionPoints, 0);
	const auto Again = UWindStationStateSubsystem::MigrateSaveStateForV13(Migrated, TEXT("1.6.0"), 7, TEXT("1.6.0"));
	TestEqual(TEXT("Migration is one-time"), Again.DialogueLedger.FindChecked(TEXT("gu_heng")).UsedTurns, 1);
	FWhiteoutRulesEngine Rules; FString Error;
	TestTrue(TEXT("Load v1.6 rules"), Rules.LoadConfig(FPaths::ProjectContentDir() / TEXT("Rules/WhiteoutStationRules.v1.6.json"), Error));
	EWSReasonCode Reason; TArray<FString> Changes; Rules.BeginDayPhase(EWSHeatingZone::MedicalRoom, Reason, Changes);
	Rules.GetMutableStateForTesting().PhaseActionPoints = 0;
	Rules.GetMutableStateForTesting().ActionPoints = 0;
	FWSActionRequest Talk; Talk.ActionId = TEXT("talk_gu_heng");
	TestTrue(TEXT("Zero AP permits conversation"), Rules.Preview(Talk).bCanExecute);
	TestEqual(TEXT("Zero AP bypasses minimum action clamp"), Rules.Preview(Talk).APCost, 0);
	FWSActionRequest Rest; Rest.ActionId = TEXT("rest");
	TestFalse(TEXT("Real action still needs AP"), Rules.Preview(Rest).bCanExecute);
	TestTrue(TEXT("Real action cost preserved"), Rules.Preview(Rest).APCost > 0);
	return true;
}
#endif
