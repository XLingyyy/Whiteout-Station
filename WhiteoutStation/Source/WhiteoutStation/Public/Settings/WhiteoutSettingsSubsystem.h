#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WhiteoutSettingsSubsystem.generated.h"

class USoundClass;
class USoundMix;

DECLARE_MULTICAST_DELEGATE(FWSLLMSettingsChanged);

/** Local-only presentation settings stored in the platform GameUserSettings ini. */
UCLASS()
class WHITEOUTSTATION_API UWhiteoutSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	float GetFieldOfView() const { return FieldOfView; }
	float GetMasterVolume() const { return MasterVolume; }
	float GetAmbienceVolume() const { return AmbienceVolume; }
	float GetEffectsVolume() const { return EffectsVolume; }
	float GetFeedbackVolume() const { return FeedbackVolume; }
	float GetTextScale() const { return TextScale; }
	bool IsReducedMotionEnabled() const { return bReducedMotion; }
	const FString& GetLLMProviderId() const { return LLMProviderId; }
	const FString& GetLLMBaseUrl() const { return LLMBaseUrl; }
	const FString& GetLLMModelId() const { return LLMModelId; }
	const FString& GetSessionLLMApiKey() const { return SessionLLMApiKey; }
	const FString& GetSessionLLMApiKeyProviderId() const { return SessionLLMApiKeyProviderId; }
	bool IsLLMEnabled() const { return bLLMEnabled; }
	bool HasSessionLLMApiKey() const
	{
		return !SessionLLMApiKey.IsEmpty()
			&& SessionLLMApiKeyProviderId == LLMProviderId;
	}
	bool HasSavedLLMConfiguration() const { return bHasSavedLLMConfiguration; }

	void SetFieldOfView(float Value, UObject* WorldContextObject);
	void SetMasterVolume(float Value, UObject* WorldContextObject);
	void SetAmbienceVolume(float Value, UObject* WorldContextObject);
	void SetEffectsVolume(float Value, UObject* WorldContextObject);
	void SetFeedbackVolume(float Value, UObject* WorldContextObject);
	void SetTextScale(float Value, UObject* WorldContextObject);
	void SetReducedMotionEnabled(bool bEnabled);
	bool SetLLMConfiguration(
		const FString& ProviderId,
		const FString& BaseUrl,
		const FString& ApiKey,
		const FString& ModelId,
		bool bEnabled,
		FString& OutError);
	void Apply(UObject* WorldContextObject);

	FWSLLMSettingsChanged OnLLMSettingsChanged;

private:
	static constexpr float MinFieldOfView = 75.0f;
	static constexpr float MaxFieldOfView = 105.0f;

	float FieldOfView = 90.0f;
	float MasterVolume = 1.0f;
	float AmbienceVolume = 1.0f;
	float EffectsVolume = 1.0f;
	float FeedbackVolume = 1.0f;
	float TextScale = 1.0f;
	bool bReducedMotion = false;
	FString LLMProviderId = TEXT("deepseek");
	FString LLMBaseUrl = TEXT("https://api.deepseek.com");
	FString LLMModelId = TEXT("deepseek-v4-flash");
	FString SessionLLMApiKey;
	FString SessionLLMApiKeyProviderId;
	bool bLLMEnabled = false;
	bool bHasSavedLLMConfiguration = false;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> RuntimeSoundMix;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> AmbienceSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> FoleySoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> UISoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> CinematicSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MusicSoundClass;

	TWeakObjectPtr<UWorld> AppliedWorld;

	void Load();
	void Save() const;
	void LoadAudioAssets();
};
