#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"
#include "UObject/Object.h"
#include "WSAgentGateway.generated.h"

class IHttpRequest;
namespace FHttpRetrySystem
{
	class FManager;
}

DECLARE_DELEGATE_OneParam(FWSAgentReplyCallback, const FWSAgentReply&);

USTRUCT(BlueprintType)
struct FWSDialogueIntentResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bMapped = false;

	UPROPERTY(BlueprintReadOnly)
	EWSDialogueAct DialogueAct = EWSDialogueAct::Ask;

	UPROPERTY(BlueprintReadOnly)
	FName PromiseCondition;

	UPROPERTY(BlueprintReadOnly)
	float Confidence = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FString Source = TEXT("wheel_only");

	UPROPERTY(BlueprintReadOnly)
	FString Reason;
};

DECLARE_DELEGATE_OneParam(FWSDialogueIntentCallback, const FWSDialogueIntentResult&);

struct FWSAgentDialogueTurn
{
	FString UserContextJson;
	FString AssistantPayloadJson;
};

UCLASS()
class WHITEOUTSTATION_API UWSAgentGateway : public UObject
{
	GENERATED_BODY()

public:
	void Initialize();
	void ResetSession();
	virtual void BeginDestroy() override;
	bool HasLiveProvider() const;
	void RequestExpression(
		const FWSActionRequest& ActionRequest,
		const FWSGameState& State,
		bool bAllowLiveProvider,
		FWSAgentReplyCallback Completion);
	void RequestExpression(
		FName ActionId,
		const FWSGameState& State,
		bool bAllowLiveProvider,
		FWSAgentReplyCallback Completion,
		const FString& PlayerSaid = FString());
	void RequestDialogueIntent(
		const FString& UserText,
		bool bAllowLiveProvider,
		FWSDialogueIntentCallback Completion);

	static FWSDialogueIntentResult ClassifyLocalIntent(const FString& UserText);
	static bool ContainsAdversarialInstruction(const FString& UserText);
	static bool IsOfficialDeepSeekEndpoint(const FString& CandidateEndpoint);
	static bool IsLoopbackEndpoint(const FString& CandidateEndpoint);
	static bool IsAllowedEndpoint(const FString& CandidateEndpoint);
	static bool ShouldAttachApiKeyToEndpoint(const FString& CandidateEndpoint);
	static bool ValidateIntentPayload(
		const FString& Payload,
		const FString& UserText,
		FWSDialogueIntentResult& OutIntent,
		FString& OutReason);
	static bool ExtractProviderContent(const FString& ProviderPayload, FString& OutContent, FString& OutReason);
	static bool ExtractProviderContent(
		const FString& ProviderPayload,
		FString& OutContent,
		FString& OutFinishReason,
		FString& OutReason);

	static bool ValidateModelPayload(
		const FString& Payload,
		const FWSAgentReply& Decision,
		const TArray<FName>& AllowedFactIds,
		FWSAgentReply& OutReply,
		FString& OutReason);

	FString GetProviderName() const { return ProviderName; }
	FString GetModelName() const { return ModelName; }
	FString GetCredentialSource() const { return CredentialSource; }

private:
	FString Endpoint;
	FString ApiKey;
	FString CredentialSource = TEXT("none");
	FString ProviderName = TEXT("preset");
	FString ModelName = TEXT("deepseek-v4-flash");
	float TimeoutSeconds = 4.0f;
	bool bLLMEnabled = false;
	bool bRequiresApiKey = false;
	uint64 SessionGeneration = 1;

	TSharedPtr<FHttpRetrySystem::FManager, ESPMode::ThreadSafe> RetryManager;
	TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> ActiveRequests;
	TMap<FGuid, TArray<FWSAgentDialogueTurn>> DialogueHistory;
	FCriticalSection AuditMutex;

	void LoadConfig();
	FString BuildExpressionContextJson(
		const FWSAgentReply& Decision,
		const TArray<FName>& AllowedFactIds,
		const FWSGameState& State,
		const FWSActionRequest& ActionRequest) const;
	FString BuildRequestJson(
		const FWSAgentReply& Decision,
		const TArray<FName>& AllowedFactIds,
		const FWSGameState& State,
		const FWSActionRequest& ActionRequest) const;
	FString BuildIntentRequestJson(const FString& UserText) const;
	static FString BuildHistoryAssistantJson(const FWSAgentReply& Reply);
	void RecordDialogueTurn(
		const FWSActionRequest& ActionRequest,
		const FString& UserContextJson,
		const FString& AssistantPayloadJson);
	static bool HasPromiseKeyword(const FString& UserText, FName PromiseCondition);
	static FString IntentResultJson(const FWSDialogueIntentResult& Intent);
	void UntrackRequest(const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request);
	void AppendAuditRecord(
		const FString& Kind,
		const FString& Provider,
		const FGuid& RequestId,
		FName ActionId,
		EWSDialogueAct DialogueAct,
		int32 HttpStatus,
		const FString& FinishReason,
		int64 RequestBytes,
		int64 ResponseBytes,
		double ElapsedMilliseconds,
		const FString& Outcome,
		int32 PromptTokens = -1,
		int32 CompletionTokens = -1);
};
