#include "Presentation/WSPresentationData.h"

#include "Presentation/WSPresentationText.h"

namespace
{
	void AddPlacement(
		TArray<FWSStationMeshPlacement>& Placements,
		const TCHAR* Label,
		const TCHAR* Zone,
		const FString& MeshPath,
		const FVector Location,
		const FRotator Rotation = FRotator::ZeroRotator,
		const FVector Scale = FVector::OneVector,
		const TCHAR* MaterialPath = TEXT("/Game/WindStation/Art/Materials/M_WS_PaintedMetal.M_WS_PaintedMetal"),
		const bool bCollision = true)
	{
		FWSStationMeshPlacement& Placement = Placements.AddDefaulted_GetRef();
		Placement.Label = FName(Label);
		Placement.Zone = FName(Zone);
		Placement.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		Placement.Transform = FTransform(Rotation, Location, Scale);
		Placement.Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath));
		Placement.bCollision = bCollision;
	}
}

UWSStationAssemblyData::UWSStationAssemblyData()
{
	const TCHAR* Q = TEXT("/Game/WindStation/Art/Environment/Quaternius/");
	const TCHAR* Paint = TEXT("/Game/WindStation/Art/Materials/M_WS_PaintedMetal.M_WS_PaintedMetal");
	const TCHAR* Rust = TEXT("/Game/WindStation/Art/Materials/M_WS_RustedMetal.M_WS_RustedMetal");
	const TCHAR* Concrete = TEXT("/Game/WindStation/Art/Materials/M_WS_Concrete.M_WS_Concrete");

	// Control room sample: modular wall skin, window rhythm and a dense radio desk.
	AddPlacement(Placements, TEXT("控制室墙板01"), TEXT("Control"), FString(Q) + TEXT("Walls/WallAstra_Straight.WallAstra_Straight"), FVector(-180, -284, 120), FRotator(0, 0, 0), FVector(1.0f), Paint, false);
	AddPlacement(Placements, TEXT("控制室墙板02"), TEXT("Control"), FString(Q) + TEXT("Walls/WallAstra_Straight_Window.WallAstra_Straight_Window"), FVector(60, -284, 120), FRotator(0, 0, 0), FVector(1.0f), Paint, false);
	AddPlacement(Placements, TEXT("控制室墙板03"), TEXT("Control"), FString(Q) + TEXT("Walls/WallBand_Straight.WallBand_Straight"), FVector(300, -284, 120), FRotator(0, 0, 0), FVector(1.0f), Paint, false);
	AddPlacement(Placements, TEXT("控制台主机01"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_Computer.Prop_Computer"), FVector(-115, -150, 58), FRotator(0, 90, 0), FVector(1.15f), Paint);
	AddPlacement(Placements, TEXT("控制台主机02"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_Computer.Prop_Computer"), FVector(20, -150, 58), FRotator(0, 90, 0), FVector(1.15f), Paint);
	AddPlacement(Placements, TEXT("控制台主机03"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_Computer.Prop_Computer"), FVector(155, -150, 58), FRotator(0, 90, 0), FVector(1.15f), Paint);
	AddPlacement(Placements, TEXT("无线电接入点"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_AccessPoint.Prop_AccessPoint"), FVector(385, -210, 115), FRotator(0, 90, 0), FVector(1.1f), Rust);
	AddPlacement(Placements, TEXT("顶棚线槽"), TEXT("Control"), FString(Q) + TEXT("Walls/TopCables_Straight.TopCables_Straight"), FVector(60, -260, 310), FRotator(0, 0, 0), FVector(1.0f), Rust, false);
	AddPlacement(Placements, TEXT("控制室灯带"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_Light_Wide.Prop_Light_Wide"), FVector(80, 120, 325), FRotator(90, 0, 0), FVector(1.15f), Paint, false);

	AddPlacement(Placements, TEXT("维修间结构柱"), TEXT("Repair"), FString(Q) + TEXT("Columns/Column_MetalSupport.Column_MetalSupport"), FVector(760, -250, 0), FRotator::ZeroRotator, FVector(1.1f), Rust);
	AddPlacement(Placements, TEXT("维修间管束"), TEXT("Repair"), FString(Q) + TEXT("Columns/Column_Pipes.Column_Pipes"), FVector(1510, -245, 0), FRotator::ZeroRotator, FVector(1.0f), Rust);
	AddPlacement(Placements, TEXT("维修间通风机"), TEXT("Repair"), FString(Q) + TEXT("Props/Prop_Vent_Big.Prop_Vent_Big"), FVector(1180, -270, 195), FRotator(0, 0, 0), FVector(1.4f), Rust, false);
	AddPlacement(Placements, TEXT("维修间物资箱01"), TEXT("Repair"), FString(Q) + TEXT("Props/Prop_Crate3.Prop_Crate3"), FVector(1510, 150, 40), FRotator(0, 15, 0), FVector(1.0f), Paint);
	AddPlacement(Placements, TEXT("维修间油桶"), TEXT("Repair"), FString(Q) + TEXT("Props/Prop_Barrel_Large.Prop_Barrel_Large"), FVector(1580, -145, 55), FRotator::ZeroRotator, FVector(1.0f), Rust);

	AddPlacement(Placements, TEXT("医务室墙灯"), TEXT("Medical"), FString(Q) + TEXT("Props/Prop_Light_Corner.Prop_Light_Corner"), FVector(-220, 750, 240), FRotator(0, 90, 0), FVector(1.0f), Paint, false);
	AddPlacement(Placements, TEXT("医务室设备"), TEXT("Medical"), FString(Q) + TEXT("Props/Prop_Computer.Prop_Computer"), FVector(-125, 910, 70), FRotator(0, -90, 0), FVector(0.9f), Paint);
	AddPlacement(Placements, TEXT("医务室通风口"), TEXT("Medical"), FString(Q) + TEXT("Props/Prop_Vent_Wide.Prop_Vent_Wide"), FVector(360, 1080, 230), FRotator(0, 180, 0), FVector(1.2f), Paint, false);

	AddPlacement(Placements, TEXT("宿舍物资箱01"), TEXT("Quarters"), FString(Q) + TEXT("Props/Prop_Crate4.Prop_Crate4"), FVector(1460, 980, 45), FRotator(0, -12, 0), FVector(1.0f), Paint);
	AddPlacement(Placements, TEXT("宿舍物资箱02"), TEXT("Quarters"), FString(Q) + TEXT("Props/Prop_Crate3.Prop_Crate3"), FVector(1515, 890, 40), FRotator(0, 8, 0), FVector(0.8f), Paint);
	AddPlacement(Placements, TEXT("厨房排风"), TEXT("Quarters"), FString(Q) + TEXT("Props/Prop_Fan_Small.Prop_Fan_Small"), FVector(1290, 1080, 210), FRotator(0, 180, 0), FVector(1.25f), Rust, false);

	AddPlacement(Placements, TEXT("室外平台01"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Platform_Metal.Platform_Metal"), FVector(2050, 400, 5), FRotator::ZeroRotator, FVector(2.2f, 2.2f, 1.0f), Rust);
	AddPlacement(Placements, TEXT("室外平台02"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Platform_Metal.Platform_Metal"), FVector(2420, 400, 5), FRotator::ZeroRotator, FVector(2.2f, 2.2f, 1.0f), Rust);
	AddPlacement(Placements, TEXT("室外护栏北"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Platform_Rails_4.Platform_Rails_4"), FVector(2250, 80, 45), FRotator(0, 0, 0), FVector(1.8f), Paint);
	AddPlacement(Placements, TEXT("室外护栏南"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Platform_Rails_4.Platform_Rails_4"), FVector(2250, 720, 45), FRotator(0, 180, 0), FVector(1.8f), Paint);
	AddPlacement(Placements, TEXT("室外台阶"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Platform_Stairs_4.Platform_Stairs_4"), FVector(1780, 400, 0), FRotator(0, 90, 0), FVector(1.3f), Rust);
	AddPlacement(Placements, TEXT("天线检修门"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Door_Frame_SquareTall.Door_Frame_SquareTall"), FVector(1710, 400, 115), FRotator(0, 90, 0), FVector(1.05f), Concrete, false);
}

UWSUIDesignData::UWSUIDesignData() = default;

UWSReasonPresentationData::UWSReasonPresentationData()
{
	for (int32 Value = static_cast<int32>(EWSReasonCode::Ok); Value <= static_cast<int32>(EWSReasonCode::NeedsAntenna); ++Value)
	{
		const EWSReasonCode Reason = static_cast<EWSReasonCode>(Value);
		FWSReasonPresentation& Copy = Reasons.Add(Reason);
		Copy.Cause = FWSPresentationText::ReasonCause(Reason);
		Copy.ChangeCondition = FWSPresentationText::ReasonNextStep(Reason);
	}
}
