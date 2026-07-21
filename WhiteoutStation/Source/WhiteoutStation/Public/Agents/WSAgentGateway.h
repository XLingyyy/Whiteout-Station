#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"
#include "UObject/Object.h"
#include "WSAgentGateway.generated.h"

class IHttpRequest;

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

UCLASS()
class WHITEOUTSTATION_API UWSAgentGateway : public UObject
{
	GENERATED_BODY()

public:
	void Initialize();
	bool HasLiveProvider() const;
	void RequestExpression(
		FName ActionId,
		const FWSGameState& State,
		bool bAllowLiveProvider,
		FWSAgentReplyCallback Completion);
	void RequestDialogueIntent(
		const FString& UserText,
		bool bAllowLiveProvider,
		FWSDialogueIntentCallback Completion);

	static FWSDialogueIntentResult ClassifyLocalIntent(const FString& UserText);
	static bool ValidateIntentPayload(
		const FString& Payload,
		const FString& UserText,
		FWSDialogueIntentResult& OutIntent,
		FString& OutReason);
	static bool ExtractProviderContent(const FString& ProviderPayload, FString& OutContent, FString& OutReason);
	static void ResetSessionModelBudget(int32 AlreadyUsed = 0);
	static int32 GetSessionModelCalls();

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

	void LoadConfig();
	FString BuildRequestJson(const FWSAgentReply& Decision, const TArray<FName>& AllowedFactIds, const FWSGameState& State) const;
	FString BuildIntentRequestJson(const FString& UserText) const;
	static bool TryConsumeSessionModelCall();
	static bool HasPromiseKeyword(const FString& UserText, FName PromiseCondition);
	static bool ContainsAdversarialInstruction(const FString& UserText);
	static FString IntentResultJson(const FWSDialogueIntentResult& Intent);
	static void AppendAuditRecord(
		const FString& Kind,
		const FString& Provider,
		const FString& RequestPayload,
		const FString& ResponsePayload,
		const FString& Outcome);
};
