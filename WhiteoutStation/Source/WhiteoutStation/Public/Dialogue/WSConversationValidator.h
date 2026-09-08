#pragma once
#include "CoreMinimal.h"
#include "State/WindStationTypes.h"

// The verifier receives the same frozen, permission-filtered facts as the writer.
// Its verdict is local runtime evidence, never a field accepted from the writer.
class WHITEOUTSTATION_API FWSConversationValidator
{
public:
	static bool ParseReply(const FString& Json, const FWSPreparedDialogue& Prepared, FWSDialogueOutcome& Outcome, FString& Error);
	static bool ApplyVerdict(const FString& Json, const FWSPreparedDialogue& Prepared, FWSDialogueOutcome& Outcome, FString& Error);
	static bool NeedsSemanticCheck(const FWSPreparedDialogue& Prepared, const FWSDialogueOutcome& Outcome);
	static bool ValidateCommitted(const FWSPreparedDialogue& Prepared, const FWSDialogueOutcome& Outcome, FString& Error);
};
