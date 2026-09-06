#pragma once
#include "CoreMinimal.h"
#include "State/WhiteoutRulesEngine.h"

enum class EWSStatusVisibility : uint8 { Hidden, Unknown, Observed, Confirmed };
enum class EWSStatusSource : uint8 { Self, Observation, Dialogue, Diagnosis };
enum class EWSStatusSeverity : uint8 { Normal, Attention, Critical, Unknown };

struct FWSStatusField
{
	FString Label;
	FString DisplayValue;
	EWSStatusSeverity Severity = EWSStatusSeverity::Normal;
	EWSStatusVisibility Visibility = EWSStatusVisibility::Observed;
	EWSStatusSource Source = EWSStatusSource::Observation;
	TOptional<float> DisplayRatio;
};

struct FWSStatusCardViewModel
{
	FString Title;
	TArray<FWSStatusField> Fields;
	FString PublicEffectHint;
};

namespace WSStatusPresenter
{
	WHITEOUTSTATION_API FWSStatusCardViewModel Build(EWSCharacterId Character,
		const FWSGameState& State, const FWhiteoutRuleConfig& Config);
}
