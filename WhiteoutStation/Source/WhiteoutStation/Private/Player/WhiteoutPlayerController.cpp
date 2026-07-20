#include "Player/WhiteoutPlayerController.h"

void AWhiteoutPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}
