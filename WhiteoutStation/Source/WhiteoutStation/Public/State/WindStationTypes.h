#pragma once

#include "CoreMinimal.h"
#include "WindStationTypes.generated.h"

UENUM(BlueprintType)
enum class EWSGamePhase : uint8
{
	Opening,
	ActionPhase,
	ResolvingAction,
	DialogueFeedback,
	MidCrisis,
	PostActionWindow,
	EndingChoice,
	Ending,
	Results
};

UENUM(BlueprintType)
enum class EWSCharacterId : uint8
{
	Player,
	GuHeng,
	YeCheng
};

UENUM(BlueprintType)
enum class EWSKnowledgeLevel : uint8
{
	Unknown,
	Claimed,
	Suspected,
	Confirmed
};

UENUM(BlueprintType)
enum class EWSResourceType : uint8
{
	Medicine,
	HeatPack
};

UENUM(BlueprintType)
enum class EWSDialogueAct : uint8
{
	Ask,
	Challenge,
	Command,
	Promise,
	Trade,
	Reassure,
	PublishInformation
};

UENUM(BlueprintType)
enum class EWSResponseType : uint8
{
	Accept,
	ConditionalAccept,
	Refuse,
	PartialDisclosure,
	FullDisclosure,
	Deflect,
	Accuse,
	Reassure
};

UENUM(BlueprintType)
enum class EWSReasonCode : uint8
{
	Ok,
	Committed,
	UnknownAction,
	PhaseLocked,
	InsufficientAP,
	AlreadyCompleted,
	UseLimitReached,
	DuplicateTransaction,
	AlreadyHeated,
	NeedsFuel,
	InvalidFoodAllocation,
	EmptyFoodAllocation,
	InsufficientFood,
	NeedsMedicalHeat,
	NeedsDiagnosis,
	HeatPackHidden,
	InvalidTreatmentResource,
	NeedsMedicine,
	NeedsHeatPack,
	NeedsRelayEvidence,
	HeaterAlreadyDismantled,
	GeneratorAlreadyRepaired,
	GuHengCritical,
	NeedsCooperation,
	NeedsGeneratorRecords,
	SelfRepairAlreadyUsed,
	NeedsGenerator,
	AntennaAlreadyCalibrated,
	PlayerTooCold,
	NeedsAntenna
};

UENUM(BlueprintType)
enum class EWSEndingType : uint8
{
	TaskSuccess,
	SurvivalWait,
	CostUncontrolled,
	TotalCollapse
};

USTRUCT(BlueprintType)
struct FWSCharacterState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Health = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Temperature = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Hunger = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Fatigue = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Pressure = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Trust = 0.0f;
};

USTRUCT(BlueprintType)
struct FWSResourceState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 Fuel = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 Food = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 Medicine = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 HeatPack = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 ReplacementRelay = 0;
};

USTRUCT(BlueprintType)
struct FWSTaskState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 GeneratorProgress = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 AntennaCalibration = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bSignalSent = false;
};

USTRUCT(BlueprintType)
struct FWSWorldFlags
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bKitchenHeaterIntact = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bHeatPackRevealed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bRepairRoomHeated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bMedicalRoomHeated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bGuHengDiagnosed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bGuHengTreated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bGuHengFed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bGuHengCooperative = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bRelayCompatibilityKnown = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bRelayInstalled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bSelfRepairUsed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bRecordsPreserved = false;
};

USTRUCT(BlueprintType)
struct FWSPromiseRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName PromiseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName ConditionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bRecognized = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bSettled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bFulfilled = false;
};

USTRUCT(BlueprintType)
struct FWSEventRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 Index = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FGuid TransactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 APBefore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 APAfter = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSReasonCode ReasonCode = EWSReasonCode::Committed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FString> Changes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bCrisisTriggered = false;
};

USTRUCT(BlueprintType)
struct FWSScoreBreakdown
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float TaskQuality = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float People = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float EffectiveReserves = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float SocialStability = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float InformationResponsibility = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Total = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FString Rating = TEXT("D");
};

USTRUCT(BlueprintType)
struct FWSGameState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 ActionPoints = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSGamePhase Phase = EWSGamePhase::ActionPhase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bMidCrisisTriggered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FWSResourceState Resources;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FWSTaskState Tasks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FWSWorldFlags Flags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TMap<EWSCharacterId, FWSCharacterState> Characters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TMap<FName, EWSKnowledgeLevel> PlayerKnowledge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FName> Evidence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FName> PublicFacts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TMap<FName, int32> ActionCounts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FGuid> CommittedTransactions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FWSPromiseRecord> Promises;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FWSEventRecord> EventLog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 ModelCalls = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSEndingType Ending = EWSEndingType::SurvivalWait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FWSScoreBreakdown Score;
};

USTRUCT(BlueprintType)
struct FWSActionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid TransactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FoodForPlayer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FoodForGuHeng = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FoodForYeCheng = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSResourceType TreatmentResource = EWSResourceType::Medicine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSDialogueAct DialogueAct = EWSDialogueAct::Ask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName PromiseCondition;
};

USTRUCT(BlueprintType)
struct FWSActionPreview
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCanExecute = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSReasonCode ReasonCode = EWSReasonCode::UnknownAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 APCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText PreviewText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText RiskText;
};

USTRUCT(BlueprintType)
struct FWSActionResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCommitted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSReasonCode ReasonCode = EWSReasonCode::UnknownAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 APBefore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 APAfter = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid TransactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> Changes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCrisisTriggered = false;
};
