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
enum class EWSDayPhase : uint8
{
	Morning,
	Afternoon,
	Dusk,
	Complete
};

UENUM(BlueprintType)
enum class EWSHeatingZone : uint8
{
	None,
	RepairRoom,
	MedicalRoom,
	Kitchen,
	ControlRoom
};

UENUM(BlueprintType)
enum class EWSCharacterLocation : uint8
{
	ControlRoom,
	RepairRoom,
	MedicalRoom,
	Kitchen,
	OutdoorAntenna
};

UENUM(BlueprintType)
enum class EWSInjurySeverity : uint8
{
	Normal,
	Restricted,
	Critical
};

UENUM(BlueprintType)
enum class EWSWorkReadiness : uint8
{
	Ready,
	Strained,
	HighRisk,
	Unavailable
};

UENUM(BlueprintType)
enum class EWSTreatmentMethod : uint8
{
	Bandage,
	Full,
	HeatPack
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
enum class EWSDialogueQueryType : uint8
{
	Unknown,
	Requirements,
	Status,
	Cause,
	Alternative,
	Evidence,
	Consequence
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
enum class EWSNPCMovementIntent : uint8
{
	Stay,
	StepCloser,
	StepBack,
	ReturnToPost
};

UENUM(BlueprintType)
enum class EWSNPCReaction : uint8
{
	Neutral,
	Acknowledge,
	Consider,
	Reassure,
	Reject,
	Alarmed
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
	NeedsAntenna,
	DialogueActUnavailable,
	InvalidPromiseCondition,
	DuplicatePromise,
	PhaseNotStarted,
	HeatingLocked,
	UnknownHeatingZone,
	WindowClosed,
	ExecutorExhausted,
	ExecutorHypothermic,
	RelevantInjuryCritical,
	InvalidCollaborator,
	CollaboratorUnavailable,
	HotMealUnavailable,
	InvalidMealType,
	UnknownTarget,
	InvalidTreatmentMethod,
	TreatmentNotNeeded,
	NeedsHeatedMedicalRoom,
	YeChengExhausted,
	NeedsRelayKnowledge,
	GuHengRefused,
	NeedsGuHengConditions,
	NeedsReplacementRelay
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
	float Health = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Temperature = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Hunger = 6.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Fatigue = 6.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Pressure = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Trust = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 Stamina = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSInjurySeverity InjurySeverity = EWSInjurySeverity::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName InjuryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 InjuryWorseningMarks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 BandageProtection = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 TemporarySupportUses = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSDayPhase TemporarySupportPhase = EWSDayPhase::Complete;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSCharacterLocation Location = EWSCharacterLocation::ControlRoom;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bGeneratorStable = false;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bPlayerFed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bYeChengFed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bCabinetInspected = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bLogPenaltyActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 ForcedActionCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 RiskyRepairCount = 0;
};

USTRUCT(BlueprintType)
struct FWSActionCostModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 Delta = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSCharacterId Character = EWSCharacterId::Player;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FText Explanation;
};

USTRUCT(BlueprintType)
struct FWSDialogueSemanticFrame
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSDialogueAct SpeechAct = EWSDialogueAct::Ask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSDialogueQueryType QueryType = EWSDialogueQueryType::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TargetActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TargetFactId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSCharacterId TargetCharacter = EWSCharacterId::GuHeng;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Confidence = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Source;
};

USTRUCT(BlueprintType)
struct FWSDialogueIntentResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bMapped = false;

	UPROPERTY(BlueprintReadOnly)
	EWSDialogueAct DialogueAct = EWSDialogueAct::Ask;

	UPROPERTY(BlueprintReadOnly)
	FName PromiseCondition;

	UPROPERTY(BlueprintReadOnly)
	EWSDialogueQueryType QueryType = EWSDialogueQueryType::Unknown;

	UPROPERTY(BlueprintReadOnly)
	FName TargetActionId;

	UPROPERTY(BlueprintReadOnly)
	FName TargetFactId;

	UPROPERTY(BlueprintReadOnly)
	EWSCharacterId TargetCharacter = EWSCharacterId::GuHeng;

	UPROPERTY(BlueprintReadOnly)
	float Confidence = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FString Source = TEXT("wheel_only");

	UPROPERTY(BlueprintReadOnly)
	FString Reason;

	FWSDialogueSemanticFrame ToSemanticFrame() const
	{
		FWSDialogueSemanticFrame Result;
		Result.SpeechAct = DialogueAct;
		Result.QueryType = QueryType;
		Result.TargetActionId = TargetActionId;
		Result.TargetFactId = TargetFactId;
		Result.TargetCharacter = TargetCharacter;
		Result.Confidence = Confidence;
		Result.Source = Source;
		return Result;
	}
};

USTRUCT(BlueprintType)
struct FWSRequirementItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RequirementId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSatisfied = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDisclosable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RemediationActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Explanation;
};

USTRUCT(BlueprintType)
struct FWSRequirementPlan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName PlanId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWSRequirementItem> Requirements;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 EstimatedAP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RiskScore = 0.0f;
};

USTRUCT(BlueprintType)
struct FWSActionRequirementReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCurrentlyExecutable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWSRequirementItem> UniversalRequirements;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWSRequirementPlan> AlternativePlans;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWSRequirementItem> Risks;
};

USTRUCT(BlueprintType)
struct FWSDialogueAnswerContract
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSDialogueQueryType QueryType = EWSDialogueQueryType::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TargetActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> MustCoverConditionIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> AllowedOptionalConditionIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> AllowedRiskIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxSentences = 3;
};

USTRUCT(BlueprintType)
struct FWSNPCDialoguePlan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSCharacterId Speaker = EWSCharacterId::GuHeng;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSResponseType Stance = EWSResponseType::Deflect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SemanticSpine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWSDialogueAnswerContract Contract;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> AllowedFactIds;
};

USTRUCT(BlueprintType)
struct FWSNegotiationOffer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName OfferId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSCharacterId Issuer = EWSCharacterId::GuHeng;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName TargetActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FName> RequiredConditionIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName PromisedNPCActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSDayPhase ExpiryPhase = EWSDayPhase::Complete;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bAccepted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bFulfilled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bBroken = false;
};

USTRUCT(BlueprintType)
struct FWSHeatingSelectionRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSDayPhase Phase = EWSDayPhase::Morning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSHeatingZone Zone = EWSHeatingZone::None;
};

USTRUCT(BlueprintType)
struct FWSHeatingState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSHeatingZone CurrentZone = EWSHeatingZone::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FWSHeatingSelectionRecord> History;
};

USTRUCT(BlueprintType)
struct FWSPhaseSummary
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSDayPhase Phase = EWSDayPhase::Morning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSHeatingZone HeatingZone = EWSHeatingZone::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 UnusedAPDiscarded = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FString> OrderedSteps;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FString> Changes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName PhaseEvent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName NPCReaction;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 HeatingHistoryCountAtRecognition = 0;
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
	EWSDialogueAct DialogueAct = EWSDialogueAct::Ask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName PromiseCondition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bPromiseRecorded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FString> Changes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bCrisisTriggered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSDayPhase DayPhase = EWSDayPhase::Morning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 BaseAP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 ActualAP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSWorkReadiness WorkReadiness = EWSWorkReadiness::Ready;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FWSActionCostModifier> CostModifiers;
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
	int32 ActionPoints = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 RulesSchemaVersion = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FString RulesVersion = TEXT("1.0.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EWSDayPhase DayPhase = EWSDayPhase::Morning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 PhaseActionPoints = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bDayPhaseStarted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bDayWindowClosed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FWSHeatingState Heating;

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
	TArray<FWSNegotiationOffer> NegotiationOffers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FName> PinnedRequirementActions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FWSEventRecord> EventLog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FWSPhaseSummary> PhaseSummaries;

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
	EWSTreatmentMethod TreatmentMethod = EWSTreatmentMethod::Full;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSCharacterId TreatmentTarget = EWSCharacterId::GuHeng;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHotMeal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bForce = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseRelay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasCollaborator = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSCharacterId Collaborator = EWSCharacterId::Player;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSCharacterId RestTarget = EWSCharacterId::Player;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSCharacterLocation RestLocation = EWSCharacterLocation::ControlRoom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSDialogueAct DialogueAct = EWSDialogueAct::Ask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName PromiseCondition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PlayerSaid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid DialogueSessionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWSDialogueSemanticFrame SemanticFrame;
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
	int32 BaseAP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RawAP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWSActionCostModifier> CostModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSWorkReadiness WorkReadiness = EWSWorkReadiness::Unavailable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ExpectedGeneratorProgress = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUsesTemporarySupport = false;

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
	EWSDialogueAct DialogueAct = EWSDialogueAct::Ask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName PromiseCondition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPromiseRecorded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> Changes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCrisisTriggered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BaseAP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ActualAP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWSActionCostModifier> CostModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSWorkReadiness WorkReadiness = EWSWorkReadiness::Unavailable;
};

USTRUCT(BlueprintType)
struct FWSAgentReply
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSCharacterId Speaker = EWSCharacterId::GuHeng;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid TransactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid DialogueSessionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSResponseType ResponseType = EWSResponseType::Deflect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Utterance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SemanticSpine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PersonaTail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString AnswerSource = TEXT("spine_only");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> CoveredConditionIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWSActionRequirementReport RequirementReport;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWSDialogueAnswerContract AnswerContract;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Emotion = TEXT("guarded");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> ReferencedFactIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSNPCMovementIntent MovementIntent = EWSNPCMovementIntent::Stay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWSNPCReaction Reaction = EWSNPCReaction::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAccepted = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bFallback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Provider = TEXT("preset");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ValidationReason = TEXT("preset");
};
