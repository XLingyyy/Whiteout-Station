#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WhiteoutPlayerController.generated.h"

UCLASS()
class WHITEOUTSTATION_API AWhiteoutPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
};
