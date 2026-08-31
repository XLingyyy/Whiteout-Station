#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "State/WindStationStateSubsystem.h"

namespace WhiteoutDialogueAuditTests
{
	class FScopedStateSubsystem
	{
	public:
		FScopedStateSubsystem()
		{
			const FString UniqueId =
				FGuid::NewGuid().ToString(EGuidFormats::Digits);
			SaveSlot = TEXT("WhiteoutStation_AuditAutomation_") + UniqueId;
			AuditPath = FPaths::ProjectSavedDir()
				/ FString::Printf(
					TEXT("Automation/DialogueAudit_%s.jsonl"),
					*UniqueId);
			EventLogPath = FPaths::ProjectSavedDir()
				/ FString::Printf(
					TEXT("Automation/EventLog_%s.json"),
					*UniqueId);
			IFileManager::Get().Delete(*AuditPath, false, true, true);
			IFileManager::Get().Delete(*EventLogPath, false, true, true);

			GameInstance = NewObject<UGameInstance>();
			GameInstance->AddToRoot();
			GameInstance->Init();
			StateSubsystem =
				GameInstance->GetSubsystem<UWindStationStateSubsystem>();
			if (StateSubsystem)
			{
				StateSubsystem->SetAutomationSaveSlot(SaveSlot);
				StateSubsystem->SetDialogueAuditPathForTest(AuditPath);
				StateSubsystem->SetEventLogExportPathForTest(EventLogPath);
				UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
				StateSubsystem->NewGame();
				EWSReasonCode Reason = EWSReasonCode::UnknownAction;
				TArray<FString> Changes;
				bReady = StateSubsystem->BeginDayPhase(
					EWSHeatingZone::RepairRoom,
					Reason,
					Changes);
			}
		}

		~FScopedStateSubsystem()
		{
			if (StateSubsystem)
			{
				StateSubsystem->SetDialogueRealizeTestHook({});
				StateSubsystem->SetDialogueAuditPathForTest({});
				StateSubsystem->SetEventLogExportPathForTest({});
			}
			UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
			if (GameInstance)
			{
				GameInstance->Shutdown();
				GameInstance->RemoveFromRoot();
			}
			IFileManager::Get().Delete(*AuditPath, false, true, true);
			IFileManager::Get().Delete(*EventLogPath, false, true, true);
		}

		UWindStationStateSubsystem* Get() const
		{
			return StateSubsystem;
		}

		bool IsReady() const
		{
			return bReady;
		}

		const FString& GetAuditPath() const
		{
			return AuditPath;
		}

	private:
		UGameInstance* GameInstance = nullptr;
		UWindStationStateSubsystem* StateSubsystem = nullptr;
		FString SaveSlot;
		FString AuditPath;
		FString EventLogPath;
		bool bReady = false;
	};

	FWSActionRequest MakeTalkRequest()
	{
		FWSActionRequest Request;
		Request.ActionId = TEXT("talk_gu_heng");
		Request.TransactionId = FGuid::NewGuid();
		Request.DialogueSessionId = FGuid::NewGuid();
		Request.DialogueAct = EWSDialogueAct::Ask;
		Request.PlayerSaid = TEXT("审计里不能出现这句玩家原话");
		Request.SemanticFrame.SpeechAct = EWSDialogueAct::Ask;
		Request.SemanticFrame.QueryType = EWSDialogueQueryType::Status;
		Request.SemanticFrame.TargetCharacter = EWSCharacterId::GuHeng;
		Request.SemanticFrame.Confidence = 1.0f;
		Request.SemanticFrame.Source = TEXT("automation_test");
		return Request;
	}

	bool HasExactKeys(
		const TSharedPtr<FJsonObject>& Object,
		const TSet<FString>& Expected)
	{
		if (!Object.IsValid() || Object->Values.Num() != Expected.Num())
		{
			return false;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
		{
			if (!Expected.Contains(Field.Key))
			{
				return false;
			}
		}
		return true;
	}

	bool IsSafeAuditToken(const FString& Value)
	{
		if (Value.IsEmpty()
			|| Value[0] < TEXT('a')
			|| Value[0] > TEXT('z'))
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			const bool bLetter = Character >= TEXT('a')
				&& Character <= TEXT('z');
			const bool bDigit = Character >= TEXT('0')
				&& Character <= TEXT('9');
			if (!bLetter && !bDigit && Character != TEXT('_'))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutDialogueAuditAndEventExportTest,
	"WhiteoutStation.Dialogue.V13.AuditAndEventExport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutDialogueAuditAndEventExportTest::RunTest(
	const FString& Parameters)
{
	using namespace WhiteoutDialogueAuditTests;
	FScopedStateSubsystem Fixture;
	UWindStationStateSubsystem* StateSubsystem = Fixture.Get();
	if (!TestNotNull(TEXT("State subsystem initializes"), StateSubsystem)
		|| !TestTrue(TEXT("Day phase starts"), Fixture.IsReady()))
	{
		return false;
	}

	StateSubsystem->SetDialogueRealizeTestHook(
		[](const FWSPreparedDialogue& Prepared, FWSDialogueRealizeTestCallback Reply)
		{
			Reply(Prepared.LocalFallback);
			Reply(Prepared.LocalFallback);
		});
	const FWSActionRequest Request = MakeTalkRequest();
	const FWSActionResult Result =
		StateSubsystem->SubmitDialogueAction(Request);
	TestTrue(TEXT("Dialogue commits"), Result.bCommitted);

	FString AuditContents;
	TestTrue(
		TEXT("Dialogue audit is written"),
		FFileHelper::LoadFileToString(
			AuditContents,
			*Fixture.GetAuditPath()));
	TArray<FString> AuditLines;
	AuditContents.ParseIntoArrayLines(AuditLines, true);
	TestEqual(
		TEXT("A duplicate callback still appends one audit line"),
		AuditLines.Num(),
		1);
	if (AuditLines.Num() != 1)
	{
		return false;
	}
	TSharedPtr<FJsonObject> Audit;
	const TSharedRef<TJsonReader<>> AuditReader =
		TJsonReaderFactory<>::Create(AuditLines[0]);
	TestTrue(
		TEXT("Audit line is valid JSON"),
		FJsonSerializer::Deserialize(AuditReader, Audit) && Audit.IsValid());
	if (!Audit.IsValid())
	{
		return false;
	}
	const TSet<FString> AuditFields = {
		TEXT("kind"),
		TEXT("transaction_id"),
		TEXT("speaker"),
		TEXT("query_type"),
		TEXT("target_action_id"),
		TEXT("planned_disclosure_fact_ids"),
		TEXT("final_disclosed_fact_ids"),
		TEXT("required_atom_ids"),
		TEXT("realized_atom_ids"),
		TEXT("answer_source"),
		TEXT("validation_outcome"),
		TEXT("prompt_tokens"),
		TEXT("completion_tokens")};
	TestTrue(
		TEXT("Audit has exactly the thirteen whitelisted fields"),
		HasExactKeys(Audit, AuditFields));
	TestEqual(
		TEXT("Audit kind is stable"),
		Audit->GetStringField(TEXT("kind")),
		FString(TEXT("dialogue_expression")));
	TestEqual(
		TEXT("Speaker is normalized"),
		Audit->GetStringField(TEXT("speaker")),
		FString(TEXT("gu_heng")));
	TestEqual(
		TEXT("Query type is normalized"),
		Audit->GetStringField(TEXT("query_type")),
		FString(TEXT("status")));
	TestEqual(
		TEXT("Fallback answer source is normalized"),
		Audit->GetStringField(TEXT("answer_source")),
		FString(TEXT("local_natural_fallback")));
	TestTrue(
		TEXT("Validation outcome is a safe ASCII token"),
		IsSafeAuditToken(
			Audit->GetStringField(TEXT("validation_outcome"))));
	TestEqual(
		TEXT("Unavailable prompt usage is explicit"),
		static_cast<int32>(Audit->GetNumberField(TEXT("prompt_tokens"))),
		-1);
	TestEqual(
		TEXT("Unavailable completion usage is explicit"),
		static_cast<int32>(Audit->GetNumberField(TEXT("completion_tokens"))),
		-1);
	TestFalse(
		TEXT("Player input is absent from the audit"),
		AuditLines[0].Contains(Request.PlayerSaid));
	TestFalse(
		TEXT("NPC line is absent from the audit"),
		AuditLines[0].Contains(StateSubsystem->GetLatestDialogue().Utterance));

	FString EventLogPath;
	TestTrue(
		TEXT("Event log exports"),
		StateSubsystem->ExportEventLog(EventLogPath));
	FString EventLogContents;
	TestTrue(
		TEXT("Event log is readable"),
		FFileHelper::LoadFileToString(EventLogContents, *EventLogPath));
	TSharedPtr<FJsonObject> EventLog;
	const TSharedRef<TJsonReader<>> EventLogReader =
		TJsonReaderFactory<>::Create(EventLogContents);
	TestTrue(
		TEXT("Event log is valid JSON"),
		FJsonSerializer::Deserialize(EventLogReader, EventLog)
			&& EventLog.IsValid());
	if (!EventLog.IsValid())
	{
		return false;
	}
	const TArray<FString> RequiredRootFields = {
		TEXT("remaining_ap"),
		TEXT("phase_action_points"),
		TEXT("phase"),
		TEXT("day_phase"),
		TEXT("day_phase_started"),
		TEXT("day_window_closed"),
		TEXT("heating"),
		TEXT("ending"),
		TEXT("score"),
		TEXT("player_knowledge"),
		TEXT("disclosed_fact_ids"),
		TEXT("resources"),
		TEXT("related_flags"),
		TEXT("tasks"),
		TEXT("characters"),
		TEXT("requirement_cards")};
	for (const FString& Field : RequiredRootFields)
	{
		TestTrue(
			*FString::Printf(TEXT("Event log contains %s"), *Field),
			EventLog->HasField(Field));
	}

	const TSharedPtr<FJsonObject>* Resources = nullptr;
	const TSharedPtr<FJsonObject>* Flags = nullptr;
	const TSharedPtr<FJsonObject>* Tasks = nullptr;
	const TSharedPtr<FJsonObject>* Characters = nullptr;
	TestTrue(
		TEXT("Resources are structured"),
		EventLog->TryGetObjectField(TEXT("resources"), Resources));
	TestTrue(
		TEXT("Flags are structured"),
		EventLog->TryGetObjectField(TEXT("related_flags"), Flags));
	TestTrue(
		TEXT("Tasks are structured"),
		EventLog->TryGetObjectField(TEXT("tasks"), Tasks));
	TestTrue(
		TEXT("Characters are structured"),
		EventLog->TryGetObjectField(TEXT("characters"), Characters));
	if (!Resources || !Flags || !Tasks || !Characters)
	{
		return false;
	}
	TestTrue(
		TEXT("Every resource field is exported"),
		HasExactKeys(
			*Resources,
			{TEXT("fuel"), TEXT("food"), TEXT("medicine"),
				TEXT("heat_pack"), TEXT("replacement_relay")}));
	TestTrue(
		TEXT("Every task field is exported"),
		HasExactKeys(
			*Tasks,
			{TEXT("generator_progress"), TEXT("antenna_calibration"),
				TEXT("signal_sent"), TEXT("generator_stable")}));
	TestEqual(
		TEXT("Every world flag field is exported"),
		(*Flags)->Values.Num(),
		18);
	const TSet<FString> CharacterFields = {
		TEXT("health"), TEXT("temperature"), TEXT("hunger"),
		TEXT("fatigue"), TEXT("pressure"), TEXT("trust"),
		TEXT("stamina"), TEXT("injury_severity"), TEXT("injury_id"),
		TEXT("injury_worsening_marks"), TEXT("bandage_protection"),
		TEXT("temporary_support_uses"), TEXT("temporary_support_phase"),
		TEXT("location")};
	for (const FString CharacterId : {
			FString(TEXT("player")),
			FString(TEXT("gu_heng")),
			FString(TEXT("ye_cheng"))})
	{
		const TSharedPtr<FJsonObject>* Character = nullptr;
		TestTrue(
			*FString::Printf(TEXT("Character %s is exported"), *CharacterId),
			(*Characters)->TryGetObjectField(CharacterId, Character));
		if (Character)
		{
			TestTrue(
				*FString::Printf(
					TEXT("Character %s has every rule field"),
					*CharacterId),
				HasExactKeys(*Character, CharacterFields));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Cards = nullptr;
	TestTrue(
		TEXT("Requirement cards are an array"),
		EventLog->TryGetArrayField(TEXT("requirement_cards"), Cards));
	FString PreviousRequirementId;
	if (Cards)
	{
		for (const TSharedPtr<FJsonValue>& CardValue : *Cards)
		{
			const TSharedPtr<FJsonObject> Card = CardValue->AsObject();
			TestTrue(
				TEXT("Requirement card schema is stable"),
				HasExactKeys(
					Card,
					{TEXT("action_id"), TEXT("requirement_id"),
						TEXT("met"), TEXT("player_facing_detail")}));
			const FString RequirementId =
				Card->GetStringField(TEXT("requirement_id"));
			TestTrue(
				TEXT("Requirement cards are sorted by id"),
				PreviousRequirementId.IsEmpty()
					|| PreviousRequirementId.Compare(
						RequirementId,
						ESearchCase::CaseSensitive) < 0);
			PreviousRequirementId = RequirementId;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
	TestTrue(
		TEXT("Event list is exported"),
		EventLog->TryGetArrayField(TEXT("events"), Events));
	if (Events && !Events->IsEmpty())
	{
		const TSharedPtr<FJsonObject> Event = Events->Last()->AsObject();
		for (const FString& Field : {
			TEXT("speaker"),
			TEXT("planned_disclosure_fact_ids"),
			TEXT("final_disclosed_fact_ids"),
			TEXT("realized_atom_ids"),
			TEXT("answer_source")})
		{
			TestTrue(
				*FString::Printf(TEXT("Dialogue event contains %s"), *Field),
				Event->HasField(Field));
		}
	}
	return true;
}

#endif
