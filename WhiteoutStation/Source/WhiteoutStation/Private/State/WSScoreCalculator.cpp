#include "State/WSScoreCalculator.h"

#include "State/WhiteoutRulesEngine.h"

FWSScoreBreakdown UWSScoreCalculator::Calculate(const FWSGameState& State)
{
	FWhiteoutRulesEngine Engine;
	Engine.SetState(State);
	return Engine.CalculateScore();
}
