#include "Presentation/WSStatusPresenter.h"
#include "State/WSKnowledgePolicy.h"

FWSStatusCardViewModel WSStatusPresenter::Build(EWSCharacterId Id, const FWSGameState& State,
	const FWhiteoutRuleConfig& Config)
{
	FWSStatusCardViewModel View;
	const FWSCharacterState* Character = State.Characters.Find(Id);
	if (!Character) return View;
	const bool Self = Id == EWSCharacterId::Player;
	View.Title = Self ? TEXT("我的状态") : Id == EWSCharacterId::GuHeng ? TEXT("顾衡 · 工程师") : TEXT("叶澄 · 医生");
	const auto Add = [&](const TCHAR* Label, FString Value, EWSStatusSeverity Severity)
	{
		FWSStatusField Field; Field.Label = Label; Field.DisplayValue = Value; Field.Severity = Severity;
		Field.Source = Self ? EWSStatusSource::Self : EWSStatusSource::Observation;
		Field.Visibility = Self ? EWSStatusVisibility::Confirmed : EWSStatusVisibility::Observed;
		View.Fields.Add(MoveTemp(Field));
	};
	const bool Cold = Character->Temperature < Config.WarmTemperature;
	const bool CriticalCold = Character->Temperature <= Config.HypothermicTemperature;
	Add(TEXT("体温"), CriticalCold ? TEXT("危险") : Cold ? TEXT("偏低") : TEXT("稳定"),
		CriticalCold ? EWSStatusSeverity::Critical : Cold ? EWSStatusSeverity::Attention : EWSStatusSeverity::Normal);
	Add(TEXT("体能"), Self ? FString::Printf(TEXT("%d / 2"), Character->Stamina)
		: Character->Stamina >= 2 ? TEXT("充足") : Character->Stamina == 1 ? TEXT("吃紧") : TEXT("不足"),
		Character->Stamina > 0 ? EWSStatusSeverity::Normal : EWSStatusSeverity::Attention);
	if (Self) View.Fields.Last().DisplayRatio = Character->Stamina / 2.0f;
	const bool InjuryKnown = Self || (Id == EWSCharacterId::GuHeng && FWSKnowledgePolicy::IsGuHengInjuryVisible(State));
	const FString Injury = !InjuryKnown ? TEXT("尚未确认") : Character->InjurySeverity == EWSInjurySeverity::Normal
		? TEXT("正常") : Character->InjurySeverity == EWSInjurySeverity::Restricted ? TEXT("操作受限") : TEXT("严重");
	Add(TEXT("伤势"), Injury, !InjuryKnown ? EWSStatusSeverity::Unknown
		: Character->InjurySeverity == EWSInjurySeverity::Critical ? EWSStatusSeverity::Critical
		: Character->InjurySeverity == EWSInjurySeverity::Restricted ? EWSStatusSeverity::Attention : EWSStatusSeverity::Normal);
	View.Fields.Last().Visibility = InjuryKnown ? EWSStatusVisibility::Confirmed : EWSStatusVisibility::Unknown;
	View.Fields.Last().Source = Self ? EWSStatusSource::Self : InjuryKnown ? EWSStatusSource::Diagnosis : EWSStatusSource::Observation;
	if (Self)
	{
		Add(TEXT("压力"), Character->Pressure >= Config.CriticalPressure ? TEXT("危急")
			: Character->Pressure >= 6.0f ? TEXT("紧张") : TEXT("可控"),
			Character->Pressure >= Config.CriticalPressure ? EWSStatusSeverity::Critical
			: Character->Pressure >= 6.0f ? EWSStatusSeverity::Attention : EWSStatusSeverity::Normal);
	}
	else
	{
		Add(TEXT("态度"), Character->Trust >= 5.5f ? TEXT("愿意配合") : Character->Trust >= 4.0f ? TEXT("中立") : TEXT("有所戒备"), EWSStatusSeverity::Normal);
	}
	return View;
}
