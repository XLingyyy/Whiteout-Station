#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Player/WhiteoutCharacter.h"
#include "World/WSInteractableActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhiteoutV15FocusLeaseTest,
	"WhiteoutStation.UI.V15.Focus.CollisionAndLease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWhiteoutV15FocusLeaseTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, GEngine->GetDefaultWorldFeatureLevel(), &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AWhiteoutCharacter* Player = World->SpawnActor<AWhiteoutCharacter>();
	const auto NPC = [&](FName Action, FVector Location)
	{
		AWSInteractableActor* Actor = World->SpawnActor<AWSInteractableActor>();
		Actor->ActionId = Action;
		Actor->bCharacterPresentation = true;
		Actor->SetActorScale3D(FVector::OneVector);
		Actor->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Actor->InteractionCollision->SetRelativeLocation(FVector::ZeroVector);
		Actor->InteractionCollision->SetBoxExtent(FVector(20, 20, 50));
		Actor->SetActorLocation(Location);
		return Actor;
	};
	AWSInteractableActor* Gu = NPC(TEXT("talk_gu_heng"), FVector(310, 0, 64));
	AWSInteractableActor* Ye = NPC(TEXT("talk_ye_cheng"), FVector(300, 80, 64));
	Player->FirstPersonCamera->SetWorldLocation(FVector(0, 0, 64));
	Player->FirstPersonCamera->SetWorldRotation(FRotator::ZeroRotator);
	TestTrue(TEXT("Distance uses collision surface at 290 cm, despite origin at 310 cm"), Player->IsDialogueTargetVisible(Gu, 300, false));
	Player->UpdateInteractionLease(0.10f);
	TestNull(TEXT("Acquisition requires continuous 150 ms"), Player->FocusedInteractable.Get());
	Player->UpdateInteractionLease(0.06f);
	TestEqual(TEXT("Near centered target becomes the single focus"), Player->FocusedInteractable.Get(), Gu);
	Gu->SetActorLocation(FVector(350, 0, 64));
	TestFalse(TEXT("Surface at 330 cm cannot be newly acquired"), Player->IsDialogueTargetVisible(Gu, 300, false));
	Player->UpdateInteractionLease(0.1f);
	TestEqual(TEXT("Existing target remains within the 340 cm lease"), Player->FocusedInteractable.Get(), Gu);
	Gu->SetActorLocation(FVector(361, 0, 64));
	Player->UpdateInteractionLease(0.01f);
	TestNull(TEXT("Beyond 340 cm clears immediately"), Player->FocusedInteractable.Get());
	Gu->SetActorLocation(FVector(310, 0, 64));
	Player->UpdateInteractionLease(0.16f);
	AActor* Wall = World->SpawnActor<AActor>();
	UBoxComponent* Blocker = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Blocker);
	Blocker->SetBoxExtent(FVector(5, 60, 100));
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Blocker->SetCollisionResponseToAllChannels(ECR_Block);
	Blocker->RegisterComponent();
	Wall->SetActorLocation(FVector(100, 0, 64));
	Player->UpdateInteractionLease(0.001f);
	TestNull(TEXT("A real visibility blocker clears without angular grace"), Player->FocusedInteractable.Get());
	TestFalse(TEXT("Established conversation cannot read through a blocker"), Player->IsDialogueTargetVisible(Gu, 340, true));
	Wall->Destroy();
	Player->UpdateInteractionLease(0.16f);
	Player->FirstPersonCamera->SetWorldRotation(FRotator(0, 4.5f, 0));
	Player->UpdateInteractionLease(0.10f);
	TestEqual(TEXT("A small edge miss retains focus during grace"), Player->FocusedInteractable.Get(), Gu);
	Player->UpdateInteractionLease(0.11f);
	TestNull(TEXT("Angular grace expires after 200 ms"), Player->FocusedInteractable.Get());
	Player->FirstPersonCamera->SetWorldRotation(FRotator(0, 90, 0));
	Player->UpdateInteractionLease(0.2f);
	TestNull(TEXT("Being nearby while looking away reveals no NPC"), Player->FocusedInteractable.Get());
	Player->FirstPersonCamera->SetWorldRotation(FVector(300,80,0).Rotation());
	Player->UpdateInteractionLease(0.16f);
	TestEqual(TEXT("Switching gaze acquires Ye without retaining Gu"), Player->FocusedInteractable.Get(), Ye);
	Ye->Destroy();
	Player->UpdateInteractionLease(0.001f);
	TestNull(TEXT("Destroyed target clears safely"), Player->FocusedInteractable.Get());
	Player->HandleApplicationActivation(false);
	TestFalse(TEXT("Background application suspends reacquisition"), Player->bApplicationActive);
	TestNull(TEXT("Background application has no focus"), Player->FocusedInteractable.Get());
	Player->HandleApplicationActivation(true);
	TestTrue(TEXT("Activation permits reacquisition"), Player->bApplicationActive);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
