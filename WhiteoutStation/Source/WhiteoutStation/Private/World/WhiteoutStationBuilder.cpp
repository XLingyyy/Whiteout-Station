#include "World/WhiteoutStationBuilder.h"

#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
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
		if (StateSubsystem->GetStateSnapshot().bMidCrisisTriggered)
		{
			ApplyCrisisLighting();
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
	SpawnPointLight(TEXT("Control emergency light"), FVector(80, 130, 250), FLinearColor(0.2f, 0.55f, 1.0f), 2400.0f, 850.0f);
	SpawnPointLight(TEXT("Repair warning light"), FVector(1120, 120, 260), FLinearColor(1.0f, 0.28f, 0.08f), 2100.0f, 820.0f);
	SpawnPointLight(TEXT("Medical task light"), FVector(80, 780, 245), FLinearColor(0.35f, 0.85f, 0.72f), 2300.0f, 820.0f);
	SpawnPointLight(TEXT("Quarters practical"), FVector(1120, 780, 245), FLinearColor(1.0f, 0.62f, 0.22f), 1900.0f, 820.0f);
	SpawnPointLight(TEXT("Antenna work light"), FVector(2100, 400, 330), FLinearColor(0.28f, 0.48f, 1.0f), 2600.0f, 1000.0f);
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

	// Silhouette props make every room legible before bespoke meshes arrive.
	SpawnBlock(TEXT("Metal Control Desk"), FVector(-60, -165, 48), FVector(3.4f, 0.7f, 0.7f), Control);
	SpawnBlock(TEXT("Metal Radio Stack"), FVector(390, -175, 92), FVector(0.8f, 0.55f, 1.45f), Control);
	SpawnBlock(TEXT("Metal Generator Base"), FVector(1260, -120, 42), FVector(3.0f, 0.9f, 0.55f), Repair);
	SpawnBlock(TEXT("Metal Pipe Run A"), FVector(1080, -245, 292), FVector(7.0f, 0.12f, 0.12f), Repair);
	SpawnBlock(TEXT("Metal Pipe Run B"), FVector(1520, 150, 292), FVector(0.12f, 4.0f, 0.12f), Repair);
	SpawnBlock(TEXT("Medical Bed"), FVector(330, 690, 42), FVector(2.3f, 0.85f, 0.35f), Medical);
	SpawnBlock(TEXT("Medical Cabinet"), FVector(-170, 930, 90), FVector(0.65f, 1.0f, 1.55f), Medical);
	SpawnBlock(TEXT("Kitchen Counter"), FVector(1260, 1015, 55), FVector(3.8f, 0.6f, 0.85f), Quarter);
	SpawnBlock(TEXT("Quarters Bunk A"), FVector(820, 560, 40), FVector(2.4f, 0.7f, 0.32f), Quarter);
	SpawnBlock(TEXT("Quarters Bunk B"), FVector(820, 930, 40), FVector(2.4f, 0.7f, 0.32f), Quarter);

	SpawnSign(TEXT("CONTROL"), FVector(-180, 365, 215), FRotator(0, 90, 0), FLinearColor(0.35f, 0.75f, 1.0f));
	SpawnSign(TEXT("REPAIR"), FVector(820, 365, 215), FRotator(0, 90, 0), FLinearColor(1.0f, 0.55f, 0.15f));
	SpawnSign(TEXT("MEDICAL"), FVector(-180, 1080, 215), FRotator(0, 90, 0), FLinearColor(0.3f, 1.0f, 0.75f));
	SpawnSign(TEXT("QUARTERS"), FVector(820, 1080, 215), FRotator(0, 90, 0), FLinearColor(0.95f, 0.8f, 0.35f));
	SpawnSign(TEXT("ANTENNA"), FVector(2050, 650, 215), FRotator(0, 180, 0), FLinearColor(0.45f, 0.75f, 1.0f));

	SpawnHotspot(TEXT("investigate_generator_log"), TEXT("Deep generator log"), FVector(-120, 50, 70), Control);
	SpawnHotspot(TEXT("send_signal"), TEXT("Emergency radio"), FVector(280, 50, 70), Control);
	SpawnHotspot(TEXT("inspect_control_cabinet"), TEXT("Burnt control cabinet"), FVector(850, 20, 70), Repair);
	SpawnHotspot(TEXT("heat_repair_room"), TEXT("Repair-room heating"), FVector(1050, -80, 70), Repair);
	SpawnHotspot(TEXT("repair_generator"), TEXT("Diesel generator"), FVector(1250, 80, 90), Repair, FVector(1.2f, 0.7f, 1.1f));
	SpawnHotspot(TEXT("forced_self_repair"), TEXT("Manual repair kit"), FVector(1450, 220, 55), Repair, FVector(0.45f));
	SpawnHotspot(TEXT("talk_gu_heng"), TEXT("Gu Heng / engineer"), FVector(980, 260, 95), FLinearColor(0.75f, 0.28f, 0.16f), FVector(0.45f, 0.45f, 1.9f));

	SpawnHotspot(TEXT("heat_medical_room"), TEXT("Medical-room heating"), FVector(-120, 680, 70), Medical);
	SpawnHotspot(TEXT("treat_gu_heng"), TEXT("Treatment station"), FVector(270, 780, 70), Medical);
	SpawnHotspot(TEXT("talk_ye_cheng"), TEXT("Ye Cheng / doctor"), FVector(120, 980, 95), FLinearColor(0.12f, 0.65f, 0.72f), FVector(0.45f, 0.45f, 1.85f));

	SpawnHotspot(TEXT("distribute_food"), TEXT("Ration counter"), FVector(900, 760, 70), Quarter);
	SpawnHotspot(TEXT("dismantle_kitchen_heater"), TEXT("Kitchen heater"), FVector(1330, 850, 70), Quarter);
	SpawnHotspot(TEXT("calibrate_antenna"), TEXT("Frozen antenna array"), FVector(2300, 400, 135), Outdoor, FVector(0.8f, 0.8f, 2.7f));
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
	const FVector Location,
	const FLinearColor Color,
	const float Intensity,
	const float Radius)
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
	UPointLightComponent* Component = CastChecked<UPointLightComponent>(PointLight->GetLightComponent());
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetIntensity(Intensity);
	Component->SetAttenuationRadius(Radius);
	Component->SetLightColor(Color);
	RuntimeLights.Add(Component);
}

void AWhiteoutStationBuilder::HandleActionCommitted(const FWSActionResult& Result)
{
	if (Result.bCrisisTriggered)
	{
		ApplyCrisisLighting();
	}
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
			const bool bEmergencyRed = Index == 1 || Index == 3;
			Light->SetLightColor(bEmergencyRed ? FLinearColor(1.0f, 0.035f, 0.01f) : FLinearColor(0.06f, 0.16f, 0.32f));
			Light->SetIntensity(bEmergencyRed ? 1650.0f : 480.0f);
		}
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
		Hotspot->SetActorScale3D(Scale);
		Hotspot->Tags.Add(TEXT("WSRuntimeHotspot"));
	}
	return Hotspot;
}
