#include "Settings/WhiteoutSettingsSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Player/WhiteoutCharacter.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
	const TCHAR* SettingsSection = TEXT("WhiteoutStation.LocalSettings");

	float LoadClampedSetting(const TCHAR* Key, const float DefaultValue, const float Minimum, const float Maximum)
	{
		float Value = DefaultValue;
		if (GConfig)
		{
			GConfig->GetFloat(SettingsSection, Key, Value, GGameUserSettingsIni);
		}
		return FMath::Clamp(Value, Minimum, Maximum);
	}

	bool LoadBoolSetting(const TCHAR* Key, const bool bDefaultValue)
	{
		bool bValue = bDefaultValue;
		if (GConfig)
		{
			GConfig->GetBool(SettingsSection, Key, bValue, GGameUserSettingsIni);
		}
		return bValue;
	}

	USoundClass* LoadSoundClass(const TCHAR* Path)
	{
		USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, Path);
		if (!SoundClass)
		{
			UE_LOG(LogTemp, Error, TEXT("WhiteoutStation v0.3: missing settings SoundClass %s"), Path);
		}
		return SoundClass;
	}
}

void UWhiteoutSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
	LoadAudioAssets();
}

void UWhiteoutSettingsSubsystem::Load()
{
	FieldOfView = LoadClampedSetting(TEXT("FieldOfView"), 90.0f, MinFieldOfView, MaxFieldOfView);
	MasterVolume = LoadClampedSetting(TEXT("MasterVolume"), 1.0f, 0.0f, 1.0f);
	AmbienceVolume = LoadClampedSetting(TEXT("AmbienceVolume"), 1.0f, 0.0f, 1.0f);
	EffectsVolume = LoadClampedSetting(TEXT("EffectsVolume"), 1.0f, 0.0f, 1.0f);
	FeedbackVolume = LoadClampedSetting(TEXT("FeedbackVolume"), 1.0f, 0.0f, 1.0f);
	TextScale = LoadClampedSetting(TEXT("TextScale"), 1.0f, 0.9f, 1.2f);
	bReducedMotion = LoadBoolSetting(TEXT("ReducedMotion"), false);
}

void UWhiteoutSettingsSubsystem::Save() const
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetFloat(SettingsSection, TEXT("FieldOfView"), FieldOfView, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("AmbienceVolume"), AmbienceVolume, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("EffectsVolume"), EffectsVolume, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("FeedbackVolume"), FeedbackVolume, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("TextScale"), TextScale, GGameUserSettingsIni);
	GConfig->SetBool(SettingsSection, TEXT("ReducedMotion"), bReducedMotion, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UWhiteoutSettingsSubsystem::LoadAudioAssets()
{
	MasterSoundClass = LoadSoundClass(TEXT("/Game/WindStation/Audio/Mix/SC_WS_Master.SC_WS_Master"));
	AmbienceSoundClass = LoadSoundClass(TEXT("/Game/WindStation/Audio/Mix/SC_WS_Ambience.SC_WS_Ambience"));
	FoleySoundClass = LoadSoundClass(TEXT("/Game/WindStation/Audio/Mix/SC_WS_Foley.SC_WS_Foley"));
	UISoundClass = LoadSoundClass(TEXT("/Game/WindStation/Audio/Mix/SC_WS_UI.SC_WS_UI"));
	CinematicSoundClass = LoadSoundClass(TEXT("/Game/WindStation/Audio/Mix/SC_WS_Cinematic.SC_WS_Cinematic"));
	MusicSoundClass = LoadSoundClass(TEXT("/Game/WindStation/Audio/Mix/SC_WS_Music.SC_WS_Music"));
	RuntimeSoundMix = NewObject<USoundMix>(this, TEXT("WSRuntimeLocalSettingsMix"));
}

void UWhiteoutSettingsSubsystem::Apply(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return;
	}
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetApplicationScale(TextScale);
	}
	if (AWhiteoutCharacter* Character = Cast<AWhiteoutCharacter>(UGameplayStatics::GetPlayerCharacter(WorldContextObject, 0)))
	{
		if (Character->FirstPersonCamera)
		{
			Character->FirstPersonCamera->SetFieldOfView(FieldOfView);
		}
	}
	if (!RuntimeSoundMix)
	{
		LoadAudioAssets();
	}
	if (!RuntimeSoundMix)
	{
		return;
	}
	if (AppliedWorld.Get() != World)
	{
		UGameplayStatics::PushSoundMixModifier(WorldContextObject, RuntimeSoundMix);
		AppliedWorld = World;
	}
	auto OverrideClass = [WorldContextObject, this](USoundClass* SoundClass, const float Volume, const bool bApplyToChildren)
	{
		if (SoundClass)
		{
			UGameplayStatics::SetSoundMixClassOverride(
				WorldContextObject, RuntimeSoundMix, SoundClass, Volume, 1.0f, 0.0f, bApplyToChildren);
		}
	};
	OverrideClass(MasterSoundClass, MasterVolume, true);
	OverrideClass(AmbienceSoundClass, AmbienceVolume, false);
	OverrideClass(FoleySoundClass, EffectsVolume, false);
	OverrideClass(CinematicSoundClass, EffectsVolume, false);
	OverrideClass(MusicSoundClass, EffectsVolume, false);
	OverrideClass(UISoundClass, FeedbackVolume, false);
}

void UWhiteoutSettingsSubsystem::SetFieldOfView(const float Value, UObject* WorldContextObject)
{
	FieldOfView = FMath::Clamp(Value, MinFieldOfView, MaxFieldOfView);
	Save();
	Apply(WorldContextObject);
}

void UWhiteoutSettingsSubsystem::SetMasterVolume(const float Value, UObject* WorldContextObject)
{
	MasterVolume = FMath::Clamp(Value, 0.0f, 1.0f);
	Save();
	Apply(WorldContextObject);
}

void UWhiteoutSettingsSubsystem::SetAmbienceVolume(const float Value, UObject* WorldContextObject)
{
	AmbienceVolume = FMath::Clamp(Value, 0.0f, 1.0f);
	Save();
	Apply(WorldContextObject);
}

void UWhiteoutSettingsSubsystem::SetEffectsVolume(const float Value, UObject* WorldContextObject)
{
	EffectsVolume = FMath::Clamp(Value, 0.0f, 1.0f);
	Save();
	Apply(WorldContextObject);
}

void UWhiteoutSettingsSubsystem::SetFeedbackVolume(const float Value, UObject* WorldContextObject)
{
	FeedbackVolume = FMath::Clamp(Value, 0.0f, 1.0f);
	Save();
	Apply(WorldContextObject);
}

void UWhiteoutSettingsSubsystem::SetTextScale(const float Value, UObject* WorldContextObject)
{
	TextScale = FMath::Clamp(Value, 0.9f, 1.2f);
	Save();
	Apply(WorldContextObject);
}

void UWhiteoutSettingsSubsystem::SetReducedMotionEnabled(const bool bEnabled)
{
	bReducedMotion = bEnabled;
	Save();
}
