#include "State/WhiteoutRulesEngine.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace WhiteoutRules
{
	const FName InvestigateGeneratorLog(TEXT("investigate_generator_log"));
	const FName InspectControlCabinet(TEXT("inspect_control_cabinet"));
	const FName TalkGuHeng(TEXT("talk_gu_heng"));
	const FName TalkYeCheng(TEXT("talk_ye_cheng"));
	const FName HeatRepairRoom(TEXT("heat_repair_room"));
	const FName HeatMedicalRoom(TEXT("heat_medical_room"));
	const FName DistributeFood(TEXT("distribute_food"));
	const FName TreatGuHeng(TEXT("treat_gu_heng"));
	const FName TreatCharacter(TEXT("treat_character"));
	const FName Rest(TEXT("rest"));
	const FName DismantleKitchenHeater(TEXT("dismantle_kitchen_heater"));
	const FName RepairGenerator(TEXT("repair_generator"));
	const FName ForcedSelfRepair(TEXT("forced_self_repair"));
	const FName CalibrateAntenna(TEXT("calibrate_antenna"));
	const FName SendSignal(TEXT("send_signal"));

	const FName FactGeneratorProtectionStop(TEXT("FACT_GENERATOR_PROTECTION_STOP"));
	const FName FactForcedRestartSuspicion(TEXT("FACT_FORCED_RESTART_SUSPICION"));
	const FName FactBurntRelay(TEXT("FACT_BURNT_RELAY"));
	const FName FactHandInjury(TEXT("FACT_HAND_INJURY"));
	const FName FactMedicalDiagnosis(TEXT("FACT_MEDICAL_DIAGNOSIS"));
	const FName FactHeatPack(TEXT("FACT_HEAT_PACK"));
	const FName FactRelayCompatibility(TEXT("FACT_RELAY_COMPATIBILITY"));
	const FName FactForcedRestartConfirmed(TEXT("FACT_FORCED_RESTART_CONFIRMED"));

	const FName TagInvestigation(TEXT("investigation"));
	const FName TagSocial(TEXT("social"));
	const FName TagPhysical(TEXT("physical"));
	const FName TagFineMotor(TEXT("fine_motor"));
	const FName TagOutdoor(TEXT("outdoor"));
	const FName TagMedical(TEXT("medical"));
	const FName TagRecovery(TEXT("recovery"));

	FWSCharacterState MakeCharacter(
		const float Health,
		const float Temperature,
		const float Hunger,
		const float Fatigue,
		const float Pressure,
		const float Trust)
	{
		FWSCharacterState Result;
		Result.Health = Health;
		Result.Temperature = Temperature;
		Result.Hunger = Hunger;
		Result.Fatigue = Fatigue;
		Result.Pressure = Pressure;
		Result.Trust = Trust;
		return Result;
	}

	void ParseCharacter(
		const TSharedPtr<FJsonObject>& CharactersObject,
		const TCHAR* FieldName,
		const EWSCharacterId CharacterId,
		FWSGameState& State)
	{
		const TSharedPtr<FJsonObject> Object = CharactersObject->GetObjectField(FieldName);
		State.Characters.Add(
			CharacterId,
			MakeCharacter(
				Object->GetNumberField(TEXT("health")),
				Object->GetNumberField(TEXT("temperature")),
				Object->GetNumberField(TEXT("hunger")),
				Object->GetNumberField(TEXT("fatigue")),
				Object->GetNumberField(TEXT("pressure")),
				Object->GetNumberField(TEXT("trust"))));
	}

	EWSCharacterId ParseCharacterId(const FString& Value)
	{
		if (Value == TEXT("gu_heng"))
		{
			return EWSCharacterId::GuHeng;
		}
		if (Value == TEXT("ye_cheng"))
		{
			return EWSCharacterId::YeCheng;
		}
		return EWSCharacterId::Player;
	}

	EWSCharacterLocation ParseLocation(const FString& Value)
	{
		if (Value == TEXT("repair_room"))
		{
			return EWSCharacterLocation::RepairRoom;
		}
		if (Value == TEXT("medical_room"))
		{
			return EWSCharacterLocation::MedicalRoom;
		}
		if (Value == TEXT("kitchen"))
		{
			return EWSCharacterLocation::Kitchen;
		}
		if (Value == TEXT("outdoor_antenna"))
		{
			return EWSCharacterLocation::OutdoorAntenna;
		}
		return EWSCharacterLocation::ControlRoom;
	}

	FWSCharacterState ParseV11Character(
		const TSharedPtr<FJsonObject>& CharactersObject,
		const TCHAR* FieldName,
		const EWSCharacterLocation DefaultLocation)
	{
		const TSharedPtr<FJsonObject> Object = CharactersObject->GetObjectField(FieldName);
		FWSCharacterState Result;
		Result.Health = 10.0f;
		Result.Hunger = 6.5f;
		Result.Fatigue = 6.5f;
		Result.Temperature = Object->GetNumberField(TEXT("temperature"));
		Result.Stamina = Object->GetIntegerField(TEXT("stamina"));
		Result.Pressure = Object->GetNumberField(TEXT("pressure"));
		double Trust = 5.0;
		Object->TryGetNumberField(TEXT("trust"), Trust);
		Result.Trust = static_cast<float>(Trust);
		Result.Location = DefaultLocation;
		Result.InjurySeverity = EWSInjurySeverity::Normal;

		const TArray<TSharedPtr<FJsonValue>>* Injuries = nullptr;
		if (Object->TryGetArrayField(TEXT("injuries"), Injuries))
		{
			for (const TSharedPtr<FJsonValue>& InjuryValue : *Injuries)
			{
				const FString Injury = InjuryValue->AsString();
				Result.InjuryId = FName(*Injury);
				if (Injury.EndsWith(TEXT("_critical")))
				{
					Result.InjurySeverity = EWSInjurySeverity::Critical;
				}
				else if (
					Injury.EndsWith(TEXT("_restricted"))
					&& Result.InjurySeverity != EWSInjurySeverity::Critical)
				{
					Result.InjurySeverity = EWSInjurySeverity::Restricted;
				}
			}
		}
		double WorseningMarks = 0.0;
		Object->TryGetNumberField(TEXT("injury_worsening_marks"), WorseningMarks);
		Result.InjuryWorseningMarks = static_cast<int32>(WorseningMarks);
		double BandageProtection = 0.0;
		Object->TryGetNumberField(TEXT("bandage_protection"), BandageProtection);
		Result.BandageProtection = static_cast<int32>(BandageProtection);
		return Result;
	}

	void ParseV11ActionRules(
		const TSharedPtr<FJsonObject>& Root,
		TMap<FName, FWhiteoutActionRule>& OutRules)
	{
		OutRules.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
		if (!Root->TryGetArrayField(TEXT("actions"), Actions))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& ActionValue : *Actions)
		{
			const TSharedPtr<FJsonObject> ActionObject = ActionValue->AsObject();
			if (!ActionObject.IsValid())
			{
				continue;
			}
			const FName ActionId(*ActionObject->GetStringField(TEXT("id")));
			FWhiteoutActionRule Rule;
			Rule.BaseAP = ActionObject->GetIntegerField(TEXT("base_ap"));
			Rule.PrimaryExecutor = ParseCharacterId(
				ActionObject->GetStringField(TEXT("primary_executor")));
			Rule.Location = ParseLocation(ActionObject->GetStringField(TEXT("location")));
			ActionObject->TryGetBoolField(TEXT("repeatable"), Rule.bRepeatable);
			double MaxUses = Rule.bRepeatable ? 3.0 : 1.0;
			ActionObject->TryGetNumberField(TEXT("max_uses"), MaxUses);
			Rule.MaxUses = static_cast<int32>(MaxUses);
			ActionObject->TryGetBoolField(TEXT("consumes_stamina"), Rule.bConsumesStamina);
			ActionObject->TryGetBoolField(TEXT("force_allowed"), Rule.bForceAllowed);

			const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
			if (ActionObject->TryGetArrayField(TEXT("tags"), Tags))
			{
				for (const TSharedPtr<FJsonValue>& Tag : *Tags)
				{
					Rule.Tags.Add(FName(*Tag->AsString()));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Collaborators = nullptr;
			if (ActionObject->TryGetArrayField(TEXT("collaborators"), Collaborators))
			{
				for (const TSharedPtr<FJsonValue>& Collaborator : *Collaborators)
				{
					Rule.Collaborators.Add(ParseCharacterId(Collaborator->AsString()));
				}
			}
			OutRules.Add(ActionId, MoveTemp(Rule));
		}

		if (const FWhiteoutActionRule* TreatRule = OutRules.Find(TreatCharacter))
		{
			OutRules.Add(TreatGuHeng, *TreatRule);
		}
	}
}

FWhiteoutRulesEngine::FWhiteoutRulesEngine()
{
	Config.InitialState.ActionPoints = 12;
	Config.InitialState.Phase = EWSGamePhase::ActionPhase;
	Config.InitialState.Characters.Add(
		EWSCharacterId::Player,
		WhiteoutRules::MakeCharacter(10.0f, 7.2f, 6.5f, 6.5f, 4.0f, 5.0f));
	Config.InitialState.Characters.Add(
		EWSCharacterId::GuHeng,
		WhiteoutRules::MakeCharacter(6.2f, 6.6f, 5.5f, 5.0f, 7.2f, 3.5f));
	Config.InitialState.Characters.Add(
		EWSCharacterId::YeCheng,
		WhiteoutRules::MakeCharacter(9.2f, 7.0f, 6.8f, 5.5f, 4.8f, 6.0f));
	Reset();
}

bool FWhiteoutRulesEngine::LoadConfig(const FString& ConfigPath, FString& OutError)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ConfigPath))
	{
		OutError = FString::Printf(TEXT("Unable to read rules config: %s"), *ConfigPath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = FString::Printf(TEXT("Invalid JSON rules config: %s"), *ConfigPath);
		return false;
	}

	double ParsedSchemaVersion = 3.0;
	Root->TryGetNumberField(TEXT("schema_version"), ParsedSchemaVersion);
	const int32 SchemaVersion = static_cast<int32>(ParsedSchemaVersion);
	if (SchemaVersion >= 4)
	{
		Config = FWhiteoutRuleConfig();
		Config.SchemaVersion = SchemaVersion;
		Root->TryGetStringField(TEXT("rules_version"), Config.RulesVersion);

		const TSharedPtr<FJsonObject> Gameplay = Root->GetObjectField(TEXT("gameplay"));
		const TSharedPtr<FJsonObject> Initial = Root->GetObjectField(TEXT("initial_state"));
		const TSharedPtr<FJsonObject> Resources = Initial->GetObjectField(TEXT("resources"));
		const TSharedPtr<FJsonObject> Tasks = Initial->GetObjectField(TEXT("tasks"));
		const TSharedPtr<FJsonObject> Thresholds = Root->GetObjectField(TEXT("thresholds"));
		const TSharedPtr<FJsonObject> TemperatureThresholds =
			Thresholds->GetObjectField(TEXT("temperature"));
		const TSharedPtr<FJsonObject> Heating = Root->GetObjectField(TEXT("heating"));
		const TSharedPtr<FJsonObject> UnheatedDelta =
			Heating->GetObjectField(TEXT("unheated_temperature_delta"));

		Config.StartingActionPoints = 12;
		Config.ActionPointsPerPhase =
			Gameplay->GetIntegerField(TEXT("action_points_per_phase"));
		Config.GeneratorRequired = Gameplay->GetIntegerField(TEXT("generator_required"));
		Config.AntennaRequired = Gameplay->GetIntegerField(TEXT("antenna_required"));
		Config.SafeWaitFuel = Gameplay->GetIntegerField(TEXT("safe_wait_fuel"));
		Config.WarmTemperature =
			TemperatureThresholds->GetNumberField(TEXT("warm"));
		Config.HypothermicTemperature =
			TemperatureThresholds->GetNumberField(TEXT("hypothermic"));
		Config.CriticalTemperature = Config.HypothermicTemperature;
		Config.SafeAntennaTemperature = Config.WarmTemperature;
		Config.HeatedTemperatureDelta =
			Heating->GetNumberField(TEXT("heated_temperature_delta"));
		Config.UnheatedTemperatureDelta.Add(
			EWSDayPhase::Morning,
			UnheatedDelta->GetNumberField(TEXT("morning")));
		Config.UnheatedTemperatureDelta.Add(
			EWSDayPhase::Afternoon,
			UnheatedDelta->GetNumberField(TEXT("afternoon")));
		Config.UnheatedTemperatureDelta.Add(
			EWSDayPhase::Dusk,
			UnheatedDelta->GetNumberField(TEXT("dusk")));
		WhiteoutRules::ParseV11ActionRules(Root, Config.ActionRules);

		if (
			Config.ActionPointsPerPhase != 4
			|| Config.ActionRules.Num() < 12)
		{
			OutError = TEXT("v1.1 requires 4 AP per phase and all configured actions");
			return false;
		}

		FWSGameState Parsed;
		Parsed.RulesSchemaVersion = Config.SchemaVersion;
		Parsed.RulesVersion = Config.RulesVersion;
		Parsed.ActionPoints = Config.ActionPointsPerPhase;
		Parsed.PhaseActionPoints = Config.ActionPointsPerPhase;
		Parsed.Phase = EWSGamePhase::ActionPhase;
		Parsed.DayPhase = EWSDayPhase::Morning;
		Parsed.bDayPhaseStarted = false;
		Parsed.bDayWindowClosed = false;
		Parsed.Resources.Fuel = Resources->GetIntegerField(TEXT("fuel"));
		Parsed.Resources.Food = Resources->GetIntegerField(TEXT("food"));
		Parsed.Resources.Medicine = Resources->GetIntegerField(TEXT("medicine"));
		Parsed.Resources.HeatPack = Resources->GetIntegerField(TEXT("heat_pack"));
		Parsed.Resources.ReplacementRelay =
			Resources->GetIntegerField(TEXT("replacement_relay"));
		Parsed.Tasks.GeneratorProgress =
			Tasks->GetIntegerField(TEXT("generator_progress"));
		Parsed.Tasks.AntennaCalibration =
			Tasks->GetIntegerField(TEXT("antenna_calibration"));
		Parsed.Tasks.bSignalSent = Tasks->GetBoolField(TEXT("signal_sent"));
		Tasks->TryGetBoolField(
			TEXT("generator_stable"),
			Parsed.Tasks.bGeneratorStable);

		const TSharedPtr<FJsonObject> Characters =
			Initial->GetObjectField(TEXT("characters"));
		Parsed.Characters.Add(
			EWSCharacterId::Player,
			WhiteoutRules::ParseV11Character(
				Characters,
				TEXT("player"),
				EWSCharacterLocation::ControlRoom));
		Parsed.Characters.Add(
			EWSCharacterId::GuHeng,
			WhiteoutRules::ParseV11Character(
				Characters,
				TEXT("gu_heng"),
				EWSCharacterLocation::RepairRoom));
		Parsed.Characters.Add(
			EWSCharacterId::YeCheng,
			WhiteoutRules::ParseV11Character(
				Characters,
				TEXT("ye_cheng"),
				EWSCharacterLocation::MedicalRoom));

		for (const TPair<EWSCharacterId, FWSCharacterState>& Pair : Parsed.Characters)
		{
			const FWSCharacterState& CharacterState = Pair.Value;
			if (
				CharacterState.Temperature < 0.0f
				|| CharacterState.Temperature > 10.0f
				|| CharacterState.Stamina < 0
				|| CharacterState.Stamina > 2
				|| CharacterState.Pressure < 0.0f
				|| CharacterState.Pressure > 10.0f
				|| CharacterState.Trust < 0.0f
				|| CharacterState.Trust > 10.0f)
			{
				OutError = TEXT("v1.1 character state is outside configured bounds");
				return false;
			}
		}

		Config.InitialState = MoveTemp(Parsed);
		Reset();
		OutError.Reset();
		return true;
	}

	Config = FWhiteoutRuleConfig();
	Config.SchemaVersion = SchemaVersion;
	Root->TryGetStringField(TEXT("rules_version"), Config.RulesVersion);
	const TSharedPtr<FJsonObject> Gameplay = Root->GetObjectField(TEXT("gameplay"));
	const TSharedPtr<FJsonObject> Initial = Root->GetObjectField(TEXT("initial_state"));
	const TSharedPtr<FJsonObject> Resources = Initial->GetObjectField(TEXT("resources"));
	const TSharedPtr<FJsonObject> Flags = Initial->GetObjectField(TEXT("flags"));
	const TSharedPtr<FJsonObject> Tasks = Initial->GetObjectField(TEXT("tasks"));
	const TSharedPtr<FJsonObject> Balance = Root->GetObjectField(TEXT("balance"));
	const TSharedPtr<FJsonObject> Thresholds = Balance->GetObjectField(TEXT("thresholds"));

	Config.StartingActionPoints = Gameplay->GetIntegerField(TEXT("starting_action_points"));
	Config.MidCrisisThreshold = Gameplay->GetIntegerField(TEXT("mid_crisis_threshold"));
	Config.GeneratorRequired = Gameplay->GetIntegerField(TEXT("generator_required"));
	Config.AntennaRequired = Gameplay->GetIntegerField(TEXT("antenna_required"));
	Config.ModelCallHardLimit = Gameplay->GetIntegerField(TEXT("model_call_hard_limit"));
	Config.SafeAntennaTemperature = Thresholds->GetNumberField(TEXT("safe_antenna_temperature"));
	Config.CriticalHealth = Thresholds->GetNumberField(TEXT("critical_health"));
	Config.CriticalTemperature = Thresholds->GetNumberField(TEXT("critical_temperature"));
	Config.CriticalFatigue = Thresholds->GetNumberField(TEXT("critical_fatigue"));
	Config.CriticalPressure = Thresholds->GetNumberField(TEXT("critical_pressure"));

	FWSGameState Parsed;
	Parsed.ActionPoints = Config.StartingActionPoints;
	Parsed.Phase = EWSGamePhase::ActionPhase;
	Parsed.Resources.Fuel = Resources->GetIntegerField(TEXT("fuel"));
	Parsed.Resources.Food = Resources->GetIntegerField(TEXT("food"));
	Parsed.Resources.Medicine = Resources->GetIntegerField(TEXT("medicine"));
	Parsed.Resources.HeatPack = Resources->GetIntegerField(TEXT("heat_pack"));
	Parsed.Resources.ReplacementRelay = Resources->GetIntegerField(TEXT("replacement_relay"));
	Parsed.Flags.bKitchenHeaterIntact = Flags->GetBoolField(TEXT("kitchen_heater_intact"));
	Parsed.Flags.bHeatPackRevealed = Flags->GetBoolField(TEXT("heat_pack_revealed"));
	Parsed.Flags.bRepairRoomHeated = Flags->GetBoolField(TEXT("repair_room_heated"));
	Parsed.Flags.bMedicalRoomHeated = Flags->GetBoolField(TEXT("medical_room_heated"));
	Parsed.Flags.bGuHengDiagnosed = Flags->GetBoolField(TEXT("gu_heng_diagnosed"));
	Parsed.Flags.bGuHengTreated = Flags->GetBoolField(TEXT("gu_heng_treated"));
	Parsed.Flags.bGuHengFed = Flags->GetBoolField(TEXT("gu_heng_fed"));
	Parsed.Flags.bGuHengCooperative = Flags->GetBoolField(TEXT("gu_heng_cooperative"));
	Parsed.Flags.bRelayCompatibilityKnown = Flags->GetBoolField(TEXT("relay_compatibility_known"));
	Parsed.Flags.bRelayInstalled = Flags->GetBoolField(TEXT("relay_installed"));
	Parsed.Flags.bSelfRepairUsed = Flags->GetBoolField(TEXT("self_repair_used"));
	Parsed.Flags.bRecordsPreserved = Flags->GetBoolField(TEXT("records_preserved"));
	Parsed.Tasks.GeneratorProgress = Tasks->GetIntegerField(TEXT("generator_progress"));
	Parsed.Tasks.AntennaCalibration = Tasks->GetIntegerField(TEXT("antenna_calibration"));
	Parsed.Tasks.bSignalSent = Tasks->GetBoolField(TEXT("signal_sent"));

	const TSharedPtr<FJsonObject> Characters = Initial->GetObjectField(TEXT("characters"));
	WhiteoutRules::ParseCharacter(Characters, TEXT("player"), EWSCharacterId::Player, Parsed);
	WhiteoutRules::ParseCharacter(Characters, TEXT("gu_heng"), EWSCharacterId::GuHeng, Parsed);
	WhiteoutRules::ParseCharacter(Characters, TEXT("ye_cheng"), EWSCharacterId::YeCheng, Parsed);

	if (Config.StartingActionPoints != 12 || Config.MidCrisisThreshold != 6)
	{
		OutError = TEXT("v1.0 requires 12 starting AP and a 6 AP crisis threshold");
		return false;
	}
	for (const TPair<EWSCharacterId, FWSCharacterState>& Pair : Parsed.Characters)
	{
		const FWSCharacterState& CharacterState = Pair.Value;
		if (CharacterState.Health < 0.0f || CharacterState.Health > 10.0f
			|| CharacterState.Temperature < 0.0f || CharacterState.Temperature > 10.0f
			|| CharacterState.Hunger < 0.0f || CharacterState.Hunger > 10.0f
			|| CharacterState.Fatigue < 0.0f || CharacterState.Fatigue > 10.0f
			|| CharacterState.Pressure < 0.0f || CharacterState.Pressure > 10.0f
			|| CharacterState.Trust < 0.0f || CharacterState.Trust > 10.0f)
		{
			OutError = TEXT("v1.0 character attributes must remain within 0..10");
			return false;
		}
	}

	Config.InitialState = MoveTemp(Parsed);
	Reset();
	OutError.Reset();
	return true;
}

void FWhiteoutRulesEngine::Reset()
{
	State = Config.InitialState;
	State.RulesSchemaVersion = Config.SchemaVersion;
	State.RulesVersion = Config.RulesVersion;
	State.ActionPoints =
		IsV11() ? Config.ActionPointsPerPhase : Config.StartingActionPoints;
	State.PhaseActionPoints =
		IsV11() ? Config.ActionPointsPerPhase : State.ActionPoints;
	State.Phase = EWSGamePhase::ActionPhase;
	State.DayPhase = EWSDayPhase::Morning;
	State.bDayPhaseStarted = false;
	State.bDayWindowClosed = false;
	State.Heating = FWSHeatingState();
	State.bMidCrisisTriggered = false;
	State.PlayerKnowledge.Reset();
	State.Evidence.Reset();
	State.PublicFacts.Reset();
	State.ActionCounts.Reset();
	State.CommittedTransactions.Reset();
	State.Promises.Reset();
	State.EventLog.Reset();
	State.PhaseSummaries.Reset();
	State.ModelCalls = 0;
	State.Score = FWSScoreBreakdown();
}

void FWhiteoutRulesEngine::SetState(const FWSGameState& InState)
{
	State = InState;
	for (TPair<EWSCharacterId, FWSCharacterState>& Pair : State.Characters)
	{
		FWSCharacterState& CharacterState = Pair.Value;
		CharacterState.Health = FMath::Clamp(CharacterState.Health, 0.0f, 10.0f);
		CharacterState.Temperature = FMath::Clamp(CharacterState.Temperature, 0.0f, 10.0f);
		CharacterState.Hunger = FMath::Clamp(CharacterState.Hunger, 0.0f, 10.0f);
		CharacterState.Fatigue = FMath::Clamp(CharacterState.Fatigue, 0.0f, 10.0f);
		CharacterState.Pressure = FMath::Clamp(CharacterState.Pressure, 0.0f, 10.0f);
		CharacterState.Trust = FMath::Clamp(CharacterState.Trust, 0.0f, 10.0f);
		CharacterState.Stamina = FMath::Clamp(CharacterState.Stamina, 0, 2);
		CharacterState.InjuryWorseningMarks =
			FMath::Max(0, CharacterState.InjuryWorseningMarks);
		CharacterState.BandageProtection =
			FMath::Max(0, CharacterState.BandageProtection);
		CharacterState.TemporarySupportUses =
			FMath::Max(0, CharacterState.TemporarySupportUses);
	}
	if (IsV11())
	{
		State.PhaseActionPoints = FMath::Clamp(
			State.PhaseActionPoints,
			0,
			Config.ActionPointsPerPhase);
		State.ActionPoints = State.PhaseActionPoints;
	}
}

FWSActionPreview FWhiteoutRulesEngine::Preview(const FWSActionRequest& Request) const
{
	if (IsV11())
	{
		return BuildV11Preview(Request);
	}

	FWSActionPreview Result;
	Result.ActionId = Request.ActionId;
	Result.APCost = GetActionCost(Request.ActionId);
	Result.BaseAP = Result.APCost;
	Result.RawAP = Result.APCost;
	Result.PreviewText = ActionPreviewText(Request.ActionId);
	Result.RiskText = ActionRiskText(Request.ActionId);
	Result.ReasonCode = CanExecute(Request);
	Result.bCanExecute = Result.ReasonCode == EWSReasonCode::Ok;
	Result.WorkReadiness =
		Result.bCanExecute ? EWSWorkReadiness::Ready : EWSWorkReadiness::Unavailable;
	return Result;
}

FWSActionResult FWhiteoutRulesEngine::Commit(FWSActionRequest Request)
{
	if (IsV11())
	{
		return CommitV11(MoveTemp(Request));
	}

	FWSActionResult Result;
	Result.ActionId = Request.ActionId;
	Result.DialogueAct = Request.DialogueAct;
	Result.PromiseCondition = Request.PromiseCondition;
	Result.APBefore = State.ActionPoints;
	Result.APAfter = State.ActionPoints;
	if (!Request.TransactionId.IsValid())
	{
		Request.TransactionId = FGuid::NewGuid();
	}
	Result.TransactionId = Request.TransactionId;

	if (State.CommittedTransactions.Contains(Request.TransactionId))
	{
		Result.ReasonCode = EWSReasonCode::DuplicateTransaction;
		return Result;
	}

	Result.ReasonCode = CanExecute(Request);
	if (Result.ReasonCode != EWSReasonCode::Ok)
	{
		return Result;
	}

	State.Phase = EWSGamePhase::ResolvingAction;
	const int32 PromiseCountBefore = State.Promises.Num();
	ApplyEffect(Request, Result.Changes);
	Result.bPromiseRecorded = State.Promises.Num() > PromiseCountBefore;
	const int32 Cost = GetActionCost(Request.ActionId);
	ApplyEnvironment(Cost, Request.ActionId == WhiteoutRules::CalibrateAntenna, Result.Changes);
	State.ActionPoints = FMath::Max(0, Result.APBefore - Cost);
	State.ActionCounts.FindOrAdd(Request.ActionId) += 1;
	State.CommittedTransactions.Add(Request.TransactionId);

	if (
		!State.bMidCrisisTriggered && Result.APBefore > Config.MidCrisisThreshold
		&& State.ActionPoints <= Config.MidCrisisThreshold)
	{
		TriggerMidCrisis(Result.Changes);
		Result.bCrisisTriggered = true;
	}

	if (Request.ActionId == WhiteoutRules::SendSignal)
	{
		State.Phase = EWSGamePhase::EndingChoice;
	}
	else if (SignalAvailable())
	{
		State.Phase = EWSGamePhase::PostActionWindow;
	}
	else if (State.ActionPoints == 0)
	{
		State.Phase = EWSGamePhase::Ending;
	}
	else
	{
		State.Phase = EWSGamePhase::ActionPhase;
	}

	FWSEventRecord Event;
	Event.Index = State.EventLog.Num() + 1;
	Event.ActionId = Request.ActionId;
	Event.TransactionId = Request.TransactionId;
	Event.APBefore = Result.APBefore;
	Event.APAfter = State.ActionPoints;
	Event.ReasonCode = EWSReasonCode::Committed;
	Event.DialogueAct = Request.DialogueAct;
	Event.PromiseCondition = Request.PromiseCondition;
	Event.bPromiseRecorded = Result.bPromiseRecorded;
	Event.Changes = Result.Changes;
	Event.bCrisisTriggered = Result.bCrisisTriggered;
	State.EventLog.Add(MoveTemp(Event));

	Result.bCommitted = true;
	Result.ReasonCode = EWSReasonCode::Committed;
	Result.APAfter = State.ActionPoints;
	Result.BaseAP = Cost;
	Result.ActualAP = Cost;
	Result.WorkReadiness = EWSWorkReadiness::Ready;
	return Result;
}

EWSReasonCode FWhiteoutRulesEngine::CanExecute(const FWSActionRequest& Request) const
{
	if (IsV11())
	{
		return CanExecuteV11(Request);
	}

	using namespace WhiteoutRules;
	if (!IsCoreAction(Request.ActionId))
	{
		return EWSReasonCode::UnknownAction;
	}
	if (State.Phase != EWSGamePhase::ActionPhase && State.Phase != EWSGamePhase::PostActionWindow)
	{
		return EWSReasonCode::PhaseLocked;
	}
	if (GetActionCost(Request.ActionId) > State.ActionPoints)
	{
		return EWSReasonCode::InsufficientAP;
	}
	const int32 Count = ActionCount(Request.ActionId);
	if (!ActionRepeatable(Request.ActionId) && Count > 0)
	{
		return EWSReasonCode::AlreadyCompleted;
	}
	if (Count >= ActionMaxUses(Request.ActionId))
	{
		return EWSReasonCode::UseLimitReached;
	}

	if (Request.ActionId == TalkGuHeng || Request.ActionId == TalkYeCheng)
	{
		const bool bAllowedAct = Request.DialogueAct == EWSDialogueAct::Ask
			|| Request.DialogueAct == EWSDialogueAct::Challenge
			|| Request.DialogueAct == EWSDialogueAct::Reassure
			|| Request.DialogueAct == EWSDialogueAct::Promise;
		if (!bAllowedAct)
		{
			return EWSReasonCode::DialogueActUnavailable;
		}
		if (!Request.PromiseCondition.IsNone() && Request.DialogueAct != EWSDialogueAct::Promise)
		{
			return EWSReasonCode::InvalidPromiseCondition;
		}

		if (Request.DialogueAct == EWSDialogueAct::Challenge)
		{
			const bool bChallengeAvailable = Request.ActionId == TalkGuHeng
				? Knows(FactForcedRestartSuspicion) || Knows(FactBurntRelay)
				: State.Flags.bHeatPackRevealed;
			if (!bChallengeAvailable)
			{
				return EWSReasonCode::DialogueActUnavailable;
			}
		}
		else if (Request.DialogueAct == EWSDialogueAct::Reassure)
		{
			const EWSCharacterId CharacterId = Request.ActionId == TalkGuHeng
				? EWSCharacterId::GuHeng
				: EWSCharacterId::YeCheng;
			const float Pressure = Character(CharacterId).Pressure;
			const bool bReassureAvailable = State.bMidCrisisTriggered
				|| Pressure >= (CharacterId == EWSCharacterId::GuHeng ? 6.5f : 6.0f)
				|| (CharacterId == EWSCharacterId::GuHeng && State.Flags.bGuHengDiagnosed);
			if (!bReassureAvailable)
			{
				return EWSReasonCode::DialogueActUnavailable;
			}
		}

		if (Request.DialogueAct == EWSDialogueAct::Promise)
		{
			static const TSet<FName> AllowedConditions = {
				TEXT("reserve_medicine"), TEXT("keep_records"), TEXT("heat_repair_room")};
			if (!AllowedConditions.Contains(Request.PromiseCondition))
			{
				return EWSReasonCode::InvalidPromiseCondition;
			}
			if (Request.ActionId != TalkGuHeng)
			{
				return EWSReasonCode::DialogueActUnavailable;
			}
			const bool bContextAvailable =
				(Request.PromiseCondition == TEXT("reserve_medicine")
					&& State.Flags.bGuHengDiagnosed
					&& State.Resources.Medicine > 0)
				|| (Request.PromiseCondition == TEXT("keep_records")
					&& (Knows(FactForcedRestartSuspicion)
						|| Knows(FactForcedRestartConfirmed)))
				|| (Request.PromiseCondition == TEXT("heat_repair_room")
					&& (State.Flags.bGuHengDiagnosed || Knows(FactHandInjury))
					&& !State.Flags.bRepairRoomHeated);
			if (!bContextAvailable)
			{
				return EWSReasonCode::DialogueActUnavailable;
			}
			const FName PromiseId(*FString::Printf(
				TEXT("player_to_gu_heng:%s"),
				*Request.PromiseCondition.ToString()));
			if (State.Promises.ContainsByPredicate(
					[PromiseId](const FWSPromiseRecord& Promise)
					{
						return Promise.PromiseId == PromiseId;
					}))
			{
				return EWSReasonCode::DuplicatePromise;
			}
		}
	}

	if (Request.ActionId == HeatRepairRoom)
	{
		if (State.Flags.bRepairRoomHeated) return EWSReasonCode::AlreadyHeated;
		if (State.Resources.Fuel < 1) return EWSReasonCode::NeedsFuel;
	}
	else if (Request.ActionId == HeatMedicalRoom)
	{
		if (State.Flags.bMedicalRoomHeated) return EWSReasonCode::AlreadyHeated;
		if (State.Resources.Fuel < 1) return EWSReasonCode::NeedsFuel;
	}
	else if (Request.ActionId == DistributeFood)
	{
		const int32 Values[] = {Request.FoodForPlayer, Request.FoodForGuHeng, Request.FoodForYeCheng};
		int32 Total = 0;
		for (const int32 Value : Values)
		{
			if (Value < 0 || Value > 1) return EWSReasonCode::InvalidFoodAllocation;
			Total += Value;
		}
		if (Total == 0) return EWSReasonCode::EmptyFoodAllocation;
		if (Total > State.Resources.Food) return EWSReasonCode::InsufficientFood;
	}
	else if (Request.ActionId == TreatGuHeng)
	{
		if (!State.Flags.bMedicalRoomHeated) return EWSReasonCode::NeedsMedicalHeat;
		if (!State.Flags.bGuHengDiagnosed) return EWSReasonCode::NeedsDiagnosis;
		if (Request.TreatmentResource != EWSResourceType::Medicine
			&& Request.TreatmentResource != EWSResourceType::HeatPack)
		{
			return EWSReasonCode::InvalidTreatmentResource;
		}
		if (Request.TreatmentResource == EWSResourceType::HeatPack)
		{
			if (!State.Flags.bHeatPackRevealed) return EWSReasonCode::HeatPackHidden;
			if (State.Resources.HeatPack < 1) return EWSReasonCode::NeedsHeatPack;
		}
		else if (State.Resources.Medicine < 1)
		{
			return EWSReasonCode::NeedsMedicine;
		}
	}
	else if (Request.ActionId == DismantleKitchenHeater)
	{
		if (!Knows(FactBurntRelay)) return EWSReasonCode::NeedsRelayEvidence;
		if (!State.Flags.bKitchenHeaterIntact) return EWSReasonCode::HeaterAlreadyDismantled;
	}
	else if (Request.ActionId == RepairGenerator)
	{
		if (State.Tasks.GeneratorProgress >= Config.GeneratorRequired)
			return EWSReasonCode::GeneratorAlreadyRepaired;
		if (Character(EWSCharacterId::GuHeng).Health <= 3.0f) return EWSReasonCode::GuHengCritical;
		if (
			!State.Flags.bGuHengCooperative && !State.Flags.bGuHengTreated
			&& !(State.Flags.bRepairRoomHeated && State.Flags.bGuHengFed))
		{
			return EWSReasonCode::NeedsCooperation;
		}
	}
	else if (Request.ActionId == ForcedSelfRepair)
	{
		if (!Knows(FactForcedRestartSuspicion)) return EWSReasonCode::NeedsGeneratorRecords;
		if (State.Flags.bSelfRepairUsed) return EWSReasonCode::SelfRepairAlreadyUsed;
		if (State.Tasks.GeneratorProgress >= Config.GeneratorRequired)
			return EWSReasonCode::GeneratorAlreadyRepaired;
	}
	else if (Request.ActionId == CalibrateAntenna)
	{
		if (State.Tasks.GeneratorProgress < Config.GeneratorRequired) return EWSReasonCode::NeedsGenerator;
		if (State.Tasks.AntennaCalibration >= Config.AntennaRequired)
			return EWSReasonCode::AntennaAlreadyCalibrated;
		if (Character(EWSCharacterId::Player).Temperature < Config.SafeAntennaTemperature)
			return EWSReasonCode::PlayerTooCold;
	}
	else if (Request.ActionId == SendSignal)
	{
		if (State.Tasks.GeneratorProgress < Config.GeneratorRequired) return EWSReasonCode::NeedsGenerator;
		if (State.Tasks.AntennaCalibration < Config.AntennaRequired) return EWSReasonCode::NeedsAntenna;
	}
	return EWSReasonCode::Ok;
}

EWSReasonCode FWhiteoutRulesEngine::CanExecuteV11(
	const FWSActionRequest& Request) const
{
	using namespace WhiteoutRules;
	if (!IsV11Action(Request.ActionId))
	{
		return EWSReasonCode::UnknownAction;
	}
	if (State.bDayWindowClosed)
	{
		return EWSReasonCode::WindowClosed;
	}
	if (!State.bDayPhaseStarted)
	{
		return EWSReasonCode::PhaseNotStarted;
	}
	if (
		State.Phase != EWSGamePhase::ActionPhase
		&& State.Phase != EWSGamePhase::PostActionWindow)
	{
		return EWSReasonCode::PhaseLocked;
	}

	const FWhiteoutActionRule* Rule = Config.ActionRules.Find(Request.ActionId);
	if (!Rule)
	{
		return EWSReasonCode::UnknownAction;
	}
	const int32 Count = ActionCount(Request.ActionId);
	if (!Rule->bRepeatable && Count > 0)
	{
		return EWSReasonCode::AlreadyCompleted;
	}
	if (Count >= Rule->MaxUses)
	{
		return EWSReasonCode::UseLimitReached;
	}

	if (Request.ActionId == TalkGuHeng || Request.ActionId == TalkYeCheng)
	{
		const bool bAllowedAct =
			Request.DialogueAct == EWSDialogueAct::Ask
			|| Request.DialogueAct == EWSDialogueAct::Challenge
			|| Request.DialogueAct == EWSDialogueAct::Command
			|| Request.DialogueAct == EWSDialogueAct::Promise
			|| Request.DialogueAct == EWSDialogueAct::Trade
			|| Request.DialogueAct == EWSDialogueAct::Reassure;
		if (!bAllowedAct)
		{
			return EWSReasonCode::DialogueActUnavailable;
		}
		if (
			!Request.PromiseCondition.IsNone()
			&& Request.DialogueAct != EWSDialogueAct::Promise)
		{
			return EWSReasonCode::InvalidPromiseCondition;
		}
		if (Request.DialogueAct == EWSDialogueAct::Promise)
		{
			static const TSet<FName> AllowedConditions = {
				TEXT("reserve_medicine"),
				TEXT("keep_records"),
				TEXT("preserve_records"),
				TEXT("heat_repair_room")};
			if (
				Request.ActionId != TalkGuHeng
				|| !AllowedConditions.Contains(Request.PromiseCondition))
			{
				return Request.ActionId != TalkGuHeng
					? EWSReasonCode::DialogueActUnavailable
					: EWSReasonCode::InvalidPromiseCondition;
			}
			const FName PromiseId(*FString::Printf(
				TEXT("player_to_gu_heng:%s"),
				*Request.PromiseCondition.ToString()));
			if (State.Promises.ContainsByPredicate(
					[PromiseId](const FWSPromiseRecord& Promise)
					{
						return Promise.PromiseId == PromiseId;
					}))
			{
				return EWSReasonCode::DuplicatePromise;
			}
		}
	}
	else if (Request.ActionId == DistributeFood)
	{
		const int32 Values[] = {
			Request.FoodForPlayer,
			Request.FoodForGuHeng,
			Request.FoodForYeCheng};
		int32 Total = 0;
		for (const int32 Value : Values)
		{
			if (Value < 0 || Value > 1)
			{
				return EWSReasonCode::InvalidFoodAllocation;
			}
			Total += Value;
		}
		if (Total == 0)
		{
			return EWSReasonCode::EmptyFoodAllocation;
		}
		if (Total > 2)
		{
			return EWSReasonCode::InvalidFoodAllocation;
		}
		if (Total > State.Resources.Food)
		{
			return EWSReasonCode::InsufficientFood;
		}
		if (
			Request.bHotMeal
			&& (
				State.Heating.CurrentZone != EWSHeatingZone::Kitchen
				|| !State.Flags.bKitchenHeaterIntact))
		{
			return EWSReasonCode::HotMealUnavailable;
		}
	}
	else if (
		Request.ActionId == TreatCharacter
		|| Request.ActionId == TreatGuHeng)
	{
		if (Character(EWSCharacterId::YeCheng).Stamina <= 0)
		{
			return EWSReasonCode::YeChengExhausted;
		}
		const EWSCharacterId Target =
			Request.ActionId == TreatGuHeng
			? EWSCharacterId::GuHeng
			: Request.TreatmentTarget;
		const FWSCharacterState& TargetState = Character(Target);
		const EWSTreatmentMethod Method =
			Request.ActionId == TreatGuHeng
				&& Request.TreatmentResource == EWSResourceType::HeatPack
			? EWSTreatmentMethod::HeatPack
			: Request.TreatmentMethod;
		const bool bTreatmentNeeded =
			TargetState.InjurySeverity != EWSInjurySeverity::Normal
			|| TargetState.Temperature < Config.WarmTemperature;
		if (Method != EWSTreatmentMethod::HeatPack && !bTreatmentNeeded)
		{
			return EWSReasonCode::TreatmentNotNeeded;
		}
		if (Method == EWSTreatmentMethod::Full)
		{
			if (State.Heating.CurrentZone != EWSHeatingZone::MedicalRoom)
			{
				return EWSReasonCode::NeedsHeatedMedicalRoom;
			}
			if (State.Resources.Medicine < 1)
			{
				return EWSReasonCode::NeedsMedicine;
			}
		}
		else if (Method == EWSTreatmentMethod::HeatPack)
		{
			if (!State.Flags.bHeatPackRevealed)
			{
				return EWSReasonCode::HeatPackHidden;
			}
			if (State.Resources.HeatPack < 1)
			{
				return EWSReasonCode::NeedsHeatPack;
			}
		}
	}
	else if (Request.ActionId == DismantleKitchenHeater)
	{
		if (!State.Flags.bKitchenHeaterIntact)
		{
			return EWSReasonCode::HeaterAlreadyDismantled;
		}
		if (
			!State.Flags.bCabinetInspected
			|| !State.Flags.bRelayCompatibilityKnown)
		{
			return EWSReasonCode::NeedsRelayKnowledge;
		}
	}
	else if (Request.ActionId == RepairGenerator)
	{
		const EWSReasonCode RepairReason = EvaluateRepairGeneratorReason(Request, nullptr);
		if (RepairReason != EWSReasonCode::Ok)
		{
			return RepairReason;
		}
	}
	else if (Request.ActionId == ForcedSelfRepair)
	{
		if (State.Flags.bSelfRepairUsed)
		{
			return EWSReasonCode::SelfRepairAlreadyUsed;
		}
		if (State.Tasks.GeneratorProgress >= Config.GeneratorRequired)
		{
			return EWSReasonCode::GeneratorAlreadyRepaired;
		}
	}
	else if (Request.ActionId == CalibrateAntenna)
	{
		if (State.Tasks.GeneratorProgress < Config.GeneratorRequired)
		{
			return EWSReasonCode::NeedsGenerator;
		}
		if (State.Tasks.AntennaCalibration >= Config.AntennaRequired)
		{
			return EWSReasonCode::AntennaAlreadyCalibrated;
		}
	}
	else if (Request.ActionId == SendSignal)
	{
		if (State.Tasks.GeneratorProgress < Config.GeneratorRequired)
		{
			return EWSReasonCode::NeedsGenerator;
		}
		if (State.Tasks.AntennaCalibration < Config.AntennaRequired)
		{
			return EWSReasonCode::NeedsAntenna;
		}
	}
	return EWSReasonCode::Ok;
}

EWSReasonCode FWhiteoutRulesEngine::EvaluateRepairGeneratorReason(
	const FWSActionRequest& Request,
	FWSActionRequirementReport* OutReport) const
{
	using namespace WhiteoutRules;
	const FWSCharacterState& GuHeng = Character(EWSCharacterId::GuHeng);
	const bool bGeneratorIncomplete = State.Tasks.GeneratorProgress < Config.GeneratorRequired;
	const bool bGuAvailable = GuHeng.InjurySeverity != EWSInjurySeverity::Critical
		&& GuHeng.Stamina > 0;
	const bool bHardRefusal = !Request.bForce
		&& (GuHeng.Trust < 3.0f || GuHeng.Pressure >= 9.0f);
	const bool bPlayerCollaborationRequired = !Request.bForce
		&& (GuHeng.Trust < 4.5f || GuHeng.Pressure >= 8.0f);
	const bool bHasPlayerCollaboration = Request.bHasCollaborator
		&& Request.Collaborator == EWSCharacterId::Player;
	const bool bSocialConditionSatisfied = !bPlayerCollaborationRequired
		|| bHasPlayerCollaboration;
	const bool bRepairRoomHeated = State.Heating.CurrentZone == EWSHeatingZone::RepairRoom;
	const bool bStaminaReady = GuHeng.Stamina >= 2;
	const bool bSupportedPlanReady = bRepairRoomHeated && bStaminaReady;
	const bool bReplacementRelayAvailable = State.Resources.ReplacementRelay > 0;
	const bool bRelayPlanSelectedAndReady = Request.bUseRelay && bReplacementRelayAvailable;

	EWSReasonCode Result = EWSReasonCode::Ok;
	if (!bGeneratorIncomplete)
	{
		Result = EWSReasonCode::GeneratorAlreadyRepaired;
	}
	else if (!bGuAvailable || bHardRefusal)
	{
		Result = EWSReasonCode::GuHengRefused;
	}
	else if (!bSocialConditionSatisfied)
	{
		Result = EWSReasonCode::NeedsGuHengConditions;
	}
	else if (!Request.bForce && !bSupportedPlanReady && !bRelayPlanSelectedAndReady)
	{
		Result = EWSReasonCode::NeedsGuHengConditions;
	}
	else if (Request.bUseRelay && !bReplacementRelayAvailable)
	{
		Result = EWSReasonCode::NeedsReplacementRelay;
	}

	if (OutReport)
	{
		FWSActionRequirementReport& Report = *OutReport;
		Report = FWSActionRequirementReport();
		Report.ActionId = RepairGenerator;
		Report.bCurrentlyExecutable = Result == EWSReasonCode::Ok;

		FWSRequirementItem Available;
		Available.RequirementId = TEXT("gu_heng_available");
		Available.bSatisfied = bGuAvailable && !bHardRefusal;
		Available.RemediationActionId = GuHeng.Stamina <= 0 ? Rest : TreatCharacter;
		Available.Explanation = FText::FromString(
			Available.bSatisfied
				? TEXT("顾衡目前可以参与维修")
				: TEXT("先让顾衡恢复到能够安全工作的状态"));
		Report.UniversalRequirements.Add(Available);

		FWSRequirementItem Collaboration;
		Collaboration.RequirementId = TEXT("player_collaboration");
		Collaboration.bSatisfied = bSocialConditionSatisfied;
		Collaboration.RemediationActionId = TalkGuHeng;
		Collaboration.Explanation = FText::FromString(
			bSocialConditionSatisfied
				? TEXT("当前配合关系足以继续")
				: TEXT("维修时由你在旁协助，别让他独自承担精细操作"));
		Report.UniversalRequirements.Add(Collaboration);

		FWSRequirementPlan SupportedPlan;
		SupportedPlan.PlanId = TEXT("supported_repair");
		SupportedPlan.EstimatedAP = (bRepairRoomHeated ? 0 : 1) + (bStaminaReady ? 0 : 1);
		SupportedPlan.RiskScore = GuHeng.InjurySeverity == EWSInjurySeverity::Restricted ? 0.55f : 0.20f;
		FWSRequirementItem RepairHeat;
		RepairHeat.RequirementId = TEXT("repair_room_heated");
		RepairHeat.bSatisfied = bRepairRoomHeated;
		RepairHeat.RemediationActionId = HeatRepairRoom;
		RepairHeat.Explanation = FText::FromString(
			bRepairRoomHeated ? TEXT("维修间已经升温") : TEXT("先把维修间升温"));
		SupportedPlan.Requirements.Add(RepairHeat);
		FWSRequirementItem Stamina;
		Stamina.RequirementId = TEXT("gu_heng_stamina_ready");
		Stamina.bSatisfied = bStaminaReady;
		Stamina.RemediationActionId = Rest;
		Stamina.Explanation = FText::FromString(
			bStaminaReady ? TEXT("顾衡的体力足够") : TEXT("让顾衡恢复至少两点体力"));
		SupportedPlan.Requirements.Add(Stamina);
		Report.AlternativePlans.Add(SupportedPlan);

		FWSRequirementPlan RelayPlan;
		RelayPlan.PlanId = TEXT("relay_replacement");
		RelayPlan.EstimatedAP = bReplacementRelayAvailable ? 0 : 1;
		RelayPlan.RiskScore = 0.35f;
		FWSRequirementItem Relay;
		Relay.RequirementId = TEXT("replacement_relay_available");
		Relay.bSatisfied = bReplacementRelayAvailable;
		Relay.bDisclosable = true;
		Relay.RemediationActionId = State.Flags.bRelayCompatibilityKnown
			? DismantleKitchenHeater
			: InspectControlCabinet;
		Relay.Explanation = FText::FromString(
			bReplacementRelayAvailable
				? TEXT("可靠替代件已经备好")
				: State.Flags.bRelayCompatibilityKnown
					? TEXT("可以从已确认兼容的设备取得替代件")
					: TEXT("找到并确认一只可靠的替代继电器"));
		RelayPlan.Requirements.Add(Relay);
		Report.AlternativePlans.Add(RelayPlan);

		FWSRequirementItem HandRisk;
		HandRisk.RequirementId = TEXT("right_hand_injury_risk");
		HandRisk.bSatisfied = GuHeng.InjurySeverity == EWSInjurySeverity::Normal;
		HandRisk.bDisclosable = true;
		HandRisk.RemediationActionId = TreatCharacter;
		HandRisk.Explanation = FText::FromString(
			HandRisk.bSatisfied
				? TEXT("伤手不会额外增加维修风险")
				: TEXT("伤手会增加耗时和恶化风险，但不是绝对禁令"));
		Report.Risks.Add(HandRisk);
	}
	return Result;
}

FWSActionRequirementReport FWhiteoutRulesEngine::EvaluateActionRequirements(
	const FWSActionRequest& Request) const
{
	FWSActionRequirementReport Report;
	Report.ActionId = Request.ActionId;
	if (IsV11() && Request.ActionId == WhiteoutRules::RepairGenerator)
	{
		EvaluateRepairGeneratorReason(Request, &Report);
	}
	return Report;
}

FWSActionPreview FWhiteoutRulesEngine::BuildV11Preview(
	const FWSActionRequest& Request) const
{
	using namespace WhiteoutRules;
	FWSActionPreview Result;
	Result.ActionId = Request.ActionId;
	Result.PreviewText = ActionPreviewText(Request.ActionId);
	Result.RiskText = ActionRiskText(Request.ActionId);
	Result.ReasonCode = CanExecuteV11(Request);

	const FWhiteoutActionRule* Rule = Config.ActionRules.Find(Request.ActionId);
	if (!Rule)
	{
		Result.WorkReadiness = EWSWorkReadiness::Unavailable;
		return Result;
	}

	Result.BaseAP = Rule->BaseAP;
	Result.RawAP = Rule->BaseAP;
	Result.APCost = Rule->BaseAP;
	if (Rule->BaseAP == 0)
	{
		Result.bCanExecute = Result.ReasonCode == EWSReasonCode::Ok;
		Result.WorkReadiness =
			Result.bCanExecute
				? EWSWorkReadiness::Ready
				: EWSWorkReadiness::Unavailable;
		return Result;
	}

	const EWSCharacterId Executor = ResolveV11Executor(Request);
	const FWSCharacterState& ExecutorState = Character(Executor);
	const bool bPhysical =
		HasV11Tag(Request.ActionId, TagPhysical)
		|| HasV11Tag(Request.ActionId, TagOutdoor);
	const bool bLowTemperatureSensitive =
		HasV11Tag(Request.ActionId, TagFineMotor)
		|| HasV11Tag(Request.ActionId, TagOutdoor);
	const bool bInjurySensitive =
		HasV11Tag(Request.ActionId, TagFineMotor)
		|| HasV11Tag(Request.ActionId, TagPhysical);
	const bool bTemporarySupport =
		ExecutorState.TemporarySupportUses > 0
		&& ExecutorState.TemporarySupportPhase == State.DayPhase;
	bool bSupportAvailable = bTemporarySupport;
	int32 PositiveModifiers = 0;

	auto AddModifier = [&Result, &PositiveModifiers](
		const FName Source,
		const int32 Delta,
		const EWSCharacterId CharacterId,
		const TCHAR* Explanation)
	{
		FWSActionCostModifier Modifier;
		Modifier.Source = Source;
		Modifier.Delta = Delta;
		Modifier.Character = CharacterId;
		Modifier.Explanation = FText::FromString(Explanation);
		Result.CostModifiers.Add(MoveTemp(Modifier));
		Result.RawAP += Delta;
		if (Delta > 0)
		{
			++PositiveModifiers;
		}
	};

	if (bPhysical)
	{
		if (
			ExecutorState.Stamina <= 0
			&& !(Rule->bForceAllowed && Request.bForce))
		{
			Result.ReasonCode = EWSReasonCode::ExecutorExhausted;
		}
		else if (ExecutorState.Stamina == 1)
		{
			AddModifier(
				TEXT("executor_tired"),
				1,
				Executor,
				TEXT("体能疲惫 +1 AP"));
		}
	}

	if (bLowTemperatureSensitive)
	{
		if (
			ExecutorState.Temperature < Config.HypothermicTemperature
			&& !(Rule->bForceAllowed && Request.bForce))
		{
			Result.ReasonCode = EWSReasonCode::ExecutorHypothermic;
		}
		else if (ExecutorState.Temperature < Config.WarmTemperature)
		{
			const bool bHeatedCancellation =
				V11HeatingMatchesLocation(Rule->Location)
				&& (
					HasV11Tag(Request.ActionId, TagFineMotor)
					|| HasV11Tag(Request.ActionId, TagMedical));
			if (bHeatedCancellation)
			{
				AddModifier(
					TEXT("heated_room_cancels_cold"),
					0,
					Executor,
					TEXT("供暖区取消低温惩罚"));
			}
			else if (
				bSupportAvailable
				&& ExecutorState.InjurySeverity != EWSInjurySeverity::Restricted)
			{
				AddModifier(
					TEXT("temporary_support_cancels_cold"),
					0,
					Executor,
					TEXT("保温包取消一次低温惩罚"));
				Result.bUsesTemporarySupport = true;
				bSupportAvailable = false;
			}
			else
			{
				AddModifier(
					TEXT("executor_cold"),
					1,
					Executor,
					TEXT("寒冷 +1 AP"));
			}
		}
	}

	if (bInjurySensitive)
	{
		if (
			ExecutorState.InjurySeverity == EWSInjurySeverity::Critical
			&& !(Rule->bForceAllowed && Request.bForce))
		{
			Result.ReasonCode = EWSReasonCode::RelevantInjuryCritical;
		}
		else if (ExecutorState.InjurySeverity == EWSInjurySeverity::Restricted)
		{
			if (bSupportAvailable)
			{
				AddModifier(
					TEXT("temporary_support_cancels_injury"),
					0,
					Executor,
					TEXT("临时支撑取消一次伤势惩罚"));
				Result.bUsesTemporarySupport = true;
				bSupportAvailable = false;
			}
			else
			{
				AddModifier(
					TEXT("relevant_injury_restricted"),
					1,
					Executor,
					TEXT("相关伤势受限 +1 AP"));
			}
		}
	}

	if (Request.bHasCollaborator)
	{
		if (!Rule->Collaborators.Contains(Request.Collaborator))
		{
			Result.ReasonCode = EWSReasonCode::InvalidCollaborator;
		}
		else if (!IsV11CharacterAvailable(Request.Collaborator))
		{
			Result.ReasonCode = EWSReasonCode::CollaboratorUnavailable;
		}
		else if (
			Request.Collaborator != EWSCharacterId::Player
			&& (
				Character(Request.Collaborator).Trust < 3.0f
				|| Character(Request.Collaborator).Pressure >= 9.0f))
		{
			Result.ReasonCode = EWSReasonCode::CollaboratorUnavailable;
		}
		else if (
			Request.Collaborator != EWSCharacterId::Player
			&& (
				Character(Request.Collaborator).Trust < 4.5f
				|| Character(Request.Collaborator).Pressure >= 8.0f))
		{
			AddModifier(
				TEXT("reluctant_collaborator"),
				1,
				Request.Collaborator,
				TEXT("协作者压力或信任条件未满足 +1 AP"));
		}
		else
		{
			AddModifier(
				TEXT("suitable_collaborator"),
				-1,
				Request.Collaborator,
				TEXT("合适协作者 -1 AP"));
		}
	}

	if (
		Request.ActionId == RepairGenerator
		&& Request.bUseRelay
		&& State.Resources.ReplacementRelay > 0)
	{
		AddModifier(
			TEXT("replacement_relay"),
			-1,
			Executor,
			TEXT("替代继电器 -1 AP"));
	}
	if (
		Request.ActionId == InvestigateGeneratorLog
		&& State.Flags.bLogPenaltyActive)
	{
		AddModifier(
			TEXT("backup_power_saving_mode"),
			1,
			Executor,
			TEXT("备用系统节电 +1 AP"));
	}

	Result.APCost = FMath::Clamp(Result.RawAP, 1, 4);
	if (
		Result.ReasonCode == EWSReasonCode::Ok
		&& Result.APCost > State.PhaseActionPoints)
	{
		Result.ReasonCode = EWSReasonCode::InsufficientAP;
	}
	Result.bCanExecute = Result.ReasonCode == EWSReasonCode::Ok;
	if (!Result.bCanExecute)
	{
		Result.WorkReadiness = EWSWorkReadiness::Unavailable;
	}
	else if (
		PositiveModifiers >= 2
		|| ExecutorState.Pressure >= 8.5f)
	{
		Result.WorkReadiness = EWSWorkReadiness::HighRisk;
	}
	else if (PositiveModifiers == 1)
	{
		Result.WorkReadiness = EWSWorkReadiness::Strained;
	}
	else
	{
		Result.WorkReadiness = EWSWorkReadiness::Ready;
	}

	if (Request.ActionId == RepairGenerator && Result.bCanExecute)
	{
		const FWSCharacterState& GuHeng = Character(EWSCharacterId::GuHeng);
		Result.ExpectedGeneratorProgress =
			Request.bUseRelay
				|| (
					GuHeng.InjurySeverity == EWSInjurySeverity::Normal
					&& GuHeng.Stamina >= 2
					&& State.Heating.CurrentZone == EWSHeatingZone::RepairRoom)
			? 2
			: 1;
	}
	return Result;
}

FWSActionResult FWhiteoutRulesEngine::CommitV11(FWSActionRequest Request)
{
	FWSActionResult Result;
	Result.ActionId = Request.ActionId;
	Result.DialogueAct = Request.DialogueAct;
	Result.PromiseCondition = Request.PromiseCondition;
	Result.APBefore = State.PhaseActionPoints;
	Result.APAfter = State.PhaseActionPoints;
	if (!Request.TransactionId.IsValid())
	{
		Request.TransactionId = FGuid::NewGuid();
	}
	Result.TransactionId = Request.TransactionId;
	if (State.CommittedTransactions.Contains(Request.TransactionId))
	{
		Result.ReasonCode = EWSReasonCode::DuplicateTransaction;
		return Result;
	}

	const FWSActionPreview ActionPreview = BuildV11Preview(Request);
	Result.ReasonCode = ActionPreview.ReasonCode;
	Result.BaseAP = ActionPreview.BaseAP;
	Result.ActualAP = ActionPreview.APCost;
	Result.CostModifiers = ActionPreview.CostModifiers;
	Result.WorkReadiness = ActionPreview.WorkReadiness;
	if (!ActionPreview.bCanExecute)
	{
		return Result;
	}

	const FWhiteoutActionRule& Rule =
		Config.ActionRules.FindChecked(Request.ActionId);
	const EWSCharacterId Executor = ResolveV11Executor(Request);
	State.Phase = EWSGamePhase::ResolvingAction;
	const int32 PromiseCountBefore = State.Promises.Num();
	if (ActionPreview.bUsesTemporarySupport)
	{
		Character(Executor).TemporarySupportUses =
			FMath::Max(0, Character(Executor).TemporarySupportUses - 1);
	}
	if (Rule.bConsumesStamina)
	{
		ConsumeV11Stamina(Executor);
	}
	ApplyV11Effect(Request, ActionPreview, Result.Changes);
	Result.bPromiseRecorded = State.Promises.Num() > PromiseCountBefore;

	State.PhaseActionPoints = FMath::Max(
		0,
		Result.APBefore - ActionPreview.APCost);
	State.ActionPoints = State.PhaseActionPoints;
	State.ActionCounts.FindOrAdd(Request.ActionId) += 1;
	State.CommittedTransactions.Add(Request.TransactionId);
	State.Phase =
		Request.ActionId == WhiteoutRules::SendSignal
			? EWSGamePhase::EndingChoice
			: EWSGamePhase::ActionPhase;

	FWSEventRecord Event;
	Event.Index = State.EventLog.Num() + 1;
	Event.ActionId = Request.ActionId;
	Event.TransactionId = Request.TransactionId;
	Event.APBefore = Result.APBefore;
	Event.APAfter = State.PhaseActionPoints;
	Event.ReasonCode = EWSReasonCode::Committed;
	Event.DialogueAct = Request.DialogueAct;
	Event.PromiseCondition = Request.PromiseCondition;
	Event.bPromiseRecorded = Result.bPromiseRecorded;
	Event.Changes = Result.Changes;
	Event.DayPhase = State.DayPhase;
	Event.BaseAP = ActionPreview.BaseAP;
	Event.ActualAP = ActionPreview.APCost;
	Event.WorkReadiness = ActionPreview.WorkReadiness;
	Event.CostModifiers = ActionPreview.CostModifiers;
	State.EventLog.Add(MoveTemp(Event));

	Result.bCommitted = true;
	Result.ReasonCode = EWSReasonCode::Committed;
	Result.APAfter = State.PhaseActionPoints;
	return Result;
}

bool FWhiteoutRulesEngine::BeginDayPhase(
	const EWSHeatingZone HeatingZone,
	EWSReasonCode& OutReason,
	TArray<FString>& OutChanges)
{
	OutChanges.Reset();
	if (!IsV11())
	{
		OutReason = EWSReasonCode::PhaseLocked;
		return false;
	}
	if (State.bDayWindowClosed)
	{
		OutReason = EWSReasonCode::WindowClosed;
		return false;
	}
	if (State.bDayPhaseStarted || State.Heating.bLocked)
	{
		OutReason = EWSReasonCode::HeatingLocked;
		return false;
	}
	if (HeatingZone == EWSHeatingZone::None)
	{
		OutReason = EWSReasonCode::UnknownHeatingZone;
		return false;
	}
	if (State.Resources.Fuel < 1)
	{
		OutReason = EWSReasonCode::NeedsFuel;
		return false;
	}

	--State.Resources.Fuel;
	State.Heating.CurrentZone = HeatingZone;
	State.Heating.bLocked = true;
	FWSHeatingSelectionRecord Record;
	Record.Phase = State.DayPhase;
	Record.Zone = HeatingZone;
	State.Heating.History.Add(Record);
	State.bDayPhaseStarted = true;
	State.Phase = EWSGamePhase::ActionPhase;
	State.PhaseActionPoints = Config.ActionPointsPerPhase;
	State.ActionPoints = State.PhaseActionPoints;
	State.Flags.bRepairRoomHeated =
		HeatingZone == EWSHeatingZone::RepairRoom;
	State.Flags.bMedicalRoomHeated =
		HeatingZone == EWSHeatingZone::MedicalRoom;
	OutChanges.Add(FString::Printf(
		TEXT("%s：供暖区锁定为%s，燃料 -1"),
		*DayPhaseLabel(State.DayPhase),
		*HeatingZoneLabel(HeatingZone)));

	FWSEventRecord Event;
	Event.Index = State.EventLog.Num() + 1;
	Event.ActionId = TEXT("begin_phase");
	Event.APBefore = State.PhaseActionPoints;
	Event.APAfter = State.PhaseActionPoints;
	Event.ReasonCode = EWSReasonCode::Committed;
	Event.DayPhase = State.DayPhase;
	Event.Changes = OutChanges;
	State.EventLog.Add(MoveTemp(Event));
	OutReason = EWSReasonCode::Committed;
	return true;
}

bool FWhiteoutRulesEngine::SettleDayPhase(
	EWSReasonCode& OutReason,
	FWSPhaseSummary& OutSummary)
{
	if (!IsV11())
	{
		OutReason = EWSReasonCode::PhaseLocked;
		return false;
	}
	if (State.bDayWindowClosed)
	{
		OutReason = EWSReasonCode::WindowClosed;
		return false;
	}
	if (!State.bDayPhaseStarted)
	{
		OutReason = EWSReasonCode::PhaseNotStarted;
		return false;
	}

	const EWSDayPhase SettledPhase = State.DayPhase;
	const TMap<EWSCharacterId, FWSCharacterState> CharactersBefore =
		State.Characters;
	OutSummary = FWSPhaseSummary();
	OutSummary.Phase = SettledPhase;
	OutSummary.HeatingZone = State.Heating.CurrentZone;
	OutSummary.UnusedAPDiscarded = State.PhaseActionPoints;
	OutSummary.OrderedSteps.Add(TEXT("1. 已提交行动结果"));

	for (const EWSCharacterId CharacterId : {
			EWSCharacterId::Player,
			EWSCharacterId::GuHeng,
			EWSCharacterId::YeCheng})
	{
		FWSCharacterState& Current = Character(CharacterId);
		const float Delta =
			V11HeatingMatchesLocation(Current.Location)
			? Config.HeatedTemperatureDelta
			: Config.UnheatedTemperatureDelta.FindRef(SettledPhase);
		Current.Temperature =
			FMath::Clamp(Current.Temperature + Delta, 0.0f, 10.0f);
	}
	OutSummary.OrderedSteps.Add(TEXT("2. 房间温度"));
	OutSummary.OrderedSteps.Add(TEXT("3. 体能消耗"));

	for (const EWSCharacterId CharacterId : {
			EWSCharacterId::Player,
			EWSCharacterId::GuHeng,
			EWSCharacterId::YeCheng})
	{
		FWSCharacterState& Current = Character(CharacterId);
		if (Current.InjurySeverity == EWSInjurySeverity::Critical)
		{
			Current.Pressure = FMath::Clamp(Current.Pressure + 0.5f, 0.0f, 10.0f);
		}
	}
	OutSummary.OrderedSteps.Add(TEXT("4. 伤势进展"));

	for (const EWSCharacterId CharacterId : {
			EWSCharacterId::Player,
			EWSCharacterId::GuHeng,
			EWSCharacterId::YeCheng})
	{
		FWSCharacterState& Current = Character(CharacterId);
		if (Current.Temperature < Config.HypothermicTemperature)
		{
			Current.Pressure = FMath::Clamp(Current.Pressure + 0.5f, 0.0f, 10.0f);
		}
		else if (Current.Temperature < Config.WarmTemperature)
		{
			Current.Pressure = FMath::Clamp(Current.Pressure + 0.2f, 0.0f, 10.0f);
		}
	}
	OutSummary.OrderedSteps.Add(TEXT("5. 压力与关系"));

	if (SettledPhase == EWSDayPhase::Morning)
	{
		State.Flags.bLogPenaltyActive =
			!State.Flags.bRecordsPreserved
			&& State.Heating.CurrentZone != EWSHeatingZone::ControlRoom;
		OutSummary.PhaseEvent = TEXT("backup_power_saving");
	}
	else if (SettledPhase == EWSDayPhase::Afternoon)
	{
		if (State.Tasks.GeneratorProgress == 0)
		{
			for (const EWSCharacterId CharacterId : {
				EWSCharacterId::Player,
				EWSCharacterId::GuHeng,
				EWSCharacterId::YeCheng})
			{
				FWSCharacterState& Current = Character(CharacterId);
				Current.Pressure =
					FMath::Clamp(Current.Pressure + 0.6f, 0.0f, 10.0f);
			}
		}
		OutSummary.PhaseEvent = TEXT("voltage_danger_and_blizzard");
	}
	else
	{
		OutSummary.PhaseEvent = TEXT("antenna_window_closed");
	}

	if (
		!State.Flags.bHeatPackRevealed
		&& Character(EWSCharacterId::YeCheng).Trust >= 5.5f)
	{
		State.Flags.bHeatPackRevealed = true;
		OutSummary.NPCReaction = TEXT("ye_cheng_volunteer_heat_pack");
	}
	else if (Character(EWSCharacterId::GuHeng).Pressure >= 9.0f)
	{
		OutSummary.NPCReaction = TEXT("gu_heng_withhold");
	}
	OutSummary.OrderedSteps.Add(TEXT("6. 阶段事件与单次 NPC 反应"));
	OutSummary.OrderedSteps.Add(TEXT("7. 因果摘要"));
	const auto CharacterLabel = [](const EWSCharacterId CharacterId)
	{
		switch (CharacterId)
		{
		case EWSCharacterId::Player:
			return FString(TEXT("玩家"));
		case EWSCharacterId::GuHeng:
			return FString(TEXT("顾衡"));
		default:
			return FString(TEXT("叶澄"));
		}
	};
	for (const EWSCharacterId CharacterId : {
			EWSCharacterId::Player,
			EWSCharacterId::GuHeng,
			EWSCharacterId::YeCheng})
	{
		const FWSCharacterState& Before =
			CharactersBefore.FindChecked(CharacterId);
		const FWSCharacterState& After = Character(CharacterId);
		const FString Label = CharacterLabel(CharacterId);
		if (!FMath::IsNearlyEqual(Before.Temperature, After.Temperature))
		{
			OutSummary.Changes.Add(FString::Printf(
				TEXT("%s体温 %.1f→%.1f（%s供暖）"),
				*Label,
				Before.Temperature,
				After.Temperature,
				V11HeatingMatchesLocation(After.Location)
					? TEXT("当前区")
					: TEXT("未覆盖")));
		}
		if (Before.Stamina != After.Stamina)
		{
			OutSummary.Changes.Add(FString::Printf(
				TEXT("%s体能 %d→%d"),
				*Label,
				Before.Stamina,
				After.Stamina));
		}
		if (
			Before.InjurySeverity != After.InjurySeverity
			|| Before.InjuryWorseningMarks != After.InjuryWorseningMarks)
		{
			OutSummary.Changes.Add(FString::Printf(
				TEXT("%s伤势 %s→%s（恶化标记 %d→%d）"),
				*Label,
				*StaticEnum<EWSInjurySeverity>()->GetNameStringByValue(
					static_cast<int64>(Before.InjurySeverity)),
				*StaticEnum<EWSInjurySeverity>()->GetNameStringByValue(
					static_cast<int64>(After.InjurySeverity)),
				Before.InjuryWorseningMarks,
				After.InjuryWorseningMarks));
		}
		if (!FMath::IsNearlyEqual(Before.Pressure, After.Pressure))
		{
			OutSummary.Changes.Add(FString::Printf(
				TEXT("%s压力 %.1f→%.1f"),
				*Label,
				Before.Pressure,
				After.Pressure));
		}
		if (!FMath::IsNearlyEqual(Before.Trust, After.Trust))
		{
			OutSummary.Changes.Add(FString::Printf(
				TEXT("%s信任 %.1f→%.1f"),
				*Label,
				Before.Trust,
				After.Trust));
		}
	}

	for (const EWSCharacterId CharacterId : {
		EWSCharacterId::Player,
		EWSCharacterId::GuHeng,
		EWSCharacterId::YeCheng})
	{
		FWSCharacterState& Current = Character(CharacterId);
		if (Current.TemporarySupportPhase == SettledPhase)
		{
			Current.TemporarySupportUses = 0;
			Current.TemporarySupportPhase = EWSDayPhase::Complete;
		}
	}

	State.PhaseSummaries.Add(OutSummary);
	FWSEventRecord Event;
	Event.Index = State.EventLog.Num() + 1;
	Event.ActionId = TEXT("settle_phase");
	Event.APBefore = State.PhaseActionPoints;
	Event.APAfter = 0;
	Event.ReasonCode = EWSReasonCode::Committed;
	Event.DayPhase = SettledPhase;
	Event.Changes = OutSummary.OrderedSteps;
	State.EventLog.Add(MoveTemp(Event));

	State.bDayPhaseStarted = false;
	State.Heating.CurrentZone = EWSHeatingZone::None;
	State.Heating.bLocked = false;
	State.Flags.bRepairRoomHeated = false;
	State.Flags.bMedicalRoomHeated = false;
	if (SettledPhase == EWSDayPhase::Dusk)
	{
		State.DayPhase = EWSDayPhase::Complete;
		State.bDayWindowClosed = true;
		State.PhaseActionPoints = 0;
		State.ActionPoints = 0;
		State.Phase = EWSGamePhase::Ending;
	}
	else
	{
		State.DayPhase = NextDayPhase(SettledPhase);
		State.PhaseActionPoints = Config.ActionPointsPerPhase;
		State.ActionPoints = State.PhaseActionPoints;
		State.Phase = EWSGamePhase::ActionPhase;
	}
	OutReason = EWSReasonCode::Committed;
	return true;
}

void FWhiteoutRulesEngine::ApplyEffect(const FWSActionRequest& Request, TArray<FString>& OutChanges)
{
	using namespace WhiteoutRules;
	if (Request.ActionId == InvestigateGeneratorLog)
	{
		AddEvidence(TEXT("EVIDENCE_DEEP_GENERATOR_LOG"), &OutChanges);
		DiscoverFact(FactGeneratorProtectionStop, EWSKnowledgeLevel::Confirmed, &OutChanges);
		DiscoverFact(FactForcedRestartSuspicion, EWSKnowledgeLevel::Suspected, &OutChanges);
		State.Flags.bRecordsPreserved = true;
		OutChanges.Add(TEXT("Generator records preserved"));
	}
	else if (Request.ActionId == InspectControlCabinet)
	{
		AddEvidence(TEXT("EVIDENCE_BURNT_RELAY"), &OutChanges);
		AddEvidence(TEXT("EVIDENCE_ARC_MARKS"), &OutChanges);
		AddEvidence(TEXT("EVIDENCE_HAND_OBSERVATION"), &OutChanges);
		DiscoverFact(FactBurntRelay, EWSKnowledgeLevel::Confirmed, &OutChanges);
		DiscoverFact(FactHandInjury, EWSKnowledgeLevel::Suspected, &OutChanges);
	}
	else if (Request.ActionId == TalkYeCheng)
	{
		ChangeCharacter(EWSCharacterId::YeCheng, 0, 0, 0, 0, -0.4f, 0.4f);
		State.Flags.bGuHengDiagnosed = true;
		AddEvidence(TEXT("EVIDENCE_MEDICAL_DIAGNOSIS"), &OutChanges);
		DiscoverFact(FactHandInjury, EWSKnowledgeLevel::Confirmed, &OutChanges);
		DiscoverFact(FactMedicalDiagnosis, EWSKnowledgeLevel::Confirmed, &OutChanges);
		if (Character(EWSCharacterId::YeCheng).Trust >= 6.0f)
		{
			State.Flags.bHeatPackRevealed = true;
			AddEvidence(TEXT("EVIDENCE_HEAT_PACK"), &OutChanges);
			DiscoverFact(FactHeatPack, EWSKnowledgeLevel::Confirmed, &OutChanges);
		}
		OutChanges.Add(TEXT("Ye Cheng trust +0.4, pressure -0.4"));
		if (Request.DialogueAct == EWSDialogueAct::Challenge)
		{
			ChangeCharacter(EWSCharacterId::YeCheng, 0, 0, 0, 0, 0.3f, -0.3f);
			OutChanges.Add(TEXT("Challenge modifier: Ye Cheng trust -0.3, pressure +0.3"));
		}
		else if (Request.DialogueAct == EWSDialogueAct::Reassure)
		{
			ChangeCharacter(EWSCharacterId::YeCheng, 0, 0, 0, 0, -0.4f, 0.3f);
			OutChanges.Add(TEXT("Reassure modifier: Ye Cheng trust +0.3, pressure -0.4"));
		}
	}
	else if (Request.ActionId == TalkGuHeng)
	{
		const bool bStrongEvidence = Knows(FactForcedRestartSuspicion) && Knows(FactBurntRelay);
		if (State.Flags.bGuHengTreated)
		{
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, -0.8f, 1.2f);
			State.Flags.bGuHengCooperative = true;
			OutChanges.Add(TEXT("Gu Heng accepts cooperation after treatment"));
		}
		else if (bStrongEvidence)
		{
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 0.3f, 0.8f);
			State.Flags.bGuHengCooperative = true;
			State.Flags.bRelayCompatibilityKnown = true;
			AddEvidence(TEXT("EVIDENCE_HEATER_SERVICE_LABEL"), &OutChanges);
			DiscoverFact(FactRelayCompatibility, EWSKnowledgeLevel::Confirmed, &OutChanges);
			DiscoverFact(FactForcedRestartConfirmed, EWSKnowledgeLevel::Confirmed, &OutChanges);
			OutChanges.Add(TEXT("Gu Heng confirms the technical route"));
		}
		else
		{
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 0.4f, -0.3f);
			OutChanges.Add(TEXT("Gu Heng refuses an unsupported request"));
		}
		if (Request.DialogueAct == EWSDialogueAct::Challenge)
		{
			if (bStrongEvidence)
			{
				ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 0.2f, 0.2f);
				OutChanges.Add(TEXT("Evidence challenge modifier: Gu Heng trust +0.2, pressure +0.2"));
			}
			else
			{
				ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 0.3f, -0.3f);
				OutChanges.Add(TEXT("Challenge modifier: Gu Heng trust -0.3, pressure +0.3"));
			}
		}
		else if (Request.DialogueAct == EWSDialogueAct::Reassure)
		{
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, -0.4f, 0.3f);
			OutChanges.Add(TEXT("Reassure modifier: Gu Heng trust +0.3, pressure -0.4"));
		}
		else if (Request.DialogueAct == EWSDialogueAct::Promise)
		{
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, -0.2f, 0.2f);
			OutChanges.Add(TEXT("Promise modifier: Gu Heng trust +0.2, pressure -0.2"));
		}
		if (Request.DialogueAct == EWSDialogueAct::Promise)
		{
			RecognizePromise(Request, OutChanges);
		}
	}
	else if (Request.ActionId == HeatRepairRoom)
	{
		State.Resources.Fuel -= 1;
		State.Flags.bRepairRoomHeated = true;
		ChangeCharacter(EWSCharacterId::GuHeng, 0, 0.6f, 0, 0, -0.4f, 0);
		OutChanges.Add(TEXT("Repair room heated; fuel -1"));
	}
	else if (Request.ActionId == HeatMedicalRoom)
	{
		State.Resources.Fuel -= 1;
		State.Flags.bMedicalRoomHeated = true;
		ChangeCharacter(EWSCharacterId::GuHeng, 0, 0.4f, 0, 0, 0, 0);
		ChangeCharacter(EWSCharacterId::YeCheng, 0, 0.3f, 0, 0, -0.3f, 0);
		OutChanges.Add(TEXT("Medical room heated; fuel -1"));
	}
	else if (Request.ActionId == DistributeFood)
	{
		const int32 Total = Request.FoodForPlayer + Request.FoodForGuHeng + Request.FoodForYeCheng;
		State.Resources.Food -= Total;
		ChangeCharacter(EWSCharacterId::Player, 0, 0, Request.FoodForPlayer * 2.0f, 0, 0, 0);
		ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, Request.FoodForGuHeng * 2.0f, 0, 0, Request.FoodForGuHeng ? 1.2f : -0.6f);
		ChangeCharacter(EWSCharacterId::YeCheng, 0, 0, Request.FoodForYeCheng * 2.0f, 0, 0, Request.FoodForYeCheng ? 0.5f : -0.5f);
		State.Flags.bGuHengFed = Request.FoodForGuHeng > 0;
		OutChanges.Add(FString::Printf(TEXT("Food allocated: player=%d gu=%d ye=%d"), Request.FoodForPlayer, Request.FoodForGuHeng, Request.FoodForYeCheng));
	}
	else if (Request.ActionId == TreatGuHeng)
	{
		if (Request.TreatmentResource == EWSResourceType::Medicine)
		{
			State.Resources.Medicine -= 1;
			ChangeCharacter(EWSCharacterId::GuHeng, 2.2f, 0.5f, 0, 0.8f, -1.6f, 0.8f);
			OutChanges.Add(TEXT("Gu Heng treated with medicine"));
		}
		else
		{
			State.Resources.HeatPack -= 1;
			ChangeCharacter(EWSCharacterId::GuHeng, 1.2f, 1.0f, 0, 0.5f, -1.0f, 0.5f);
			OutChanges.Add(TEXT("Gu Heng stabilized with heat pack"));
		}
		State.Flags.bGuHengTreated = true;
		State.Flags.bGuHengCooperative = true;
	}
	else if (Request.ActionId == DismantleKitchenHeater)
	{
		const bool bCooperative = State.Flags.bGuHengCooperative && State.Flags.bRelayCompatibilityKnown;
		if (bCooperative)
		{
			ChangeCharacter(EWSCharacterId::GuHeng, -0.2f, 0, 0, 0, -0.3f, 0);
		}
		else
		{
			ChangeCharacter(EWSCharacterId::Player, -0.4f, 0, 0, -0.5f, 0, 0);
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 0, -0.3f);
		}
		State.Resources.ReplacementRelay = 1;
		State.Flags.bKitchenHeaterIntact = false;
		OutChanges.Add(TEXT("Replacement relay recovered; kitchen heater lost"));
	}
	else if (Request.ActionId == RepairGenerator)
	{
		const bool bStable = State.Flags.bGuHengTreated || (State.Resources.ReplacementRelay > 0 && State.Flags.bRepairRoomHeated);
		const int32 Progress = bStable ? 2 : 1;
		if (State.Resources.ReplacementRelay > 0)
		{
			State.Resources.ReplacementRelay = 0;
			State.Flags.bRelayInstalled = true;
		}
		State.Tasks.GeneratorProgress = FMath::Min(Config.GeneratorRequired, State.Tasks.GeneratorProgress + Progress);
		if (bStable)
		{
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, -0.4f, -0.4f, 0);
		}
		else
		{
			ChangeCharacter(EWSCharacterId::GuHeng, -0.6f, -0.2f, 0, -0.6f, 0.7f, 0);
		}
		OutChanges.Add(FString::Printf(TEXT("Generator progress +%d"), Progress));
	}
	else if (Request.ActionId == ForcedSelfRepair)
	{
		State.Flags.bSelfRepairUsed = true;
		State.Tasks.GeneratorProgress = FMath::Min(Config.GeneratorRequired, State.Tasks.GeneratorProgress + 1);
		ChangeCharacter(EWSCharacterId::Player, -1.0f, -0.4f, 0, -1.5f, 0.8f, 0);
		OutChanges.Add(TEXT("Player forced one generator progress at high cost"));
	}
	else if (Request.ActionId == CalibrateAntenna)
	{
		State.Tasks.AntennaCalibration = Config.AntennaRequired;
		OutChanges.Add(TEXT("Antenna calibrated"));
	}
	else if (Request.ActionId == SendSignal)
	{
		State.Tasks.bSignalSent = true;
		OutChanges.Add(TEXT("Rescue signal sent"));
	}
}

void FWhiteoutRulesEngine::ApplyV11Effect(
	const FWSActionRequest& Request,
	const FWSActionPreview& Preview,
	TArray<FString>& OutChanges)
{
	using namespace WhiteoutRules;
	const EWSCharacterId Executor = ResolveV11Executor(Request);
	const FWhiteoutActionRule& Rule =
		Config.ActionRules.FindChecked(Request.ActionId);

	if (Request.ActionId == InvestigateGeneratorLog)
	{
		AddEvidence(TEXT("EVIDENCE_DEEP_GENERATOR_LOG"), &OutChanges);
		DiscoverFact(FactGeneratorProtectionStop, EWSKnowledgeLevel::Confirmed, &OutChanges);
		DiscoverFact(FactForcedRestartSuspicion, EWSKnowledgeLevel::Suspected, &OutChanges);
		State.Flags.bRecordsPreserved = true;
		State.Flags.bLogPenaltyActive = false;
		OutChanges.Add(TEXT("保留深层日志并发现旁路重启疑点"));
	}
	else if (Request.ActionId == InspectControlCabinet)
	{
		AddEvidence(TEXT("EVIDENCE_BURNT_RELAY"), &OutChanges);
		AddEvidence(TEXT("EVIDENCE_HAND_OBSERVATION"), &OutChanges);
		DiscoverFact(FactBurntRelay, EWSKnowledgeLevel::Confirmed, &OutChanges);
		DiscoverFact(FactHandInjury, EWSKnowledgeLevel::Suspected, &OutChanges);
		State.Flags.bCabinetInspected = true;
		OutChanges.Add(TEXT("确认继电器烧毁与右手伤势线索"));
	}
	else if (Request.ActionId == TalkYeCheng)
	{
		FWSCharacterState& YeCheng = Character(EWSCharacterId::YeCheng);
		YeCheng.Trust = FMath::Clamp(YeCheng.Trust + 0.4f, 0.0f, 10.0f);
		YeCheng.Pressure = FMath::Clamp(YeCheng.Pressure - 0.4f, 0.0f, 10.0f);
		State.Flags.bGuHengDiagnosed = true;
		State.Flags.bHeatPackRevealed = true;
		DiscoverFact(FactMedicalDiagnosis, EWSKnowledgeLevel::Confirmed, &OutChanges);
		DiscoverFact(FactHandInjury, EWSKnowledgeLevel::Confirmed, &OutChanges);
		DiscoverFact(FactHeatPack, EWSKnowledgeLevel::Confirmed, &OutChanges);
		OutChanges.Add(TEXT("叶澄披露诊断与保温包信息"));
	}
	else if (Request.ActionId == TalkGuHeng)
	{
		FWSCharacterState& GuHeng = Character(EWSCharacterId::GuHeng);
		const bool bEvidenceBacked =
			Knows(FactForcedRestartSuspicion)
			|| State.Flags.bCabinetInspected;
		if (Request.DialogueAct == EWSDialogueAct::Challenge && bEvidenceBacked)
		{
			GuHeng.Trust = FMath::Clamp(GuHeng.Trust + 0.8f, 0.0f, 10.0f);
			GuHeng.Pressure = FMath::Clamp(GuHeng.Pressure + 0.2f, 0.0f, 10.0f);
			State.Flags.bRelayCompatibilityKnown = true;
			DiscoverFact(FactRelayCompatibility, EWSKnowledgeLevel::Confirmed, &OutChanges);
			if (Knows(FactForcedRestartSuspicion))
			{
				DiscoverFact(
					FactForcedRestartConfirmed,
					EWSKnowledgeLevel::Confirmed,
					&OutChanges);
			}
		}
		else if (Request.DialogueAct == EWSDialogueAct::Command)
		{
			GuHeng.Trust = FMath::Clamp(GuHeng.Trust - 0.4f, 0.0f, 10.0f);
			GuHeng.Pressure = FMath::Clamp(GuHeng.Pressure + 0.6f, 0.0f, 10.0f);
			++State.Flags.ForcedActionCount;
		}
		else if (Request.DialogueAct == EWSDialogueAct::Promise)
		{
			GuHeng.Trust = FMath::Clamp(GuHeng.Trust + 0.6f, 0.0f, 10.0f);
			GuHeng.Pressure = FMath::Clamp(GuHeng.Pressure - 0.4f, 0.0f, 10.0f);
			RecognizePromise(Request, OutChanges);
		}
		else if (Request.DialogueAct == EWSDialogueAct::Reassure)
		{
			GuHeng.Trust = FMath::Clamp(GuHeng.Trust + 0.3f, 0.0f, 10.0f);
			GuHeng.Pressure = FMath::Clamp(GuHeng.Pressure - 0.6f, 0.0f, 10.0f);
		}
		else
		{
			GuHeng.Trust = FMath::Clamp(GuHeng.Trust + 0.2f, 0.0f, 10.0f);
			GuHeng.Pressure = FMath::Clamp(GuHeng.Pressure - 0.2f, 0.0f, 10.0f);
		}
		OutChanges.Add(TEXT("顾衡立场由确定性规则结算"));
	}
	else if (Request.ActionId == DistributeFood)
	{
		const int32 Total =
			Request.FoodForPlayer
			+ Request.FoodForGuHeng
			+ Request.FoodForYeCheng;
		State.Resources.Food -= Total;
		const TPair<EWSCharacterId, int32> Allocations[] = {
			{EWSCharacterId::Player, Request.FoodForPlayer},
			{EWSCharacterId::GuHeng, Request.FoodForGuHeng},
			{EWSCharacterId::YeCheng, Request.FoodForYeCheng}};
		for (const TPair<EWSCharacterId, int32>& Allocation : Allocations)
		{
			FWSCharacterState& Current = Character(Allocation.Key);
			if (Allocation.Value > 0)
			{
				Current.Stamina = FMath::Min(2, Current.Stamina + 1);
				Current.Pressure = FMath::Clamp(
					Current.Pressure - (Request.bHotMeal ? 0.4f : 0.1f),
					0.0f,
					10.0f);
				if (Request.bHotMeal)
				{
					Current.Temperature =
						FMath::Clamp(Current.Temperature + 0.3f, 0.0f, 10.0f);
				}
				Current.Location = EWSCharacterLocation::Kitchen;
				if (Allocation.Key != EWSCharacterId::Player)
				{
					Current.Trust = FMath::Clamp(
						Current.Trust + (Request.bHotMeal ? 0.7f : 0.5f),
						0.0f,
						10.0f);
				}
			}
			else if (Allocation.Key != EWSCharacterId::Player)
			{
				Current.Trust =
					FMath::Clamp(Current.Trust - 0.3f, 0.0f, 10.0f);
			}
		}
		State.Flags.bPlayerFed |= Request.FoodForPlayer > 0;
		State.Flags.bGuHengFed |= Request.FoodForGuHeng > 0;
		State.Flags.bYeChengFed |= Request.FoodForYeCheng > 0;
		OutChanges.Add(FString::Printf(
			TEXT("%s分配：玩家%d 顾衡%d 叶澄%d"),
			Request.bHotMeal ? TEXT("热餐") : TEXT("冷口粮"),
			Request.FoodForPlayer,
			Request.FoodForGuHeng,
			Request.FoodForYeCheng));
	}
	else if (Request.ActionId == Rest)
	{
		FWSCharacterState& Target = Character(Request.RestTarget);
		Target.Location = Request.RestLocation;
		if (
			V11HeatingMatchesLocation(Request.RestLocation)
			&& Target.Stamina < 2)
		{
			++Target.Stamina;
			OutChanges.Add(TEXT("供暖区休整：体能 +1"));
		}
		else
		{
			Target.Pressure = FMath::Clamp(Target.Pressure - 0.4f, 0.0f, 10.0f);
			OutChanges.Add(
				V11HeatingMatchesLocation(Request.RestLocation)
					? TEXT("供暖区休整：体能已满，压力 -0.4")
					: TEXT("未供暖区等待：压力 -0.4"));
		}
	}
	else if (
		Request.ActionId == TreatCharacter
		|| Request.ActionId == TreatGuHeng)
	{
		const EWSCharacterId TargetId =
			Request.ActionId == TreatGuHeng
			? EWSCharacterId::GuHeng
			: Request.TreatmentTarget;
		const EWSTreatmentMethod Method =
			Request.ActionId == TreatGuHeng
				&& Request.TreatmentResource == EWSResourceType::HeatPack
			? EWSTreatmentMethod::HeatPack
			: Request.TreatmentMethod;
		FWSCharacterState& Target = Character(TargetId);
		Character(EWSCharacterId::YeCheng).Location =
			EWSCharacterLocation::MedicalRoom;
		Target.Location = EWSCharacterLocation::MedicalRoom;
		if (Method == EWSTreatmentMethod::Bandage)
		{
			if (Target.InjuryWorseningMarks > 0)
			{
				--Target.InjuryWorseningMarks;
			}
			else
			{
				Target.BandageProtection = 1;
			}
			Target.Pressure = FMath::Clamp(Target.Pressure - 0.3f, 0.0f, 10.0f);
			OutChanges.Add(TEXT("简单包扎阻止下一次恶化，伤势惩罚保留"));
		}
		else if (Method == EWSTreatmentMethod::Full)
		{
			--State.Resources.Medicine;
			Target.InjurySeverity = EWSInjurySeverity::Normal;
			Target.InjuryId = NAME_None;
			Target.InjuryWorseningMarks = 0;
			Target.BandageProtection = 0;
			Target.Pressure = FMath::Clamp(Target.Pressure - 1.0f, 0.0f, 10.0f);
			Target.Temperature =
				FMath::Clamp(Target.Temperature + 0.4f, 0.0f, 10.0f);
			OutChanges.Add(TEXT("完整治疗永久移除受限伤势"));
		}
		else
		{
			--State.Resources.HeatPack;
			Target.TemporarySupportUses = 1;
			Target.TemporarySupportPhase = State.DayPhase;
			Target.Temperature =
				FMath::Clamp(Target.Temperature + 0.5f, 0.0f, 10.0f);
			OutChanges.Add(TEXT("保温包可取消本阶段一次低温或伤势惩罚"));
		}
		if (TargetId != EWSCharacterId::Player)
		{
			Target.Trust = FMath::Clamp(
				Target.Trust + (Method == EWSTreatmentMethod::Full ? 0.8f : 0.2f),
				0.0f,
				10.0f);
		}
		if (TargetId == EWSCharacterId::GuHeng)
		{
			State.Flags.bGuHengTreated =
				Method == EWSTreatmentMethod::Full;
		}
	}
	else if (Request.ActionId == DismantleKitchenHeater)
	{
		State.Flags.bKitchenHeaterIntact = false;
		++State.Resources.ReplacementRelay;
		if (
			Character(Executor).InjurySeverity == EWSInjurySeverity::Restricted
			&& !Preview.bUsesTemporarySupport)
		{
			Character(Executor).Pressure =
				FMath::Clamp(Character(Executor).Pressure + 0.3f, 0.0f, 10.0f);
		}
		OutChanges.Add(TEXT("拆除厨房加热器并取得替代继电器"));
	}
	else if (Request.ActionId == RepairGenerator)
	{
		const int32 Progress = FMath::Max(1, Preview.ExpectedGeneratorProgress);
		if (Request.bUseRelay && State.Resources.ReplacementRelay > 0)
		{
			--State.Resources.ReplacementRelay;
			State.Flags.bRelayInstalled = true;
		}
		State.Tasks.GeneratorProgress = FMath::Min(
			Config.GeneratorRequired,
			State.Tasks.GeneratorProgress + Progress);
		const bool bStable =
			Request.bUseRelay
			|| (
				Character(EWSCharacterId::GuHeng).InjurySeverity
					== EWSInjurySeverity::Normal
				&& State.Heating.CurrentZone == EWSHeatingZone::RepairRoom);
		State.Tasks.bGeneratorStable |= bStable;
		if (
			Character(EWSCharacterId::GuHeng).InjurySeverity
				== EWSInjurySeverity::Restricted
			&& !Preview.bUsesTemporarySupport)
		{
			WorsenV11Injury(EWSCharacterId::GuHeng, OutChanges);
			++State.Flags.RiskyRepairCount;
			Character(EWSCharacterId::GuHeng).Pressure = FMath::Clamp(
				Character(EWSCharacterId::GuHeng).Pressure + 0.6f,
				0.0f,
				10.0f);
		}
		if (Request.bForce)
		{
			++State.Flags.ForcedActionCount;
			Character(EWSCharacterId::GuHeng).Trust = FMath::Clamp(
				Character(EWSCharacterId::GuHeng).Trust - 0.4f,
				0.0f,
				10.0f);
		}
		OutChanges.Add(FString::Printf(TEXT("发电机进度 +%d"), Progress));
	}
	else if (Request.ActionId == ForcedSelfRepair)
	{
		State.Flags.bSelfRepairUsed = true;
		++State.Flags.ForcedActionCount;
		State.Tasks.GeneratorProgress = FMath::Min(
			Config.GeneratorRequired,
			State.Tasks.GeneratorProgress + 1);
		FWSCharacterState& Player = Character(EWSCharacterId::Player);
		Player.Pressure = FMath::Clamp(Player.Pressure + 1.0f, 0.0f, 10.0f);
		if (Player.InjurySeverity == EWSInjurySeverity::Normal)
		{
			Player.InjurySeverity = EWSInjurySeverity::Restricted;
			Player.InjuryId = TEXT("right_hand_restricted");
		}
		else
		{
			WorsenV11Injury(EWSCharacterId::Player, OutChanges);
		}
		OutChanges.Add(TEXT("强行自行维修获得 1 点进度并造成手伤"));
	}
	else if (Request.ActionId == CalibrateAntenna)
	{
		State.Tasks.AntennaCalibration = Config.AntennaRequired;
		FWSCharacterState& Player = Character(EWSCharacterId::Player);
		Player.Temperature =
			FMath::Clamp(Player.Temperature - 1.5f, 0.0f, 10.0f);
		Player.Location = EWSCharacterLocation::OutdoorAntenna;
		if (Request.bForce)
		{
			++State.Flags.ForcedActionCount;
			Player.Pressure =
				FMath::Clamp(Player.Pressure + 1.0f, 0.0f, 10.0f);
			if (Player.InjurySeverity == EWSInjurySeverity::Normal)
			{
				Player.InjurySeverity = EWSInjurySeverity::Restricted;
				Player.InjuryId = TEXT("cold_exposure_restricted");
			}
			else
			{
				WorsenV11Injury(EWSCharacterId::Player, OutChanges);
			}
			OutChanges.Add(TEXT("强行校准：玩家压力 +1，并产生或恶化冻伤"));
		}
		OutChanges.Add(TEXT("天线校准完成；玩家体温 -1.5"));
	}
	else if (Request.ActionId == SendSignal)
	{
		State.Tasks.bSignalSent = true;
		Character(EWSCharacterId::Player).Location =
			EWSCharacterLocation::ControlRoom;
		OutChanges.Add(TEXT("求救信号已发送"));
	}

	if (
		Request.ActionId != TalkGuHeng
		&& Request.ActionId != TalkYeCheng
		&& Request.ActionId != Rest
		&& Request.ActionId != DistributeFood
		&& Request.ActionId != TreatCharacter
		&& Request.ActionId != TreatGuHeng
		&& Request.ActionId != SendSignal)
	{
		Character(Executor).Location = Rule.Location;
	}
	if (
		Request.bHasCollaborator
		&& Request.ActionId != CalibrateAntenna)
	{
		Character(Request.Collaborator).Location = Rule.Location;
	}
}

void FWhiteoutRulesEngine::ApplyEnvironment(const int32 APCost, const bool bOutdoors, TArray<FString>& OutChanges)
{
	for (int32 Index = 0; Index < APCost; ++Index)
	{
		for (const EWSCharacterId CharacterId : {EWSCharacterId::Player, EWSCharacterId::GuHeng, EWSCharacterId::YeCheng})
		{
			ChangeCharacter(CharacterId, 0, -0.1f, -0.1f, -0.1f, 0, 0);
		}
		if (bOutdoors)
		{
			ChangeCharacter(EWSCharacterId::Player, 0, -0.5f, 0, -0.4f, 0, 0);
		}
	}
	if (APCost > 0)
	{
		OutChanges.Add(FString::Printf(TEXT("Environment settled for %d AP"), APCost));
	}
}

void FWhiteoutRulesEngine::TriggerMidCrisis(TArray<FString>& OutChanges)
{
	State.bMidCrisisTriggered = true;
	if (State.Tasks.GeneratorProgress == 0)
	{
		ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 0.3f, 0);
		ChangeCharacter(EWSCharacterId::YeCheng, 0, 0, 0, 0, 0.3f, 0);
	}
	if (!State.Flags.bGuHengTreated && ActionCount(WhiteoutRules::RepairGenerator) > 0)
	{
		ChangeCharacter(EWSCharacterId::GuHeng, -0.2f, 0, 0, 0, 0.4f, 0);
	}
	if (Character(EWSCharacterId::YeCheng).Trust >= 6.0f)
	{
		State.Flags.bHeatPackRevealed = true;
		DiscoverFact(WhiteoutRules::FactHeatPack, EWSKnowledgeLevel::Confirmed, &OutChanges);
	}
	OutChanges.Add(TEXT("Mid-crisis: backup battery voltage collapsed"));
}

void FWhiteoutRulesEngine::RecognizePromise(const FWSActionRequest& Request, TArray<FString>& OutChanges)
{
	static const TSet<FName> AllowedConditions = {
		TEXT("reserve_medicine"),
		TEXT("keep_records"),
		TEXT("preserve_records"),
		TEXT("heat_repair_room")};
	if (!AllowedConditions.Contains(Request.PromiseCondition))
	{
		return;
	}
	const FName PromiseId(*FString::Printf(TEXT("player_to_gu_heng:%s"), *Request.PromiseCondition.ToString()));
	if (State.Promises.ContainsByPredicate([PromiseId](const FWSPromiseRecord& Promise) { return Promise.PromiseId == PromiseId; }))
	{
		return;
	}
	FWSPromiseRecord Promise;
	Promise.PromiseId = PromiseId;
	Promise.ConditionId = Request.PromiseCondition;
	Promise.bRecognized = true;
	Promise.HeatingHistoryCountAtRecognition =
		State.Heating.History.Num();
	State.Promises.Add(Promise);
	OutChanges.Add(FString::Printf(TEXT("Promise recognized: %s"), *Request.PromiseCondition.ToString()));
}

void FWhiteoutRulesEngine::SettlePromises()
{
	for (FWSPromiseRecord& Promise : State.Promises)
	{
		if (Promise.bSettled)
		{
			continue;
		}
		if (Promise.ConditionId == TEXT("reserve_medicine"))
		{
			Promise.bFulfilled = State.Resources.Medicine > 0;
		}
		else if (Promise.ConditionId == TEXT("keep_records"))
		{
			Promise.bFulfilled = State.Flags.bRecordsPreserved;
		}
		else if (Promise.ConditionId == TEXT("preserve_records"))
		{
			Promise.bFulfilled = State.Flags.bRecordsPreserved;
		}
		else if (Promise.ConditionId == TEXT("heat_repair_room"))
		{
			if (IsV11())
			{
				Promise.bFulfilled = false;
				const int32 FirstEligibleHeating =
					FMath::Clamp(
						Promise.HeatingHistoryCountAtRecognition,
						0,
						State.Heating.History.Num());
				for (
					int32 Index = FirstEligibleHeating;
					Index < State.Heating.History.Num();
					++Index)
				{
					if (
						State.Heating.History[Index].Zone
						== EWSHeatingZone::RepairRoom)
					{
						Promise.bFulfilled = true;
						break;
					}
				}
			}
			else
			{
				Promise.bFulfilled =
					State.Flags.bRepairRoomHeated;
			}
		}
		Promise.bSettled = true;
		ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 0, Promise.bFulfilled ? 0.6f : -1.2f);
	}
}

bool FWhiteoutRulesEngine::SignalAvailable() const
{
	return !State.Tasks.bSignalSent && State.Tasks.GeneratorProgress >= Config.GeneratorRequired
		&& State.Tasks.AntennaCalibration >= Config.AntennaRequired;
}

void FWhiteoutRulesEngine::EndGame()
{
	SettlePromises();
	State.Ending = ClassifyEnding();
	State.Score = CalculateScore();
	State.Phase = EWSGamePhase::Results;
}

bool FWhiteoutRulesEngine::TryRecordModelCall()
{
	if (State.ModelCalls >= Config.ModelCallHardLimit)
	{
		return false;
	}
	++State.ModelCalls;
	return true;
}

EWSEndingType FWhiteoutRulesEngine::ClassifyEnding() const
{
	if (IsV11())
	{
		bool bCritical = false;
		for (const EWSCharacterId CharacterId : {
			EWSCharacterId::Player,
			EWSCharacterId::GuHeng,
			EWSCharacterId::YeCheng})
		{
			bCritical |= IsV11Critical(Character(CharacterId));
		}
		if (State.Tasks.bSignalSent)
		{
			const FWSCharacterState& GuHeng =
				Character(EWSCharacterId::GuHeng);
			const FWSCharacterState& YeCheng =
				Character(EWSCharacterId::YeCheng);
			const bool bTeamCooperating =
				(
					GuHeng.Trust >= 3.0f
					&& GuHeng.Pressure < 9.0f)
				|| (
					YeCheng.Trust >= 3.0f
					&& YeCheng.Pressure < 9.0f);
			return !bCritical
				&& State.Tasks.bGeneratorStable
				&& bTeamCooperating
				? EWSEndingType::TaskSuccess
				: EWSEndingType::CostUncontrolled;
		}
		const bool bSafeWait =
			!bCritical
			&& State.Resources.Fuel >= Config.SafeWaitFuel
			&& State.Flags.bKitchenHeaterIntact;
		return bSafeWait
			? EWSEndingType::SurvivalWait
			: EWSEndingType::TotalCollapse;
	}

	bool bCritical = false;
	for (const EWSCharacterId CharacterId : {EWSCharacterId::Player, EWSCharacterId::GuHeng, EWSCharacterId::YeCheng})
	{
		bCritical |= IsCritical(Character(CharacterId));
	}
	if (State.Tasks.bSignalSent)
	{
		return bCritical ? EWSEndingType::CostUncontrolled : EWSEndingType::TaskSuccess;
	}
	if (!bCritical && State.Resources.Fuel >= 2)
	{
		return EWSEndingType::SurvivalWait;
	}
	if (State.Tasks.GeneratorProgress > 0 || !bCritical)
	{
		return EWSEndingType::CostUncontrolled;
	}
	return EWSEndingType::TotalCollapse;
}

FWSScoreBreakdown FWhiteoutRulesEngine::CalculateScore() const
{
	if (IsV11())
	{
		FWSScoreBreakdown Score;
		Score.TaskQuality =
			12.0f
				* FMath::Min(
					1.0f,
					static_cast<float>(State.Tasks.GeneratorProgress)
						/ Config.GeneratorRequired)
			+ 8.0f
				* FMath::Min(
					1.0f,
					static_cast<float>(State.Tasks.AntennaCalibration)
						/ Config.AntennaRequired)
			+ (State.Tasks.bSignalSent ? 10.0f : 0.0f);

		bool bAnyExhausted = false;
		bool bAnyCriticalInjury = false;
		bool bAnyHypothermic = false;
		for (const EWSCharacterId CharacterId : {
			EWSCharacterId::Player,
			EWSCharacterId::GuHeng,
			EWSCharacterId::YeCheng})
		{
			const FWSCharacterState& Current = Character(CharacterId);
			const float TemperaturePoints =
				Current.Temperature < Config.HypothermicTemperature
				? 0.0f
				: Current.Temperature < Config.WarmTemperature
					? 2.0f
					: 4.0f;
			const float InjuryPoints =
				Current.InjurySeverity == EWSInjurySeverity::Critical
				? 0.0f
				: Current.InjurySeverity == EWSInjurySeverity::Restricted
					? 1.0f
					: 2.0f;
			const float PressurePoints =
				2.0f * (1.0f - Current.Pressure / 10.0f);
			Score.People += FMath::Clamp(
				TemperaturePoints
					+ static_cast<float>(Current.Stamina)
					+ InjuryPoints
					+ PressurePoints,
				0.0f,
				10.0f);
			bAnyExhausted |= Current.Stamina == 0;
			bAnyCriticalInjury |=
				Current.InjurySeverity == EWSInjurySeverity::Critical;
			bAnyHypothermic |=
				Current.Temperature < Config.HypothermicTemperature;
		}

		float FuelScore =
			5.0f * FMath::Min(1.0f, State.Resources.Fuel / 2.0f);
		float FoodScore =
			4.0f * FMath::Min(1.0f, State.Resources.Food / 2.0f);
		float MedicalScore =
			State.Resources.Medicine + State.Resources.HeatPack > 0
			? 4.0f
			: 0.0f;
		const float KitchenScore =
			State.Flags.bKitchenHeaterIntact ? 2.0f : 0.0f;
		if (bAnyHypothermic)
		{
			FuelScore *= 0.25f;
		}
		if (bAnyExhausted)
		{
			FoodScore *= 0.25f;
		}
		if (bAnyCriticalInjury)
		{
			MedicalScore *= 0.25f;
		}
		Score.EffectiveReserves =
			FMath::Min(
				15.0f,
				FuelScore + FoodScore + MedicalScore + KitchenScore);

		const float TrustAverage =
			(
				Character(EWSCharacterId::GuHeng).Trust
				+ Character(EWSCharacterId::YeCheng).Trust)
			/ 20.0f;
		int32 BrokenPromises = 0;
		for (const FWSPromiseRecord& Promise : State.Promises)
		{
			BrokenPromises +=
				Promise.bSettled && !Promise.bFulfilled ? 1 : 0;
		}
		Score.SocialStability = FMath::Clamp(
			12.0f * TrustAverage
				+ (BrokenPromises == 0 ? 3.0f : 0.0f)
				- 1.5f * State.Flags.ForcedActionCount,
			0.0f,
			15.0f);

		int32 Confirmed = 0;
		for (const TPair<FName, EWSKnowledgeLevel>& Pair : State.PlayerKnowledge)
		{
			Confirmed += Pair.Value == EWSKnowledgeLevel::Confirmed ? 1 : 0;
		}
		Score.InformationResponsibility =
			FMath::Min(9.0f, Confirmed * 1.5f)
			+ (State.Flags.bRecordsPreserved ? 1.0f : 0.0f);
		Score.InformationResponsibility =
			FMath::Min(10.0f, Score.InformationResponsibility);

		Score.Total = FMath::Clamp(
			Score.TaskQuality
				+ Score.People
				+ Score.EffectiveReserves
				+ Score.SocialStability
				+ Score.InformationResponsibility,
			0.0f,
			100.0f);
		Score.Rating =
			Score.Total >= 90.0f
			? TEXT("S")
			: Score.Total >= 80.0f
				? TEXT("A")
				: Score.Total >= 70.0f
					? TEXT("B")
					: Score.Total >= 60.0f
						? TEXT("C")
						: TEXT("D");
		if (
			!State.Tasks.bSignalSent
			&& (
				Score.Rating == TEXT("S")
				|| Score.Rating == TEXT("A")
				|| Score.Rating == TEXT("B")))
		{
			Score.Rating = TEXT("C");
		}
		if (
			(bAnyCriticalInjury || bAnyHypothermic)
			&& (
				Score.Rating == TEXT("S")
				|| Score.Rating == TEXT("A")))
		{
			Score.Rating = TEXT("B");
		}
		return Score;
	}

	FWSScoreBreakdown Score;
	Score.TaskQuality += 10.0f * FMath::Min(1.0f, static_cast<float>(State.Tasks.GeneratorProgress) / Config.GeneratorRequired);
	Score.TaskQuality += 8.0f * FMath::Min(1.0f, static_cast<float>(State.Tasks.AntennaCalibration) / Config.AntennaRequired);
	Score.TaskQuality += State.Tasks.bSignalSent ? 7.0f : 0.0f;
	if (State.Tasks.bSignalSent)
	{
		Score.TaskQuality += FMath::Min(5.0f, State.ActionPoints * 2.5f);
	}

	for (const EWSCharacterId CharacterId : {EWSCharacterId::Player, EWSCharacterId::GuHeng, EWSCharacterId::YeCheng})
	{
		const FWSCharacterState& Current = Character(CharacterId);
		const float Normalized = (
			0.40f * Current.Health + 0.25f * Current.Temperature + 0.20f * Current.Fatigue
			+ 0.15f * Current.Hunger) / 10.0f;
		Score.People += 10.0f * FMath::Clamp(Normalized, 0.0f, 1.0f);
	}

	const float FuelScore = 6.0f * FMath::Min(1.0f, State.Resources.Fuel / 2.0f);
	const float FoodScore = 4.0f * FMath::Min(1.0f, State.Resources.Food / 2.0f);
	float MedicalScore = State.Resources.Medicine + State.Resources.HeatPack > 0 ? 5.0f : 0.0f;
	const FWSCharacterState& Gu = Character(EWSCharacterId::GuHeng);
	if (IsCritical(Gu) && State.Resources.Medicine > 0)
	{
		MedicalScore = FMath::Min(MedicalScore, 1.25f);
	}
	float HeaterScore = 0.0f;
	if (State.Flags.bKitchenHeaterIntact)
	{
		HeaterScore = State.Resources.Fuel >= 2 ? 5.0f : 1.25f;
	}
	Score.EffectiveReserves = FuelScore + FoodScore + MedicalScore + HeaterScore;

	const float GuTrust = FMath::Clamp(Character(EWSCharacterId::GuHeng).Trust / 10.0f, 0.0f, 1.0f);
	const float YeTrust = FMath::Clamp(Character(EWSCharacterId::YeCheng).Trust / 10.0f, 0.0f, 1.0f);
	Score.SocialStability = 12.0f * (GuTrust + YeTrust) / 2.0f;
	for (const FWSPromiseRecord& Promise : State.Promises)
	{
		if (Promise.bSettled && !Promise.bFulfilled)
		{
			Score.SocialStability = FMath::Max(0.0f, Score.SocialStability - 2.0f);
		}
	}

	int32 Confirmed = 0;
	for (const TPair<FName, EWSKnowledgeLevel>& Pair : State.PlayerKnowledge)
	{
		Confirmed += Pair.Value == EWSKnowledgeLevel::Confirmed ? 1 : 0;
	}
	Score.InformationResponsibility = 8.0f * FMath::Min(1.0f, Confirmed / 8.0f);
	if (State.Flags.bRecordsPreserved)
	{
		Score.InformationResponsibility = FMath::Min(8.0f, Score.InformationResponsibility + 1.0f);
	}

	Score.TaskQuality = FMath::Min(30.0f, Score.TaskQuality);
	Score.People = FMath::Min(30.0f, Score.People);
	Score.EffectiveReserves = FMath::Min(20.0f, Score.EffectiveReserves);
	Score.SocialStability = FMath::Min(12.0f, Score.SocialStability);
	Score.InformationResponsibility = FMath::Min(8.0f, Score.InformationResponsibility);
	Score.Total = Score.TaskQuality + Score.People + Score.EffectiveReserves + Score.SocialStability + Score.InformationResponsibility;
	Score.Rating = Score.Total >= 90.0f ? TEXT("S") : Score.Total >= 80.0f ? TEXT("A") : Score.Total >= 70.0f ? TEXT("B") : Score.Total >= 60.0f ? TEXT("C") : TEXT("D");
	return Score;
}

bool FWhiteoutRulesEngine::IsCritical(const FWSCharacterState& CharacterState) const
{
	if (IsV11())
	{
		return IsV11Critical(CharacterState);
	}
	return CharacterState.Health <= Config.CriticalHealth
		|| CharacterState.Temperature <= Config.CriticalTemperature
		|| CharacterState.Fatigue <= Config.CriticalFatigue
		|| CharacterState.Pressure >= Config.CriticalPressure;
}

bool FWhiteoutRulesEngine::IsV11Critical(
	const FWSCharacterState& CharacterState) const
{
	return CharacterState.InjurySeverity == EWSInjurySeverity::Critical
		|| CharacterState.Temperature < Config.HypothermicTemperature;
}

void FWhiteoutRulesEngine::ConsumeV11Stamina(
	const EWSCharacterId CharacterId)
{
	FWSCharacterState& Current = Character(CharacterId);
	Current.Stamina = FMath::Max(0, Current.Stamina - 1);
}

void FWhiteoutRulesEngine::WorsenV11Injury(
	const EWSCharacterId CharacterId,
	TArray<FString>& OutChanges)
{
	FWSCharacterState& Current = Character(CharacterId);
	if (Current.InjurySeverity != EWSInjurySeverity::Restricted)
	{
		return;
	}
	if (Current.BandageProtection > 0)
	{
		--Current.BandageProtection;
		OutChanges.Add(TEXT("包扎阻止本次伤势恶化"));
		return;
	}
	if (Current.InjuryWorseningMarks == 0)
	{
		Current.InjuryWorseningMarks = 1;
		OutChanges.Add(TEXT("受限伤势获得恶化标记"));
		return;
	}
	Current.InjuryWorseningMarks = 2;
	Current.InjurySeverity = EWSInjurySeverity::Critical;
	if (!Current.InjuryId.IsNone())
	{
		Current.InjuryId = FName(
			*Current.InjuryId.ToString().Replace(
				TEXT("_restricted"),
				TEXT("_critical")));
	}
	OutChanges.Add(TEXT("第二次未处理带伤维修使伤势进入危重"));
}

EWSCharacterId FWhiteoutRulesEngine::ResolveV11Executor(
	const FWSActionRequest& Request) const
{
	if (const FWhiteoutActionRule* Rule =
			Config.ActionRules.Find(Request.ActionId))
	{
		return Rule->PrimaryExecutor;
	}
	return EWSCharacterId::Player;
}

bool FWhiteoutRulesEngine::IsV11Action(const FName ActionId) const
{
	return Config.ActionRules.Contains(ActionId);
}

bool FWhiteoutRulesEngine::HasV11Tag(
	const FName ActionId,
	const FName Tag) const
{
	const FWhiteoutActionRule* Rule = Config.ActionRules.Find(ActionId);
	return Rule && Rule->Tags.Contains(Tag);
}

bool FWhiteoutRulesEngine::IsV11CharacterAvailable(
	const EWSCharacterId CharacterId) const
{
	const FWSCharacterState& Current = Character(CharacterId);
	return Current.Stamina > 0
		&& Current.Temperature >= Config.HypothermicTemperature
		&& Current.InjurySeverity != EWSInjurySeverity::Critical;
}

bool FWhiteoutRulesEngine::V11HeatingMatchesLocation(
	const EWSCharacterLocation Location) const
{
	return HeatingZoneForLocation(Location) == State.Heating.CurrentZone
		&& State.Heating.CurrentZone != EWSHeatingZone::None;
}

EWSHeatingZone FWhiteoutRulesEngine::HeatingZoneForLocation(
	const EWSCharacterLocation Location)
{
	switch (Location)
	{
	case EWSCharacterLocation::RepairRoom:
		return EWSHeatingZone::RepairRoom;
	case EWSCharacterLocation::MedicalRoom:
		return EWSHeatingZone::MedicalRoom;
	case EWSCharacterLocation::Kitchen:
		return EWSHeatingZone::Kitchen;
	case EWSCharacterLocation::ControlRoom:
		return EWSHeatingZone::ControlRoom;
	default:
		return EWSHeatingZone::None;
	}
}

EWSDayPhase FWhiteoutRulesEngine::NextDayPhase(
	const EWSDayPhase DayPhase)
{
	switch (DayPhase)
	{
	case EWSDayPhase::Morning:
		return EWSDayPhase::Afternoon;
	case EWSDayPhase::Afternoon:
		return EWSDayPhase::Dusk;
	default:
		return EWSDayPhase::Complete;
	}
}

FString FWhiteoutRulesEngine::DayPhaseLabel(const EWSDayPhase DayPhase)
{
	switch (DayPhase)
	{
	case EWSDayPhase::Morning:
		return TEXT("上午");
	case EWSDayPhase::Afternoon:
		return TEXT("下午");
	case EWSDayPhase::Dusk:
		return TEXT("黄昏");
	default:
		return TEXT("结束");
	}
}

FString FWhiteoutRulesEngine::HeatingZoneLabel(
	const EWSHeatingZone HeatingZone)
{
	switch (HeatingZone)
	{
	case EWSHeatingZone::RepairRoom:
		return TEXT("维修间");
	case EWSHeatingZone::MedicalRoom:
		return TEXT("医务室");
	case EWSHeatingZone::Kitchen:
		return TEXT("厨房");
	case EWSHeatingZone::ControlRoom:
		return TEXT("控制室");
	default:
		return TEXT("未选择");
	}
}

TArray<FName> FWhiteoutRulesEngine::BuildAllowedFactIds(const EWSCharacterId CharacterId) const
{
	using namespace WhiteoutRules;
	TArray<FName> Result;
	if (CharacterId == EWSCharacterId::GuHeng)
	{
		Result = {FactGeneratorProtectionStop, FactBurntRelay, FactHandInjury, FactRelayCompatibility, FactForcedRestartConfirmed};
	}
	else if (CharacterId == EWSCharacterId::YeCheng)
	{
		Result = {FactHandInjury, FactMedicalDiagnosis, FactHeatPack};
	}
	for (const FName PublicFact : State.PublicFacts)
	{
		Result.AddUnique(PublicFact);
	}
	return Result;
}

bool FWhiteoutRulesEngine::ValidateAgentResponse(
	const FString& Utterance,
	const TArray<FName>& ReferencedFactIds,
	const TArray<FName>& AllowedFactIds,
	const bool bContainsRuleMutation,
	FString& OutReason)
{
	if (Utterance.TrimStartAndEnd().IsEmpty())
	{
		OutReason = TEXT("missing_utterance");
		return false;
	}
	if (Utterance.Len() > 240)
	{
		OutReason = TEXT("utterance_too_long");
		return false;
	}
	if (bContainsRuleMutation)
	{
		OutReason = TEXT("model_attempted_rule_change");
		return false;
	}
	for (const FName FactId : ReferencedFactIds)
	{
		if (!AllowedFactIds.Contains(FactId))
		{
			OutReason = TEXT("fact_permission_violation");
			return false;
		}
	}
	OutReason = TEXT("ok");
	return true;
}

bool FWhiteoutRulesEngine::Knows(const FName FactId, const EWSKnowledgeLevel Minimum) const
{
	const EWSKnowledgeLevel* Current = State.PlayerKnowledge.Find(FactId);
	return Current && static_cast<uint8>(*Current) >= static_cast<uint8>(Minimum);
}

void FWhiteoutRulesEngine::DiscoverFact(const FName FactId, const EWSKnowledgeLevel Level, TArray<FString>* OutChanges)
{
	EWSKnowledgeLevel& Current = State.PlayerKnowledge.FindOrAdd(FactId, EWSKnowledgeLevel::Unknown);
	if (static_cast<uint8>(Level) > static_cast<uint8>(Current))
	{
		Current = Level;
		if (OutChanges)
		{
			OutChanges->Add(FString::Printf(TEXT("Fact updated: %s"), *FactId.ToString()));
		}
	}
}

void FWhiteoutRulesEngine::AddEvidence(const FName EvidenceId, TArray<FString>* OutChanges)
{
	if (State.Evidence.AddUnique(EvidenceId) != INDEX_NONE && OutChanges)
	{
		OutChanges->Add(FString::Printf(TEXT("Evidence acquired: %s"), *EvidenceId.ToString()));
	}
}

void FWhiteoutRulesEngine::ChangeCharacter(
	const EWSCharacterId CharacterId,
	const float Health,
	const float Temperature,
	const float Hunger,
	const float Fatigue,
	const float Pressure,
	const float Trust)
{
	FWSCharacterState& Current = Character(CharacterId);
	Current.Health = FMath::Clamp(Current.Health + Health, 0.0f, 10.0f);
	Current.Temperature = FMath::Clamp(Current.Temperature + Temperature, 0.0f, 10.0f);
	Current.Hunger = FMath::Clamp(Current.Hunger + Hunger, 0.0f, 10.0f);
	Current.Fatigue = FMath::Clamp(Current.Fatigue + Fatigue, 0.0f, 10.0f);
	Current.Pressure = FMath::Clamp(Current.Pressure + Pressure, 0.0f, 10.0f);
	Current.Trust = FMath::Clamp(Current.Trust + Trust, 0.0f, 10.0f);
}

FWSCharacterState& FWhiteoutRulesEngine::Character(const EWSCharacterId CharacterId)
{
	return State.Characters.FindChecked(CharacterId);
}

const FWSCharacterState& FWhiteoutRulesEngine::Character(const EWSCharacterId CharacterId) const
{
	return State.Characters.FindChecked(CharacterId);
}

int32 FWhiteoutRulesEngine::ActionCount(const FName ActionId) const
{
	return State.ActionCounts.FindRef(ActionId);
}

int32 FWhiteoutRulesEngine::GetActionCost(const FName ActionId)
{
	using namespace WhiteoutRules;
	if (ActionId == ForcedSelfRepair || ActionId == CalibrateAntenna) return 2;
	if (ActionId == SendSignal) return 0;
	return IsCoreAction(ActionId) ? 1 : 0;
}

bool FWhiteoutRulesEngine::IsCoreAction(const FName ActionId)
{
	using namespace WhiteoutRules;
	return ActionId == InvestigateGeneratorLog || ActionId == InspectControlCabinet || ActionId == TalkGuHeng
		|| ActionId == TalkYeCheng || ActionId == HeatRepairRoom || ActionId == HeatMedicalRoom
		|| ActionId == DistributeFood || ActionId == Rest || ActionId == TreatGuHeng
		|| ActionId == TreatCharacter || ActionId == DismantleKitchenHeater
		|| ActionId == RepairGenerator || ActionId == ForcedSelfRepair || ActionId == CalibrateAntenna
		|| ActionId == SendSignal;
}

bool FWhiteoutRulesEngine::ActionRepeatable(const FName ActionId)
{
	return ActionId == WhiteoutRules::TalkGuHeng || ActionId == WhiteoutRules::TalkYeCheng || ActionId == WhiteoutRules::RepairGenerator;
}

int32 FWhiteoutRulesEngine::ActionMaxUses(const FName ActionId)
{
	return ActionRepeatable(ActionId) ? 2 : 1;
}

FText FWhiteoutRulesEngine::ActionPreviewText(const FName ActionId)
{
	using namespace WhiteoutRules;
	if (ActionId == InvestigateGeneratorLog) return FText::FromString(TEXT("读取保护系统与手动旁路记录。"));
	if (ActionId == InspectControlCabinet) return FText::FromString(TEXT("检查烧毁继电器、电弧痕迹与伤手线索。"));
	if (ActionId == TalkGuHeng) return FText::FromString(TEXT("询问、质疑或交换维修条件。"));
	if (ActionId == TalkYeCheng) return FText::FromString(TEXT("获取顾衡伤势与暴雪风险判断。"));
	if (ActionId == HeatRepairRoom) return FText::FromString(TEXT("消耗1燃料，提高维修效率并保护顾衡。"));
	if (ActionId == HeatMedicalRoom) return FText::FromString(TEXT("消耗1燃料，允许完整诊断与治疗。"));
	if (ActionId == DistributeFood) return FText::FromString(TEXT("一次性分配最多2份食物。"));
	if (ActionId == Rest) return FText::FromString(TEXT("在供暖区恢复体能，或在其他区域降低压力。"));
	if (ActionId == TreatGuHeng) return FText::FromString(TEXT("消耗药品或已披露的保温包，改善伤手。"));
	if (ActionId == TreatCharacter) return FText::FromString(TEXT("包扎、完整治疗或使用保温包支撑指定角色。"));
	if (ActionId == DismantleKitchenHeater) return FText::FromString(TEXT("取得替代继电器。"));
	if (ActionId == RepairGenerator) return FText::FromString(TEXT("根据治疗、供暖与继电器状态产生1—2点进度。"));
	if (ActionId == ForcedSelfRepair) return FText::FromString(TEXT("不依赖顾衡，2AP只获得1点进度。"));
	if (ActionId == CalibrateAntenna) return FText::FromString(TEXT("发电机恢复后完成天线校准。"));
	if (ActionId == SendSignal) return FText::FromString(TEXT("发电机2/2且天线1/1时立即发信。"));
	return FText::GetEmpty();
}

FText FWhiteoutRulesEngine::ActionRiskText(const FName ActionId)
{
	using namespace WhiteoutRules;
	if (ActionId == CalibrateAntenna) return FText::FromString(TEXT("结算两次室外暴露，明显降低体温与精力。"));
	if (ActionId == DismantleKitchenHeater) return FText::FromString(TEXT("破坏夜间供暖；独自拆解会受轻伤。"));
	if (ActionId == RepairGenerator) return FText::FromString(TEXT("顾衡带伤维修会继续恶化。"));
	if (ActionId == ForcedSelfRepair) return FText::FromString(TEXT("玩家受伤并大幅疲劳；只能使用一次。"));
	if (ActionId == TreatCharacter) return FText::FromString(TEXT("叶澄连续治疗会消耗体能；完整治疗需要本阶段供暖医务室。"));
	if (ActionId == HeatRepairRoom || ActionId == HeatMedicalRoom) return FText::FromString(TEXT("会消耗夜间燃料储备。"));
	return FText::FromString(TEXT("占用一个行动窗口。"));
}
