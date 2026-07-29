#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WhiteoutSettingsSubsystem.generated.h"

class USoundClass;
class USoundMix;

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

	void SetFieldOfView(float Value, UObject* WorldContextObject);
	void SetMasterVolume(float Value, UObject* WorldContextObject);
	void SetAmbienceVolume(float Value, UObject* WorldContextObject);
	void SetEffectsVolume(float Value, UObject* WorldContextObject);
	void SetFeedbackVolume(float Value, UObject* WorldContextObject);
	void SetTextScale(float Value, UObject* WorldContextObject);
	void SetReducedMotionEnabled(bool bEnabled);
	void Apply(UObject* WorldContextObject);

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
