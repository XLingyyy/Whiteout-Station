#pragma once

#include "CoreMinimal.h"
#include "Agents/WSRoleplayTypes.h"
#include "UObject/Object.h"
#include "WSRoleplayResponseValidator.generated.h"

UCLASS()
class WHITEOUTSTATION_API UWSRoleplayResponseValidator : public UObject
{
	GENERATED_BODY()

public:
	static bool Validate(
		const FWSRoleplayRequest& Request,
		const FWSRoleplayResponse& Response,
		FString& OutReason);

	static bool ValidateAndDeriveDisclosures(
		const FWSRoleplayRequest& Request,
		const FWSRoleplayResponse& Response,
		TArray<FName>& OutGameFactIds,
		FString& OutReason);
};
