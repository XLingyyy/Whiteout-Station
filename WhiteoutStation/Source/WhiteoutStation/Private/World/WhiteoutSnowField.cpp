#include "World/WhiteoutSnowField.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AWhiteoutSnowField::AWhiteoutSnowField()
{
	PrimaryActorTick.bCanEverTick = true;
	SnowInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SnowInstances"));
	SetRootComponent(SnowInstances);
	SnowInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SnowInstances->SetCastShadow(false);
	SnowInstances->SetReceivesDecals(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FlakeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (FlakeMesh.Succeeded())
	{
		SnowInstances->SetStaticMesh(FlakeMesh.Object);
	}
}

void AWhiteoutSnowField::BeginPlay()
{
	Super::BeginPlay();
	ParticlePositions.Reserve(ParticleCount);
	ParticleSpeeds.Reserve(ParticleCount);
	for (int32 Index = 0; Index < ParticleCount; ++Index)
	{
		const FVector Position(
			RandomStream.FRandRange(MinX, MaxX),
			RandomStream.FRandRange(MinY, MaxY),
			RandomStream.FRandRange(MinZ, MaxZ));
		const float Speed = RandomStream.FRandRange(0.7f, 1.35f);
		ParticlePositions.Add(Position);
		ParticleSpeeds.Add(Speed);
		const FVector Scale(RandomStream.FRandRange(0.012f, 0.026f), 0.008f, RandomStream.FRandRange(0.025f, 0.055f));
		SnowInstances->AddInstance(FTransform(FRotator(18.0f, -34.0f, 0.0f), Position, Scale), true);
	}
}

void AWhiteoutSnowField::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (int32 Index = 0; Index < ParticlePositions.Num(); ++Index)
	{
		FVector& Position = ParticlePositions[Index];
		const float Speed = ParticleSpeeds[Index];
		Position += FVector(-430.0f, 105.0f, -115.0f) * Speed * DeltaSeconds;
		if (Position.X < MinX || Position.Y > MaxY || Position.Z < MinZ)
		{
			Position.X = MaxX;
			Position.Y = RandomStream.FRandRange(MinY, MaxY);
			Position.Z = RandomStream.FRandRange(MaxZ * 0.6f, MaxZ);
		}
		FTransform Transform;
		if (SnowInstances->GetInstanceTransform(Index, Transform, true))
		{
			Transform.SetLocation(Position);
			SnowInstances->UpdateInstanceTransform(Index, Transform, true, Index == ParticlePositions.Num() - 1, true);
		}
	}
}
