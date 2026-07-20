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

private:
	FString Endpoint;
	FString ProviderName = TEXT("preset");
	FString ModelName = TEXT("shared-dialogue");
	float TimeoutSeconds = 4.0f;

	void LoadConfig();
	static FString BuildRequestJson(const FWSAgentReply& Decision, const TArray<FName>& AllowedFactIds, const FWSGameState& State);
};
