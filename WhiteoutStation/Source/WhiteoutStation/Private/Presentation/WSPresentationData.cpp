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

	void AddLight(
		TArray<FWSStationLightPlacement>& Lights,
		const TCHAR* Label,
		const TCHAR* Zone,
		const FVector Location,
		const FLinearColor Color,
		const float Intensity,
		const float Radius,
		const bool bEmergencyRed = false,
		const bool bGeneratorPowered = false)
	{
		FWSStationLightPlacement& Light = Lights.AddDefaulted_GetRef();
		Light.Label = FName(Label);
		Light.Zone = FName(Zone);
		Light.Location = Location;
		Light.Color = Color;
		Light.Intensity = Intensity;
		Light.Radius = Radius;
		Light.bEmergencyRed = bEmergencyRed;
		Light.bGeneratorPowered = bGeneratorPowered;
	}
}

UWSStationAssemblyData::UWSStationAssemblyData()
{
	const TCHAR* Q = TEXT("/Game/WindStation/Art/Environment/Quaternius/");
	const TCHAR* I = TEXT("/Game/WindStation/Art/Environment/Quaternius/Interior/");
	const TCHAR* Paint = TEXT("/Game/WindStation/Art/Materials/M_WS_PaintedMetal.M_WS_PaintedMetal");
	const TCHAR* Rust = TEXT("/Game/WindStation/Art/Materials/M_WS_RustedMetal.M_WS_RustedMetal");
	const TCHAR* Concrete = TEXT("/Game/WindStation/Art/Materials/M_WS_Concrete.M_WS_Concrete");
	const TCHAR* Fabric = TEXT("/Game/WindStation/Art/Materials/M_WS_WinterFabric.M_WS_WinterFabric");

	AddLight(Lights, TEXT("控制室冷色补光"), TEXT("Control"), FVector(80, 130, 250), FLinearColor(0.2f, 0.55f, 1.0f), 1550.0f, 850.0f);
	AddLight(Lights, TEXT("控制台暖色任务灯"), TEXT("Control"), FVector(40, -80, 205), FLinearColor(1.0f, 0.46f, 0.18f), 650.0f, 420.0f, true);
	AddLight(Lights, TEXT("维修间警示灯"), TEXT("Repair"), FVector(1120, 120, 260), FLinearColor(1.0f, 0.28f, 0.08f), 1450.0f, 820.0f, true, true);
	AddLight(Lights, TEXT("医务室任务灯"), TEXT("Medical"), FVector(80, 780, 245), FLinearColor(0.35f, 0.85f, 0.72f), 1450.0f, 820.0f);
	AddLight(Lights, TEXT("厨房宿舍生活灯"), TEXT("Quarters"), FVector(1120, 780, 245), FLinearColor(1.0f, 0.62f, 0.22f), 1900.0f, 820.0f, true);
	AddLight(Lights, TEXT("天线检修灯"), TEXT("Outdoor"), FVector(2100, 400, 330), FLinearColor(0.28f, 0.48f, 1.0f), 2600.0f, 1000.0f);

	// Control room sample: a dense radio desk against the station's collision-safe shell.
	AddPlacement(Placements, TEXT("控制台主机01"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_Computer.Prop_Computer"), FVector(-155, -190, 0), FRotator(0, 90, 0), FVector(0.72f), Paint);
	AddPlacement(Placements, TEXT("控制台主机02"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_Computer.Prop_Computer"), FVector(0, -190, 0), FRotator(0, 90, 0), FVector(0.72f), Paint);
	AddPlacement(Placements, TEXT("控制台主机03"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_Computer.Prop_Computer"), FVector(155, -190, 0), FRotator(0, 90, 0), FVector(0.72f), Paint);
	AddPlacement(Placements, TEXT("无线电接入点"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_AccessPoint.Prop_AccessPoint"), FVector(385, -270, 115), FRotator(0, 90, 0), FVector(1.1f), Paint, false);
	AddPlacement(Placements, TEXT("控制台座椅01"), TEXT("Control"), FString(I) + TEXT("Chair_1.Chair_1"), FVector(-85, -55, 0), FRotator(0, -90, 0), FVector(0.55f), Fabric);
	AddPlacement(Placements, TEXT("控制台座椅02"), TEXT("Control"), FString(I) + TEXT("Chair_1.Chair_1"), FVector(115, -55, 0), FRotator(0, -90, 0), FVector(0.55f), Fabric);
	AddPlacement(Placements, TEXT("控制台线缆束"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_Cable_3.Prop_Cable_3"), FVector(260, -265, 18), FRotator(0, 90, 0), FVector(1.1f), Rust, false);
	AddPlacement(Placements, TEXT("控制室记录架"), TEXT("Control"), FString(I) + TEXT("Bookshelf.Bookshelf"), FVector(-255, 210, 0), FRotator(0, 90, 0), FVector(0.55f), Paint);
	AddPlacement(Placements, TEXT("控制室备用箱"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_Crate3.Prop_Crate3"), FVector(430, 230, 0), FRotator(0, -12, 0), FVector(0.72f), Paint);
	AddPlacement(Placements, TEXT("控制室顶灯"), TEXT("Control"), FString(Q) + TEXT("Props/Prop_Light_Wide.Prop_Light_Wide"), FVector(60, -275, 275), FRotator(0, 0, 0), FVector(1.25f), Paint, false);

	AddPlacement(Placements, TEXT("维修间结构柱"), TEXT("Repair"), FString(Q) + TEXT("Columns/Column_MetalSupport.Column_MetalSupport"), FVector(760, -250, 0), FRotator::ZeroRotator, FVector(1.1f), Rust);
	AddPlacement(Placements, TEXT("维修间管束"), TEXT("Repair"), FString(Q) + TEXT("Columns/Column_Pipes.Column_Pipes"), FVector(1510, -245, 0), FRotator::ZeroRotator, FVector(1.0f), Rust);
	AddPlacement(Placements, TEXT("维修间通风机"), TEXT("Repair"), FString(Q) + TEXT("Props/Prop_Vent_Big.Prop_Vent_Big"), FVector(1180, -270, 195), FRotator(0, 0, 0), FVector(1.4f), Rust, false);
	AddPlacement(Placements, TEXT("维修间物资箱01"), TEXT("Repair"), FString(Q) + TEXT("Props/Prop_Crate3.Prop_Crate3"), FVector(1370, 235, 0), FRotator(0, 15, 0), FVector(0.9f), Paint);
	AddPlacement(Placements, TEXT("维修间油桶"), TEXT("Repair"), FString(Q) + TEXT("Props/Prop_Barrel_Large.Prop_Barrel_Large"), FVector(1580, -145, 0), FRotator::ZeroRotator, FVector(1.0f), Rust);
	AddPlacement(Placements, TEXT("维修间管卡01"), TEXT("Repair"), FString(Q) + TEXT("Props/Prop_PipeHolder.Prop_PipeHolder"), FVector(980, -275, 185), FRotator(0, 0, 0), FVector(1.0f), Rust, false);
	AddPlacement(Placements, TEXT("维修间管卡02"), TEXT("Repair"), FString(Q) + TEXT("Props/Prop_PipeHolder.Prop_PipeHolder"), FVector(1380, -275, 185), FRotator(0, 0, 0), FVector(1.0f), Rust, false);
	AddPlacement(Placements, TEXT("维修间地面线缆"), TEXT("Repair"), FString(Q) + TEXT("Props/Prop_Cable_1.Prop_Cable_1"), FVector(1180, 245, 8), FRotator(0, 24, 0), FVector(1.35f), Rust, false);
	AddPlacement(Placements, TEXT("维修间工具架"), TEXT("Repair"), FString(I) + TEXT("Shelf_Large.Shelf_Large"), FVector(1600, 205, 0), FRotator(0, -90, 0), FVector(0.58f), Paint);
	AddPlacement(Placements, TEXT("维修间工作凳"), TEXT("Repair"), FString(I) + TEXT("Stool.Stool"), FVector(1420, 220, 0), FRotator::ZeroRotator, FVector(0.58f), Paint);
	AddPlacement(Placements, TEXT("维修间零件箱"), TEXT("Repair"), FString(Q) + TEXT("Props/Prop_Crate4.Prop_Crate4"), FVector(1515, 60, 0), FRotator(0, -8, 0), FVector(0.72f), Paint);

	AddPlacement(Placements, TEXT("医务室墙灯"), TEXT("Medical"), FString(Q) + TEXT("Props/Prop_Light_Corner.Prop_Light_Corner"), FVector(-220, 750, 240), FRotator(0, 90, 0), FVector(1.0f), Paint, false);
	AddPlacement(Placements, TEXT("医务室设备"), TEXT("Medical"), FString(Q) + TEXT("Props/Prop_Computer.Prop_Computer"), FVector(-125, 910, 0), FRotator(0, -90, 0), FVector(0.82f), Paint);
	AddPlacement(Placements, TEXT("医务室通风口"), TEXT("Medical"), FString(Q) + TEXT("Props/Prop_Vent_Wide.Prop_Vent_Wide"), FVector(360, 1080, 230), FRotator(0, 180, 0), FVector(1.2f), Paint, false);
	AddPlacement(Placements, TEXT("医务床"), TEXT("Medical"), FString(I) + TEXT("Bed_Single.Bed_Single"), FVector(340, 760, 0), FRotator(0, 90, 0), FVector(0.65f), Fabric);
	AddPlacement(Placements, TEXT("医务器械柜"), TEXT("Medical"), FString(I) + TEXT("Kitchen_Cabinet1.Kitchen_Cabinet1"), FVector(-215, 975, 0), FRotator(0, 90, 0), FVector(0.65f), Paint);
	AddPlacement(Placements, TEXT("医务抽屉柜"), TEXT("Medical"), FString(I) + TEXT("Drawer_2.Drawer_2"), FVector(365, 985, 0), FRotator(0, 180, 0), FVector(0.6f), Paint);
	AddPlacement(Placements, TEXT("医务床头柜"), TEXT("Medical"), FString(I) + TEXT("NightStand_1.NightStand_1"), FVector(505, 760, 0), FRotator(0, 90, 0), FVector(0.6f), Paint);
	AddPlacement(Placements, TEXT("医务陪护椅"), TEXT("Medical"), FString(I) + TEXT("Chair_1.Chair_1"), FVector(180, 930, 0), FRotator(0, 20, 0), FVector(0.52f), Fabric);
	AddPlacement(Placements, TEXT("医务药品架"), TEXT("Medical"), FString(I) + TEXT("Shelf_Large.Shelf_Large"), FVector(-225, 560, 0), FRotator(0, 90, 0), FVector(0.55f), Paint);
	AddPlacement(Placements, TEXT("医务搪瓷盘"), TEXT("Medical"), FString(I) + TEXT("Plate_1.Plate_1"), FVector(505, 760, 74), FRotator::ZeroRotator, FVector(0.75f), Paint, false);
	AddPlacement(Placements, TEXT("医务备用柜"), TEXT("Medical"), FString(I) + TEXT("Kitchen_CabinetSmall.Kitchen_CabinetSmall"), FVector(-190, 835, 0), FRotator(0, 90, 0), FVector(0.58f), Paint);

	AddPlacement(Placements, TEXT("宿舍物资箱01"), TEXT("Quarters"), FString(Q) + TEXT("Props/Prop_Crate4.Prop_Crate4"), FVector(1460, 980, 0), FRotator(0, -12, 0), FVector(0.9f), Paint);
	AddPlacement(Placements, TEXT("宿舍物资箱02"), TEXT("Quarters"), FString(Q) + TEXT("Props/Prop_Crate3.Prop_Crate3"), FVector(1515, 890, 0), FRotator(0, 8, 0), FVector(0.72f), Paint);
	AddPlacement(Placements, TEXT("厨房排风"), TEXT("Quarters"), FString(Q) + TEXT("Props/Prop_Fan_Small.Prop_Fan_Small"), FVector(1290, 1080, 210), FRotator(0, 180, 0), FVector(1.25f), Paint, false);
	AddPlacement(Placements, TEXT("宿舍双层床01"), TEXT("Quarters"), FString(I) + TEXT("Bed_Bunk.Bed_Bunk"), FVector(900, 590, 0), FRotator(0, 90, 0), FVector(0.62f), Fabric);
	AddPlacement(Placements, TEXT("宿舍双层床02"), TEXT("Quarters"), FString(I) + TEXT("Bed_Bunk.Bed_Bunk"), FVector(900, 900, 0), FRotator(0, 90, 0), FVector(0.62f), Fabric);
	AddPlacement(Placements, TEXT("厨房水槽"), TEXT("Quarters"), FString(I) + TEXT("Kitchen_Sink.Kitchen_Sink"), FVector(1180, 1030, 0), FRotator(0, 180, 0), FVector(0.65f), Paint);
	AddPlacement(Placements, TEXT("厨房灶台"), TEXT("Quarters"), FString(I) + TEXT("Kitchen_Oven_Large.Kitchen_Oven_Large"), FVector(1370, 1030, 0), FRotator(0, 180, 0), FVector(0.65f), Paint);
	AddPlacement(Placements, TEXT("厨房抽屉"), TEXT("Quarters"), FString(I) + TEXT("Kitchen_2Drawers.Kitchen_2Drawers"), FVector(1020, 1030, 0), FRotator(0, 180, 0), FVector(0.65f), Paint);
	AddPlacement(Placements, TEXT("厨房壁柜01"), TEXT("Quarters"), FString(I) + TEXT("Kitchen_Cabinet1.Kitchen_Cabinet1"), FVector(1090, 1060, 175), FRotator(0, 180, 0), FVector(0.6f), Paint, false);
	AddPlacement(Placements, TEXT("厨房壁柜02"), TEXT("Quarters"), FString(I) + TEXT("Kitchen_Cabinet2.Kitchen_Cabinet2"), FVector(1280, 1060, 175), FRotator(0, 180, 0), FVector(0.6f), Paint, false);
	AddPlacement(Placements, TEXT("厨房餐桌"), TEXT("Quarters"), FString(I) + TEXT("Table_RoundLarge.Table_RoundLarge"), FVector(1190, 655, 0), FRotator::ZeroRotator, FVector(0.6f), Paint);
	AddPlacement(Placements, TEXT("厨房凳01"), TEXT("Quarters"), FString(I) + TEXT("Stool.Stool"), FVector(1080, 650, 0), FRotator::ZeroRotator, FVector(0.6f), Paint);
	AddPlacement(Placements, TEXT("厨房凳02"), TEXT("Quarters"), FString(I) + TEXT("Stool.Stool"), FVector(1300, 650, 0), FRotator::ZeroRotator, FVector(0.6f), Paint);
	AddPlacement(Placements, TEXT("厨房冰箱"), TEXT("Quarters"), FString(I) + TEXT("Kitchen_Fridge.Kitchen_Fridge"), FVector(1510, 1030, 0), FRotator(0, 180, 0), FVector(0.65f), Paint);
	AddPlacement(Placements, TEXT("餐桌搪瓷盘01"), TEXT("Quarters"), FString(I) + TEXT("Plate_1.Plate_1"), FVector(1145, 650, 78), FRotator::ZeroRotator, FVector(0.72f), Paint, false);
	AddPlacement(Placements, TEXT("餐桌搪瓷盘02"), TEXT("Quarters"), FString(I) + TEXT("Plate_1.Plate_1"), FVector(1235, 650, 78), FRotator(0, 40, 0), FVector(0.72f), Paint, false);
	AddPlacement(Placements, TEXT("宿舍书架"), TEXT("Quarters"), FString(I) + TEXT("Bookshelf.Bookshelf"), FVector(1600, 825, 0), FRotator(0, -90, 0), FVector(0.50f), Paint);

	AddPlacement(Placements, TEXT("室外平台01"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Platform_Metal.Platform_Metal"), FVector(2050, 400, 5), FRotator::ZeroRotator, FVector(2.2f, 2.2f, 1.0f), Rust, false);
	AddPlacement(Placements, TEXT("室外平台02"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Platform_Metal.Platform_Metal"), FVector(2420, 400, 5), FRotator::ZeroRotator, FVector(2.2f, 2.2f, 1.0f), Rust, false);
	AddPlacement(Placements, TEXT("室外护栏北"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Platform_Rails_4.Platform_Rails_4"), FVector(2250, 80, 45), FRotator(0, 0, 0), FVector(1.8f), Paint);
	AddPlacement(Placements, TEXT("室外护栏南"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Platform_Rails_4.Platform_Rails_4"), FVector(2250, 720, 45), FRotator(0, 180, 0), FVector(1.8f), Paint);
	AddPlacement(Placements, TEXT("室外台阶"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Platform_Stairs_4.Platform_Stairs_4"), FVector(1780, 400, 0), FRotator(0, 90, 0), FVector(1.3f), Paint, false);
	AddPlacement(Placements, TEXT("天线检修门"), TEXT("Outdoor"), FString(Q) + TEXT("Platforms/Door_Frame_SquareTall.Door_Frame_SquareTall"), FVector(1710, 400, 115), FRotator(0, 90, 0), FVector(1.05f), Concrete, false);
	AddPlacement(Placements, TEXT("天线支撑柱北"), TEXT("Outdoor"), FString(Q) + TEXT("Columns/Column_MetalSupport.Column_MetalSupport"), FVector(2440, 150, 0), FRotator::ZeroRotator, FVector(0.8f), Rust);
	AddPlacement(Placements, TEXT("天线支撑柱南"), TEXT("Outdoor"), FString(Q) + TEXT("Columns/Column_MetalSupport.Column_MetalSupport"), FVector(2440, 650, 0), FRotator::ZeroRotator, FVector(0.8f), Rust);
	AddPlacement(Placements, TEXT("天线中央桅杆"), TEXT("Outdoor"), FString(Q) + TEXT("Columns/Column_Pipes.Column_Pipes"), FVector(2440, 400, 0), FRotator::ZeroRotator, FVector(0.58f, 0.58f, 2.15f), Rust);
	AddPlacement(Placements, TEXT("天线横向支臂"), TEXT("Outdoor"), FString(Q) + TEXT("Columns/Column_MetalSupport.Column_MetalSupport"), FVector(2440, 400, 285), FRotator(90, 0, 0), FVector(0.42f, 0.42f, 1.35f), Rust, false);
	AddPlacement(Placements, TEXT("天线接收阵列北"), TEXT("Outdoor"), FString(Q) + TEXT("Props/Prop_AccessPoint.Prop_AccessPoint"), FVector(2420, 315, 330), FRotator(-28, 90, 0), FVector(1.65f), Paint, false);
	AddPlacement(Placements, TEXT("天线接收阵列南"), TEXT("Outdoor"), FString(Q) + TEXT("Props/Prop_AccessPoint.Prop_AccessPoint"), FVector(2420, 485, 330), FRotator(-28, -90, 0), FVector(1.65f), Paint, false);
	AddPlacement(Placements, TEXT("天线控制终端"), TEXT("Outdoor"), FString(Q) + TEXT("Props/Prop_Computer.Prop_Computer"), FVector(2260, 500, 0), FRotator(0, -90, 0), FVector(0.72f), Paint);
	AddPlacement(Placements, TEXT("室外检修箱"), TEXT("Outdoor"), FString(Q) + TEXT("Props/Prop_Crate3.Prop_Crate3"), FVector(2050, 690, 0), FRotator(0, 18, 0), FVector(0.75f), Paint);
	AddPlacement(Placements, TEXT("室外电缆"), TEXT("Outdoor"), FString(Q) + TEXT("Props/Prop_Cable_3.Prop_Cable_3"), FVector(2140, 240, 20), FRotator(0, -28, 0), FVector(1.3f), Rust, false);
	AddPlacement(Placements, TEXT("天线检修灯具"), TEXT("Outdoor"), FString(Q) + TEXT("Props/Prop_Light_Wide.Prop_Light_Wide"), FVector(2080, 400, 265), FRotator(0, 90, 0), FVector(1.15f), Paint, false);
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
