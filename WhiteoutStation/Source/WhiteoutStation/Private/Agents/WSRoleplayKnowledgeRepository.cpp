#include "Agents/WSRoleplayKnowledgeRepository.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FParsedRoleplayRepository
	{
		TMap<FName, FWSRoleplayProfile> Profiles;
		TMap<FName, FWSRoleplayKnowledgeItem> KnowledgeById;
		FWSRoleplayPolicy Policy;
		TArray<FWSRoleplayFallback> Fallbacks;
	};

	bool Fail(FString& OutError, const FString& Context, const FString& Reason)
	{
		OutError = FString::Printf(TEXT("%s: %s"), *Context, *Reason);
		return false;
	}

	bool HasExactFields(
		const TSharedPtr<FJsonObject>& Object,
		std::initializer_list<const TCHAR*> ExpectedFields,
		const FString& Context,
		FString& OutError)
	{
		if (!Object.IsValid())
		{
			return Fail(OutError, Context, TEXT("expected an object"));
		}

		TSet<FString> Expected;
		for (const TCHAR* Field : ExpectedFields)
		{
			Expected.Add(Field);
		}
		if (Object->Values.Num() != Expected.Num())
		{
			return Fail(OutError, Context, TEXT("field count does not match schema"));
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			if (!Expected.Contains(Pair.Key))
			{
				return Fail(
					OutError,
					Context,
					FString::Printf(TEXT("unknown field '%s'"), *Pair.Key));
			}
		}
		for (const FString& Field : Expected)
		{
			if (!Object->HasField(Field))
			{
				return Fail(
					OutError,
					Context,
					FString::Printf(TEXT("missing field '%s'"), *Field));
			}
		}
		return true;
	}

	bool ReadString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		FString& OutValue,
		const FString& Context,
		FString& OutError,
		const bool bAllowEmpty = false)
	{
		if (!Object->TryGetStringField(Field, OutValue))
		{
			return Fail(
				OutError,
				Context,
				FString::Printf(TEXT("field '%s' must be a string"), Field));
		}
		OutValue.TrimStartAndEndInline();
		if (!bAllowEmpty && OutValue.IsEmpty())
		{
			return Fail(
				OutError,
				Context,
				FString::Printf(TEXT("field '%s' cannot be empty"), Field));
		}
		return true;
	}

	bool ReadBool(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		bool& OutValue,
		const FString& Context,
		FString& OutError)
	{
		if (!Object->TryGetBoolField(Field, OutValue))
		{
			return Fail(
				OutError,
				Context,
				FString::Printf(TEXT("field '%s' must be a boolean"), Field));
		}
		return true;
	}

	bool ReadNumber(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		double& OutValue,
		const FString& Context,
		FString& OutError)
	{
		if (!Object->TryGetNumberField(Field, OutValue)
			|| !FMath::IsFinite(OutValue))
		{
			return Fail(
				OutError,
				Context,
				FString::Printf(TEXT("field '%s' must be a finite number"), Field));
		}
		return true;
	}

	bool ReadInteger(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		int32& OutValue,
		const FString& Context,
		FString& OutError)
	{
		double Number = 0.0;
		if (!ReadNumber(Object, Field, Number, Context, OutError)
			|| Number < static_cast<double>(MIN_int32)
			|| Number > static_cast<double>(MAX_int32)
			|| !FMath::IsNearlyEqual(Number, FMath::RoundToDouble(Number)))
		{
			if (OutError.IsEmpty())
			{
				Fail(
					OutError,
					Context,
					FString::Printf(TEXT("field '%s' must be an integer"), Field));
			}
			return false;
		}
		OutValue = static_cast<int32>(Number);
		return true;
	}

	bool ReadStringArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		TArray<FString>& OutValues,
		const FString& Context,
		FString& OutError,
		const bool bAllowEmptyArray = false)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(Field, Values) || Values == nullptr)
		{
			return Fail(
				OutError,
				Context,
				FString::Printf(TEXT("field '%s' must be an array"), Field));
		}
		if (!bAllowEmptyArray && Values->IsEmpty())
		{
			return Fail(
				OutError,
				Context,
				FString::Printf(TEXT("field '%s' cannot be empty"), Field));
		}

		OutValues.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString StringValue;
			if (!Value.IsValid() || !Value->TryGetString(StringValue))
			{
				return Fail(
					OutError,
					Context,
					FString::Printf(TEXT("field '%s' must contain only strings"), Field));
			}
			StringValue.TrimStartAndEndInline();
			if (StringValue.IsEmpty())
			{
				return Fail(
					OutError,
					Context,
					FString::Printf(TEXT("field '%s' contains an empty value"), Field));
			}
			OutValues.Add(MoveTemp(StringValue));
		}
		return true;
	}

	bool IsStableSnakeCaseId(const FString& Value)
	{
		if (Value.IsEmpty() || Value[0] < TEXT('a') || Value[0] > TEXT('z'))
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if ((Character < TEXT('a') || Character > TEXT('z'))
				&& (Character < TEXT('0') || Character > TEXT('9'))
				&& Character != TEXT('_'))
			{
				return false;
			}
		}
		return true;
	}

	bool ParseEpistemicStatus(
		const FString& Value,
		EWSEpistemicStatus& OutStatus)
	{
		if (Value == TEXT("known")) OutStatus = EWSEpistemicStatus::Known;
		else if (Value == TEXT("observed")) OutStatus = EWSEpistemicStatus::Observed;
		else if (Value == TEXT("believed")) OutStatus = EWSEpistemicStatus::Believed;
		else if (Value == TEXT("suspected")) OutStatus = EWSEpistemicStatus::Suspected;
		else if (Value == TEXT("false_belief")) OutStatus = EWSEpistemicStatus::FalseBelief;
		else if (Value == TEXT("unknown")) OutStatus = EWSEpistemicStatus::Unknown;
		else return false;
		return true;
	}

	bool ParseDisclosureLevel(
		const FString& Value,
		EWSRoleplayDisclosureLevel& OutLevel)
	{
		if (Value == TEXT("hidden")) OutLevel = EWSRoleplayDisclosureLevel::Hidden;
		else if (Value == TEXT("evasive")) OutLevel = EWSRoleplayDisclosureLevel::Evasive;
		else if (Value == TEXT("hint")) OutLevel = EWSRoleplayDisclosureLevel::Hint;
		else if (Value == TEXT("partial")) OutLevel = EWSRoleplayDisclosureLevel::Partial;
		else if (Value == TEXT("explicit")) OutLevel = EWSRoleplayDisclosureLevel::Explicit;
		else return false;
		return true;
	}

	bool ParseSpeechFunction(
		const FString& Value,
		EWSRoleplaySpeechFunction& OutFunction)
	{
		if (Value == TEXT("answer")) OutFunction = EWSRoleplaySpeechFunction::Answer;
		else if (Value == TEXT("answer_with_uncertainty")) OutFunction = EWSRoleplaySpeechFunction::AnswerWithUncertainty;
		else if (Value == TEXT("clarify")) OutFunction = EWSRoleplaySpeechFunction::Clarify;
		else if (Value == TEXT("deflect")) OutFunction = EWSRoleplaySpeechFunction::Deflect;
		else if (Value == TEXT("refuse")) OutFunction = EWSRoleplaySpeechFunction::Refuse;
		else if (Value == TEXT("reassure")) OutFunction = EWSRoleplaySpeechFunction::Reassure;
		else if (Value == TEXT("challenge")) OutFunction = EWSRoleplaySpeechFunction::Challenge;
		else if (Value == TEXT("acknowledge")) OutFunction = EWSRoleplaySpeechFunction::Acknowledge;
		else if (Value == TEXT("conditional_cooperation")) OutFunction = EWSRoleplaySpeechFunction::ConditionalCooperation;
		else if (Value == TEXT("suggest_action")) OutFunction = EWSRoleplaySpeechFunction::SuggestAction;
		else if (Value == TEXT("evade")) OutFunction = EWSRoleplaySpeechFunction::Evade;
		else if (Value == TEXT("suggest")) OutFunction = EWSRoleplaySpeechFunction::Suggest;
		else if (Value == TEXT("conditional_offer")) OutFunction = EWSRoleplaySpeechFunction::ConditionalOffer;
		else if (Value == TEXT("crisis_response")) OutFunction = EWSRoleplaySpeechFunction::CrisisResponse;
		else return false;
		return true;
	}

	bool ParseProposalType(
		const FString& Value,
		EWSRoleplayProposalType& OutType)
	{
		if (Value == TEXT("conditional_cooperation")) OutType = EWSRoleplayProposalType::ConditionalCooperation;
		else if (Value == TEXT("suggest_action")) OutType = EWSRoleplayProposalType::SuggestAction;
		else if (Value == TEXT("refuse_action")) OutType = EWSRoleplayProposalType::RefuseAction;
		else return false;
		return true;
	}

	bool IsValidAvailability(const FString& Predicate)
	{
		static const TSet<FString> FixedPredicates{
			TEXT("always"),
			TEXT("gu_heng_diagnosed"),
			TEXT("ye_diagnosis_disclosable"),
			TEXT("gu_heng_treated"),
			TEXT("cabinet_inspected"),
			TEXT("relay_compatibility_known"),
			TEXT("heat_pack_revealed"),
			TEXT("heating_locked"),
			TEXT("heating_unlocked"),
			TEXT("ye_heat_pack_disclosable"),
			TEXT("gu_relay_disclosable"),
			TEXT("gu_restart_disclosable")};
		if (FixedPredicates.Contains(Predicate))
		{
			return true;
		}
		static const FString PlayerKnowsPrefix(TEXT("player_knows:"));
		static const FString PlayerMissingPrefix(TEXT("player_missing:"));
		return (Predicate.StartsWith(PlayerKnowsPrefix)
				&& Predicate.Len() > PlayerKnowsPrefix.Len())
			|| (Predicate.StartsWith(PlayerMissingPrefix)
				&& Predicate.Len() > PlayerMissingPrefix.Len());
	}

	bool LoadJsonObject(
		const FString& FilePath,
		TSharedPtr<FJsonObject>& OutRoot,
		FString& OutError)
	{
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
		{
			return Fail(OutError, FilePath, TEXT("file cannot be read"));
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
		{
			return Fail(OutError, FilePath, TEXT("invalid JSON object"));
		}
		return true;
	}

	bool ValidateSchemaVersion(
		const TSharedPtr<FJsonObject>& Root,
		const FString& Context,
		FString& OutError)
	{
		int32 SchemaVersion = 0;
		return ReadInteger(
			Root,
			TEXT("schema_version"),
			SchemaVersion,
			Context,
			OutError)
			&& (SchemaVersion == 1
				|| Fail(OutError, Context, TEXT("schema_version must be 1")));
	}

	bool ParseProfile(
		const TSharedPtr<FJsonObject>& Object,
		FWSRoleplayProfile& OutProfile,
		const FString& Context,
		FString& OutError)
	{
		if (!HasExactFields(
			Object,
			{TEXT("id"), TEXT("display_name"), TEXT("identity"),
				TEXT("personality"), TEXT("current_goals"), TEXT("fears"),
				TEXT("speaking_style")},
			Context,
			OutError))
		{
			return false;
		}

		FString Id;
		if (!ReadString(Object, TEXT("id"), Id, Context, OutError)
			|| !IsStableSnakeCaseId(Id))
		{
			if (OutError.IsEmpty())
			{
				Fail(OutError, Context, TEXT("profile id must be stable snake_case"));
			}
			return false;
		}
		OutProfile.Id = FName(*Id);
		return ReadString(Object, TEXT("display_name"), OutProfile.DisplayName, Context, OutError)
			&& ReadString(Object, TEXT("identity"), OutProfile.Identity, Context, OutError)
			&& ReadStringArray(Object, TEXT("personality"), OutProfile.Personality, Context, OutError)
			&& ReadStringArray(Object, TEXT("current_goals"), OutProfile.CurrentGoals, Context, OutError)
			&& ReadStringArray(Object, TEXT("fears"), OutProfile.Fears, Context, OutError)
			&& ReadStringArray(Object, TEXT("speaking_style"), OutProfile.SpeakingStyle, Context, OutError);
	}

	bool ParseKnowledgeItem(
		const TSharedPtr<FJsonObject>& Object,
		FWSRoleplayKnowledgeItem& OutItem,
		const FString& Context,
		FString& OutError)
	{
		if (!HasExactFields(
			Object,
			{TEXT("knowledge_id"), TEXT("owner"), TEXT("subject"),
				TEXT("category"), TEXT("content"), TEXT("epistemic_status"),
				TEXT("confidence"), TEXT("topic_tags"), TEXT("max_disclosure"),
				TEXT("salience"), TEXT("public"), TEXT("game_fact_id"),
				TEXT("creates_game_fact"), TEXT("availability"),
				TEXT("secret_family")},
			Context,
			OutError))
		{
			return false;
		}

		FString KnowledgeId;
		FString Owner;
		FString Subject;
		FString Category;
		FString EpistemicStatus;
		FString Disclosure;
		FString GameFactId;
		FString SecretFamily;
		double Confidence = 0.0;
		double Salience = 0.0;
		TArray<FString> TopicTags;
		if (!ReadString(Object, TEXT("knowledge_id"), KnowledgeId, Context, OutError)
			|| !ReadString(Object, TEXT("owner"), Owner, Context, OutError)
			|| !ReadString(Object, TEXT("subject"), Subject, Context, OutError)
			|| !ReadString(Object, TEXT("category"), Category, Context, OutError)
			|| !ReadString(Object, TEXT("content"), OutItem.RoleplayContent, Context, OutError)
			|| !ReadString(Object, TEXT("epistemic_status"), EpistemicStatus, Context, OutError)
			|| !ReadNumber(Object, TEXT("confidence"), Confidence, Context, OutError)
			|| !ReadStringArray(Object, TEXT("topic_tags"), TopicTags, Context, OutError)
			|| !ReadString(Object, TEXT("max_disclosure"), Disclosure, Context, OutError)
			|| !ReadNumber(Object, TEXT("salience"), Salience, Context, OutError)
			|| !ReadBool(Object, TEXT("public"), OutItem.bPublic, Context, OutError)
			|| !ReadString(Object, TEXT("game_fact_id"), GameFactId, Context, OutError, true)
			|| !ReadBool(Object, TEXT("creates_game_fact"), OutItem.bCreatesGameFact, Context, OutError)
			|| !ReadStringArray(Object, TEXT("availability"), OutItem.Availability, Context, OutError)
			|| !ReadString(Object, TEXT("secret_family"), SecretFamily, Context, OutError, true))
		{
			return false;
		}
		if (!IsStableSnakeCaseId(Owner) || !IsStableSnakeCaseId(Subject))
		{
			return Fail(OutError, Context, TEXT("owner and subject must be stable snake_case"));
		}
		if (!ParseEpistemicStatus(EpistemicStatus, OutItem.EpistemicStatus))
		{
			return Fail(OutError, Context, TEXT("invalid epistemic_status"));
		}
		if (!ParseDisclosureLevel(Disclosure, OutItem.MaxDisclosure))
		{
			return Fail(OutError, Context, TEXT("invalid max_disclosure"));
		}
		if (Confidence < 0.0 || Confidence > 1.0 || Salience < 0.0)
		{
			return Fail(OutError, Context, TEXT("confidence or salience is out of range"));
		}
		for (const FString& Predicate : OutItem.Availability)
		{
			if (!IsValidAvailability(Predicate))
			{
				return Fail(
					OutError,
					Context,
					FString::Printf(TEXT("unknown availability predicate '%s'"), *Predicate));
			}
		}
		if (OutItem.bCreatesGameFact && GameFactId.IsEmpty())
		{
			return Fail(OutError, Context, TEXT("creates_game_fact requires game_fact_id"));
		}

		OutItem.KnowledgeId = FName(*KnowledgeId);
		OutItem.Owner = FName(*Owner);
		OutItem.SubjectId = FName(*Subject);
		OutItem.CategoryId = FName(*Category);
		OutItem.Confidence = static_cast<float>(Confidence);
		OutItem.Salience = static_cast<float>(Salience);
		OutItem.GameFactId = GameFactId.IsEmpty() ? NAME_None : FName(*GameFactId);
		OutItem.SecretFamily = SecretFamily.IsEmpty() ? NAME_None : FName(*SecretFamily);
		for (const FString& TopicTag : TopicTags)
		{
			OutItem.TopicTags.Add(FName(*TopicTag));
		}
		return true;
	}

	bool ParseKnowledgeArray(
		const TSharedPtr<FJsonObject>& Root,
		const FString& Context,
		FParsedRoleplayRepository& OutParsed,
		FString& OutError,
		const FName RequiredOwner = NAME_None)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Root->TryGetArrayField(TEXT("knowledge"), Values)
			|| Values == nullptr
			|| Values->IsEmpty())
		{
			return Fail(OutError, Context, TEXT("knowledge must be a non-empty array"));
		}
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> ItemObject = (*Values)[Index].IsValid()
				? (*Values)[Index]->AsObject()
				: nullptr;
			const FString ItemContext = FString::Printf(
				TEXT("%s knowledge[%d]"), *Context, Index);
			FWSRoleplayKnowledgeItem Item;
			if (!ParseKnowledgeItem(ItemObject, Item, ItemContext, OutError))
			{
				return false;
			}
			if (!RequiredOwner.IsNone() && Item.Owner != RequiredOwner)
			{
				return Fail(
					OutError,
					ItemContext,
					FString::Printf(
						TEXT("owner must be '%s'"),
						*RequiredOwner.ToString()));
			}
			if (OutParsed.KnowledgeById.Contains(Item.KnowledgeId))
			{
				return Fail(OutError, ItemContext, TEXT("duplicate knowledge_id"));
			}
			OutParsed.KnowledgeById.Add(Item.KnowledgeId, MoveTemp(Item));
		}
		return true;
	}

	bool ParseKnowledgeFile(
		const FString& FilePath,
		FParsedRoleplayRepository& OutParsed,
		FString& OutError,
		const bool bHasProfile,
		const FName RequiredOwner = NAME_None)
	{
		TSharedPtr<FJsonObject> Root;
		if (!LoadJsonObject(FilePath, Root, OutError)
			|| !HasExactFields(
				Root,
				bHasProfile
					? std::initializer_list<const TCHAR*>{TEXT("schema_version"), TEXT("profile"), TEXT("knowledge")}
					: std::initializer_list<const TCHAR*>{TEXT("schema_version"), TEXT("knowledge")},
				FilePath,
				OutError)
			|| !ValidateSchemaVersion(Root, FilePath, OutError))
		{
			return false;
		}

		FName ProfileOwner = RequiredOwner;
		if (bHasProfile)
		{
			const TSharedPtr<FJsonObject>* ProfileObject = nullptr;
			if (!Root->TryGetObjectField(TEXT("profile"), ProfileObject)
				|| ProfileObject == nullptr)
			{
				return Fail(OutError, FilePath, TEXT("profile must be an object"));
			}
			FWSRoleplayProfile Profile;
			if (!ParseProfile(*ProfileObject, Profile, FilePath + TEXT(" profile"), OutError))
			{
				return false;
			}
			if (!RequiredOwner.IsNone() && Profile.Id != RequiredOwner)
			{
				return Fail(OutError, FilePath, TEXT("profile id does not match file"));
			}
			if (OutParsed.Profiles.Contains(Profile.Id))
			{
				return Fail(OutError, FilePath, TEXT("duplicate profile id"));
			}
			ProfileOwner = Profile.Id;
			OutParsed.Profiles.Add(Profile.Id, MoveTemp(Profile));
		}
		return ParseKnowledgeArray(
			Root,
			FilePath,
			OutParsed,
			OutError,
			ProfileOwner);
	}

	bool ParsePolicyFile(
		const FString& FilePath,
		FParsedRoleplayRepository& OutParsed,
		FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		if (!LoadJsonObject(FilePath, Root, OutError)
			|| !HasExactFields(
				Root,
				{TEXT("schema_version"), TEXT("top_k"), TEXT("max_sentences"),
					TEXT("max_characters"), TEXT("max_output_tokens"), TEXT("temperature"),
					TEXT("allowed_speech_functions"), TEXT("allowed_proposal_types")},
				FilePath,
				OutError)
			|| !ValidateSchemaVersion(Root, FilePath, OutError))
		{
			return false;
		}

		double Temperature = 0.0;
		TArray<FString> SpeechFunctions;
		TArray<FString> ProposalTypes;
		if (!ReadInteger(Root, TEXT("top_k"), OutParsed.Policy.TopK, FilePath, OutError)
			|| !ReadInteger(Root, TEXT("max_sentences"), OutParsed.Policy.MaxSentences, FilePath, OutError)
			|| !ReadInteger(Root, TEXT("max_characters"), OutParsed.Policy.MaxCharacters, FilePath, OutError)
			|| !ReadInteger(Root, TEXT("max_output_tokens"), OutParsed.Policy.MaxOutputTokens, FilePath, OutError)
			|| !ReadNumber(Root, TEXT("temperature"), Temperature, FilePath, OutError)
			|| !ReadStringArray(Root, TEXT("allowed_speech_functions"), SpeechFunctions, FilePath, OutError)
			|| !ReadStringArray(Root, TEXT("allowed_proposal_types"), ProposalTypes, FilePath, OutError))
		{
			return false;
		}
		if (OutParsed.Policy.TopK < 8 || OutParsed.Policy.TopK > 12
			|| OutParsed.Policy.MaxSentences <= 0
			|| OutParsed.Policy.MaxCharacters <= 0
			|| OutParsed.Policy.MaxOutputTokens <= 0
			|| Temperature < 0.0 || Temperature > 2.0)
		{
			return Fail(OutError, FilePath, TEXT("policy value is out of range"));
		}
		OutParsed.Policy.Temperature = static_cast<float>(Temperature);
		for (const FString& Value : SpeechFunctions)
		{
			EWSRoleplaySpeechFunction Function;
			if (!ParseSpeechFunction(Value, Function))
			{
				return Fail(OutError, FilePath, TEXT("invalid allowed_speech_functions value"));
			}
			OutParsed.Policy.AllowedSpeechFunctions.AddUnique(Function);
		}
		for (const FString& Value : ProposalTypes)
		{
			EWSRoleplayProposalType Type;
			if (!ParseProposalType(Value, Type))
			{
				return Fail(OutError, FilePath, TEXT("invalid allowed_proposal_types value"));
			}
			OutParsed.Policy.AllowedProposalTypes.AddUnique(Type);
		}
		return true;
	}

	bool ParseFallbackFile(
		const FString& FilePath,
		FParsedRoleplayRepository& OutParsed,
		FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		if (!LoadJsonObject(FilePath, Root, OutError)
			|| !HasExactFields(
				Root,
				{TEXT("schema_version"), TEXT("fallbacks")},
				FilePath,
				OutError)
			|| !ValidateSchemaVersion(Root, FilePath, OutError))
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Root->TryGetArrayField(TEXT("fallbacks"), Values)
			|| Values == nullptr
			|| Values->IsEmpty())
		{
			return Fail(OutError, FilePath, TEXT("fallbacks must be a non-empty array"));
		}
		TSet<FName> FallbackIds;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> Object = (*Values)[Index].IsValid()
				? (*Values)[Index]->AsObject()
				: nullptr;
			const FString Context = FString::Printf(
				TEXT("%s fallbacks[%d]"), *FilePath, Index);
			if (!HasExactFields(
				Object,
				{TEXT("fallback_id"), TEXT("speaker"), TEXT("target"),
					TEXT("topic_tags"), TEXT("speech_function"), TEXT("line"),
					TEXT("referenced_knowledge_ids"), TEXT("availability")},
				Context,
				OutError))
			{
				return false;
			}

			FString Id;
			FString Speaker;
			FString Target;
			FString SpeechFunction;
			TArray<FString> TopicTags;
			TArray<FString> ReferencedIds;
			FWSRoleplayFallback Fallback;
			if (!ReadString(Object, TEXT("fallback_id"), Id, Context, OutError)
				|| !ReadString(Object, TEXT("speaker"), Speaker, Context, OutError)
				|| !ReadString(Object, TEXT("target"), Target, Context, OutError)
				|| !ReadStringArray(Object, TEXT("topic_tags"), TopicTags, Context, OutError)
				|| !ReadString(Object, TEXT("speech_function"), SpeechFunction, Context, OutError)
				|| !ReadString(Object, TEXT("line"), Fallback.Line, Context, OutError)
				|| !ReadStringArray(Object, TEXT("referenced_knowledge_ids"), ReferencedIds, Context, OutError, true)
				|| !ReadStringArray(Object, TEXT("availability"), Fallback.Availability, Context, OutError))
			{
				return false;
			}
			if ((!IsStableSnakeCaseId(Speaker) && Speaker != TEXT("any"))
				|| (!IsStableSnakeCaseId(Target) && Target != TEXT("any")))
			{
				return Fail(OutError, Context, TEXT("speaker and target must be stable ids or 'any'"));
			}
			if (!ParseSpeechFunction(SpeechFunction, Fallback.SpeechFunction))
			{
				return Fail(OutError, Context, TEXT("invalid speech_function"));
			}
			for (const FString& Predicate : Fallback.Availability)
			{
				if (!IsValidAvailability(Predicate))
				{
					return Fail(OutError, Context, TEXT("invalid availability predicate"));
				}
			}

			Fallback.FallbackId = FName(*Id);
			Fallback.SpeakerId = FName(*Speaker);
			Fallback.TargetSubjectId = FName(*Target);
			if (FallbackIds.Contains(Fallback.FallbackId))
			{
				return Fail(OutError, Context, TEXT("duplicate fallback_id"));
			}
			FallbackIds.Add(Fallback.FallbackId);
			for (const FString& TopicTag : TopicTags)
			{
				Fallback.TopicTags.Add(FName(*TopicTag));
			}
			for (const FString& ReferencedId : ReferencedIds)
			{
				const FName KnowledgeId(*ReferencedId);
				if (!OutParsed.KnowledgeById.Contains(KnowledgeId))
				{
					return Fail(
						OutError,
						Context,
						FString::Printf(
							TEXT("unknown referenced knowledge '%s'"),
							*ReferencedId));
				}
				Fallback.ReferencedKnowledgeIds.Add(KnowledgeId);
			}
			OutParsed.Fallbacks.Add(MoveTemp(Fallback));
		}
		return true;
	}

	bool ValidateAvailabilityFactReferences(
		const FParsedRoleplayRepository& Parsed,
		const FString& Context,
		FString& OutError)
	{
		TSet<FName> DeclaredFactIds;
		for (const TPair<FName, FWSRoleplayKnowledgeItem>& Pair :
			Parsed.KnowledgeById)
		{
			if (!Pair.Value.GameFactId.IsNone())
			{
				DeclaredFactIds.Add(Pair.Value.GameFactId);
			}
		}
		const auto ValidatePredicates =
			[&DeclaredFactIds, &OutError](
				const TArray<FString>& Predicates,
				const FString& ItemContext)
			{
				for (const FString& Predicate : Predicates)
				{
					int32 Separator = INDEX_NONE;
					if (!Predicate.FindChar(TEXT(':'), Separator))
					{
						continue;
					}
					const FName FactId(*Predicate.RightChop(Separator + 1));
					if (!DeclaredFactIds.Contains(FactId))
					{
						return Fail(
							OutError,
							ItemContext,
							FString::Printf(
								TEXT("availability references undeclared fact '%s'"),
								*FactId.ToString()));
					}
				}
				return true;
			};

		for (const TPair<FName, FWSRoleplayKnowledgeItem>& Pair :
			Parsed.KnowledgeById)
		{
			if (!ValidatePredicates(
					Pair.Value.Availability,
					Context + TEXT(" knowledge ") + Pair.Key.ToString()))
			{
				return false;
			}
		}
		for (const FWSRoleplayFallback& Fallback : Parsed.Fallbacks)
		{
			if (!ValidatePredicates(
					Fallback.Availability,
					Context + TEXT(" fallback ") + Fallback.FallbackId.ToString()))
			{
				return false;
			}
		}
		return true;
	}
}

bool UWSRoleplayKnowledgeRepository::LoadDefault(FString& OutError)
{
	return LoadFromDirectory(
		FPaths::ProjectContentDir() / TEXT("Dialogue/v1.4"),
		OutError);
}

bool UWSRoleplayKnowledgeRepository::LoadFromDirectory(
	const FString& Directory,
	FString& OutError)
{
	ResetRepository();
	OutError.Reset();
	const FString NormalizedDirectory = FPaths::ConvertRelativePathToFull(Directory);
	FParsedRoleplayRepository Parsed;
	if (!ParseKnowledgeFile(
			NormalizedDirectory / TEXT("WorldKnowledge.json"),
			Parsed,
			OutError,
			false,
			TEXT("world"))
		|| !ParseKnowledgeFile(
			NormalizedDirectory / TEXT("NPC_GuHeng.json"),
			Parsed,
			OutError,
			true,
			TEXT("gu_heng"))
		|| !ParseKnowledgeFile(
			NormalizedDirectory / TEXT("NPC_YeCheng.json"),
			Parsed,
			OutError,
			true,
			TEXT("ye_cheng"))
		|| !ParseKnowledgeFile(
			NormalizedDirectory / TEXT("Relationship_GuHeng_YeCheng.json"),
			Parsed,
			OutError,
			false)
		|| !ParsePolicyFile(
			NormalizedDirectory / TEXT("DialoguePolicy.json"),
			Parsed,
			OutError)
		|| !ParseFallbackFile(
			NormalizedDirectory / TEXT("SafeFallbacks.json"),
			Parsed,
			OutError))
	{
		return false;
	}
	if (!ValidateAvailabilityFactReferences(
		Parsed,
		NormalizedDirectory,
		OutError))
	{
		return false;
	}

	if (!Parsed.Profiles.Contains(TEXT("gu_heng"))
		|| !Parsed.Profiles.Contains(TEXT("ye_cheng")))
	{
		return Fail(OutError, NormalizedDirectory, TEXT("required NPC profiles are missing"));
	}

	Profiles = MoveTemp(Parsed.Profiles);
	KnowledgeById = MoveTemp(Parsed.KnowledgeById);
	Policy = MoveTemp(Parsed.Policy);
	Fallbacks = MoveTemp(Parsed.Fallbacks);
	bAvailable = true;
	return true;
}

bool UWSRoleplayKnowledgeRepository::GetProfile(
	const FName ProfileId,
	FWSRoleplayProfile& OutProfile) const
{
	if (!bAvailable)
	{
		return false;
	}
	const FWSRoleplayProfile* Profile = Profiles.Find(ProfileId);
	if (Profile == nullptr)
	{
		return false;
	}
	OutProfile = *Profile;
	return true;
}

bool UWSRoleplayKnowledgeRepository::GetKnowledgeItem(
	const FName KnowledgeId,
	FWSRoleplayKnowledgeItem& OutKnowledge) const
{
	if (!bAvailable)
	{
		return false;
	}
	const FWSRoleplayKnowledgeItem* Knowledge = KnowledgeById.Find(KnowledgeId);
	if (Knowledge == nullptr)
	{
		return false;
	}
	OutKnowledge = *Knowledge;
	return true;
}

TArray<FWSRoleplayKnowledgeItem>
UWSRoleplayKnowledgeRepository::GetGlobalKnowledge() const
{
	TArray<FWSRoleplayKnowledgeItem> Result;
	if (!bAvailable)
	{
		return Result;
	}
	for (const TPair<FName, FWSRoleplayKnowledgeItem>& Pair : KnowledgeById)
	{
		if (Pair.Value.Owner == TEXT("world")
			|| Pair.Value.Owner == TEXT("station"))
		{
			Result.Add(Pair.Value);
		}
	}
	Result.Sort([](
		const FWSRoleplayKnowledgeItem& Left,
		const FWSRoleplayKnowledgeItem& Right)
	{
		return Left.KnowledgeId.LexicalLess(Right.KnowledgeId);
	});
	return Result;
}

TArray<FWSRoleplayKnowledgeItem>
UWSRoleplayKnowledgeRepository::GetKnowledgeForOwner(const FName Owner) const
{
	TArray<FWSRoleplayKnowledgeItem> Result;
	if (!bAvailable)
	{
		return Result;
	}
	for (const TPair<FName, FWSRoleplayKnowledgeItem>& Pair : KnowledgeById)
	{
		if (Pair.Value.Owner == Owner)
		{
			Result.Add(Pair.Value);
		}
	}
	Result.Sort([](
		const FWSRoleplayKnowledgeItem& Left,
		const FWSRoleplayKnowledgeItem& Right)
	{
		return Left.KnowledgeId.LexicalLess(Right.KnowledgeId);
	});
	return Result;
}

void UWSRoleplayKnowledgeRepository::ResetRepository()
{
	bAvailable = false;
	Profiles.Reset();
	KnowledgeById.Reset();
	Policy = FWSRoleplayPolicy();
	Fallbacks.Reset();
}
