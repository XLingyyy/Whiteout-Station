#include "World/WhiteoutStationBuilder.h"

#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Presentation/WSPresentationData.h"
#include "State/WindStationStateSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "World/WSInteractableActor.h"
#include "World/WhiteoutSnowField.h"

AWhiteoutStationBuilder::AWhiteoutStationBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWhiteoutStationBuilder::BeginPlay()
{
	Super::BeginPlay();
	BuildStation();
	if (UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		StateSubsystem->OnActionCommitted.AddDynamic(this, &AWhiteoutStationBuilder::HandleActionCommitted);
		StateSubsystem->OnStateChanged.AddDynamic(this, &AWhiteoutStationBuilder::HandleStateChanged);
		const FWSGameState State = StateSubsystem->GetStateSnapshot();
		if (State.bMidCrisisTriggered)
		{
			ApplyCrisisLighting();
		}
		if (State.Phase == EWSGamePhase::Results)
		{
			ApplyEndingPresentation(State.Ending);
		}
	}
}

void AWhiteoutStationBuilder::BuildStation()
{
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation: building five-zone station and 13 action hotspots"));
	ADirectionalLight* DirectionalLight = GetWorld()->SpawnActor<ADirectionalLight>(FVector(600, 400, 900), FRotator(-52, -28, 0));
	if (DirectionalLight)
	{
		ExteriorLight = CastChecked<UDirectionalLightComponent>(DirectionalLight->GetLightComponent());
		ExteriorLight->SetIntensity(3.0f);
		ExteriorLight->SetLightColor(FLinearColor(0.58f, 0.7f, 0.9f));
		ExteriorLight->SetMobility(EComponentMobility::Movable);
		ExteriorLight->SetAtmosphereSunLight(true);
	}
	GetWorld()->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator);
	AExponentialHeightFog* HeightFog = GetWorld()->SpawnActor<AExponentialHeightFog>(FVector(0, 0, -80), FRotator::ZeroRotator);
	if (HeightFog)
	{
		HeightFog->GetComponent()->SetFogDensity(0.012f);
		HeightFog->GetComponent()->SetFogHeightFalloff(0.26f);
		HeightFog->GetComponent()->SetFogInscatteringColor(FLinearColor(0.24f, 0.34f, 0.48f));
		HeightFog->GetComponent()->SetVolumetricFog(true);
		HeightFog->GetComponent()->SetVolumetricFogExtinctionScale(0.65f);
	}
	ASkyLight* SkyLight = GetWorld()->SpawnActor<ASkyLight>(FVector(600, 400, 500), FRotator::ZeroRotator);
	if (SkyLight)
	{
		SkyLight->GetLightComponent()->SetIntensity(0.65f);
		SkyLight->GetLightComponent()->SetLightColor(FLinearColor(0.35f, 0.48f, 0.7f));
		SkyLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		SkyLight->GetLightComponent()->RecaptureSky();
	}
	if (APostProcessVolume* PostProcess = GetWorld()->SpawnActor<APostProcessVolume>())
	{
		PostProcess->Tags.Add(TEXT("WSRuntimePresentation"));
		PostProcess->bUnbound = true;
		PostProcess->Priority = 10.0f;
		PostProcess->Settings.bOverride_WhiteTemp = true;
		PostProcess->Settings.WhiteTemp = 6500.0f;
		PostProcess->Settings.bOverride_ColorSaturation = true;
		PostProcess->Settings.ColorSaturation = FVector4(0.92f, 0.96f, 1.02f, 1.0f);
		PostProcess->Settings.bOverride_ColorContrast = true;
		PostProcess->Settings.ColorContrast = FVector4(1.08f, 1.08f, 1.10f, 1.0f);
		PostProcess->Settings.bOverride_AutoExposureBias = true;
		PostProcess->Settings.AutoExposureBias = -0.20f;
		PostProcess->Settings.bOverride_BloomIntensity = true;
		PostProcess->Settings.BloomIntensity = 0.28f;
		PostProcess->Settings.bOverride_VignetteIntensity = true;
		PostProcess->Settings.VignetteIntensity = 0.22f;
	}
	GetWorld()->SpawnActor<AWhiteoutSnowField>(FVector::ZeroVector, FRotator::ZeroRotator);
	const FLinearColor Control(0.15f, 0.55f, 0.9f);
	const FLinearColor Repair(0.95f, 0.36f, 0.12f);
	const FLinearColor Medical(0.12f, 0.75f, 0.55f);
	const FLinearColor Quarter(0.9f, 0.65f, 0.12f);
	const FLinearColor Outdoor(0.4f, 0.65f, 1.0f);
	const FLinearColor FloorColor(0.08f, 0.11f, 0.14f, 1.0f);
	const FLinearColor WallColor(0.22f, 0.27f, 0.30f, 1.0f);
	SpawnBlock(TEXT("Indoor Floor"), FVector(700, 400, -25), FVector(20, 14, 0.5f), FloorColor);
	SpawnBlock(TEXT("Outdoor Platform"), FVector(2300, 400, -25), FVector(10, 7, 0.5f), FloorColor);
	SpawnBlock(TEXT("Snow Apron North"), FVector(2300, -450, -48), FVector(14, 9, 0.32f), FloorColor);
	SpawnBlock(TEXT("Snow Apron South"), FVector(2300, 1250, -48), FVector(14, 9, 0.32f), FloorColor);
	SpawnBlock(TEXT("Station Ceiling"), FVector(700, 400, 365), FVector(20, 14, 0.22f), WallColor);
	SpawnBlock(TEXT("North Wall"), FVector(700, -300, 150), FVector(20, 0.25f, 3.5f), WallColor);
	SpawnBlock(TEXT("South Wall"), FVector(700, 1100, 150), FVector(20, 0.25f, 3.5f), WallColor);
	SpawnBlock(TEXT("West Wall"), FVector(-300, 400, 150), FVector(0.25f, 14, 3.5f), WallColor);
	SpawnBlock(TEXT("East Wall A"), FVector(1700, 0, 150), FVector(0.25f, 6, 3.5f), WallColor);
	SpawnBlock(TEXT("East Wall B"), FVector(1700, 900, 150), FVector(0.25f, 4, 3.5f), WallColor);
	SpawnBlock(TEXT("Central Partition A"), FVector(700, -50, 150), FVector(0.2f, 5, 3.5f), WallColor);
	SpawnBlock(TEXT("Central Partition B"), FVector(700, 850, 150), FVector(0.2f, 5, 3.5f), WallColor);
	SpawnBlock(TEXT("Cross Partition A"), FVector(100, 400, 150), FVector(8, 0.2f, 3.5f), WallColor);
	SpawnBlock(TEXT("Cross Partition B"), FVector(1300, 400, 150), FVector(8, 0.2f, 3.5f), WallColor);
	SpawnStationAssembly();

	// Generator and pipe volumes remain collision-safe shells underneath the imported machinery.
	SpawnBlock(TEXT("Metal Generator Base"), FVector(1260, -120, 42), FVector(3.0f, 0.9f, 0.55f), Repair);
	SpawnBlock(TEXT("Metal Pipe Run A"), FVector(1080, -245, 292), FVector(7.0f, 0.12f, 0.12f), Repair);
	SpawnBlock(TEXT("Metal Pipe Run B"), FVector(1520, 150, 292), FVector(0.12f, 4.0f, 0.12f), Repair);

	SpawnSign(TEXT("控制室"), FVector(-180, 365, 215), FRotator(0, 90, 0), FLinearColor(0.35f, 0.75f, 1.0f));
	SpawnSign(TEXT("维修间"), FVector(820, 365, 215), FRotator(0, 90, 0), FLinearColor(1.0f, 0.55f, 0.15f));
	SpawnSign(TEXT("医务室"), FVector(-180, 1080, 215), FRotator(0, 90, 0), FLinearColor(0.3f, 1.0f, 0.75f));
	SpawnSign(TEXT("厨房与宿舍"), FVector(820, 1080, 215), FRotator(0, 90, 0), FLinearColor(0.95f, 0.8f, 0.35f));
	SpawnSign(TEXT("室外天线"), FVector(2050, 650, 215), FRotator(0, 180, 0), FLinearColor(0.45f, 0.75f, 1.0f));

	SpawnHotspot(TEXT("investigate_generator_log"), TEXT("发电机运行记录"), FVector(-120, 50, 70), Control, FVector(0.62f));
	SpawnHotspot(TEXT("send_signal"), TEXT("应急无线电"), FVector(280, 50, 70), Control, FVector(0.58f));
	SpawnHotspot(TEXT("inspect_control_cabinet"), TEXT("烧毁的控制柜"), FVector(850, 20, 70), Repair, FVector(0.9f));
	SpawnHotspot(TEXT("heat_repair_room"), TEXT("维修间供暖控制器"), FVector(1050, -80, 70), Repair, FVector(0.7f));
	SpawnHotspot(TEXT("repair_generator"), TEXT("柴油发电机"), FVector(1250, 80, 90), Repair, FVector(1.2f, 0.7f, 1.1f));
	SpawnHotspot(TEXT("forced_self_repair"), TEXT("手动维修工具"), FVector(1450, 220, 55), Repair, FVector(0.65f));
	SpawnHotspot(TEXT("talk_gu_heng"), TEXT("顾衡｜工程师"), FVector(860, 160, 0), FLinearColor(0.75f, 0.28f, 0.16f), FVector(0.45f, 0.45f, 1.9f));

	SpawnHotspot(TEXT("heat_medical_room"), TEXT("医务室供暖控制器"), FVector(-120, 680, 70), Medical, FVector(0.7f));
	SpawnHotspot(TEXT("treat_gu_heng"), TEXT("治疗台"), FVector(270, 780, 70), Medical, FVector(0.8f));
	SpawnHotspot(TEXT("talk_ye_cheng"), TEXT("叶澄｜医生"), FVector(120, 850, 0), FLinearColor(0.12f, 0.65f, 0.72f), FVector(0.45f, 0.45f, 1.85f));

	SpawnHotspot(TEXT("distribute_food"), TEXT("口粮台"), FVector(900, 760, 70), Quarter, FVector(0.72f));
	SpawnHotspot(TEXT("dismantle_kitchen_heater"), TEXT("厨房加热器"), FVector(1330, 850, 70), Quarter, FVector(0.72f));
	SpawnHotspot(TEXT("calibrate_antenna"), TEXT("结冰的天线阵列"), FVector(2300, 400, 135), Outdoor, FVector(0.8f, 0.8f, 2.7f));
}

void AWhiteoutStationBuilder::SpawnStationAssembly()
{
	UWSStationAssemblyData* Assembly = LoadObject<UWSStationAssemblyData>(
		nullptr,
		TEXT("/Game/WindStation/Presentation/DA_WS_StationAssembly.DA_WS_StationAssembly"));
	if (!Assembly)
	{
		Assembly = NewObject<UWSStationAssemblyData>(this, TEXT("RuntimeStationAssembly"));
		UE_LOG(LogTemp, Warning, TEXT("WhiteoutStation v0.2: assembly asset missing; using class defaults"));
	}
	for (const FWSStationMeshPlacement& Placement : Assembly->Placements)
	{
		SpawnAssemblyMesh(Placement);
	}
	for (const FWSStationLightPlacement& Placement : Assembly->Lights)
	{
		SpawnAssemblyLight(Placement);
	}
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: spawned %d presentation meshes and %d data-driven lights"), Assembly->Placements.Num(), Assembly->Lights.Num());
}

void AWhiteoutStationBuilder::SpawnAssemblyMesh(const FWSStationMeshPlacement& Placement)
{
	UStaticMesh* StaticMesh = Placement.Mesh.LoadSynchronous();
	if (!StaticMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("WhiteoutStation v0.2: missing assembly mesh for %s"), *Placement.Label.ToString());
		return;
	}
	AStaticMeshActor* MeshActor = GetWorld()->SpawnActor<AStaticMeshActor>(
		Placement.Transform.GetLocation(),
		Placement.Transform.Rotator());
	if (!MeshActor)
	{
		return;
	}
#if WITH_EDITOR
	MeshActor->SetActorLabel(Placement.Label.ToString());
#endif
	MeshActor->Tags.Add(TEXT("WSRuntimePresentation"));
	MeshActor->Tags.Add(Placement.Zone);
	MeshActor->SetActorScale3D(Placement.Transform.GetScale3D());
	UStaticMeshComponent* Component = MeshActor->GetStaticMeshComponent();
	Component->SetStaticMesh(StaticMesh);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(Placement.bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (UMaterialInterface* Material = Placement.Material.LoadSynchronous())
	{
		for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
		{
			Component->SetMaterial(Index, Material);
		}
	}
}

void AWhiteoutStationBuilder::SpawnAssemblyLight(const FWSStationLightPlacement& Placement)
{
	SpawnPointLight(
		Placement.Label.ToString(),
		Placement.Zone,
		Placement.Location,
		Placement.Color,
		Placement.Intensity,
		Placement.Radius,
		Placement.bEmergencyRed,
		Placement.bGeneratorPowered);
}

void AWhiteoutStationBuilder::SpawnBlock(
	const FString& Label,
	const FVector Location,
	const FVector Scale,
	const FLinearColor Color)
{
	AStaticMeshActor* Block = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
	if (!Block)
	{
		return;
	}
#if WITH_EDITOR
	Block->SetActorLabel(Label);
#endif
	Block->Tags.Add(TEXT("WSRuntimeGeometry"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Block->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Block->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Block->SetActorScale3D(Scale);
	const bool bSnowSurface = Label.Contains(TEXT("Outdoor")) || Label.Contains(TEXT("Snow"));
	const bool bMetalSurface = Label.Contains(TEXT("Metal")) || Label.Contains(TEXT("Pipe"));
	const TCHAR* MaterialPath = bSnowSurface
		? TEXT("/Game/WindStation/Art/Materials/M_WS_Snow.M_WS_Snow")
		: bMetalSurface
			? TEXT("/Game/WindStation/Art/Materials/M_WS_RustedMetal.M_WS_RustedMetal")
			: TEXT("/Game/WindStation/Art/Materials/M_WS_Concrete.M_WS_Concrete");
	if (UMaterialInterface* SurfaceMaterial = LoadObject<UMaterialInterface>(nullptr, MaterialPath))
	{
		Block->GetStaticMeshComponent()->SetMaterial(0, SurfaceMaterial);
	}
	else if (UMaterialInterface* BaseMaterial = Block->GetStaticMeshComponent()->GetMaterial(0))
	{
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, Block);
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		Block->GetStaticMeshComponent()->SetMaterial(0, DynamicMaterial);
	}
}

void AWhiteoutStationBuilder::SpawnSign(
	const FString& Text,
	const FVector Location,
	const FRotator Rotation,
	const FLinearColor Color)
{
	ATextRenderActor* Sign = GetWorld()->SpawnActor<ATextRenderActor>(Location, Rotation);
	if (!Sign)
	{
		return;
	}
#if WITH_EDITOR
	Sign->SetActorLabel(Text + TEXT(" Sign"));
#endif
	Sign->Tags.Add(TEXT("WSRuntimeSign"));
	Sign->GetTextRender()->SetText(FText::FromString(Text));
	Sign->GetTextRender()->SetTextRenderColor(Color.ToFColor(true));
	Sign->GetTextRender()->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	Sign->GetTextRender()->SetWorldSize(48.0f);
}

void AWhiteoutStationBuilder::SpawnPointLight(
	const FString& Label,
	const FName Zone,
	const FVector Location,
	const FLinearColor Color,
	const float Intensity,
	const float Radius,
	const bool bEmergencyRed,
	const bool bGeneratorPowered)
{
	APointLight* PointLight = GetWorld()->SpawnActor<APointLight>(Location, FRotator::ZeroRotator);
	if (!PointLight)
	{
		return;
	}
#if WITH_EDITOR
	PointLight->SetActorLabel(Label);
#endif
	PointLight->Tags.Add(TEXT("WSRuntimeLight"));
	PointLight->Tags.Add(Zone);
	UPointLightComponent* Component = CastChecked<UPointLightComponent>(PointLight->GetLightComponent());
	Component->SetMobility(EComponentMobility::Movable);
	bool bGeneratorOnline = false;
	if (const UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		bGeneratorOnline = StateSubsystem->GetStateSnapshot().Tasks.GeneratorProgress >= 2;
	}
	Component->SetIntensity(bGeneratorPowered && !bGeneratorOnline ? Intensity * 0.22f : Intensity);
	Component->SetAttenuationRadius(Radius);
	Component->SetLightColor(Color);
	RuntimeLights.Add(Component);
	RuntimeEmergencyLights.Add(bEmergencyRed);
	RuntimeGeneratorLights.Add(bGeneratorPowered);
	RuntimeBaseLightIntensities.Add(Intensity);
	RuntimeBaseLightColors.Add(Color);
}

void AWhiteoutStationBuilder::HandleActionCommitted(const FWSActionResult& Result)
{
	if (Result.bCrisisTriggered)
	{
		BeginCrisisLightingSequence();
	}
	if (Result.ActionId == TEXT("repair_generator"))
	{
		RestoreGeneratorLighting();
	}
}

void AWhiteoutStationBuilder::HandleStateChanged(const FWSGameState& State)
{
	if (State.Phase == EWSGamePhase::Results && !bEndingPresentationApplied)
	{
		ApplyEndingPresentation(State.Ending);
	}
}

void AWhiteoutStationBuilder::BeginCrisisLightingSequence()
{
	if (bCrisisSequenceStarted)
	{
		return;
	}
	bCrisisSequenceStarted = true;
	CrisisLightingStep = 0;
	AdvanceCrisisLightingSequence();
	UE_LOG(LogTemp, Warning, TEXT("WhiteoutStation v0.2: voltage-drop lighting sequence started"));
}

void AWhiteoutStationBuilder::AdvanceCrisisLightingSequence()
{
	if (CrisisLightingStep >= 4)
	{
		ApplyCrisisLighting();
		return;
	}
	const bool bBlackoutFrame = CrisisLightingStep == 1 || CrisisLightingStep == 3;
	if (ExteriorLight)
	{
		ExteriorLight->SetIntensity(bBlackoutFrame ? 0.08f : CrisisLightingStep == 0 ? 5.2f : 3.8f);
		ExteriorLight->SetLightColor(bBlackoutFrame
			? FLinearColor(0.04f, 0.06f, 0.10f)
			: FLinearColor(0.78f, 0.90f, 1.0f));
	}
	for (int32 Index = 0; Index < RuntimeLights.Num(); ++Index)
	{
		if (!RuntimeBaseLightIntensities.IsValidIndex(Index) || !RuntimeBaseLightColors.IsValidIndex(Index))
		{
			continue;
		}
		RuntimeLights[Index]->SetLightColor(bBlackoutFrame
			? FLinearColor(0.02f, 0.03f, 0.05f)
			: FLinearColor(0.78f, 0.90f, 1.0f));
		RuntimeLights[Index]->SetIntensity(bBlackoutFrame
			? 22.0f
			: RuntimeBaseLightIntensities[Index] * (CrisisLightingStep == 0 ? 1.75f : 1.18f));
	}
	const float Delay = CrisisLightingStep == 0 ? 0.10f : CrisisLightingStep == 1 ? 0.16f : CrisisLightingStep == 2 ? 0.13f : 0.22f;
	++CrisisLightingStep;
	GetWorldTimerManager().SetTimer(CrisisLightingTimer, this, &AWhiteoutStationBuilder::AdvanceCrisisLightingSequence, Delay, false);
}

void AWhiteoutStationBuilder::ApplyCrisisLighting()
{
	UE_LOG(LogTemp, Warning, TEXT("WhiteoutStation: backup power failure lighting engaged"));
	if (ExteriorLight)
	{
		ExteriorLight->SetIntensity(0.65f);
		ExteriorLight->SetLightColor(FLinearColor(0.18f, 0.25f, 0.38f));
	}
	for (int32 Index = 0; Index < RuntimeLights.Num(); ++Index)
	{
		if (UPointLightComponent* Light = RuntimeLights[Index])
		{
			const bool bEmergencyRed = RuntimeEmergencyLights.IsValidIndex(Index) && RuntimeEmergencyLights[Index];
			Light->SetLightColor(bEmergencyRed ? FLinearColor(1.0f, 0.035f, 0.01f) : FLinearColor(0.06f, 0.16f, 0.32f));
			Light->SetIntensity(bEmergencyRed ? 1650.0f : 480.0f);
		}
	}
}

void AWhiteoutStationBuilder::RestoreGeneratorLighting()
{
	const UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>();
	if (!StateSubsystem || StateSubsystem->GetStateSnapshot().Tasks.GeneratorProgress < 2)
	{
		return;
	}
	for (int32 Index = 0; Index < RuntimeLights.Num(); ++Index)
	{
		if (RuntimeGeneratorLights.IsValidIndex(Index) && RuntimeGeneratorLights[Index] && RuntimeBaseLightIntensities.IsValidIndex(Index))
		{
			RuntimeLights[Index]->SetIntensity(RuntimeBaseLightIntensities[Index]);
			RuntimeLights[Index]->SetLightColor(FLinearColor(1.0f, 0.42f, 0.12f));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: generator-powered repair lighting restored"));
}

void AWhiteoutStationBuilder::ApplyEndingPresentation(const EWSEndingType Ending)
{
	bEndingPresentationApplied = true;
	GetWorldTimerManager().ClearTimer(CrisisLightingTimer);
	if (ExteriorLight)
	{
		if (Ending == EWSEndingType::TaskSuccess)
		{
			ExteriorLight->SetIntensity(2.6f);
			ExteriorLight->SetLightColor(FLinearColor(1.0f, 0.72f, 0.42f));
		}
		else if (Ending == EWSEndingType::SurvivalWait)
		{
			ExteriorLight->SetIntensity(0.48f);
			ExteriorLight->SetLightColor(FLinearColor(0.20f, 0.34f, 0.56f));
		}
		else if (Ending == EWSEndingType::CostUncontrolled)
		{
			ExteriorLight->SetIntensity(0.30f);
			ExteriorLight->SetLightColor(FLinearColor(0.56f, 0.18f, 0.09f));
		}
		else
		{
			ExteriorLight->SetIntensity(0.035f);
			ExteriorLight->SetLightColor(FLinearColor(0.03f, 0.04f, 0.07f));
		}
	}
	for (int32 Index = 0; Index < RuntimeLights.Num(); ++Index)
	{
		if (!RuntimeBaseLightIntensities.IsValidIndex(Index) || !RuntimeBaseLightColors.IsValidIndex(Index))
		{
			continue;
		}
		const bool bEmergencyRed = RuntimeEmergencyLights.IsValidIndex(Index) && RuntimeEmergencyLights[Index];
		if (Ending == EWSEndingType::TaskSuccess)
		{
			RuntimeLights[Index]->SetIntensity(RuntimeBaseLightIntensities[Index] * 0.92f);
			RuntimeLights[Index]->SetLightColor(FLinearColor(1.0f, 0.48f, 0.18f));
		}
		else if (Ending == EWSEndingType::SurvivalWait)
		{
			RuntimeLights[Index]->SetIntensity(RuntimeBaseLightIntensities[Index] * 0.28f);
			RuntimeLights[Index]->SetLightColor(FLinearColor(0.10f, 0.24f, 0.48f));
		}
		else if (Ending == EWSEndingType::CostUncontrolled)
		{
			RuntimeLights[Index]->SetIntensity(bEmergencyRed ? 1120.0f : 190.0f);
			RuntimeLights[Index]->SetLightColor(bEmergencyRed
				? FLinearColor(1.0f, 0.025f, 0.008f)
				: FLinearColor(0.42f, 0.08f, 0.03f));
		}
		else
		{
			RuntimeLights[Index]->SetIntensity(bEmergencyRed ? 70.0f : 8.0f);
			RuntimeLights[Index]->SetLightColor(FLinearColor(0.055f, 0.035f, 0.045f));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation v0.2: ending lighting staged for %s"),
		*StaticEnum<EWSEndingType>()->GetNameStringByValue(static_cast<int64>(Ending)));
}

void AWhiteoutStationBuilder::SetLightingPreviewState(const bool bCrisis, const bool bGeneratorOnline)
{
	if (bCrisis)
	{
		ApplyCrisisLighting();
		return;
	}
	if (ExteriorLight)
	{
		ExteriorLight->SetIntensity(3.0f);
		ExteriorLight->SetLightColor(FLinearColor(0.58f, 0.7f, 0.9f));
	}
	for (int32 Index = 0; Index < RuntimeLights.Num(); ++Index)
	{
		if (!RuntimeBaseLightIntensities.IsValidIndex(Index) || !RuntimeBaseLightColors.IsValidIndex(Index))
		{
			continue;
		}
		const bool bGeneratorPowered = RuntimeGeneratorLights.IsValidIndex(Index) && RuntimeGeneratorLights[Index];
		RuntimeLights[Index]->SetLightColor(RuntimeBaseLightColors[Index]);
		RuntimeLights[Index]->SetIntensity(bGeneratorPowered && !bGeneratorOnline
			? RuntimeBaseLightIntensities[Index] * 0.22f
			: RuntimeBaseLightIntensities[Index]);
	}
}

AWSInteractableActor* AWhiteoutStationBuilder::SpawnHotspot(
	const TCHAR* ActionId,
	const TCHAR* Label,
	const FVector Location,
	const FLinearColor Color,
	const FVector Scale)
{
	AWSInteractableActor* Hotspot = GetWorld()->SpawnActor<AWSInteractableActor>(Location, FRotator::ZeroRotator);
	if (Hotspot)
	{
		Hotspot->Configure(FName(ActionId), FText::FromString(Label), Color);
		if (Hotspot->IsCharacterHotspot())
		{
			Hotspot->SetActorScale3D(FVector::OneVector);
			Hotspot->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
		}
		else
		{
			Hotspot->SetActorScale3D(Scale);
		}
		Hotspot->Tags.Add(TEXT("WSRuntimeHotspot"));
	}
	return Hotspot;
}
