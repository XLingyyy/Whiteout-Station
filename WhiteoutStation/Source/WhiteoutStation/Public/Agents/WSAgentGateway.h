#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"
#include "UObject/Object.h"
#include "WSAgentGateway.generated.h"

class IHttpRequest;

DECLARE_DELEGATE_OneParam(FWSAgentReplyCallback, const FWSAgentReply&);

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
};
