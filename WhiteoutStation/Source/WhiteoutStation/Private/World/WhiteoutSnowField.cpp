#include "World/WhiteoutSnowField.h"

#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "State/WindStationStateSubsystem.h"

AWhiteoutSnowField::AWhiteoutSnowField()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	for (int32 Index = 0; Index < 12; ++Index)
	{
		UNiagaraComponent* Layer = CreateDefaultSubobject<UNiagaraComponent>(
			FName(*FString::Printf(TEXT("BlizzardLayer%d"), Index + 1)));
		Layer->SetupAttachment(SceneRoot);
		Layer->SetAutoActivate(false);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BlizzardLayers.Add(Layer);
	}
}

void AWhiteoutSnowField::BeginPlay()
{
	Super::BeginPlay();
	Tags.Add(TEXT("WSRuntimeNiagaraBlizzard"));
	UNiagaraSystem* BlizzardSystem = LoadObject<UNiagaraSystem>(
		nullptr,
		TEXT("/Game/WindStation/Art/VFX/NS_WS_BlizzardStreaks.NS_WS_BlizzardStreaks"));
	if (!BlizzardSystem)
	{
		UE_LOG(LogTemp, Error, TEXT("WhiteoutStation v0.2: Niagara blizzard asset is missing"));
		return;
	}
	const TArray<FVector> LayerLocations = {
		FVector(2200, 400, 260), FVector(2450, 200, 300),
		FVector(2450, 600, 300), FVector(1950, 0, 300),
		FVector(1950, 800, 300), FVector(2850, 400, 380),
		FVector(2200, -300, 400), FVector(2200, 1100, 400),
		FVector(3200, 0, 500), FVector(3200, 800, 500),
		FVector(2700, -500, 480), FVector(2700, 1300, 480)};
	for (int32 Index = 0; Index < BlizzardLayers.Num(); ++Index)
	{
		UNiagaraComponent* Layer = BlizzardLayers[Index];
		Layer->SetAsset(BlizzardSystem);
		Layer->SetWorldLocation(LayerLocations[Index]);
		Layer->SetWorldRotation(FRotator(-14.0f, -24.0f, 0.0f));
		Layer->SetWorldScale3D(FVector(2.4f, 2.4f, 2.0f));
		Layer->SetVariableVec3(TEXT("User.WindVelocity"), FVector(-760.0f, 150.0f, -125.0f));
		Layer->SetVariableFloat(TEXT("User.SpawnRate"), 420.0f);
	}
	bool bCrisis = false;
	if (UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		StateSubsystem->OnActionCommitted.AddDynamic(this, &AWhiteoutSnowField::HandleActionCommitted);
		StateSubsystem->OnStateChanged.AddDynamic(this, &AWhiteoutSnowField::HandleStateChanged);
		const FWSGameState State = StateSubsystem->GetStateSnapshot();
		bCrisis = State.bMidCrisisTriggered;
		if (State.Phase == EWSGamePhase::Results)
		{
			SetEndingIntensity(State.Ending);
			return;
		}
	}
	SetBlizzardIntensity(bCrisis);
}

void AWhiteoutSnowField::HandleStateChanged(const FWSGameState& State)
{
	if (State.Phase == EWSGamePhase::Results)
	{
		SetEndingIntensity(State.Ending);
	}
}

void AWhiteoutSnowField::HandleActionCommitted(const FWSActionResult& Result)
{
	if (Result.bCrisisTriggered)
	{
		SetBlizzardIntensity(true);
	}
}

void AWhiteoutSnowField::SetBlizzardIntensity(const bool bCrisis)
{
	for (int32 Index = 0; Index < BlizzardLayers.Num(); ++Index)
	{
		UNiagaraComponent* Layer = BlizzardLayers[Index];
		const bool bActive = bCrisis || Index < 6;
		Layer->SetVariableFloat(TEXT("User.SpawnRate"), bCrisis ? 860.0f : 420.0f);
		Layer->SetVariableVec3(
			TEXT("User.WindVelocity"),
			bCrisis ? FVector(-1120.0f, 260.0f, -180.0f) : FVector(-760.0f, 150.0f, -125.0f));
		Layer->SetWorldScale3D(bCrisis ? FVector(3.2f, 3.2f, 2.6f) : FVector(2.4f, 2.4f, 2.0f));
		if (bActive) Layer->Activate(true);
		else Layer->Deactivate();
	}
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: Niagara blizzard intensity=%s layers=%d"), bCrisis ? TEXT("crisis") : TEXT("normal"), bCrisis ? 12 : 6);
}

void AWhiteoutSnowField::SetEndingIntensity(const EWSEndingType Ending)
{
	const int32 ActiveLayers = Ending == EWSEndingType::TaskSuccess ? 4
		: Ending == EWSEndingType::SurvivalWait ? 8
		: 12;
	const float SpawnRate = Ending == EWSEndingType::TaskSuccess ? 260.0f
		: Ending == EWSEndingType::SurvivalWait ? 590.0f
		: Ending == EWSEndingType::CostUncontrolled ? 920.0f
		: 1120.0f;
	const FVector WindVelocity = Ending == EWSEndingType::TaskSuccess
		? FVector(-520.0f, 90.0f, -80.0f)
		: Ending == EWSEndingType::SurvivalWait
			? FVector(-880.0f, 190.0f, -145.0f)
			: FVector(-1280.0f, 310.0f, -210.0f);
	for (int32 Index = 0; Index < BlizzardLayers.Num(); ++Index)
	{
		UNiagaraComponent* Layer = BlizzardLayers[Index];
		Layer->SetVariableFloat(TEXT("User.SpawnRate"), SpawnRate);
		Layer->SetVariableVec3(TEXT("User.WindVelocity"), WindVelocity);
		Layer->SetWorldScale3D(Ending == EWSEndingType::TaskSuccess
			? FVector(2.1f, 2.1f, 1.8f)
			: FVector(3.4f, 3.4f, 2.8f));
		if (Index < ActiveLayers) Layer->Activate(true);
		else Layer->Deactivate();
	}
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: ending blizzard layers=%d spawn=%.0f"), ActiveLayers, SpawnRate);
}
