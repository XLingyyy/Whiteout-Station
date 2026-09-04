#pragma once

#include "CoreMinimal.h"
#include "Agents/WSRoleplayTypes.h"
#include "UObject/Object.h"
#include "WSRoleplayKnowledgeRepository.generated.h"

UCLASS()
class WHITEOUTSTATION_API UWSRoleplayKnowledgeRepository : public UObject
{
	GENERATED_BODY()

public:
	bool LoadDefault(FString& OutError);
	bool LoadFromDirectory(const FString& Directory, FString& OutError);

	bool IsAvailable() const { return bAvailable; }
	int32 GetKnowledgeCount() const { return KnowledgeById.Num(); }

	bool GetProfile(FName ProfileId, FWSRoleplayProfile& OutProfile) const;
	bool GetKnowledgeItem(
		FName KnowledgeId,
		FWSRoleplayKnowledgeItem& OutKnowledge) const;
	TArray<FWSRoleplayKnowledgeItem> GetGlobalKnowledge() const;
	TArray<FWSRoleplayKnowledgeItem> GetKnowledgeForOwner(FName Owner) const;
	const FWSRoleplayPolicy& GetPolicy() const { return Policy; }
	const TArray<FWSRoleplayFallback>& GetFallbacks() const
	{
		return Fallbacks;
	}

private:
	void ResetRepository();

	UPROPERTY(Transient)
	bool bAvailable = false;

	UPROPERTY(Transient)
	TMap<FName, FWSRoleplayProfile> Profiles;

	UPROPERTY(Transient)
	TMap<FName, FWSRoleplayKnowledgeItem> KnowledgeById;

	UPROPERTY(Transient)
	FWSRoleplayPolicy Policy;

	UPROPERTY(Transient)
	TArray<FWSRoleplayFallback> Fallbacks;
};
