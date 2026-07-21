#include "Agents/WSAgentGateway.h"

#include "Agents/WSNPCDecisionService.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "State/WhiteoutRulesEngine.h"

namespace WhiteoutAgentValidation
{
	struct FProtectedPhraseSet
	{
		FName FactId;
		TArray<FString> Phrases;
	};

	bool ContainsProtectedUnauthorizedClaim(
		const FString& Utterance,
		const TArray<FName>& AllowedFactIds,
		FString& OutFactId)
	{
		const TArray<FProtectedPhraseSet> ProtectedFacts = {
			{TEXT("FACT_HEAT_PACK"), {TEXT("保温包"), TEXT("heat pack")}},
			{TEXT("FACT_RELAY_COMPATIBILITY"), {TEXT("规格能对上"), TEXT("继电器能替"), TEXT("compatible relay")}},
			{TEXT("FACT_FORCED_RESTART_SUSPICION"), {TEXT("手动旁路"), TEXT("强制重启"), TEXT("forced restart"), TEXT("manual bypass")}},
			{TEXT("FACT_FORCED_RESTART_CONFIRMED"), {TEXT("越过保护"), TEXT("绕过保护"), TEXT("bypassed protection")}},
			{TEXT("FACT_MEDICAL_DIAGNOSIS"), {TEXT("完整诊断"), TEXT("medical diagnosis")}}
		};
		for (const FProtectedPhraseSet& Protected : ProtectedFacts)
		{
			if (AllowedFactIds.Contains(Protected.FactId))
			{
				continue;
			}
			for (const FString& Phrase : Protected.Phrases)
			{
				if (Utterance.Contains(Phrase, ESearchCase::IgnoreCase))
				{
					OutFactId = Protected.FactId.ToString();
					return true;
				}
			}
		}
		return false;
	}
}

void UWSAgentGateway::Initialize()
{
	LoadConfig();
}

bool UWSAgentGateway::HasLiveProvider() const
{
	return bLLMEnabled && !Endpoint.IsEmpty() && (!bRequiresApiKey || !ApiKey.IsEmpty());
}

void UWSAgentGateway::RequestExpression(
	const FName ActionId,
	const FWSGameState& State,
	const bool bAllowLiveProvider,
	FWSAgentReplyCallback Completion)
{
	const FWSAgentReply Decision = UWSNPCDecisionService::BuildDeterministicReply(ActionId, State);
	const TArray<FName> AllowedFacts = UWSNPCDecisionService::BuildAllowedFacts(ActionId, Decision.Speaker, State);
	if (!bAllowLiveProvider || !HasLiveProvider())
	{
		Completion.ExecuteIfBound(Decision);
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Endpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
	if (!ApiKey.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	}
	Request->SetTimeout(TimeoutSeconds);
	Request->SetContentAsString(BuildRequestJson(Decision, AllowedFacts, State));
	Request->OnProcessRequestComplete().BindLambda(
		[Decision, AllowedFacts, Completion](FHttpRequestPtr, FHttpResponsePtr Response, const bool bSucceeded)
		{
			FWSAgentReply Reply;
			FString Reason;
			if (bSucceeded && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode())
				&& ValidateModelPayload(Response->GetContentAsString(), Decision, AllowedFacts, Reply, Reason))
			{
				Completion.ExecuteIfBound(Reply);
				return;
			}
			FWSAgentReply Fallback = Decision;
			Fallback.ValidationReason = Reason.IsEmpty() ? TEXT("provider_unavailable") : Reason;
			Completion.ExecuteIfBound(Fallback);
		});
	if (!Request->ProcessRequest())
	{
		FWSAgentReply Fallback = Decision;
		Fallback.ValidationReason = TEXT("request_not_started");
		Completion.ExecuteIfBound(Fallback);
	}
}

bool UWSAgentGateway::ValidateModelPayload(
	const FString& Payload,
	const FWSAgentReply& Decision,
	const TArray<FName>& AllowedFactIds,
	FWSAgentReply& OutReply,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutReason = TEXT("invalid_json");
		return false;
	}
	if (Root->HasField(TEXT("state_changes")) || Root->HasField(TEXT("rule_changes"))
		|| Root->HasField(TEXT("ap_delta")) || Root->HasField(TEXT("resource_delta")))
	{
		OutReason = TEXT("model_attempted_rule_change");
		return false;
	}

	FString Utterance;
	FString Emotion;
	FString ResponseType;
	if (!Root->TryGetStringField(TEXT("utterance"), Utterance)
		|| !Root->TryGetStringField(TEXT("emotion"), Emotion)
		|| !Root->TryGetStringField(TEXT("response_type"), ResponseType))
	{
		OutReason = TEXT("missing_required_field");
		return false;
	}
	const FString ExpectedResponse = StaticEnum<EWSResponseType>()->GetNameStringByValue(static_cast<int64>(Decision.ResponseType));
	if (!ResponseType.Equals(ExpectedResponse, ESearchCase::IgnoreCase))
	{
		OutReason = TEXT("decision_mismatch");
		return false;
	}

	TArray<FName> ReferencedFacts;
	const TArray<TSharedPtr<FJsonValue>>* FactValues = nullptr;
	if (Root->TryGetArrayField(TEXT("referenced_fact_ids"), FactValues) && FactValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *FactValues)
		{
			FString FactString;
			if (!Value.IsValid() || !Value->TryGetString(FactString))
			{
				OutReason = TEXT("invalid_fact_list");
				return false;
			}
			ReferencedFacts.Add(FName(FactString));
		}
	}

	if (!FWhiteoutRulesEngine::ValidateAgentResponse(Utterance, ReferencedFacts, AllowedFactIds, false, OutReason))
	{
		return false;
	}
	FString UnauthorizedFactId;
	if (WhiteoutAgentValidation::ContainsProtectedUnauthorizedClaim(Utterance, AllowedFactIds, UnauthorizedFactId))
	{
		OutReason = FString::Printf(TEXT("semantic_fact_permission_violation:%s"), *UnauthorizedFactId);
		return false;
	}
	for (const FName FactId : AllowedFactIds)
	{
		if (Utterance.Contains(FactId.ToString(), ESearchCase::IgnoreCase) && !ReferencedFacts.Contains(FactId))
		{
			OutReason = TEXT("uncited_fact_reference");
			return false;
		}
	}

	OutReply = Decision;
	OutReply.Utterance = Utterance.TrimStartAndEnd();
	OutReply.Emotion = Emotion.Left(32);
	OutReply.ReferencedFactIds = ReferencedFacts;
	OutReply.bFallback = false;
	OutReply.Provider = TEXT("shared-model");
	OutReply.ValidationReason = TEXT("ok");
	OutReason = TEXT("ok");
	return true;
}

void UWSAgentGateway::LoadConfig()
{
	Endpoint = TEXT("https://api.deepseek.com/chat/completions");
	ProviderName = TEXT("deepseek");
	ModelName = TEXT("deepseek-v4-flash");
	bLLMEnabled = false;

	FString JsonText;
	const FString ConfigPath = FPaths::ProjectContentDir() / TEXT("Agents/AgentRuntime.v0.1.json");
	if (FFileHelper::LoadFileToString(JsonText, *ConfigPath))
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			Root->TryGetStringField(TEXT("endpoint"), Endpoint);
			Root->TryGetStringField(TEXT("provider_name"), ProviderName);
			Root->TryGetStringField(TEXT("model"), ModelName);
			Root->TryGetBoolField(TEXT("llm_enabled"), bLLMEnabled);
			double ConfigTimeout = TimeoutSeconds;
			if (Root->TryGetNumberField(TEXT("timeout_seconds"), ConfigTimeout))
			{
				TimeoutSeconds = FMath::Clamp(static_cast<float>(ConfigTimeout), 1.0f, 10.0f);
			}
		}
	}

	const FString LocalConfigPath = FPaths::ProjectDir() / TEXT("LocalConfig/WhiteoutLLM.ini");
	FConfigFile LocalConfig;
	LocalConfig.Read(LocalConfigPath);
	if (LocalConfig.Num() > 0)
	{
		FString LocalEnabled;
		if (LocalConfig.GetString(TEXT("WhiteoutLLM"), TEXT("Enabled"), LocalEnabled))
		{
			bLLMEnabled = LocalEnabled.ToBool();
		}
		LocalConfig.GetString(TEXT("WhiteoutLLM"), TEXT("Endpoint"), Endpoint);
		LocalConfig.GetString(TEXT("WhiteoutLLM"), TEXT("Model"), ModelName);
		FString LocalTimeout;
		if (LocalConfig.GetString(TEXT("WhiteoutLLM"), TEXT("TimeoutSeconds"), LocalTimeout))
		{
			TimeoutSeconds = FMath::Clamp(FCString::Atof(*LocalTimeout), 1.0f, 15.0f);
		}
		if (LocalConfig.GetString(TEXT("WhiteoutLLM"), TEXT("ApiKey"), ApiKey))
		{
			ApiKey = ApiKey.TrimStartAndEnd();
			CredentialSource = ApiKey.IsEmpty() ? TEXT("none") : TEXT("local_ini");
		}
	}

	const FString EnvironmentKey = FPlatformMisc::GetEnvironmentVariable(TEXT("WHITEOUT_LLM_API_KEY")).TrimStartAndEnd();
	if (!EnvironmentKey.IsEmpty())
	{
		ApiKey = EnvironmentKey;
		CredentialSource = TEXT("environment");
	}
	const FString EnvironmentEnabled = FPlatformMisc::GetEnvironmentVariable(TEXT("WHITEOUT_LLM_ENABLED")).TrimStartAndEnd();
	if (!EnvironmentEnabled.IsEmpty())
	{
		bLLMEnabled = EnvironmentEnabled.ToBool();
	}

	FString CommandLineEndpoint;
	if (FParse::Value(FCommandLine::Get(), TEXT("WhiteoutAgentEndpoint="), CommandLineEndpoint))
	{
		Endpoint = CommandLineEndpoint.TrimStartAndEnd();
		ProviderName = TEXT("command-line-provider");
	}
	FString CommandLineEnabled;
	if (FParse::Value(FCommandLine::Get(), TEXT("WhiteoutLLMEnabled="), CommandLineEnabled))
	{
		bLLMEnabled = CommandLineEnabled.ToBool();
	}
	bRequiresApiKey = Endpoint.StartsWith(TEXT("https://api.deepseek.com"), ESearchCase::IgnoreCase);
	if (!bLLMEnabled || Endpoint.IsEmpty() || (bRequiresApiKey && ApiKey.IsEmpty()))
	{
		ProviderName = TEXT("preset");
	}
}

FString UWSAgentGateway::BuildRequestJson(
	const FWSAgentReply& Decision,
	const TArray<FName>& AllowedFactIds,
	const FWSGameState& State) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), ModelName);
	Root->SetStringField(TEXT("instruction"), TEXT("Rewrite only the utterance. Do not change decisions, rules, AP, resources, facts, or outcomes."));
	Root->SetStringField(TEXT("speaker"), UWSNPCDecisionService::SpeakerLabel(Decision.Speaker));
	Root->SetStringField(TEXT("action_id"), Decision.ActionId.ToString());
	Root->SetStringField(TEXT("response_type"), StaticEnum<EWSResponseType>()->GetNameStringByValue(static_cast<int64>(Decision.ResponseType)));
	Root->SetStringField(TEXT("emotion"), Decision.Emotion);
	Root->SetStringField(TEXT("preset_utterance"), Decision.Utterance);
	Root->SetNumberField(TEXT("remaining_ap"), State.ActionPoints);
	Root->SetNumberField(TEXT("max_characters"), 240);

	TArray<TSharedPtr<FJsonValue>> Facts;
	for (const FName FactId : AllowedFactIds)
	{
		Facts.Add(MakeShared<FJsonValueString>(FactId.ToString()));
	}
	Root->SetArrayField(TEXT("allowed_fact_ids"), Facts);
	Root->SetArrayField(TEXT("referenced_fact_ids"), Facts);

	TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
	Schema->SetStringField(TEXT("utterance"), TEXT("string"));
	Schema->SetStringField(TEXT("emotion"), TEXT("string"));
	Schema->SetStringField(TEXT("response_type"), TEXT("must equal input response_type"));
	Schema->SetStringField(TEXT("referenced_fact_ids"), TEXT("array containing only allowed_fact_ids"));
	Root->SetObjectField(TEXT("output_schema"), Schema);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	return Json;
}
