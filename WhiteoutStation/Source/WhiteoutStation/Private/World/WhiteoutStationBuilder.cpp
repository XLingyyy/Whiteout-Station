#include "World/WhiteoutStationBuilder.h"

#include "Components/BoxComponent.h"
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
#include "Engine/World.h"
#include "Engine/TextRenderActor.h"
#include "EngineUtils.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/WhiteoutCharacter.h"
#include "Presentation/WSPresentationData.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "State/WindStationStateSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "WorldCollision.h"
#include "World/WSInteractableActor.h"
#include "World/WhiteoutSnowField.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

namespace
{
	const FName EditableStationTag(TEXT("WSEditableStation"));
	const FName ExteriorLightTag(TEXT("WSExteriorLight"));
	const FName EmergencyLightTag(TEXT("WSEmergencyLight"));
	const FName GeneratorLightTag(TEXT("WSGeneratorPowered"));

	struct FRequiredHotspotDefinition
	{
		FName ActionId;
		const TCHAR* Label;
		FVector Location;
		FLinearColor Color;
		FVector Scale;
	};

	const TArray<FRequiredHotspotDefinition>& RequiredHotspotDefinitions()
	{
		static const TArray<FRequiredHotspotDefinition> Definitions = {
			{FName(TEXT("investigate_generator_log")), TEXT("发电机运行记录"), FVector(-120, -165, 70), FLinearColor(0.15f, 0.55f, 0.9f), FVector(0.62f)},
			{FName(TEXT("send_signal")), TEXT("应急无线电"), FVector(280, -165, 70), FLinearColor(0.15f, 0.55f, 0.9f), FVector(0.58f)},
			{FName(TEXT("heat_control_room")), TEXT("控制室供暖控制器"), FVector(400, 180, 70), FLinearColor(0.15f, 0.55f, 0.9f), FVector(0.7f)},
			{FName(TEXT("inspect_control_cabinet")), TEXT("烧毁的控制柜"), FVector(850, 20, 70), FLinearColor(0.95f, 0.36f, 0.12f), FVector(0.9f)},
			{FName(TEXT("heat_repair_room")), TEXT("维修间供暖控制器"), FVector(1050, -80, 70), FLinearColor(0.95f, 0.36f, 0.12f), FVector(0.7f)},
			{FName(TEXT("repair_generator")), TEXT("柴油发电机"), FVector(1250, 80, 90), FLinearColor(0.95f, 0.36f, 0.12f), FVector(1.2f, 0.7f, 1.1f)},
			{FName(TEXT("forced_self_repair")), TEXT("手动维修工具"), FVector(1450, 220, 55), FLinearColor(0.95f, 0.36f, 0.12f), FVector(0.65f)},
			{FName(TEXT("talk_gu_heng")), TEXT("顾衡｜工程师"), FVector(860, 160, 0), FLinearColor(0.75f, 0.28f, 0.16f), FVector(0.45f, 0.45f, 1.9f)},
			{FName(TEXT("heat_medical_room")), TEXT("医务室供暖控制器"), FVector(-120, 680, 70), FLinearColor(0.12f, 0.75f, 0.55f), FVector(0.7f)},
			{FName(TEXT("treat_character")), TEXT("诊断与治疗台"), FVector(500, 560, 70), FLinearColor(0.12f, 0.75f, 0.55f), FVector(0.72f)},
			{FName(TEXT("talk_ye_cheng")), TEXT("叶澄｜医生"), FVector(120, 850, 0), FLinearColor(0.12f, 0.65f, 0.72f), FVector(0.45f, 0.45f, 1.85f)},
			{FName(TEXT("distribute_food")), TEXT("口粮台"), FVector(900, 760, 70), FLinearColor(0.9f, 0.65f, 0.12f), FVector(0.72f)},
			{FName(TEXT("heat_kitchen")), TEXT("厨房供暖控制器"), FVector(1120, 740, 70), FLinearColor(0.9f, 0.65f, 0.12f), FVector(0.7f)},
			{FName(TEXT("rest")), TEXT("休整床位"), FVector(1500, 780, 0), FLinearColor(0.9f, 0.65f, 0.12f), FVector(0.72f)},
			{FName(TEXT("dismantle_kitchen_heater")), TEXT("厨房加热器"), FVector(1330, 850, 70), FLinearColor(0.9f, 0.65f, 0.12f), FVector(0.72f)},
			{FName(TEXT("calibrate_antenna")), TEXT("室外天线控制终端"), FVector(2050, 500, 0), FLinearColor(0.4f, 0.65f, 1.0f), FVector(0.72f)},
		};
		return Definitions;
	}
}

AWhiteoutStationBuilder::AWhiteoutStationBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWhiteoutStationBuilder::BeginPlay()
{
	Super::BeginPlay();
	const bool bUsingEditableLayout = RegisterEditableStationActors();
	if (!bUsingEditableLayout)
	{
		BuildStation();
	}
	EnsureRequiredHotspots();
	if (bUsingEditableLayout
		&& FParse::Param(FCommandLine::Get(), TEXT("WhiteoutSceneAudit")))
	{
		GetWorldTimerManager().SetTimer(
			SceneAuditTimer,
			this,
			&AWhiteoutStationBuilder::AuditStationLayout,
			1.0f,
			false);
	}
	if (UWindStationStateSubsystem* StateSubsystem = GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>())
	{
		StateSubsystem->OnActionCommitted.AddDynamic(this, &AWhiteoutStationBuilder::HandleActionCommitted);
		StateSubsystem->OnStateChanged.AddDynamic(this, &AWhiteoutStationBuilder::HandleStateChanged);
		const FWSGameState State = StateSubsystem->GetStateSnapshot();
		SetLightingPreviewState(false, State.Tasks.GeneratorProgress >= 2);
		if (State.bMidCrisisTriggered)
		{
			ApplyCrisisLighting();
		}
		if (State.Phase == EWSGamePhase::Results)
		{
			ApplyEndingPresentation(State.Ending);
		}
	}
	if (FParse::Param(
			FCommandLine::Get(),
			TEXT("WhiteoutAntennaInputSmokeReady")))
	{
		FTimerHandle AntennaInputSmokePrepTimer;
		GetWorldTimerManager().SetTimer(
			AntennaInputSmokePrepTimer,
			[this]()
			{
				UWindStationStateSubsystem* StateSubsystem =
					GetGameInstance()
						? GetGameInstance()->GetSubsystem<UWindStationStateSubsystem>()
						: nullptr;
				if (!StateSubsystem)
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("WhiteoutStation AntennaInputSmokePrep: missing state subsystem"));
					return;
				}
				const auto Commit =
					[StateSubsystem](FWSActionRequest Request)
					{
						Request.TransactionId = FGuid::NewGuid();
						return StateSubsystem->CommitAction(Request).bCommitted;
					};
				bool bPrepared = true;
				FWSActionRequest HeatRequest;
				HeatRequest.ActionId = TEXT("heat_repair_room");
				bPrepared &= Commit(HeatRequest);
				FWSActionRequest FoodRequest;
				FoodRequest.ActionId = TEXT("distribute_food");
				FoodRequest.FoodForPlayer = 1;
				FoodRequest.FoodForGuHeng = 1;
				bPrepared &= Commit(FoodRequest);
				FWSActionRequest RepairRequest;
				RepairRequest.ActionId = TEXT("repair_generator");
				RepairRequest.bHasCollaborator = true;
				RepairRequest.Collaborator = EWSCharacterId::Player;
				bPrepared &= Commit(RepairRequest);
				RepairRequest.TransactionId.Invalidate();
				RepairRequest.bForce = true;
				bPrepared &= Commit(RepairRequest);
				EWSReasonCode SettleReason = EWSReasonCode::PhaseLocked;
				FWSPhaseSummary PhaseSummary;
				bPrepared &= StateSubsystem->SettleCurrentDayPhase(
					SettleReason,
					PhaseSummary);
				EWSReasonCode BeginReason = EWSReasonCode::PhaseLocked;
				TArray<FString> BeginChanges;
				bPrepared &= StateSubsystem->BeginDayPhase(
					EWSHeatingZone::ControlRoom,
					BeginReason,
					BeginChanges);
				FWSActionRequest CalibrationRequest;
				CalibrationRequest.ActionId = TEXT("calibrate_antenna");
				const FWSActionPreview CalibrationPreview =
					StateSubsystem->PreviewAction(CalibrationRequest);
				const FWSGameState PreparedState =
					StateSubsystem->GetStateSnapshot();
				bPrepared &=
					PreparedState.Tasks.GeneratorProgress >= 2
					&& CalibrationPreview.bCanExecute;
				if (bPrepared)
				{
					UE_LOG(
						LogTemp,
						Display,
						TEXT("WhiteoutStation AntennaInputSmokePrep: ready=1 AP=%d generator=%d antenna=%d calibration_cost=%d"),
						PreparedState.ActionPoints,
						PreparedState.Tasks.GeneratorProgress,
						PreparedState.Tasks.AntennaCalibration,
						CalibrationPreview.APCost);
				}
				else
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("WhiteoutStation AntennaInputSmokePrep: ready=0 AP=%d generator=%d antenna=%d calibration_reason=%s"),
						PreparedState.ActionPoints,
						PreparedState.Tasks.GeneratorProgress,
						PreparedState.Tasks.AntennaCalibration,
						*StaticEnum<EWSReasonCode>()->GetNameStringByValue(
							static_cast<int64>(
								CalibrationPreview.ReasonCode)));
					FPlatformMisc::RequestExitWithStatus(false, 3);
				}
			},
			0.2f,
			false);
	}
}

void AWhiteoutStationBuilder::GenerateEditableStationLayout()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->WorldType != EWorldType::Editor)
	{
		UE_LOG(LogTemp, Warning, TEXT("WhiteoutStation: editable layout can only be generated in an editor level"));
		return;
	}

	const FScopedTransaction Transaction(
		NSLOCTEXT("WhiteoutStation", "GenerateEditableLayout", "Reset Whiteout Station Editable Layout"));
	Modify();
	ClearEditableStationLayoutInternal();
	bBuildingEditableLayout = true;
	BuildStation();
	bBuildingEditableLayout = false;
	EditableStationActorCount = GetEditableStationActorCount();
	if (GetLevel())
	{
		GetLevel()->MarkPackageDirty();
	}
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation: generated %d editable level actors"), EditableStationActorCount);
#endif
}

void AWhiteoutStationBuilder::ClearEditableStationLayout()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	const FScopedTransaction Transaction(
		NSLOCTEXT("WhiteoutStation", "ClearEditableLayout", "Clear Whiteout Station Editable Layout"));
	Modify();
	ClearEditableStationLayoutInternal();
	if (GetLevel())
	{
		GetLevel()->MarkPackageDirty();
	}
#endif
}

int32 AWhiteoutStationBuilder::GetEditableStationActorCount() const
{
	int32 Count = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (*It != this && It->ActorHasTag(EditableStationTag))
			{
				++Count;
			}
		}
	}
	return Count;
}

void AWhiteoutStationBuilder::ResetRuntimeActorCache()
{
	RuntimeLights.Reset();
	RuntimeEmergencyLights.Reset();
	RuntimeGeneratorLights.Reset();
	RuntimeBaseLightIntensities.Reset();
	RuntimeBaseLightColors.Reset();
	RuntimeAssemblyMeshes.Reset();
	RuntimeHotspots.Reset();
	ExteriorLight = nullptr;
	RuntimeBaseExteriorIntensity = 3.0f;
	RuntimeBaseExteriorColor = FLinearColor(0.58f, 0.7f, 0.9f);
}

bool AWhiteoutStationBuilder::RegisterEditableStationActors()
{
	ResetRuntimeActorCache();
	EditableStationActorCount = 0;
	UMaterialInterface* LegacyConcrete = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/WindStation/Art/Materials/M_WS_Concrete.M_WS_Concrete"));
	UMaterialInterface* FloorDeckMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/WindStation/Art/Materials/M_WS_FloorDeck_V08.M_WS_FloorDeck_V08"));
	UMaterialInterface* WallPanelMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/WindStation/Art/Materials/M_WS_WallPanel_V08.M_WS_WallPanel_V08"));
	int32 UpgradedArchitectureSurfaces = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == this || !Actor->ActorHasTag(EditableStationTag))
		{
			continue;
		}

		++EditableStationActorCount;
		if (ADirectionalLight* DirectionalLight = Cast<ADirectionalLight>(Actor);
			DirectionalLight && DirectionalLight->ActorHasTag(ExteriorLightTag))
		{
			ExteriorLight = Cast<UDirectionalLightComponent>(DirectionalLight->GetLightComponent());
			if (ExteriorLight)
			{
				RuntimeBaseExteriorIntensity = ExteriorLight->Intensity;
				RuntimeBaseExteriorColor = ExteriorLight->GetLightColor();
			}
		}
		if (APointLight* PointLight = Cast<APointLight>(Actor);
			PointLight && PointLight->ActorHasTag(TEXT("WSRuntimeLight")))
		{
			if (UPointLightComponent* Component = Cast<UPointLightComponent>(PointLight->GetLightComponent()))
			{
				RuntimeLights.Add(Component);
				RuntimeEmergencyLights.Add(PointLight->ActorHasTag(EmergencyLightTag));
				RuntimeGeneratorLights.Add(PointLight->ActorHasTag(GeneratorLightTag));
				RuntimeBaseLightIntensities.Add(Component->Intensity);
				RuntimeBaseLightColors.Add(Component->GetLightColor());
			}
		}
		if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
			MeshActor && MeshActor->ActorHasTag(TEXT("WSRuntimePresentation")))
		{
			RuntimeAssemblyMeshes.Add(MeshActor);
		}
		if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
			MeshActor && MeshActor->ActorHasTag(TEXT("WSRuntimeGeometry")))
		{
			UStaticMeshComponent* Component = MeshActor->GetStaticMeshComponent();
			if (Component
				&& LegacyConcrete
				&& Component->GetMaterial(0) == LegacyConcrete)
			{
				const bool bFloorSurface =
					Component->Bounds.BoxExtent.Z <= 80.0f
					&& MeshActor->GetActorLocation().Z <= 125.0f;
				UMaterialInterface* UpgradedMaterial =
					bFloorSurface ? FloorDeckMaterial : WallPanelMaterial;
				if (UpgradedMaterial)
				{
					Component->SetMaterial(0, UpgradedMaterial);
					++UpgradedArchitectureSurfaces;
				}
			}
		}
		if (AWSInteractableActor* Hotspot = Cast<AWSInteractableActor>(Actor);
			Hotspot && Hotspot->ActorHasTag(TEXT("WSRuntimeHotspot")))
		{
			RuntimeHotspots.Add(Hotspot);
		}
	}

	if (EditableStationActorCount > 0)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("WhiteoutStation: using %d editable actors saved in the level; upgraded %d legacy architecture surfaces"),
			EditableStationActorCount,
			UpgradedArchitectureSurfaces);
		return true;
	}
	return false;
}

void AWhiteoutStationBuilder::EnsureRequiredHotspots()
{
	for (AWSInteractableActor* Hotspot : RuntimeHotspots)
	{
		if (Hotspot && Hotspot->ActionId == TEXT("treat_gu_heng"))
		{
			Hotspot->Configure(
				TEXT("treat_character"),
				FText::FromString(TEXT("诊断与治疗台")),
				FLinearColor(0.12f, 0.75f, 0.55f));
		}
	}
	for (const FRequiredHotspotDefinition& Definition : RequiredHotspotDefinitions())
	{
		const bool bAlreadyPresent = RuntimeHotspots.ContainsByPredicate(
			[&Definition](const TObjectPtr<AWSInteractableActor>& Hotspot)
			{
				return Hotspot && Hotspot->ActionId == Definition.ActionId;
			});
		if (bAlreadyPresent)
		{
			continue;
		}

		const FVector SpawnLocation =
			Definition.ActionId == TEXT("distribute_food")
				? ResolveFoodHotspotLocation()
				: Definition.Location;
		AWSInteractableActor* RestoredHotspot = SpawnHotspot(
			*Definition.ActionId.ToString(),
			Definition.Label,
			SpawnLocation,
			Definition.Color,
			Definition.Scale);
		if (RestoredHotspot)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("WhiteoutStation: restored missing editable-layout hotspot %s at runtime (%s)"),
				*Definition.ActionId.ToString(),
				*SpawnLocation.ToCompactString());
		}
		else
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("WhiteoutStation: failed to restore missing editable-layout hotspot %s at runtime (%s)"),
				*Definition.ActionId.ToString(),
				*SpawnLocation.ToCompactString());
		}
	}

	for (AWSInteractableActor* Hotspot : RuntimeHotspots)
	{
		if (Hotspot && Hotspot->ActionId == TEXT("calibrate_antenna"))
		{
			ConfigureAntennaControlProxy(Hotspot);
		}
	}
}

FVector AWhiteoutStationBuilder::ResolveFoodHotspotLocation() const
{
	FVector FoodStationLocation(900.0f, 760.0f, 70.0f);
	FVector KitchenAnchor = FoodStationLocation;
	for (const TObjectPtr<AWSInteractableActor>& Hotspot : RuntimeHotspots)
	{
		if (Hotspot
			&& Hotspot->ActionId == TEXT("dismantle_kitchen_heater"))
		{
			KitchenAnchor = Hotspot->GetActorLocation();
			break;
		}
	}
	const TArray<FVector> CandidateOffsets = {
		FVector(-180.0f, -180.0f, 0.0f),
		FVector(-260.0f, -120.0f, 0.0f),
		FVector(-120.0f, -260.0f, 0.0f),
		FVector(180.0f, -180.0f, 0.0f),
		FVector(-300.0f, -260.0f, 0.0f),
		FVector(260.0f, -120.0f, 0.0f),
	};
	for (const FVector& Offset : CandidateOffsets)
	{
		FVector Candidate = KitchenAnchor + Offset;
		Candidate.Z = 70.0f;
		if (!GetWorld()->OverlapBlockingTestByChannel(
				FVector(Candidate.X, Candidate.Y, 90.0f),
				FQuat::Identity,
				ECC_Visibility,
				FCollisionShape::MakeBox(FVector(55.0f, 55.0f, 85.0f))))
		{
			return Candidate;
		}
	}
	return FoodStationLocation;
}

FVector AWhiteoutStationBuilder::ResolveAntennaControlAnchor() const
{
	const FVector DefaultAnchor(2260.0f, 500.0f, 0.0f);
	AStaticMeshActor* ClosestTerminal = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (AStaticMeshActor* MeshActor : RuntimeAssemblyMeshes)
	{
		if (!MeshActor
			|| !MeshActor->ActorHasTag(TEXT("Outdoor"))
			|| !MeshActor->GetStaticMeshComponent()
			|| !MeshActor->GetStaticMeshComponent()->GetStaticMesh()
			|| !MeshActor->GetStaticMeshComponent()->GetStaticMesh()
				->GetPathName().Contains(TEXT("Prop_Computer")))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared2D(
			MeshActor->GetActorLocation(),
			DefaultAnchor);
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestTerminal = MeshActor;
			ClosestDistanceSquared = DistanceSquared;
		}
	}
	return ClosestTerminal
		? ClosestTerminal->GetActorLocation()
		: DefaultAnchor;
}

void AWhiteoutStationBuilder::ConfigureAntennaControlProxy(
	AWSInteractableActor* Hotspot)
{
	if (!Hotspot)
	{
		return;
	}

	const FVector OriginalLocation = Hotspot->GetActorLocation();
	Hotspot->Configure(
		FName(TEXT("calibrate_antenna")),
		FText::FromString(TEXT("室外天线控制终端")),
		FLinearColor(0.4f, 0.65f, 1.0f));
	Hotspot->SetActorScale3D(FVector(0.72f));
	Hotspot->Tags.AddUnique(TEXT("WSAntennaControlProxy"));

	const FVector Anchor = ResolveAntennaControlAnchor();
	const TArray<FVector> CandidateLocations = {
		Anchor + FVector(160.0f, 0.0f, 0.0f),
		Anchor + FVector(0.0f, -160.0f, 0.0f),
		Anchor + FVector(0.0f, 160.0f, 0.0f),
		Anchor + FVector(-160.0f, 0.0f, 0.0f),
		Anchor + FVector(-140.0f, -120.0f, 0.0f),
		Anchor + FVector(-140.0f, 120.0f, 0.0f),
		FVector(2050.0f, 500.0f, 0.0f),
		FVector(2050.0f, 300.0f, 0.0f),
		FVector(1950.0f, 550.0f, 0.0f),
	};
	for (const FVector& CandidateLocation : CandidateLocations)
	{
		if (PlaceHotspotAtGroundedLocation(Hotspot, CandidateLocation)
			&& IsHotspotInteractionReachable(Hotspot))
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("WhiteoutStation: antenna control proxy ready old=%s anchor=%s proxy=%s"),
				*OriginalLocation.ToCompactString(),
				*Anchor.ToCompactString(),
				*Hotspot->GetActorLocation().ToCompactString());
			return;
		}
	}

	PlaceHotspotAtGroundedLocation(
		Hotspot,
		FVector(2050.0f, 500.0f, 0.0f));
	UE_LOG(
		LogTemp,
		Error,
		TEXT("WhiteoutStation: antenna control proxy has no verified interaction approach at %s"),
		*Hotspot->GetActorLocation().ToCompactString());
}

bool AWhiteoutStationBuilder::PlaceHotspotAtGroundedLocation(
	AWSInteractableActor* Hotspot,
	const FVector& Location) const
{
	if (!Hotspot || !GetWorld())
	{
		return false;
	}
	Hotspot->SetActorLocation(
		FVector(Location.X, Location.Y, 0.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	const FBox InitialBounds = Hotspot->GetComponentsBoundingBox(true);
	if (!InitialBounds.IsValid)
	{
		return false;
	}
	Hotspot->AddActorWorldOffset(
		FVector(0.0f, 0.0f, -InitialBounds.Min.Z),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	Hotspot->Tags.AddUnique(TEXT("WSGroundedHotspot"));

	const FBox Bounds = Hotspot->GetComponentsBoundingBox(true);
	FVector OccupancyExtent = Bounds.GetExtent();
	OccupancyExtent.X = FMath::Min(OccupancyExtent.X, 70.0f);
	OccupancyExtent.Y = FMath::Min(OccupancyExtent.Y, 70.0f);
	OccupancyExtent.Z = FMath::Max(20.0f, OccupancyExtent.Z - 10.0f);
	FCollisionQueryParams Params(
		SCENE_QUERY_STAT(WhiteoutHotspotPlacement),
		false,
		Hotspot);
	Params.AddIgnoredActor(this);
	return !GetWorld()->OverlapBlockingTestByChannel(
		Bounds.GetCenter() + FVector(0.0f, 0.0f, 10.0f),
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeBox(OccupancyExtent),
		Params);
}

bool AWhiteoutStationBuilder::IsHotspotInteractionReachable(
	const AWSInteractableActor* Hotspot) const
{
	if (!Hotspot || !GetWorld())
	{
		return false;
	}

	FVector BoundsOrigin = Hotspot->GetActorLocation();
	FVector BoundsExtent = FVector::ZeroVector;
	Hotspot->GetActorBounds(false, BoundsOrigin, BoundsExtent);
	const FVector AimPoint = Hotspot->InteractionCollision
		? Hotspot->InteractionCollision->Bounds.Origin
		: BoundsOrigin;
	const float MinimumDistance = FMath::Clamp(
		FMath::Max(BoundsExtent.X, BoundsExtent.Y) + 90.0f,
		140.0f,
		220.0f);
	const TArray<float> CandidateDistances = {
		MinimumDistance,
		FMath::Min(MinimumDistance + 60.0f, 390.0f),
		FMath::Min(MinimumDistance + 120.0f, 390.0f),
		FMath::Min(MinimumDistance + 180.0f, 390.0f),
		390.0f};
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	constexpr int32 CandidateCount = 16;
	int32 FloorCandidateCount = 0;
	int32 ClearApproachCount = 0;
	int32 InteractionCandidateCount = 0;
	FName LastSelectedAction = NAME_None;
	for (const float CandidateDistance : CandidateDistances)
	{
		for (int32 CandidateIndex = 0;
			CandidateIndex < CandidateCount;
			++CandidateIndex)
		{
			const float Angle = 2.0f * PI
				* static_cast<float>(CandidateIndex)
				/ static_cast<float>(CandidateCount);
			const FVector CandidateLocation = BoundsOrigin
				+ FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f)
					* CandidateDistance;
			FCollisionQueryParams Params(
				SCENE_QUERY_STAT(WhiteoutHotspotApproach),
				false,
				Hotspot);
			Params.AddIgnoredActor(this);
			if (PlayerPawn)
			{
				Params.AddIgnoredActor(PlayerPawn);
			}
			FHitResult FloorHit;
			if (!GetWorld()->LineTraceSingleByChannel(
					FloorHit,
					FVector(CandidateLocation.X, CandidateLocation.Y, 180.0f),
					FVector(CandidateLocation.X, CandidateLocation.Y, -120.0f),
					ECC_Visibility,
					Params))
			{
				continue;
			}
			++FloorCandidateCount;
			const FVector PawnLocation(
				CandidateLocation.X,
				CandidateLocation.Y,
				FloorHit.ImpactPoint.Z + 94.0f);
			if (GetWorld()->OverlapBlockingTestByChannel(
					PawnLocation,
					FQuat::Identity,
					ECC_Pawn,
					FCollisionShape::MakeCapsule(42.0f, 92.0f),
					Params))
			{
				continue;
			}
			++ClearApproachCount;
			const FVector CameraLocation =
				PawnLocation + FVector(0.0f, 0.0f, 64.0f);
			AWSInteractableActor* Selected =
				AWhiteoutCharacter::FindInteractableFromView(
					GetWorld(),
					CameraLocation,
					AimPoint - CameraLocation,
					this);
			if (Selected)
			{
				++InteractionCandidateCount;
				LastSelectedAction = Selected->ActionId;
			}
			if (Selected == Hotspot)
			{
				return true;
			}
		}
	}
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("WhiteoutStation HotspotReachability: action=%s location=%s aim=%s extent=%s floor=%d clear=%d selected=%d last=%s"),
		*Hotspot->ActionId.ToString(),
		*Hotspot->GetActorLocation().ToCompactString(),
		*AimPoint.ToCompactString(),
		*BoundsExtent.ToCompactString(),
		FloorCandidateCount,
		ClearApproachCount,
		InteractionCandidateCount,
		*LastSelectedAction.ToString());
	return false;
}

void AWhiteoutStationBuilder::ClearEditableStationLayoutInternal()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TArray<AActor*> ActorsToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor != this && Actor->ActorHasTag(EditableStationTag))
		{
			ActorsToDestroy.Add(Actor);
		}
	}
	for (AActor* Actor : ActorsToDestroy)
	{
		Actor->Modify();
		World->EditorDestroyActor(Actor, true);
	}
	EditableStationActorCount = 0;
	ResetRuntimeActorCache();
#endif
}

void AWhiteoutStationBuilder::MarkEditableStationActor(AActor* Actor, const TCHAR* Folder)
{
#if WITH_EDITOR
	if (!bBuildingEditableLayout || !Actor)
	{
		return;
	}
	Actor->SetFlags(RF_Transactional);
	Actor->Modify();
	Actor->Tags.AddUnique(EditableStationTag);
	Actor->SetFolderPath(FName(*FString::Printf(TEXT("WS Editable Station/%s"), Folder)));
#endif
}

void AWhiteoutStationBuilder::BuildStation()
{
	ResetRuntimeActorCache();
	UE_LOG(LogTemp, Display, TEXT("WhiteoutStation: building five-zone station and 13 action hotspots"));
	ADirectionalLight* DirectionalLight = GetWorld()->SpawnActor<ADirectionalLight>(FVector(600, 400, 900), FRotator(-52, -28, 0));
	if (DirectionalLight)
	{
		DirectionalLight->Tags.AddUnique(ExteriorLightTag);
		ExteriorLight = CastChecked<UDirectionalLightComponent>(DirectionalLight->GetLightComponent());
		ExteriorLight->SetIntensity(RuntimeBaseExteriorIntensity);
		ExteriorLight->SetLightColor(RuntimeBaseExteriorColor);
		ExteriorLight->SetMobility(EComponentMobility::Movable);
		ExteriorLight->SetAtmosphereSunLight(true);
		MarkEditableStationActor(DirectionalLight, TEXT("Lighting"));
	}
	ASkyAtmosphere* SkyAtmosphere = GetWorld()->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator);
	MarkEditableStationActor(SkyAtmosphere, TEXT("Lighting"));
	AExponentialHeightFog* HeightFog = GetWorld()->SpawnActor<AExponentialHeightFog>(FVector(0, 0, -80), FRotator::ZeroRotator);
	if (HeightFog)
	{
		HeightFog->GetComponent()->SetFogDensity(0.012f);
		HeightFog->GetComponent()->SetFogHeightFalloff(0.26f);
		HeightFog->GetComponent()->SetFogInscatteringColor(FLinearColor(0.24f, 0.34f, 0.48f));
		HeightFog->GetComponent()->SetVolumetricFog(true);
		HeightFog->GetComponent()->SetVolumetricFogExtinctionScale(0.65f);
		MarkEditableStationActor(HeightFog, TEXT("Lighting"));
	}
	ASkyLight* SkyLight = GetWorld()->SpawnActor<ASkyLight>(FVector(600, 400, 500), FRotator::ZeroRotator);
	if (SkyLight)
	{
		SkyLight->GetLightComponent()->SetIntensity(0.65f);
		SkyLight->GetLightComponent()->SetLightColor(FLinearColor(0.35f, 0.48f, 0.7f));
		SkyLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		SkyLight->GetLightComponent()->RecaptureSky();
		MarkEditableStationActor(SkyLight, TEXT("Lighting"));
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
		PostProcess->Settings.BloomIntensity = 0.16f;
		PostProcess->Settings.bOverride_VignetteIntensity = true;
		PostProcess->Settings.VignetteIntensity = 0.22f;
		MarkEditableStationActor(PostProcess, TEXT("Lighting"));
	}
	AWhiteoutSnowField* SnowField = GetWorld()->SpawnActor<AWhiteoutSnowField>(FVector::ZeroVector, FRotator::ZeroRotator);
	MarkEditableStationActor(SnowField, TEXT("VFX"));
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

	SpawnHotspot(TEXT("investigate_generator_log"), TEXT("发电机运行记录"), FVector(-120, -165, 70), Control, FVector(0.62f));
	SpawnHotspot(TEXT("send_signal"), TEXT("应急无线电"), FVector(280, -165, 70), Control, FVector(0.58f));
	SpawnHotspot(TEXT("heat_control_room"), TEXT("控制室供暖控制器"), FVector(400, 180, 70), Control, FVector(0.7f));
	SpawnHotspot(TEXT("inspect_control_cabinet"), TEXT("烧毁的控制柜"), FVector(850, 20, 70), Repair, FVector(0.9f));
	SpawnHotspot(TEXT("heat_repair_room"), TEXT("维修间供暖控制器"), FVector(1050, -80, 70), Repair, FVector(0.7f));
	SpawnHotspot(TEXT("repair_generator"), TEXT("柴油发电机"), FVector(1250, 80, 90), Repair, FVector(1.2f, 0.7f, 1.1f));
	SpawnHotspot(TEXT("forced_self_repair"), TEXT("手动维修工具"), FVector(1450, 220, 55), Repair, FVector(0.65f));
	SpawnHotspot(TEXT("talk_gu_heng"), TEXT("顾衡｜工程师"), FVector(860, 160, 0), FLinearColor(0.75f, 0.28f, 0.16f), FVector(0.45f, 0.45f, 1.9f));

	SpawnHotspot(TEXT("heat_medical_room"), TEXT("医务室供暖控制器"), FVector(-120, 680, 70), Medical, FVector(0.7f));
	SpawnHotspot(TEXT("treat_character"), TEXT("诊断与治疗台"), FVector(500, 560, 70), Medical, FVector(0.72f));
	SpawnHotspot(TEXT("talk_ye_cheng"), TEXT("叶澄｜医生"), FVector(120, 850, 0), FLinearColor(0.12f, 0.65f, 0.72f), FVector(0.45f, 0.45f, 1.85f));

	SpawnHotspot(TEXT("distribute_food"), TEXT("口粮台"), FVector(900, 760, 70), Quarter, FVector(0.72f));
	SpawnHotspot(TEXT("heat_kitchen"), TEXT("厨房供暖控制器"), FVector(1120, 740, 70), Quarter, FVector(0.7f));
	SpawnHotspot(TEXT("rest"), TEXT("休整床位"), FVector(1500, 780, 0), Quarter, FVector(0.72f));
	SpawnHotspot(TEXT("dismantle_kitchen_heater"), TEXT("厨房加热器"), FVector(1330, 850, 70), Quarter, FVector(0.72f));
	SpawnHotspot(TEXT("calibrate_antenna"), TEXT("结冰的天线阵列"), FVector(2300, 400, 135), Outdoor, FVector(0.8f, 0.8f, 2.7f));

	if (FParse::Param(FCommandLine::Get(), TEXT("WhiteoutSceneAudit")))
	{
		GetWorldTimerManager().SetTimer(SceneAuditTimer, this, &AWhiteoutStationBuilder::AuditStationLayout, 1.0f, false);
	}
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
	MeshActor->Tags.Add(Placement.bCollision ? TEXT("WSExpectedCollision") : TEXT("WSExpectedNoCollision"));
	MeshActor->SetActorScale3D(Placement.Transform.GetScale3D());
	UStaticMeshComponent* Component = MeshActor->GetStaticMeshComponent();
	Component->SetStaticMesh(StaticMesh);
	Component->SetMobility(EComponentMobility::Movable);
	MeshActor->SetActorEnableCollision(Placement.bCollision);
	Component->SetCollisionProfileName(Placement.bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
	Component->SetCollisionEnabled(Placement.bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (UMaterialInterface* Material = Placement.Material.LoadSynchronous())
	{
		for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
		{
			Component->SetMaterial(Index, Material);
		}
	}
	if (Placement.bCollision && Placement.Transform.GetLocation().Z <= 100.0f)
	{
		const FBox Bounds = MeshActor->GetComponentsBoundingBox(true);
		if (Bounds.IsValid)
		{
			MeshActor->AddActorWorldOffset(FVector(0.0f, 0.0f, -Bounds.Min.Z), false, nullptr, ETeleportType::TeleportPhysics);
			MeshActor->Tags.Add(TEXT("WSFloorProp"));
		}
	}
	RuntimeAssemblyMeshes.Add(MeshActor);
	MarkEditableStationActor(
		MeshActor,
		*FString::Printf(TEXT("Presentation/%s"), *Placement.Zone.ToString()));
}

void AWhiteoutStationBuilder::SpawnAssemblyLight(const FWSStationLightPlacement& Placement)
{
	FVector Location = Placement.Location;
	FLinearColor Color = Placement.Color;
	float Intensity = Placement.Intensity;
	float Radius = Placement.Radius;
	if (Placement.Label == TEXT("控制室冷色补光"))
	{
		// Keep the ceiling strip readable instead of driving its white surface into clipping.
		Color = FLinearColor(0.28f, 0.50f, 0.78f);
		Intensity = 820.0f;
		Radius = 700.0f;
	}
	else if (Placement.Label == TEXT("天线检修灯"))
	{
		// A broad camera-side fill preserves the antenna silhouette against the snow sky.
		Location = FVector(2140.0f, 400.0f, 245.0f);
		Color = FLinearColor(0.48f, 0.63f, 0.92f);
		Intensity = 4800.0f;
		Radius = 1150.0f;
	}
	SpawnPointLight(
		Placement.Label.ToString(),
		Placement.Zone,
		Location,
		Color,
		Intensity,
		Radius,
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
	const bool bFloorSurface =
		!bSnowSurface
		&& !bMetalSurface
		&& Location.Z <= 125.0f
		&& Scale.Z <= 0.8f;
	const TCHAR* MaterialPath = bSnowSurface
		? TEXT("/Game/WindStation/Art/Materials/M_WS_Snow.M_WS_Snow")
		: bMetalSurface
			? TEXT("/Game/WindStation/Art/Materials/M_WS_RustedMetal.M_WS_RustedMetal")
			: bFloorSurface
				? TEXT("/Game/WindStation/Art/Materials/M_WS_FloorDeck_V08.M_WS_FloorDeck_V08")
				: TEXT("/Game/WindStation/Art/Materials/M_WS_WallPanel_V08.M_WS_WallPanel_V08");
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
	MarkEditableStationActor(Block, TEXT("Geometry"));
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
	MarkEditableStationActor(Sign, TEXT("Signs"));
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
	if (bEmergencyRed)
	{
		PointLight->Tags.AddUnique(EmergencyLightTag);
	}
	if (bGeneratorPowered)
	{
		PointLight->Tags.AddUnique(GeneratorLightTag);
	}
	UPointLightComponent* Component = CastChecked<UPointLightComponent>(PointLight->GetLightComponent());
	Component->SetMobility(EComponentMobility::Movable);
	bool bGeneratorOnline = bBuildingEditableLayout;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UWindStationStateSubsystem* StateSubsystem = GameInstance->GetSubsystem<UWindStationStateSubsystem>())
		{
			bGeneratorOnline = StateSubsystem->GetStateSnapshot().Tasks.GeneratorProgress >= 2;
		}
	}
	Component->SetIntensity(bGeneratorPowered && !bGeneratorOnline ? Intensity * 0.22f : Intensity);
	Component->SetAttenuationRadius(Radius);
	Component->SetLightColor(Color);
	RuntimeLights.Add(Component);
	RuntimeEmergencyLights.Add(bEmergencyRed);
	RuntimeGeneratorLights.Add(bGeneratorPowered);
	RuntimeBaseLightIntensities.Add(Intensity);
	RuntimeBaseLightColors.Add(Color);
	MarkEditableStationActor(PointLight, TEXT("Lighting"));
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
		ExteriorLight->SetIntensity(RuntimeBaseExteriorIntensity);
		ExteriorLight->SetLightColor(RuntimeBaseExteriorColor);
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
			const FBox Bounds = Hotspot->GetComponentsBoundingBox(true);
			if (Bounds.IsValid)
			{
				Hotspot->AddActorWorldOffset(FVector(0.0f, 0.0f, -Bounds.Min.Z), false, nullptr, ETeleportType::TeleportPhysics);
				Hotspot->Tags.Add(TEXT("WSGroundedHotspot"));
			}
		}
		Hotspot->Tags.Add(TEXT("WSRuntimeHotspot"));
		RuntimeHotspots.Add(Hotspot);
		MarkEditableStationActor(Hotspot, TEXT("Hotspots"));
	}
	return Hotspot;
}

void AWhiteoutStationBuilder::AuditStationLayout()
{
	int32 GroundedChecked = 0;
	int32 GroundedPassed = 0;
	int32 CollisionChecked = 0;
	int32 CollisionMatched = 0;
	TArray<TSharedPtr<FJsonValue>> CollisionMismatches;
	TArray<TSharedPtr<FJsonValue>> UnsupportedProps;
	for (AStaticMeshActor* MeshActor : RuntimeAssemblyMeshes)
	{
		if (!MeshActor || !MeshActor->GetStaticMeshComponent())
		{
			continue;
		}
		const bool bExpectedCollision = MeshActor->ActorHasTag(TEXT("WSExpectedCollision"));
		const bool bCollisionEnabled = MeshActor->GetStaticMeshComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
		++CollisionChecked;
		if (bExpectedCollision == bCollisionEnabled)
		{
			++CollisionMatched;
		}
		else
		{
			CollisionMismatches.Add(MakeShared<FJsonValueString>(MeshActor->GetActorNameOrLabel()));
		}
		if (!MeshActor->ActorHasTag(TEXT("WSFloorProp")))
		{
			continue;
		}
		++GroundedChecked;
		const FBox Bounds = MeshActor->GetComponentsBoundingBox(true);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(WhiteoutGroundAudit), false, MeshActor);
		Params.AddIgnoredActor(this);
		const FVector Start(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z + 12.0f);
		const FVector End(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z - 30.0f);
		const bool bRaySupported = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)
			&& FMath::Abs(Hit.ImpactPoint.Z - Bounds.Min.Z) <= 3.0f;
		// Runtime placement snaps low props to the station's shared Z=0 floor plane.
		// The plane check covers open-frame props whose center ray passes between legs.
		const bool bSupported =
			bRaySupported || FMath::Abs(Bounds.Min.Z) <= 6.0f;
		if (bSupported)
		{
			++GroundedPassed;
		}
		else
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("WhiteoutStation GroundAudit: actor=%s location=%s bounds_min=%s center_hit=%d impact=%s mesh=%s"),
				*MeshActor->GetActorNameOrLabel(),
				*MeshActor->GetActorLocation().ToCompactString(),
				*Bounds.Min.ToCompactString(),
				bRaySupported,
				*Hit.ImpactPoint.ToCompactString(),
				MeshActor->GetStaticMeshComponent()
					&& MeshActor->GetStaticMeshComponent()->GetStaticMesh()
					? *MeshActor->GetStaticMeshComponent()->GetStaticMesh()
						->GetPathName()
					: TEXT("None"));
			UnsupportedProps.Add(MakeShared<FJsonValueString>(MeshActor->GetActorNameOrLabel()));
		}
	}

	struct FCorridorProbe
	{
		const TCHAR* Name;
		FVector Start;
		FVector End;
	};
	const TArray<FCorridorProbe> CorridorProbes = {
		{TEXT("central_east_west"), FVector(500.0f, 300.0f, 90.0f), FVector(900.0f, 300.0f, 90.0f)},
		{TEXT("central_north_south"), FVector(700.0f, 285.0f, 90.0f), FVector(700.0f, 515.0f, 90.0f)},
		{TEXT("outdoor_airlock"), FVector(1500.0f, 500.0f, 90.0f), FVector(1900.0f, 500.0f, 90.0f)}};
	int32 CorridorsPassed = 0;
	TArray<TSharedPtr<FJsonValue>> BlockedCorridors;
	for (const FCorridorProbe& Probe : CorridorProbes)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(WhiteoutCorridorAudit), false, this);
		const bool bBlocked = GetWorld()->SweepTestByChannel(
			Probe.Start,
			Probe.End,
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeCapsule(34.0f, 88.0f),
			Params);
		if (!bBlocked)
		{
			++CorridorsPassed;
		}
		else
		{
			BlockedCorridors.Add(MakeShared<FJsonValueString>(Probe.Name));
		}
	}

	int32 ReachableHotspots = 0;
	int32 HotspotsChecked = 0;
	TArray<TSharedPtr<FJsonValue>> UnreachableHotspots;
	TMap<FName, int32> HotspotActionCounts;
	for (AWSInteractableActor* Hotspot : RuntimeHotspots)
	{
		if (!Hotspot)
		{
			continue;
		}
		++HotspotsChecked;
		HotspotActionCounts.FindOrAdd(Hotspot->ActionId) += 1;
		if (IsHotspotInteractionReachable(Hotspot))
		{
			++ReachableHotspots;
		}
		else
		{
			UnreachableHotspots.Add(MakeShared<FJsonValueString>(Hotspot->ActionId.ToString()));
		}
	}

	TArray<TSharedPtr<FJsonValue>> MissingRequiredHotspots;
	TArray<TSharedPtr<FJsonValue>> DuplicateHotspots;
	for (const FRequiredHotspotDefinition& Definition : RequiredHotspotDefinitions())
	{
		const int32 Count = HotspotActionCounts.FindRef(Definition.ActionId);
		if (Count == 0)
		{
			MissingRequiredHotspots.Add(
				MakeShared<FJsonValueString>(Definition.ActionId.ToString()));
		}
		else if (Count > 1)
		{
			DuplicateHotspots.Add(
				MakeShared<FJsonValueString>(Definition.ActionId.ToString()));
		}
	}

	const bool bPassed = GroundedChecked > 0 && GroundedPassed == GroundedChecked
		&& CollisionChecked == RuntimeAssemblyMeshes.Num() && CollisionMatched == CollisionChecked
		&& CorridorsPassed == CorridorProbes.Num()
		&& ReachableHotspots == HotspotsChecked
		&& MissingRequiredHotspots.IsEmpty()
		&& DuplicateHotspots.IsEmpty();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("passed"), bPassed);
	Root->SetNumberField(TEXT("assembly_meshes"), RuntimeAssemblyMeshes.Num());
	Root->SetNumberField(TEXT("collision_checked"), CollisionChecked);
	Root->SetNumberField(TEXT("collision_matched"), CollisionMatched);
	Root->SetArrayField(TEXT("collision_mismatches"), CollisionMismatches);
	Root->SetNumberField(TEXT("grounded_checked"), GroundedChecked);
	Root->SetNumberField(TEXT("grounded_passed"), GroundedPassed);
	Root->SetArrayField(TEXT("unsupported_props"), UnsupportedProps);
	Root->SetNumberField(TEXT("corridors_checked"), CorridorProbes.Num());
	Root->SetNumberField(TEXT("corridors_passed"), CorridorsPassed);
	Root->SetArrayField(TEXT("blocked_corridors"), BlockedCorridors);
	Root->SetNumberField(TEXT("required_hotspots"), RequiredHotspotDefinitions().Num());
	Root->SetNumberField(TEXT("hotspots_checked"), HotspotsChecked);
	Root->SetNumberField(TEXT("hotspots_reachable"), ReachableHotspots);
	Root->SetArrayField(TEXT("unreachable_hotspots"), UnreachableHotspots);
	Root->SetArrayField(TEXT("missing_required_hotspots"), MissingRequiredHotspots);
	Root->SetArrayField(TEXT("duplicate_hotspots"), DuplicateHotspots);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	const FString OutputPath = FPaths::ProjectSavedDir() / TEXT("Automation/v03-g3-scene-audit.json");
	FFileHelper::SaveStringToFile(Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Display,
		TEXT("WhiteoutStation SceneAudit: passed=%d ground=%d/%d collision=%d/%d corridors=%d/%d hotspots=%d/%d output=%s"),
		bPassed, GroundedPassed, GroundedChecked, CollisionMatched, CollisionChecked,
		CorridorsPassed, CorridorProbes.Num(), ReachableHotspots, HotspotsChecked, *OutputPath);
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}
