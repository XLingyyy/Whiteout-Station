#pragma once

#include "CoreMinimal.h"
#include "WSRoleplayTypes.generated.h"

UENUM(BlueprintType)
enum class EWSEpistemicStatus : uint8
{
	Known,
	Observed,
	Believed,
	Suspected,
	FalseBelief,
	Unknown
};

UENUM(BlueprintType)
enum class EWSRoleplayDisclosureLevel : uint8
{
	Hidden,
	Evasive,
	Hint,
	Partial,
	Explicit
};

UENUM(BlueprintType)
enum class EWSRoleplaySpeechFunction : uint8
{
	Unknown,
	Answer,
	AnswerWithUncertainty,
	Clarify,
	Deflect,
	Refuse,
	Reassure,
	Challenge,
	Acknowledge,
	ConditionalCooperation,
	SuggestAction,
	Evade,
	Suggest,
	ConditionalOffer,
	CrisisResponse
};

UENUM(BlueprintType)
enum class EWSRoleplayClaimMode : uint8
{
	Unknown,
	Stated,
	Observation,
	Belief,
	Suspected,
	Denied,
	Promised,
	Withheld
};

UENUM(BlueprintType)
enum class EWSRoleplayProposalType : uint8
{
	None,
	ConditionalCooperation,
	SuggestAction,
	RefuseAction
};

USTRUCT(BlueprintType)
struct FWSRoleplayProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> Personality;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> CurrentGoals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> Fears;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> SpeakingStyle;
};

USTRUCT(BlueprintType)
struct FWSRoleplayKnowledgeItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName KnowledgeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Owner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SubjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName CategoryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RoleplayContent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSEpistemicStatus EpistemicStatus = EWSEpistemicStatus::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Confidence = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> TopicTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSRoleplayDisclosureLevel MaxDisclosure =
		EWSRoleplayDisclosureLevel::Hidden;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Salience = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPublic = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName GameFactId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCreatesGameFact = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> Availability;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SecretFamily;
};

USTRUCT(BlueprintType)
struct FWSRoleplaySubjectiveState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName PhaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName DayPhaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName HeatingStateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName HeatingZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHeatingLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName GeneratorStateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 GeneratorProgress = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SpeakerLocationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeakerTrust = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeakerPressure = 0.0f;
};

USTRUCT(BlueprintType)
struct FWSRoleplayAssertion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName KnowledgeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSRoleplayClaimMode Mode = EWSRoleplayClaimMode::Unknown;
};

USTRUCT(BlueprintType)
struct FWSRoleplayActionProposal
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSRoleplayProposalType Type = EWSRoleplayProposalType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> RequestedConditionIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ExpiresAtPhase;
};

USTRUCT(BlueprintType)
struct FWSRoleplayMemoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName MemoryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName Owner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName TopicId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSRoleplayClaimMode ClaimMode = EWSRoleplayClaimMode::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FName> KnowledgeIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FString SafeSummary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Importance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 TurnIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bPublic = false;
};

USTRUCT(BlueprintType)
struct FWSRoleplayPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TopK = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxSentences = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxCharacters = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxOutputTokens = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Temperature = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EWSRoleplaySpeechFunction> AllowedSpeechFunctions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EWSRoleplayProposalType> AllowedProposalTypes;
};

USTRUCT(BlueprintType)
struct FWSRoleplayFallback
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName FallbackId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SpeakerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TargetSubjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> TopicTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSRoleplaySpeechFunction SpeechFunction =
		EWSRoleplaySpeechFunction::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Line;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> ReferencedKnowledgeIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> Availability;
};

USTRUCT(BlueprintType)
struct FWSRoleplayRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SpeakerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TargetSubjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PlayerLine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TurnIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RemainingTurns = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWSRoleplayProfile RoleProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWSRoleplaySubjectiveState SubjectiveState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> TopicTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWSRoleplayKnowledgeItem> AvailableKnowledge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> ForbiddenFactIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWSRoleplayActionProposal> AllowedActionProposals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWSRoleplayMemoryEntry> RecentMemory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWSRoleplayPolicy ResponsePolicy;
};

USTRUCT(BlueprintType)
struct FWSRoleplayResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString NpcLine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSRoleplaySpeechFunction SpeechFunction =
		EWSRoleplaySpeechFunction::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> ReferencedKnowledgeIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWSRoleplayAssertion> Assertions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasProposedAction = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWSRoleplayActionProposal ProposedAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString MemorySummary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Emotion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString MovementIntent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ReactionAction;
};
