#include "State/WindStationStateSubsystem.h"

#include "Actions/WSActionResolver.h"
#include "Dom/JsonObject.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Save/WindStationSaveGame.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

const FString UWindStationStateSubsystem::SaveSlot(TEXT("WhiteoutStation_Autosave_v0_1"));

void UWindStationStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Error;
	const FString ConfigPath = FPaths::ProjectContentDir() / TEXT("Rules/WhiteoutStationRules.v0.1.json");
	if (!RulesEngine.LoadConfig(ConfigPath, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Whiteout rules config fallback: %s"), *Error);
	}
	ActionResolver = NewObject<UWSActionResolver>(this);
	ActionResolver->Initialize(this);
}

void UWindStationStateSubsystem::Deinitialize()
{
	ActionResolver = nullptr;
	Super::Deinitialize();
}

void UWindStationStateSubsystem::NewGame()
{
	RulesEngine.Reset();
	BroadcastState();
}

FWSGameState UWindStationStateSubsystem::GetStateSnapshot() const
{
	return RulesEngine.GetState();
}

FWSActionPreview UWindStationStateSubsystem::PreviewAction(const FWSActionRequest& Request) const
{
	return RulesEngine.Preview(Request);
}

FWSActionResult UWindStationStateSubsystem::CommitAction(const FWSActionRequest& Request)
{
	FWSActionResult Result = RulesEngine.Commit(Request);
	if (Result.bCommitted)
	{
		SaveSnapshot();
		OnActionCommitted.Broadcast(Result);
		BroadcastState();
	}
	return Result;
}

FWSGameState UWindStationStateSubsystem::EndGame()
{
	RulesEngine.EndGame();
	SaveSnapshot();
	BroadcastState();
	return RulesEngine.GetState();
}

bool UWindStationStateSubsystem::SaveSnapshot()
{
	UWindStationSaveGame* Save = Cast<UWindStationSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UWindStationSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}
	Save->State = RulesEngine.GetState();
	return UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0);
}

bool UWindStationStateSubsystem::LoadSnapshot()
{
	UWindStationSaveGame* Save = Cast<UWindStationSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	if (!Save || Save->SaveVersion != TEXT("0.1.0"))
	{
		return false;
	}
	RulesEngine.SetState(Save->State);
	BroadcastState();
	return true;
}

bool UWindStationStateSubsystem::HasSnapshot() const
{
	return UGameplayStatics::DoesSaveGameExist(SaveSlot, 0);
}

bool UWindStationStateSubsystem::ExportEventLog(FString& OutFilePath) const
{
	TArray<TSharedPtr<FJsonValue>> Events;
	for (const FWSEventRecord& Event : RulesEngine.GetState().EventLog)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("index"), Event.Index);
		Object->SetStringField(TEXT("action_id"), Event.ActionId.ToString());
		Object->SetStringField(TEXT("transaction_id"), Event.TransactionId.ToString());
		Object->SetNumberField(TEXT("ap_before"), Event.APBefore);
		Object->SetNumberField(TEXT("ap_after"), Event.APAfter);
		Object->SetStringField(TEXT("reason_code"), StaticEnum<EWSReasonCode>()->GetNameStringByValue(static_cast<int64>(Event.ReasonCode)));
		Object->SetBoolField(TEXT("crisis_triggered"), Event.bCrisisTriggered);
		TArray<TSharedPtr<FJsonValue>> Changes;
		for (const FString& Change : Event.Changes)
		{
			Changes.Add(MakeShared<FJsonValueString>(Change));
		}
		Object->SetArrayField(TEXT("changes"), Changes);
		Events.Add(MakeShared<FJsonValueObject>(Object));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("rules_version"), TEXT("0.1.0"));
	Root->SetArrayField(TEXT("events"), Events);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}

	OutFilePath = FPaths::ProjectSavedDir() / TEXT("Logs/WhiteoutStation_EventLog.json");
	return FFileHelper::SaveStringToFile(Json, *OutFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UWindStationStateSubsystem::BroadcastState()
{
	OnStateChanged.Broadcast(RulesEngine.GetState());
}
