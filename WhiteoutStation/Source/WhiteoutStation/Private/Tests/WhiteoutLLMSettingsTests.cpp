#if WITH_DEV_AUTOMATION_TESTS

#include "Agents/WSAgentGateway.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/WhiteoutSettingsSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutLLMProviderEndpointTest,
	"WhiteoutStation.LLM.ProviderEndpointAllowList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutLLMProviderEndpointTest::RunTest(const FString& Parameters)
{
	const TArray<FWSLLMProviderPreset> Presets = UWSAgentGateway::GetProviderPresets();
	TestEqual(TEXT("All expected provider presets are available"), Presets.Num(), 8);
	for (const FWSLLMProviderPreset& Preset : Presets)
	{
		FString Endpoint;
		FString Reason;
		TestTrue(
			*FString::Printf(TEXT("%s base URL normalizes"), *Preset.ProviderId),
			UWSAgentGateway::NormalizeEndpointForProvider(
				Preset.ProviderId,
				Preset.BaseUrl,
				Endpoint,
				Reason));
		TestTrue(
			*FString::Printf(TEXT("%s normalized URL is allowed"), *Preset.ProviderId),
			UWSAgentGateway::IsAllowedEndpoint(Endpoint));
		TestEqual(
			*FString::Printf(TEXT("%s provider is inferred exactly"), *Preset.ProviderId),
			UWSAgentGateway::ProviderForEndpoint(Endpoint),
			Preset.ProviderId);
		TestTrue(
			*FString::Printf(TEXT("%s has model candidates"), *Preset.ProviderId),
			!Preset.ModelCandidates.IsEmpty());
	}

	const TArray<FString> Rejected = {
		TEXT("https://api.openai.com.evil.invalid/v1/chat/completions"),
		TEXT("https://api.openai.com@evil.invalid/v1/chat/completions"),
		TEXT("https://api.openai.com/v1/chat/completions?target=http://127.0.0.1"),
		TEXT("https://api.openai.com/v1/chat/completions#fragment"),
		TEXT("https://api%2eopenai.com/v1/chat/completions"),
		TEXT("http://api.openai.com/v1/chat/completions"),
		TEXT("https://api.openai.com:444/v1/chat/completions"),
		TEXT("https://api.openai.com/v1/../admin"),
		TEXT("http://127.0.0.1:11434@evil.invalid/v1/chat/completions"),
		TEXT("http://localhost:99999/v1/chat/completions")};
	for (const FString& Candidate : Rejected)
	{
		TestFalse(
			*FString::Printf(TEXT("Malicious endpoint is rejected: %s"), *Candidate),
			UWSAgentGateway::IsAllowedEndpoint(Candidate));
	}

	FString Endpoint;
	FString Reason;
	TestFalse(
		TEXT("Provider and official host cannot be mixed"),
		UWSAgentGateway::NormalizeEndpointForProvider(
			TEXT("openai"),
			TEXT("https://api.deepseek.com"),
			Endpoint,
			Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutLLMRuntimeConfigurationTest,
	"WhiteoutStation.LLM.RuntimeConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutLLMRuntimeConfigurationTest::RunTest(const FString& Parameters)
{
	UWSAgentGateway* Gateway = NewObject<UWSAgentGateway>();
	FString Error;
	TestTrue(
		TEXT("OpenAI runtime configuration accepts a session credential"),
		Gateway->ConfigureRuntime(
			TEXT("openai"),
			TEXT("https://api.openai.com/v1"),
			TEXT("unit-test-credential-redaction-marker"),
			TEXT("gpt-5-mini"),
			true,
			false,
			TEXT("session_ui"),
			Error));
	TestTrue(TEXT("Configured remote provider is live"), Gateway->HasLiveProvider());
	TestEqual(TEXT("Endpoint is normalized"), Gateway->GetEndpoint(), TEXT("https://api.openai.com/v1/chat/completions"));
	TestFalse(
		TEXT("Runtime status never includes the credential"),
		Gateway->GetRuntimeStatus().Contains(TEXT("unit-test-credential-redaction-marker")));

	const FString OpenAIRequest = Gateway->BuildIntentRequestJson(TEXT("发生了什么？"));
	TSharedPtr<FJsonObject> OpenAIRoot;
	const TSharedRef<TJsonReader<>> OpenAIReader = TJsonReaderFactory<>::Create(OpenAIRequest);
	TestTrue(TEXT("OpenAI request serializes"), FJsonSerializer::Deserialize(OpenAIReader, OpenAIRoot));
	TestTrue(TEXT("OpenAI request contains model"), OpenAIRoot.IsValid() && OpenAIRoot->HasField(TEXT("model")));
	TestTrue(TEXT("OpenAI request contains messages"), OpenAIRoot.IsValid() && OpenAIRoot->HasField(TEXT("messages")));
	TestFalse(TEXT("OpenAI request omits DeepSeek thinking"), OpenAIRoot.IsValid() && OpenAIRoot->HasField(TEXT("thinking")));
	TestTrue(
		TEXT("OpenAI request bounds output tokens"),
		OpenAIRoot.IsValid()
			&& OpenAIRoot->GetIntegerField(TEXT("max_completion_tokens")) == 160);
	TestTrue(
		TEXT("OpenAI request fixes sampling temperature"),
		OpenAIRoot.IsValid()
			&& FMath::IsNearlyEqual(
				OpenAIRoot->GetNumberField(TEXT("temperature")),
				0.2));
	const TSharedPtr<FJsonObject>* OpenAIResponseFormat = nullptr;
	TestTrue(
		TEXT("OpenAI request asks for a JSON object"),
		OpenAIRoot.IsValid()
			&& OpenAIRoot->TryGetObjectField(
				TEXT("response_format"),
				OpenAIResponseFormat)
			&& OpenAIResponseFormat
			&& (*OpenAIResponseFormat)->GetStringField(TEXT("type"))
				== TEXT("json_object"));

	TestTrue(
		TEXT("DeepSeek runtime configuration accepts its official base"),
		Gateway->ConfigureRuntime(
			TEXT("deepseek"),
			TEXT("https://api.deepseek.com"),
			TEXT("unit-test-credential-redaction-marker"),
			TEXT("deepseek-v4-flash"),
			true,
			false,
			TEXT("session_ui"),
			Error));
	const FString DeepSeekRequest = Gateway->BuildIntentRequestJson(TEXT("发生了什么？"));
	TSharedPtr<FJsonObject> DeepSeekRoot;
	const TSharedRef<TJsonReader<>> DeepSeekReader = TJsonReaderFactory<>::Create(DeepSeekRequest);
	TestTrue(TEXT("DeepSeek request serializes"), FJsonSerializer::Deserialize(DeepSeekReader, DeepSeekRoot));
	TestTrue(TEXT("DeepSeek request includes thinking control"), DeepSeekRoot.IsValid() && DeepSeekRoot->HasField(TEXT("thinking")));
	TestTrue(
		TEXT("DeepSeek request bounds output tokens"),
		DeepSeekRoot.IsValid()
			&& DeepSeekRoot->GetIntegerField(TEXT("max_tokens")) == 160);
	const TSharedPtr<FJsonObject>* DeepSeekResponseFormat = nullptr;
	TestTrue(
		TEXT("DeepSeek request asks for a JSON object"),
		DeepSeekRoot.IsValid()
			&& DeepSeekRoot->TryGetObjectField(
				TEXT("response_format"),
				DeepSeekResponseFormat)
			&& DeepSeekResponseFormat
			&& (*DeepSeekResponseFormat)->GetStringField(TEXT("type"))
				== TEXT("json_object"));

	TestFalse(
		TEXT("A credential cannot be preserved across providers"),
		Gateway->ConfigureRuntime(
			TEXT("openai"),
			TEXT("https://api.openai.com/v1"),
			FString(),
			TEXT("gpt-5-mini"),
			true,
			true,
			TEXT("none"),
			Error));
	TestFalse(TEXT("Cross-provider preservation fails closed"), Gateway->HasLiveProvider());
	TestEqual(TEXT("Failed configuration clears credential source"), Gateway->GetCredentialSource(), TEXT("none"));
	TestTrue(TEXT("Failed configuration clears credential provider"), Gateway->GetCredentialProviderId().IsEmpty());

	TestTrue(
		TEXT("A new provider accepts its own explicit credential"),
		Gateway->ConfigureRuntime(
			TEXT("openai"),
			TEXT("https://api.openai.com/v1"),
			TEXT("openai-unit-test-credential"),
			TEXT("gpt-5-mini"),
			true,
			false,
			TEXT("session_ui"),
			Error));
	TestEqual(TEXT("Credential is bound to its provider"), Gateway->GetCredentialProviderId(), TEXT("openai"));
	TestFalse(
		TEXT("Invalid endpoint reconfiguration is rejected"),
		Gateway->ConfigureRuntime(
			TEXT("openai"),
			TEXT("https://api.openai.com.evil.invalid/v1"),
			FString(),
			TEXT("gpt-5-mini"),
			true,
			true,
			TEXT("none"),
			Error));
	TestFalse(TEXT("Invalid endpoint reconfiguration disables the previous provider"), Gateway->HasLiveProvider());
	TestEqual(TEXT("Invalid endpoint reconfiguration clears credential source"), Gateway->GetCredentialSource(), TEXT("none"));

	TestTrue(
		TEXT("Loopback runtime can be enabled without a credential"),
		Gateway->ConfigureRuntime(
			TEXT("loopback"),
			TEXT("http://127.0.0.1:18765/v1"),
			FString(),
			TEXT("local-model"),
			true,
			false,
			TEXT("none"),
			Error));
	TestTrue(TEXT("Loopback runtime is live"), Gateway->HasLiveProvider());
	TestFalse(TEXT("Loopback never receives authorization"), UWSAgentGateway::ShouldAttachApiKeyToEndpoint(Gateway->GetEndpoint()));

	TestFalse(
		TEXT("Remote runtime refuses enable without any credential"),
		Gateway->ConfigureRuntime(
			TEXT("openrouter"),
			TEXT("https://openrouter.ai/api/v1"),
			FString(),
			TEXT("openai/gpt-5-mini"),
			true,
			false,
			TEXT("none"),
			Error));
	TestFalse(TEXT("Missing remote credential fails closed"), Gateway->HasLiveProvider());
	TestFalse(
		TEXT("API credentials reject every control character"),
		Gateway->ConfigureRuntime(
			TEXT("openai"),
			TEXT("https://api.openai.com/v1"),
			TEXT("unit-test\tcredential"),
			TEXT("gpt-5-mini"),
			true,
			false,
			TEXT("session_ui"),
			Error));
	TestFalse(TEXT("Invalid credential keeps the provider closed"), Gateway->HasLiveProvider());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWhiteoutLLMSessionCredentialPersistenceTest,
	"WhiteoutStation.LLM.SessionCredentialIsNotPersisted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutLLMSessionCredentialPersistenceTest::RunTest(const FString& Parameters)
{
	const FString OriginalGameUserSettingsIni = GGameUserSettingsIni;
	const FString TemporaryDirectory = FPaths::ProjectSavedDir() / TEXT("Automation/LLMSettings");
	IFileManager::Get().MakeDirectory(*TemporaryDirectory, true);
	const FString TemporaryIni = FPaths::CreateTempFilename(
		*TemporaryDirectory,
		TEXT("GameUserSettings-"),
		TEXT(".ini"));
	GGameUserSettingsIni = TemporaryIni;

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UWhiteoutSettingsSubsystem* Settings =
		NewObject<UWhiteoutSettingsSubsystem>(TestGameInstance);
	FString Error;
	const FString CredentialMarker = TEXT("unit-test-session-only-credential");
	const bool bConfigured = Settings->SetLLMConfiguration(
		TEXT("openai"),
		TEXT("https://api.openai.com/v1"),
		CredentialMarker,
		TEXT("gpt-5-mini"),
		true,
		Error);
	TestTrue(TEXT("Settings accept a session credential"), bConfigured);
	TestEqual(TEXT("Credential remains available in process memory"), Settings->GetSessionLLMApiKey(), CredentialMarker);
	TestEqual(TEXT("Session credential is bound to OpenAI"), Settings->GetSessionLLMApiKeyProviderId(), TEXT("openai"));

	TestTrue(
		TEXT("Provider switch accepts the persisted non-secret fields"),
		Settings->SetLLMConfiguration(
			TEXT("deepseek"),
			TEXT("https://api.deepseek.com"),
			CredentialMarker,
			TEXT("deepseek-v4-flash"),
			false,
			Error));
	TestFalse(
		TEXT("HUD-style blank reuse cannot carry a session credential across providers"),
		Settings->HasSessionLLMApiKey());
	TestTrue(
		TEXT("A distinct credential can be supplied for the new provider"),
		Settings->SetLLMConfiguration(
			TEXT("deepseek"),
			TEXT("https://api.deepseek.com"),
			TEXT("unit-test-deepseek-session-credential"),
			TEXT("deepseek-v4-flash"),
			true,
			Error));
	TestEqual(
		TEXT("Replacement credential is bound to DeepSeek"),
		Settings->GetSessionLLMApiKeyProviderId(),
		TEXT("deepseek"));
	TestFalse(
		TEXT("Invalid settings fail closed"),
		Settings->SetLLMConfiguration(
			TEXT("openai"),
			TEXT("https://api.openai.com.evil.invalid/v1"),
			FString(),
			TEXT("gpt-5-mini"),
			true,
			Error));
	TestFalse(
		TEXT("Invalid settings disable live model use"),
		Settings->IsLLMEnabled());
	TestFalse(
		TEXT("Invalid settings clear the session credential"),
		Settings->HasSessionLLMApiKey());

	FString SavedText;
	FFileHelper::LoadFileToString(SavedText, *TemporaryIni);
	TestFalse(TEXT("Credential value is absent from persisted settings"), SavedText.Contains(CredentialMarker));
	TestFalse(
		TEXT("Replacement credential is absent from persisted settings"),
		SavedText.Contains(TEXT("unit-test-deepseek-session-credential")));
	TestFalse(TEXT("No API key field is persisted"), SavedText.Contains(TEXT("ApiKey"), ESearchCase::IgnoreCase));

	if (GConfig)
	{
		GConfig->UnloadFile(TemporaryIni);
	}
	GGameUserSettingsIni = OriginalGameUserSettingsIni;
	IFileManager::Get().Delete(*TemporaryIni, false, true, true);
	return true;
}

#endif
