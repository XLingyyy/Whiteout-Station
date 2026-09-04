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

DECLARE_DELEGATE_OneParam(FWSDialogueOutcomeCallback, const FWSDialogueOutcome&);

DECLARE_DELEGATE_OneParam(FWSDialogueIntentCallback, const FWSDialogueIntentResult&);

USTRUCT(BlueprintType)
struct FWSLLMProviderPreset
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString ProviderId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FString BaseUrl;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> ModelCandidates;

	UPROPERTY(BlueprintReadOnly)
	bool bRequiresApiKey = true;
};

struct FWSAgentDialogueTurn
{
	FString UserSemanticSummaryJson;
	FString AssistantSemanticSummaryJson;
	FString RawPlayerLine;
	FString RawNpcLine;
	FName TopicActionId;
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
	bool ConfigureRuntime(
		const FString& InProviderId,
		const FString& InBaseUrlOrEndpoint,
		const FString& InApiKey,
		const FString& InModelId,
		bool bInEnabled,
		bool bPreserveExistingCredentialIfEmpty,
		const FString& InCredentialSource,
		FString& OutError);
	void RequestExpression(
		const FWSActionRequest& ActionRequest,
		const FWSGameState& State,
		bool bAllowLiveProvider,
		FWSAgentReplyCallback Completion);
	void RequestExpression(
		const FWSActionRequest& ActionRequest,
		const FWSGameState& State,
		const FWSActionRequirementReport& RequirementReport,
		bool bAllowLiveProvider,
		FWSAgentReplyCallback Completion);
	void RequestExpression(
		FName ActionId,
		const FWSGameState& State,
		bool bAllowLiveProvider,
		FWSAgentReplyCallback Completion,
		const FString& PlayerSaid = FString());
	void RequestDialogueRealization(
		const FWSPreparedDialogue& Prepared,
		bool bAllowLiveProvider,
		FWSDialogueOutcomeCallback Completion);
	void RequestDialogueIntent(
		const FString& UserText,
		bool bAllowLiveProvider,
		FWSDialogueIntentCallback Completion);
	void RecordCommittedDialogueTurn(
		const FWSActionRequest& ActionRequest,
		const FWSAgentReply& Reply);
	void RequestDialogueIntent(
		const FString& UserText,
		FName CurrentDialogueActionId,
		FName CurrentTopicActionId,
		bool bAllowLiveProvider,
		FWSDialogueIntentCallback Completion);

	static FWSDialogueIntentResult ClassifyLocalIntent(
		const FString& UserText,
		FName CurrentDialogueActionId = NAME_None,
		FName CurrentTopicActionId = NAME_None);
	static bool ContainsAdversarialInstruction(const FString& UserText);
	static bool IsOfficialDeepSeekEndpoint(const FString& CandidateEndpoint);
	static bool IsLoopbackEndpoint(const FString& CandidateEndpoint);
	static bool IsAllowedEndpoint(const FString& CandidateEndpoint);
	static bool ShouldAttachApiKeyToEndpoint(const FString& CandidateEndpoint);
	static bool NormalizeEndpointForProvider(
		const FString& ProviderId,
		const FString& CandidateBaseUrlOrEndpoint,
		FString& OutEndpoint,
		FString& OutReason);
	static FString ProviderForEndpoint(const FString& CandidateEndpoint);
	static TArray<FWSLLMProviderPreset> GetProviderPresets();
	static TArray<FString> GetModelCandidates(const FString& ProviderId);
	static bool ValidateIntentPayload(
		const FString& Payload,
		const FString& UserText,
		FWSDialogueIntentResult& OutIntent,
		FString& OutReason,
		FName CurrentDialogueActionId = NAME_None,
		FName CurrentTopicActionId = NAME_None);
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
	static bool ValidateDialogueOutcomePayload(
		const FString& Payload,
		const FWSPreparedDialogue& Prepared,
		FWSDialogueOutcome& OutOutcome,
		FString& OutReason);
	static bool ValidateDialogueOutcome(
		const FWSPreparedDialogue& Prepared,
		const FWSDialogueOutcome& Outcome,
		FString& OutReason);
	static bool IsExpressionKnowledgeBoundaryOpen(
		EWSCharacterId Speaker,
		const TArray<FName>& AllowedFactIds);

	FString GetProviderName() const { return ProviderName; }
	FString GetModelName() const { return ModelName; }
	FString GetCredentialSource() const { return CredentialSource; }
	FString GetCredentialProviderId() const { return CredentialProviderId; }
	FString GetEndpoint() const { return Endpoint; }
	FString GetRuntimeStatus() const;
	FString BuildIntentRequestJson(
		const FString& UserText,
		FName CurrentDialogueActionId = NAME_None,
		FName CurrentTopicActionId = NAME_None) const;

private:
	FString Endpoint;
	FString ApiKey;
	FString CredentialSource = TEXT("none");
	FString CredentialProviderId;
	FString ProviderName = TEXT("preset");
	FString ModelName = TEXT("deepseek-v4-flash");
	float TimeoutSeconds = 4.0f;
	bool bLLMEnabled = false;
	bool bRequiresApiKey = false;
	bool bRuntimeContractValid = true;
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
	FString BuildDialogueRealizationContextJson(
		const FWSPreparedDialogue& Prepared) const;
	FString BuildDialogueRealizationRequestJson(
		const FWSPreparedDialogue& Prepared) const;
	static FString BuildHistoryAssistantJson(const FWSAgentReply& Reply);
	static FString BuildHistoryUserJson(
		const FWSActionRequest& ActionRequest,
		bool bTopicChanged);
	void RecordDialogueTurn(
		const FWSActionRequest& ActionRequest,
		const FWSAgentReply& Reply);
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
		int32 CompletionTokens = -1,
		const FString& SemanticSource = FString(),
		EWSDialogueQueryType QueryType = EWSDialogueQueryType::Unknown,
		FName TargetActionId = NAME_None,
		const FString& SpineSha256 = FString(),
		const FString& TailOutcome = FString(),
		const FString& FinalAnswerSource = FString());
};
