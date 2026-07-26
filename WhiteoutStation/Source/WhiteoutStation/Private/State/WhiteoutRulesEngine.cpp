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
}

FWhiteoutRulesEngine::FWhiteoutRulesEngine()
{
	Config.InitialState.ActionPoints = 8;
	Config.InitialState.Phase = EWSGamePhase::ActionPhase;
	Config.InitialState.Characters.Add(
		EWSCharacterId::Player,
		WhiteoutRules::MakeCharacter(100.0f, 72.0f, 65.0f, 65.0f, 40.0f, 0.0f));
	Config.InitialState.Characters.Add(
		EWSCharacterId::GuHeng,
		WhiteoutRules::MakeCharacter(62.0f, 66.0f, 55.0f, 50.0f, 72.0f, -15.0f));
	Config.InitialState.Characters.Add(
		EWSCharacterId::YeCheng,
		WhiteoutRules::MakeCharacter(92.0f, 70.0f, 68.0f, 55.0f, 48.0f, 10.0f));
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

	if (Config.StartingActionPoints != 8 || Config.MidCrisisThreshold != 4)
	{
		OutError = TEXT("v0.6 requires 8 starting AP and a 4 AP crisis threshold");
		return false;
	}

	Config.InitialState = MoveTemp(Parsed);
	Reset();
	OutError.Reset();
	return true;
}

void FWhiteoutRulesEngine::Reset()
{
	State = Config.InitialState;
	State.ActionPoints = Config.StartingActionPoints;
	State.Phase = EWSGamePhase::ActionPhase;
	State.bMidCrisisTriggered = false;
	State.PlayerKnowledge.Reset();
	State.Evidence.Reset();
	State.PublicFacts.Reset();
	State.ActionCounts.Reset();
	State.CommittedTransactions.Reset();
	State.Promises.Reset();
	State.EventLog.Reset();
	State.ModelCalls = 0;
	State.Score = FWSScoreBreakdown();
}

void FWhiteoutRulesEngine::SetState(const FWSGameState& InState)
{
	State = InState;
}

FWSActionPreview FWhiteoutRulesEngine::Preview(const FWSActionRequest& Request) const
{
	FWSActionPreview Result;
	Result.ActionId = Request.ActionId;
	Result.APCost = GetActionCost(Request.ActionId);
	Result.PreviewText = ActionPreviewText(Request.ActionId);
	Result.RiskText = ActionRiskText(Request.ActionId);
	Result.ReasonCode = CanExecute(Request);
	Result.bCanExecute = Result.ReasonCode == EWSReasonCode::Ok;
	return Result;
}

FWSActionResult FWhiteoutRulesEngine::Commit(FWSActionRequest Request)
{
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
	return Result;
}

EWSReasonCode FWhiteoutRulesEngine::CanExecute(const FWSActionRequest& Request) const
{
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
				|| Pressure >= (CharacterId == EWSCharacterId::GuHeng ? 65.0f : 60.0f)
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
		if (Character(EWSCharacterId::GuHeng).Health <= 30.0f) return EWSReasonCode::GuHengCritical;
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
		ChangeCharacter(EWSCharacterId::YeCheng, 0, 0, 0, 0, -4, 4);
		State.Flags.bGuHengDiagnosed = true;
		AddEvidence(TEXT("EVIDENCE_MEDICAL_DIAGNOSIS"), &OutChanges);
		DiscoverFact(FactHandInjury, EWSKnowledgeLevel::Confirmed, &OutChanges);
		DiscoverFact(FactMedicalDiagnosis, EWSKnowledgeLevel::Confirmed, &OutChanges);
		if (Character(EWSCharacterId::YeCheng).Trust >= 10.0f)
		{
			State.Flags.bHeatPackRevealed = true;
			AddEvidence(TEXT("EVIDENCE_HEAT_PACK"), &OutChanges);
			DiscoverFact(FactHeatPack, EWSKnowledgeLevel::Confirmed, &OutChanges);
		}
		OutChanges.Add(TEXT("Ye Cheng trust +4, pressure -4"));
		if (Request.DialogueAct == EWSDialogueAct::Challenge)
		{
			ChangeCharacter(EWSCharacterId::YeCheng, 0, 0, 0, 0, 3, -3);
			OutChanges.Add(TEXT("Challenge modifier: Ye Cheng trust -3, pressure +3"));
		}
		else if (Request.DialogueAct == EWSDialogueAct::Reassure)
		{
			ChangeCharacter(EWSCharacterId::YeCheng, 0, 0, 0, 0, -4, 3);
			OutChanges.Add(TEXT("Reassure modifier: Ye Cheng trust +3, pressure -4"));
		}
	}
	else if (Request.ActionId == TalkGuHeng)
	{
		const bool bStrongEvidence = Knows(FactForcedRestartSuspicion) && Knows(FactBurntRelay);
		if (State.Flags.bGuHengTreated)
		{
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, -8, 12);
			State.Flags.bGuHengCooperative = true;
			OutChanges.Add(TEXT("Gu Heng accepts cooperation after treatment"));
		}
		else if (bStrongEvidence)
		{
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 3, 8);
			State.Flags.bGuHengCooperative = true;
			State.Flags.bRelayCompatibilityKnown = true;
			AddEvidence(TEXT("EVIDENCE_HEATER_SERVICE_LABEL"), &OutChanges);
			DiscoverFact(FactRelayCompatibility, EWSKnowledgeLevel::Confirmed, &OutChanges);
			DiscoverFact(FactForcedRestartConfirmed, EWSKnowledgeLevel::Confirmed, &OutChanges);
			OutChanges.Add(TEXT("Gu Heng confirms the technical route"));
		}
		else
		{
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 4, -3);
			OutChanges.Add(TEXT("Gu Heng refuses an unsupported request"));
		}
		if (Request.DialogueAct == EWSDialogueAct::Challenge)
		{
			if (bStrongEvidence)
			{
				ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 2, 2);
				OutChanges.Add(TEXT("Evidence challenge modifier: Gu Heng trust +2, pressure +2"));
			}
			else
			{
				ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 3, -3);
				OutChanges.Add(TEXT("Challenge modifier: Gu Heng trust -3, pressure +3"));
			}
		}
		else if (Request.DialogueAct == EWSDialogueAct::Reassure)
		{
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, -4, 3);
			OutChanges.Add(TEXT("Reassure modifier: Gu Heng trust +3, pressure -4"));
		}
		else if (Request.DialogueAct == EWSDialogueAct::Promise)
		{
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, -2, 2);
			OutChanges.Add(TEXT("Promise modifier: Gu Heng trust +2, pressure -2"));
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
		ChangeCharacter(EWSCharacterId::GuHeng, 0, 6, 0, 0, -4, 0);
		OutChanges.Add(TEXT("Repair room heated; fuel -1"));
	}
	else if (Request.ActionId == HeatMedicalRoom)
	{
		State.Resources.Fuel -= 1;
		State.Flags.bMedicalRoomHeated = true;
		ChangeCharacter(EWSCharacterId::GuHeng, 0, 4, 0, 0, 0, 0);
		ChangeCharacter(EWSCharacterId::YeCheng, 0, 3, 0, 0, -3, 0);
		OutChanges.Add(TEXT("Medical room heated; fuel -1"));
	}
	else if (Request.ActionId == DistributeFood)
	{
		const int32 Total = Request.FoodForPlayer + Request.FoodForGuHeng + Request.FoodForYeCheng;
		State.Resources.Food -= Total;
		ChangeCharacter(EWSCharacterId::Player, 0, 0, Request.FoodForPlayer * 20.0f, 0, 0, 0);
		ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, Request.FoodForGuHeng * 20.0f, 0, 0, Request.FoodForGuHeng ? 12.0f : -6.0f);
		ChangeCharacter(EWSCharacterId::YeCheng, 0, 0, Request.FoodForYeCheng * 20.0f, 0, 0, Request.FoodForYeCheng ? 5.0f : -5.0f);
		State.Flags.bGuHengFed = Request.FoodForGuHeng > 0;
		OutChanges.Add(FString::Printf(TEXT("Food allocated: player=%d gu=%d ye=%d"), Request.FoodForPlayer, Request.FoodForGuHeng, Request.FoodForYeCheng));
	}
	else if (Request.ActionId == TreatGuHeng)
	{
		if (Request.TreatmentResource == EWSResourceType::Medicine)
		{
			State.Resources.Medicine -= 1;
			ChangeCharacter(EWSCharacterId::GuHeng, 22, 5, 0, 8, -16, 8);
			OutChanges.Add(TEXT("Gu Heng treated with medicine"));
		}
		else
		{
			State.Resources.HeatPack -= 1;
			ChangeCharacter(EWSCharacterId::GuHeng, 12, 10, 0, 5, -10, 5);
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
			ChangeCharacter(EWSCharacterId::GuHeng, -2, 0, 0, 0, -3, 0);
		}
		else
		{
			ChangeCharacter(EWSCharacterId::Player, -4, 0, 0, -5, 0, 0);
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 0, -3);
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
			ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, -4, -4, 0);
		}
		else
		{
			ChangeCharacter(EWSCharacterId::GuHeng, -6, -2, 0, -6, 7, 0);
		}
		OutChanges.Add(FString::Printf(TEXT("Generator progress +%d"), Progress));
	}
	else if (Request.ActionId == ForcedSelfRepair)
	{
		State.Flags.bSelfRepairUsed = true;
		State.Tasks.GeneratorProgress = FMath::Min(Config.GeneratorRequired, State.Tasks.GeneratorProgress + 1);
		ChangeCharacter(EWSCharacterId::Player, -10, -4, 0, -15, 8, 0);
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

void FWhiteoutRulesEngine::ApplyEnvironment(const int32 APCost, const bool bOutdoors, TArray<FString>& OutChanges)
{
	for (int32 Index = 0; Index < APCost; ++Index)
	{
		for (const EWSCharacterId CharacterId : {EWSCharacterId::Player, EWSCharacterId::GuHeng, EWSCharacterId::YeCheng})
		{
			ChangeCharacter(CharacterId, 0, -1, -1, -1, 0, 0);
		}
		if (bOutdoors)
		{
			ChangeCharacter(EWSCharacterId::Player, 0, -5, 0, -4, 0, 0);
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
		ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 3, 0);
		ChangeCharacter(EWSCharacterId::YeCheng, 0, 0, 0, 0, 3, 0);
	}
	if (!State.Flags.bGuHengTreated && ActionCount(WhiteoutRules::RepairGenerator) > 0)
	{
		ChangeCharacter(EWSCharacterId::GuHeng, -2, 0, 0, 0, 4, 0);
	}
	if (Character(EWSCharacterId::YeCheng).Trust >= 10.0f)
	{
		State.Flags.bHeatPackRevealed = true;
		DiscoverFact(WhiteoutRules::FactHeatPack, EWSKnowledgeLevel::Confirmed, &OutChanges);
	}
	OutChanges.Add(TEXT("Mid-crisis: backup battery voltage collapsed"));
}

void FWhiteoutRulesEngine::RecognizePromise(const FWSActionRequest& Request, TArray<FString>& OutChanges)
{
	static const TSet<FName> AllowedConditions = {
		TEXT("reserve_medicine"), TEXT("keep_records"), TEXT("heat_repair_room")};
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
		else if (Promise.ConditionId == TEXT("heat_repair_room"))
		{
			Promise.bFulfilled = State.Flags.bRepairRoomHeated;
		}
		Promise.bSettled = true;
		ChangeCharacter(EWSCharacterId::GuHeng, 0, 0, 0, 0, 0, Promise.bFulfilled ? 6.0f : -12.0f);
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
	bool bCritical = false;
	for (const EWSCharacterId CharacterId : {EWSCharacterId::Player, EWSCharacterId::GuHeng, EWSCharacterId::YeCheng})
	{
		const FWSCharacterState& Current = Character(CharacterId);
		bCritical |= Current.Health <= 30.0f || Current.Temperature <= 30.0f;
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
			+ 0.15f * Current.Hunger) / 100.0f;
		Score.People += 10.0f * FMath::Clamp(Normalized, 0.0f, 1.0f);
	}

	const float FuelScore = 6.0f * FMath::Min(1.0f, State.Resources.Fuel / 2.0f);
	const float FoodScore = 4.0f * FMath::Min(1.0f, State.Resources.Food / 2.0f);
	float MedicalScore = State.Resources.Medicine + State.Resources.HeatPack > 0 ? 5.0f : 0.0f;
	const FWSCharacterState& Gu = Character(EWSCharacterId::GuHeng);
	if ((Gu.Health <= 30.0f || Gu.Temperature <= 30.0f) && State.Resources.Medicine > 0)
	{
		MedicalScore = FMath::Min(MedicalScore, 1.25f);
	}
	float HeaterScore = 0.0f;
	if (State.Flags.bKitchenHeaterIntact)
	{
		HeaterScore = State.Resources.Fuel >= 2 ? 5.0f : 1.25f;
	}
	Score.EffectiveReserves = FuelScore + FoodScore + MedicalScore + HeaterScore;

	const float GuTrust = FMath::Clamp((Character(EWSCharacterId::GuHeng).Trust + 50.0f) / 100.0f, 0.0f, 1.0f);
	const float YeTrust = FMath::Clamp((Character(EWSCharacterId::YeCheng).Trust + 50.0f) / 100.0f, 0.0f, 1.0f);
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
	Current.Health = FMath::Clamp(Current.Health + Health, 0.0f, 100.0f);
	Current.Temperature = FMath::Clamp(Current.Temperature + Temperature, 0.0f, 100.0f);
	Current.Hunger = FMath::Clamp(Current.Hunger + Hunger, 0.0f, 100.0f);
	Current.Fatigue = FMath::Clamp(Current.Fatigue + Fatigue, 0.0f, 100.0f);
	Current.Pressure = FMath::Clamp(Current.Pressure + Pressure, 0.0f, 100.0f);
	Current.Trust = FMath::Clamp(Current.Trust + Trust, -100.0f, 100.0f);
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
		|| ActionId == DistributeFood || ActionId == TreatGuHeng || ActionId == DismantleKitchenHeater
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
	if (ActionId == TreatGuHeng) return FText::FromString(TEXT("消耗药品或已披露的保温包，改善伤手。"));
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
	if (ActionId == HeatRepairRoom || ActionId == HeatMedicalRoom) return FText::FromString(TEXT("会消耗夜间燃料储备。"));
	return FText::FromString(TEXT("占用一个行动窗口。"));
}
