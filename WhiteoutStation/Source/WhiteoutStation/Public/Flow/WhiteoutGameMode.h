#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WhiteoutGameMode.generated.h"

UCLASS()
class WHITEOUTSTATION_API AWhiteoutGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWhiteoutGameMode();
	virtual void BeginPlay() override;
};
