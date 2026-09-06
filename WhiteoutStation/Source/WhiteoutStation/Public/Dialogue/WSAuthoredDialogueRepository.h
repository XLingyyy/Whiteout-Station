#pragma once

#include "CoreMinimal.h"
#include "Dialogue/WSDialogueV15Types.h"

struct FWSAuthoredChoice
{
	FName ChoiceId;
	FName SpeakerId;
	FString Text;
	TArray<FString> VisibleIf;
	TArray<FString> EnabledIf;
	FWSCanonicalIntent Intent;
	FName LineGroupId;
};

struct FWSAuthoredLine
{
	FName LineId;
	FName GroupId;
	int32 Priority = 0;
	TArray<FString> When;
	FString Text;
	TArray<FName> ClaimIds;
	FString Reaction;
};

struct FWSDialogueClaim
{
	FName Id;
	FName KnowledgeId;
	FString Text;
};

class WHITEOUTSTATION_API FWSAuthoredDialogueRepository
{
public:
	bool Load(const FString& Directory, FString& Error);
	bool IsAvailable() const { return bAvailable; }
	const FWSAuthoredChoice* FindChoice(FName Id) const;
	const FWSAuthoredChoice* FindEquivalent(FName Speaker, const FWSDialogueSemanticFrame& Frame) const;
	TArray<FWSAuthoredChoice> GetChoices(FName Speaker, const FWSGameState& State) const;
	bool CanSelect(const FWSAuthoredChoice& Choice, FName Speaker,
		const FWSGameState& State) const;
	bool SelectLine(const FWSAuthoredChoice& Choice, const FWSGameState& State,
		const FWSRoleplayRequest& Context, FWSRoleplayFallback& Out, FString& Error) const;
	bool RenderClaim(FName Id, const FWSRoleplayRequest& Context,
		FString& Text, FName& KnowledgeId) const;
	static bool IsPredicateRegistered(const FString& Predicate);
private:
	TArray<FWSAuthoredChoice> Choices;
	TArray<FWSAuthoredLine> Lines;
	TMap<FName, FWSDialogueClaim> Claims;
	bool bAvailable = false;
	bool Matches(const TArray<FString>& Predicates, const FWSGameState& State,
		const FWSRoleplayRequest* Context = nullptr) const;
};
