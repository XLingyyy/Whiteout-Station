#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "State/WindStationStateSubsystem.h"
#include "Agents/WSAgentGateway.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutV15ConversationHistoryTest,
	"WhiteoutStation.Dialogue.V15.History.PersistenceAndIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV15ConversationHistoryTest::RunTest(const FString& Parameters)
{
	UGameInstance* Game = NewObject<UGameInstance>(); Game->AddToRoot(); Game->Init();
	auto* State = Game->GetSubsystem<UWindStationStateSubsystem>();
	if (!TestNotNull(TEXT("Subsystem"), State)) { Game->Shutdown(); Game->RemoveFromRoot(); return false; }
	const FString Slot = TEXT("Whiteout_V15_History_") + FGuid::NewGuid().ToString();
	State->SetAutomationSaveSlot(Slot); State->NewGame();
	EWSReasonCode Reason; TArray<FString> Changes;
	State->BeginDayPhase(EWSHeatingZone::MedicalRoom, Reason, Changes);
	State->SetDialogueRealizeTestHook([](const FWSPreparedDialogue& P, FWSDialogueRealizeTestCallback Callback) { Callback(P.LocalFallback); });
	const FName Gu = TEXT("talk_gu_heng"), Ye = TEXT("talk_ye_cheng");
	const FGuid FirstSession = FGuid::NewGuid(), NewSession = FGuid::NewGuid();
	const int32 InitialAP = State->GetStateSnapshot().PhaseActionPoints;
	TestTrue(TEXT("Authored reply commits"), State->SubmitAuthoredDialogueChoice(Gu, TEXT("gu_person"), FirstSession).bCommitted);
	const auto Entries = State->GetConversationHistory(Gu);
	TestEqual(TEXT("One exchange persisted"), Entries.Num(), 1);
	if (!Entries.IsEmpty())
	{
		TestFalse(TEXT("Exact player line saved"), Entries[0].PlayerLine.IsEmpty());
		TestEqual(TEXT("Exact NPC line saved"), Entries[0].NpcLine, State->GetLatestDialogue().Utterance);
		TestTrue(TEXT("Successful transaction tagged"), Entries[0].bCommitted);
	}
	State->EndDialogueSession(FirstSession);
	TestEqual(TEXT("Close keeps transcript"), State->GetConversationHistory(Gu).Num(), 1);
	TestEqual(TEXT("Other NPC cannot see private transcript"), State->GetConversationHistory(Ye).Num(), 0);
	const FString AContext = FString::Join(State->BuildOnlineConversationHistory(Gu, NewSession), TEXT("\n"));
	TestTrue(TEXT("Intent parser sees earlier question"), !Entries.IsEmpty() && AContext.Contains(Entries[0].PlayerLine));
	TestTrue(TEXT("Old session not counted as current turn"), AContext.Contains(TEXT("current_session=false committed=true current_turn=0")));
	TestFalse(TEXT("Other NPC parser cannot read Gu reply"), !Entries.IsEmpty() && FString::Join(State->BuildOnlineConversationHistory(Ye, NewSession), TEXT("\n")).Contains(Entries[0].NpcLine));

	FWSPreparedDialogue Prepared; Prepared.bRoleplayV14 = true; Prepared.bRoleplayV15 = true;
	Prepared.ReadSnapshot = State->GetStateSnapshot(); Prepared.RoleplayRequest.SpeakerId = TEXT("gu_heng");
	Prepared.OriginalRequest.DialogueSessionId = NewSession;
	auto* Gateway = NewObject<UWSAgentGateway>();
	TSharedPtr<FJsonObject> Context;
	TestTrue(TEXT("Expression context serializes"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Gateway->BuildDialogueRealizationContextJson(Prepared)), Context));
	if (Context)
	{
		const auto& Recent = Context->GetArrayField(TEXT("recent_turns"));
		TestEqual(TEXT("Expression sees earlier session"), Recent.Num(), 1);
		if (!Recent.IsEmpty()) TestEqual(TEXT("Expression remembers exact answer"), Recent[0]->AsObject()->GetStringField(TEXT("npc_line")), Entries[0].NpcLine);
	}
	TestTrue(TEXT("Save transcript"), State->SaveSnapshot());
	TestTrue(TEXT("Reload transcript"), State->LoadSnapshot());
	TestEqual(TEXT("Reload restores history"), State->GetConversationHistory(Gu).Num(), 1);
	TestEqual(TEXT("Reload did not charge AP"), State->GetStateSnapshot().PhaseActionPoints, InitialAP);

	FWSCanonicalIntent Proposal; Proposal.SpeakerId = TEXT("gu_heng"); Proposal.TopicId = TEXT("commitment");
	Proposal.Frame.SpeechAct = EWSDialogueAct::Promise; Proposal.Frame.TargetCharacter = EWSCharacterId::GuHeng;
	Proposal.Frame.Confidence = 1.0f; Proposal.Frame.Source = TEXT("canonical_v15");
	Proposal.Commitment = EWSCommitmentIntent::Proposed; Proposal.PromiseCondition = TEXT("heat_zone");
	FWSPromiseTerms Term; Term.Kind = TEXT("heat_zone"); Term.Zone = EWSHeatingZone::Kitchen; Term.PhaseOffset = 1; Proposal.Terms.Add(Term);
	FWSCanonicalIntent Resolved; FString Status;
	TestFalse(TEXT("Proposal remains unpaid"), State->ResolveParsedOnlineMessage(Gu, TEXT("下一阶段给厨房供暖。"), NewSession, Proposal, Resolved, Status));
	TestEqual(TEXT("Proposal response is visible history"), State->GetConversationHistory(Gu).Num(), 2);
	TestTrue(TEXT("Proposal response is a counted exchange"), State->GetConversationHistory(Gu).Last().bCountedTurn);
	TestEqual(TEXT("Proposal response saved exactly"), State->GetConversationHistory(Gu).Last().NpcLine, Status);
	TestEqual(TEXT("History does not register promises"), State->GetStateSnapshot().Promises.Num(), 0);
	TestEqual(TEXT("Control history does not charge"), State->GetStateSnapshot().PhaseActionPoints, InitialAP);
	State->EndDialogueSession(NewSession);
	TestNull(TEXT("Old pending ledger is gone"), State->GetDialogueSessionState(NewSession));
	TestEqual(TEXT("Pending conversation remains readable"), State->GetConversationHistory(Gu).Num(), 2);

	FWSPreparedDialogue Held; FWSDialogueRealizeTestCallback Late;
	State->SetDialogueRealizeTestHook([&](const FWSPreparedDialogue& P, FWSDialogueRealizeTestCallback Callback) { Held = P; Late = MoveTemp(Callback); });
	FWSCanonicalIntent Ask; Ask.SpeakerId = TEXT("gu_heng"); Ask.TopicId = TEXT("person");
	Ask.Frame.SpeechAct = EWSDialogueAct::Ask; Ask.Frame.TargetCharacter = EWSCharacterId::GuHeng;
	Ask.Frame.Confidence = 1.0f; Ask.Frame.Source = TEXT("canonical_v15");
	const FGuid LastSession = FGuid::NewGuid();
	TestTrue(TEXT("Reopened query resolves"), State->ResolveParsedOnlineMessage(Gu, TEXT("你为什么留在这里？"), LastSession, Ask, Resolved, Status));
	FWSActionRequest Request; Resolved.ApplyTo(Request); Request.ActionId = Gu;
	Request.DialogueSessionId = LastSession; Request.TransactionId = FGuid::NewGuid(); Request.PlayerSaid = TEXT("你为什么留在这里？");
	State->SubmitDialogueAction(Request);
	TestTrue(TEXT("Response actually pending"), State->HasPendingDialogue());
	State->CancelPendingDialogue();
	if (Late) Late(Held.LocalFallback);
	TestEqual(TEXT("Cancelled late answer never remembered"), State->GetConversationHistory(Gu).Num(), 2);
	TestEqual(TEXT("Cancelled answer never charged"), State->GetStateSnapshot().PhaseActionPoints, InitialAP);
	State->SetDialogueRealizeTestHook([](const FWSPreparedDialogue& P, FWSDialogueRealizeTestCallback Callback) { Callback(P.LocalFallback); });
	TestTrue(TEXT("Retry commits once"), State->SubmitDialogueAction(Request).bCommitted);
	TestFalse(TEXT("Duplicate cannot commit"), State->SubmitDialogueAction(Request).bCommitted);
	TestEqual(TEXT("Retry and duplicate add exactly one exchange"), State->GetConversationHistory(Gu).Num(), 3);
	TestEqual(TEXT("Reopen preserves zero AP"), State->GetStateSnapshot().PhaseActionPoints, InitialAP);
	State->NewGame(); TestEqual(TEXT("New game clears old conversations"), State->GetStateSnapshot().ConversationHistory.Num(), 0);
	UGameplayStatics::DeleteGameInSlot(Slot, 0); State->SetDialogueRealizeTestHook({});
	Game->Shutdown(); Game->RemoveFromRoot(); return true;
}
#endif
