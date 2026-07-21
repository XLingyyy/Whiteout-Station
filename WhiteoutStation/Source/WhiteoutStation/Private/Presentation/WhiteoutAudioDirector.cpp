#include "Presentation/WhiteoutAudioDirector.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "State/WindStationStateSubsystem.h"

namespace
{
	USoundBase* LoadPresentationSound(const TCHAR* Path)
	{
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, Path);
		if (!Sound)
		{
			UE_LOG(LogTemp, Error, TEXT("WhiteoutStation v0.2: missing presentation sound %s"), Path);
		}
		return Sound;
	}

	void ConfigureLoop(UAudioComponent* Component, USoundBase* Sound)
	{
		if (!Component || !Sound)
		{
			return;
		}
		Component->SetSound(Sound);
		Component->SetVolumeMultiplier(0.0f);
		Component->Play();
	}
}

AWhiteoutAudioDirector::AWhiteoutAudioDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	OutdoorWind = CreateDefaultSubobject<UAudioComponent>(TEXT("OutdoorWind"));
	OutdoorWind->SetupAttachment(SceneRoot);
	OutdoorWind->bAutoActivate = false;
	OutdoorWind->bAllowSpatialization = false;
	IndoorWind = CreateDefaultSubobject<UAudioComponent>(TEXT("IndoorWind"));
	IndoorWind->SetupAttachment(SceneRoot);
	IndoorWind->bAutoActivate = false;
	IndoorWind->bAllowSpatialization = false;
	GeneratorLoop = CreateDefaultSubobject<UAudioComponent>(TEXT("GeneratorLoop"));
	GeneratorLoop->SetupAttachment(SceneRoot);
	GeneratorLoop->bAutoActivate = false;
	GeneratorLoop->bAllowSpatialization = false;
}

void AWhiteoutAudioDirector::BeginPlay()
{
	Super::BeginPlay();
	Tags.Add(TEXT("WSRuntimeAudioDirector"));
	ConfigureLoop(
		OutdoorWind,
		LoadPresentationSound(TEXT("/Game/WindStation/Audio/Ambience/S_WindStrong_CC0.S_WindStrong_CC0")));
	ConfigureLoop(
		IndoorWind,
		LoadPresentationSound(TEXT("/Game/WindStation/Audio/Ambience/S_WindIndoor_CC0_Derivative.S_WindIndoor_CC0_Derivative")));
	ConfigureLoop(
		GeneratorLoop,
		LoadPresentationSound(TEXT("/Game/WindStation/Audio/Machinery/S_GeneratorLoop_Original.S_GeneratorLoop_Original")));
	CrisisStinger = LoadPresentationSound(TEXT("/Game/WindStation/Audio/Events/S_CrisisStinger_Original.S_CrisisStinger_Original"));
	RadioReply = LoadPresentationSound(TEXT("/Game/WindStation/Audio/Events/S_RadioReply_Original.S_RadioReply_Original"));
	EndingSuccess = LoadPresentationSound(TEXT("/Game/WindStation/Audio/Music/S_EndingSuccess_Original.S_EndingSuccess_Original"));
	EndingSurvival = LoadPresentationSound(TEXT("/Game/WindStation/Audio/Music/S_EndingSurvival_Original.S_EndingSurvival_Original"));
	EndingCost = LoadPresentationSound(TEXT("/Game/WindStation/Audio/Music/S_EndingCost_Original.S_EndingCost_Original"));
	EndingCollapse = LoadPresentationSound(TEXT("/Game/WindStation/Audio/Music/S_EndingCollapse_Original.S_EndingCollapse_Original"));

	if (UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		StateSubsystem->OnActionCommitted.AddDynamic(this, &AWhiteoutAudioDirector::HandleActionCommitted);
		StateSubsystem->OnStateChanged.AddDynamic(this, &AWhiteoutAudioDirector::HandleStateChanged);
		ApplyState(StateSubsystem->GetStateSnapshot());
	}
}

void AWhiteoutAudioDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const float RawExteriorBlend = PlayerPawn
		? static_cast<float>(FMath::GetMappedRangeValueClamped(
			FVector2D(1580.0f, 1900.0f), FVector2D(0.0f, 1.0f), PlayerPawn->GetActorLocation().X))
		: 0.0f;
	const float ExteriorBlend = FMath::SmoothStep(0.0f, 1.0f, RawExteriorBlend);
	const float CrisisMultiplier = bCrisisActive ? 1.28f : 1.0f;
	const float OutdoorTarget = FMath::Lerp(0.035f, 0.42f, ExteriorBlend) * CrisisMultiplier;
	const float IndoorTarget = FMath::Lerp(0.19f, 0.025f, ExteriorBlend) * CrisisMultiplier;
	const float GeneratorTarget = bGeneratorOnline ? FMath::Lerp(0.22f, 0.08f, ExteriorBlend) : 0.0f;
	if (OutdoorWind)
	{
		OutdoorWind->SetVolumeMultiplier(FMath::FInterpTo(OutdoorWind->VolumeMultiplier, OutdoorTarget, DeltaSeconds, 1.5f));
	}
	if (IndoorWind)
	{
		IndoorWind->SetVolumeMultiplier(FMath::FInterpTo(IndoorWind->VolumeMultiplier, IndoorTarget, DeltaSeconds, 1.9f));
	}
	if (GeneratorLoop)
	{
		GeneratorLoop->SetVolumeMultiplier(FMath::FInterpTo(GeneratorLoop->VolumeMultiplier, GeneratorTarget, DeltaSeconds, 2.2f));
	}
}

void AWhiteoutAudioDirector::HandleActionCommitted(const FWSActionResult& Result)
{
	if (Result.bCrisisTriggered)
	{
		bCrisisActive = true;
		if (CrisisStinger)
		{
			UGameplayStatics::PlaySound2D(this, CrisisStinger, 0.88f);
		}
	}
	if (Result.ActionId == TEXT("repair_generator"))
	{
		if (const UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
		{
			bGeneratorOnline = StateSubsystem->GetStateSnapshot().Tasks.GeneratorProgress >= 2;
		}
	}
}

void AWhiteoutAudioDirector::HandleStateChanged(const FWSGameState& State)
{
	ApplyState(State);
}

void AWhiteoutAudioDirector::ApplyState(const FWSGameState& State)
{
	bCrisisActive = State.bMidCrisisTriggered;
	bGeneratorOnline = State.Tasks.GeneratorProgress >= 2;
	if (State.Phase == EWSGamePhase::Results && !bEndingAudioPlayed)
	{
		bEndingAudioPlayed = true;
		PlayEndingAudio(State.Ending);
	}
}

void AWhiteoutAudioDirector::PlayEndingAudio(const EWSEndingType Ending)
{
	USoundBase* Music = EndingCollapse;
	if (Ending == EWSEndingType::TaskSuccess)
	{
		Music = EndingSuccess;
		if (RadioReply)
		{
			UGameplayStatics::PlaySound2D(this, RadioReply, 0.82f);
		}
	}
	else if (Ending == EWSEndingType::SurvivalWait)
	{
		Music = EndingSurvival;
	}
	else if (Ending == EWSEndingType::CostUncontrolled)
	{
		Music = EndingCost;
	}
	if (Music)
	{
		UGameplayStatics::PlaySound2D(this, Music, 0.72f);
	}
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: ending audio staged for %s"),
		*StaticEnum<EWSEndingType>()->GetNameStringByValue(static_cast<int64>(Ending)));
}
