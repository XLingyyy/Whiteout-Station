#pragma once

#include "CoreMinimal.h"
#include "State/WindStationTypes.h"

enum class EWSDialogueMode : uint8 { Authored, Online, InvalidConfiguration };
enum class EWSIntentPolarity : uint8 { Affirmative, Negated, Hypothetical, Quoted };
enum class EWSCommitmentIntent : uint8 { None, Proposed, ConfirmPending, RejectPending };

struct WHITEOUTSTATION_API FWSCanonicalIntent
{
	FName SpeakerId;
	FName TopicId;
	FWSDialogueSemanticFrame Frame;
	EWSIntentPolarity Polarity = EWSIntentPolarity::Affirmative;
	EWSCommitmentIntent Commitment = EWSCommitmentIntent::None;
	FName PromiseCondition;
	bool bNeedsClarification = false;
	TArray<FString> EvidenceSpans;
	int32 ResolvedFromTurn = 0;
	double DeadlineSeconds = 0;

	static bool Parse(const FString& Json, FName ExpectedSpeaker,
		const FString& PlayerText, int32 LatestContextTurn,
		FWSCanonicalIntent& Out, FString& Error);
	bool CanPlan() const;
	void ApplyTo(FWSActionRequest& Request) const;
};
