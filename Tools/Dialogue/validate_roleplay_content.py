from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Iterable


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONTENT_DIR = PROJECT_ROOT / "WhiteoutStation" / "Content" / "Dialogue" / "v1.4"

WORLD_FILE = "WorldKnowledge.json"
NPC_FILES = {
    "NPC_GuHeng.json": "gu_heng",
    "NPC_YeCheng.json": "ye_cheng",
}
RELATIONSHIP_FILE = "Relationship_GuHeng_YeCheng.json"
POLICY_FILE = "DialoguePolicy.json"
FALLBACK_FILE = "SafeFallbacks.json"
REQUIRED_FILES = frozenset(
    {
        WORLD_FILE,
        *NPC_FILES,
        RELATIONSHIP_FILE,
        POLICY_FILE,
        FALLBACK_FILE,
    }
)

KNOWLEDGE_KEYS = frozenset(
    {
        "knowledge_id",
        "owner",
        "subject",
        "category",
        "content",
        "epistemic_status",
        "confidence",
        "topic_tags",
        "max_disclosure",
        "salience",
        "public",
        "game_fact_id",
        "creates_game_fact",
        "availability",
        "secret_family",
    }
)
PROFILE_KEYS = frozenset(
    {
        "id",
        "display_name",
        "identity",
        "personality",
        "current_goals",
        "fears",
        "speaking_style",
    }
)
FALLBACK_KEYS = frozenset(
    {
        "fallback_id",
        "speaker",
        "target",
        "topic_tags",
        "speech_function",
        "line",
        "referenced_knowledge_ids",
        "availability",
    }
)
POLICY_KEYS = frozenset(
    {
        "schema_version",
        "top_k",
        "max_sentences",
        "max_characters",
        "max_output_tokens",
        "temperature",
        "allowed_speech_functions",
        "allowed_proposal_types",
    }
)

EPISTEMIC_STATUSES = frozenset(
    {"known", "observed", "believed", "suspected", "false_belief", "unknown"}
)
DISCLOSURE_LEVELS = frozenset({"hidden", "evasive", "hint", "partial", "explicit"})
STATIC_AVAILABILITY = frozenset(
    {
        "always",
        "gu_heng_diagnosed",
        "ye_diagnosis_disclosable",
        "gu_relay_disclosable",
        "gu_heng_treated",
        "cabinet_inspected",
        "relay_compatibility_known",
        "heat_pack_revealed",
        "heating_locked",
        "heating_unlocked",
        "ye_heat_pack_disclosable",
        "gu_restart_disclosable",
    }
)
EXPECTED_SPEECH_FUNCTIONS = frozenset(
    {
        "answer",
        "answer_with_uncertainty",
        "clarify",
        "evade",
        "refuse",
        "suggest",
        "conditional_offer",
        "acknowledge",
        "crisis_response",
    }
)
EXPECTED_PROPOSAL_TYPES = frozenset(
    {"conditional_cooperation", "suggest_action", "refuse_action"}
)
VALID_OWNERS = frozenset({"world", "gu_heng", "ye_cheng"})
VALID_SUBJECTS = frozenset(
    {
        "antenna",
        "communications",
        "control_cabinet",
        "crew",
        "equipment",
        "generator",
        "generator_log",
        "gu_heng",
        "heating",
        "kitchen",
        "kitchen_heater",
        "medical_resources",
        "medical_room",
        "outdoor_equipment",
        "player",
        "relay",
        "relationship_gu_heng_ye_cheng",
        "repair_room",
        "rescue",
        "station",
        "unknown",
        "weather",
        "ye_cheng",
    }
)
REQUIRED_FALLBACK_TOPICS = frozenset(
    {"character_goal", "equipment", "overall_status", "ambiguous"}
)

SNAKE_CASE_RE = re.compile(r"^[a-z][a-z0-9]*(?:_[a-z0-9]+)*$")
STABLE_ID_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
FACT_ID_RE = re.compile(r"^FACT_[A-Z0-9]+(?:_[A-Z0-9]+)*$")
PLAYER_AVAILABILITY_RE = re.compile(
    r"^player_(?:knows|missing):(FACT_[A-Z0-9]+(?:_[A-Z0-9]+)*)$"
)
PLACEHOLDER_RE = re.compile(r"\{([a-z][a-z0-9_]*)\}")
ALLOWED_FALLBACK_PLACEHOLDERS = frozenset({"heating_state", "generator_state"})

BACKSTAGE_PATTERNS = (
    (re.compile(r"(?<![A-Za-z0-9_])AP(?![A-Za-z0-9_])", re.IGNORECASE), "AP"),
    (re.compile(r"(?<![A-Za-z0-9_])stamina(?![A-Za-z0-9_])", re.IGNORECASE), "Stamina"),
    (re.compile(r"(?<![A-Za-z0-9_])prompt(?![A-Za-z0-9_])", re.IGNORECASE), "Prompt"),
    (re.compile(r"(?<![A-Za-z0-9_])token(?:s)?(?![A-Za-z0-9_])", re.IGNORECASE), "Token"),
    (re.compile(r"(?<![A-Za-z0-9_])LLM(?:s)?(?![A-Za-z0-9_])", re.IGNORECASE), "LLM"),
    (re.compile(r"(?<![A-Za-z0-9_])JSON(?![A-Za-z0-9_])", re.IGNORECASE), "JSON"),
    (re.compile(r"(?<![A-Za-z0-9_])API(?![A-Za-z0-9_])", re.IGNORECASE), "API"),
    (re.compile(r"(?<![A-Za-z0-9_])(?:knowledge|game_fact|action|fact)_id(?![A-Za-z0-9_])", re.IGNORECASE), "internal ID"),
    (re.compile(r"(?<![A-Za-z0-9_])FACT_[A-Z0-9_]+(?![A-Za-z0-9_])", re.IGNORECASE), "fact ID"),
    (re.compile(r"\binternal[ _-]*id\b", re.IGNORECASE), "internal ID"),
    (re.compile(r"\brule[ _-]*engine\b", re.IGNORECASE), "rule engine"),
    (re.compile(r"内部\s*(?:ID|标识|编号)", re.IGNORECASE), "内部 ID"),
    (re.compile(r"规则引擎"), "规则引擎"),
    (re.compile(r"(?:行动点|体力点|数值阈值|内部规则|后台规则|枚举值?)"), "后台规则术语"),
    (re.compile(r"(?:语言模型|大模型|模型输出|模型回复)"), "模型术语"),
    (re.compile(r"(?:系统|后台)(?:字段|变量|枚举|阈值|数值)"), "后台字段"),
    (re.compile(r"成功率"), "成功率"),
    (re.compile(r"提示词"), "提示词"),
    (re.compile(r"作为\s*(?:AI|人工智能)", re.IGNORECASE), "AI self-reference"),
)
FIXED_DIALOGUE_MARKERS = (
    re.compile(r"(?:玩家|顾衡|叶澄)\s*[:：]"),
    re.compile(r"[?？]\s*$"),
)


class DuplicateJsonKey(ValueError):
    pass


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateJsonKey(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def _reject_nonfinite(value: str) -> None:
    raise ValueError(f"non-finite number {value!r}")


def _load_json(path: Path, errors: list[str]) -> Any | None:
    try:
        raw = path.read_bytes()
    except OSError as exc:
        errors.append(f"{path.name}: cannot read file: {exc}")
        return None

    if raw.startswith(b"\xef\xbb\xbf"):
        errors.append(f"{path.name}: must be UTF-8 without a byte-order mark")
        return None

    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        errors.append(f"{path.name}: is not valid UTF-8: {exc}")
        return None

    try:
        return json.loads(
            text,
            object_pairs_hook=_reject_duplicate_keys,
            parse_constant=_reject_nonfinite,
        )
    except (json.JSONDecodeError, DuplicateJsonKey, ValueError) as exc:
        errors.append(f"{path.name}: invalid JSON: {exc}")
        return None


def _expect_object(value: Any, location: str, errors: list[str]) -> bool:
    if not isinstance(value, dict):
        errors.append(f"{location}: expected object")
        return False
    return True


def _expect_exact_keys(
    value: dict[str, Any], expected: frozenset[str], location: str, errors: list[str]
) -> None:
    actual = set(value)
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    if missing:
        errors.append(f"{location}: missing fields: {', '.join(missing)}")
    if extra:
        errors.append(f"{location}: unexpected fields: {', '.join(extra)}")


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _is_plain_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _nonempty_string(value: Any, location: str, errors: list[str]) -> bool:
    if not isinstance(value, str) or not value.strip():
        errors.append(f"{location}: expected a non-empty string")
        return False
    return True


def _string_array(
    value: Any,
    location: str,
    errors: list[str],
    *,
    allow_empty: bool = False,
) -> list[str] | None:
    if not isinstance(value, list) or (not value and not allow_empty):
        requirement = "a string array" if allow_empty else "a non-empty string array"
        errors.append(f"{location}: expected {requirement}")
        return None
    if any(not isinstance(item, str) or not item.strip() for item in value):
        errors.append(f"{location}: every item must be a non-empty string")
        return None
    if len({item.casefold() for item in value}) != len(value):
        errors.append(f"{location}: duplicate values are not allowed")
    return value


def _validate_snake_case(value: Any, location: str, errors: list[str]) -> bool:
    if not _nonempty_string(value, location, errors):
        return False
    if not SNAKE_CASE_RE.fullmatch(value):
        errors.append(f"{location}: must be a stable snake_case identifier")
        return False
    return True


def _validate_availability(value: Any, location: str, errors: list[str]) -> None:
    tokens = _string_array(value, location, errors)
    if tokens is None:
        return

    for token in tokens:
        if token in STATIC_AVAILABILITY or PLAYER_AVAILABILITY_RE.fullmatch(token):
            continue
        errors.append(f"{location}: unsupported availability token {token!r}")

    if "always" in tokens and len(tokens) != 1:
        errors.append(f"{location}: 'always' cannot be combined with another predicate")

    known_facts = {
        match.group(1)
        for token in tokens
        if token.startswith("player_knows:")
        and (match := PLAYER_AVAILABILITY_RE.fullmatch(token))
    }
    missing_facts = {
        match.group(1)
        for token in tokens
        if token.startswith("player_missing:")
        and (match := PLAYER_AVAILABILITY_RE.fullmatch(token))
    }
    contradictory = sorted(known_facts & missing_facts)
    if contradictory:
        errors.append(
            f"{location}: contradictory player knowledge predicates for {', '.join(contradictory)}"
        )


def _scan_backstage_language(text: str, location: str, errors: list[str]) -> None:
    for pattern, label in BACKSTAGE_PATTERNS:
        if pattern.search(text):
            errors.append(f"{location}: contains forbidden backstage language ({label})")


def _validate_profile(
    profile: Any, expected_id: str, location: str, errors: list[str]
) -> None:
    if not _expect_object(profile, location, errors):
        return
    _expect_exact_keys(profile, PROFILE_KEYS, location, errors)

    profile_id = profile.get("id")
    if _validate_snake_case(profile_id, f"{location}.id", errors) and profile_id != expected_id:
        errors.append(f"{location}.id: expected {expected_id!r}")

    _nonempty_string(profile.get("display_name"), f"{location}.display_name", errors)
    _nonempty_string(profile.get("identity"), f"{location}.identity", errors)
    for field in ("personality", "current_goals", "fears", "speaking_style"):
        _string_array(profile.get(field), f"{location}.{field}", errors)


def _validate_knowledge_item(
    item: Any,
    location: str,
    errors: list[str],
    id_registry: dict[str, str],
    *,
    expected_owner: str | None,
    world_item: bool,
) -> None:
    if not _expect_object(item, location, errors):
        return
    _expect_exact_keys(item, KNOWLEDGE_KEYS, location, errors)

    knowledge_id = item.get("knowledge_id")
    if _nonempty_string(knowledge_id, f"{location}.knowledge_id", errors):
        if not STABLE_ID_RE.fullmatch(knowledge_id):
            errors.append(f"{location}.knowledge_id: must contain only letters, digits, and underscores")
        folded = knowledge_id.casefold()
        previous = id_registry.get(folded)
        if previous is not None:
            errors.append(
                f"{location}.knowledge_id: duplicates {previous!r} when compared case-insensitively"
            )
        else:
            id_registry[folded] = knowledge_id

    owner = item.get("owner")
    if _validate_snake_case(owner, f"{location}.owner", errors):
        if owner not in VALID_OWNERS:
            errors.append(f"{location}.owner: unknown owner {owner!r}")
        if expected_owner is not None and owner != expected_owner:
            errors.append(f"{location}.owner: expected {expected_owner!r}")

    subject = item.get("subject")
    if _validate_snake_case(subject, f"{location}.subject", errors) and subject not in VALID_SUBJECTS:
        errors.append(f"{location}.subject: unknown subject {subject!r}")

    category = item.get("category")
    _validate_snake_case(category, f"{location}.category", errors)

    content = item.get("content")
    if _nonempty_string(content, f"{location}.content", errors):
        if "\n" in content or "\r" in content:
            errors.append(f"{location}.content: must be a single fact or opinion, not a dialogue block")
        for marker in FIXED_DIALOGUE_MARKERS:
            if marker.search(content):
                errors.append(f"{location}.content: looks like fixed question-and-answer dialogue")
        _scan_backstage_language(content, f"{location}.content", errors)

    status = item.get("epistemic_status")
    if status not in EPISTEMIC_STATUSES:
        errors.append(
            f"{location}.epistemic_status: expected one of {', '.join(sorted(EPISTEMIC_STATUSES))}"
        )

    for field in ("confidence", "salience"):
        value = item.get(field)
        if not _is_number(value) or not 0.0 <= value <= 1.0:
            errors.append(f"{location}.{field}: expected a number in [0, 1]")

    tags = _string_array(item.get("topic_tags"), f"{location}.topic_tags", errors)
    if tags is not None:
        for index, tag in enumerate(tags):
            _validate_snake_case(tag, f"{location}.topic_tags[{index}]", errors)

    disclosure = item.get("max_disclosure")
    if disclosure not in DISCLOSURE_LEVELS:
        errors.append(
            f"{location}.max_disclosure: expected one of {', '.join(sorted(DISCLOSURE_LEVELS))}"
        )

    public = item.get("public")
    if not isinstance(public, bool):
        errors.append(f"{location}.public: expected boolean")
    elif world_item and not public:
        errors.append(f"{location}.public: world knowledge must be public")

    game_fact_id = item.get("game_fact_id")
    if not isinstance(game_fact_id, str):
        errors.append(f"{location}.game_fact_id: expected a string; use an empty string for no fact")
    elif game_fact_id and not FACT_ID_RE.fullmatch(game_fact_id):
        errors.append(f"{location}.game_fact_id: expected an uppercase FACT_ identifier")

    creates_game_fact = item.get("creates_game_fact")
    if not isinstance(creates_game_fact, bool):
        errors.append(f"{location}.creates_game_fact: expected boolean")
    elif creates_game_fact and (
        not isinstance(game_fact_id, str) or not FACT_ID_RE.fullmatch(game_fact_id)
    ):
        errors.append(
            f"{location}.creates_game_fact: true requires a non-empty FACT_ game_fact_id"
        )

    availability = item.get("availability")
    _validate_availability(availability, f"{location}.availability", errors)
    if disclosure == "hidden" and isinstance(availability, list) and "always" in availability:
        errors.append(f"{location}: hidden knowledge cannot be available 'always'")

    secret_family = item.get("secret_family")
    if not isinstance(secret_family, str):
        errors.append(f"{location}.secret_family: expected a string; use an empty string for none")
    elif secret_family and not SNAKE_CASE_RE.fullmatch(secret_family):
        errors.append(f"{location}.secret_family: expected snake_case or an empty string")


def _validate_knowledge_document(
    document: Any,
    filename: str,
    errors: list[str],
    id_registry: dict[str, str],
    *,
    expected_owner: str | None,
    world_items: bool,
) -> list[dict[str, Any]]:
    if not _expect_object(document, filename, errors):
        return []

    expected_top_keys = frozenset({"schema_version", "knowledge"})
    if filename in NPC_FILES:
        expected_top_keys = frozenset({"schema_version", "profile", "knowledge"})
    _expect_exact_keys(document, expected_top_keys, filename, errors)

    schema_version = document.get("schema_version")
    if not _is_plain_int(schema_version) or schema_version != 1:
        errors.append(f"{filename}.schema_version: expected integer 1")

    if filename in NPC_FILES:
        _validate_profile(document.get("profile"), NPC_FILES[filename], f"{filename}.profile", errors)

    knowledge = document.get("knowledge")
    if not isinstance(knowledge, list):
        errors.append(f"{filename}.knowledge: expected array")
        return []

    if filename in NPC_FILES and not 25 <= len(knowledge) <= 35:
        errors.append(f"{filename}.knowledge: expected 25-35 items, found {len(knowledge)}")
    if filename == RELATIONSHIP_FILE and not 6 <= len(knowledge) <= 10:
        errors.append(f"{filename}.knowledge: expected 6-10 items, found {len(knowledge)}")
    if filename == WORLD_FILE and not knowledge:
        errors.append(f"{filename}.knowledge: expected at least one item")

    typed_items: list[dict[str, Any]] = []
    for index, item in enumerate(knowledge):
        _validate_knowledge_item(
            item,
            f"{filename}.knowledge[{index}]",
            errors,
            id_registry,
            expected_owner=expected_owner,
            world_item=world_items,
        )
        if isinstance(item, dict):
            typed_items.append(item)
    return typed_items


def _validate_policy(document: Any, errors: list[str]) -> set[str]:
    location = POLICY_FILE
    if not _expect_object(document, location, errors):
        return set()
    _expect_exact_keys(document, POLICY_KEYS, location, errors)

    exact_integer_fields = {
        "schema_version": 1,
        "top_k": 10,
        "max_sentences": 3,
        "max_characters": 120,
        "max_output_tokens": 320,
    }
    for field, expected in exact_integer_fields.items():
        value = document.get(field)
        if not _is_plain_int(value) or value != expected:
            errors.append(f"{location}.{field}: expected integer {expected}")

    temperature = document.get("temperature")
    if not _is_number(temperature) or temperature != 0.5:
        errors.append(f"{location}.temperature: expected 0.5")

    speech = _string_array(
        document.get("allowed_speech_functions"),
        f"{location}.allowed_speech_functions",
        errors,
    )
    speech_set = set(speech or [])
    if speech_set != EXPECTED_SPEECH_FUNCTIONS:
        errors.append(
            f"{location}.allowed_speech_functions: expected exactly "
            f"{', '.join(sorted(EXPECTED_SPEECH_FUNCTIONS))}"
        )
    for index, value in enumerate(speech or []):
        _validate_snake_case(value, f"{location}.allowed_speech_functions[{index}]", errors)

    proposals = _string_array(
        document.get("allowed_proposal_types"),
        f"{location}.allowed_proposal_types",
        errors,
    )
    if set(proposals or []) != EXPECTED_PROPOSAL_TYPES:
        errors.append(
            f"{location}.allowed_proposal_types: expected exactly "
            f"{', '.join(sorted(EXPECTED_PROPOSAL_TYPES))}"
        )
    for index, value in enumerate(proposals or []):
        _validate_snake_case(value, f"{location}.allowed_proposal_types[{index}]", errors)
    return speech_set


def _validate_fallbacks(
    document: Any,
    errors: list[str],
    id_registry: dict[str, str],
    knowledge_by_id: dict[str, dict[str, Any]],
    allowed_speech_functions: set[str],
) -> int:
    location = FALLBACK_FILE
    if not _expect_object(document, location, errors):
        return 0
    _expect_exact_keys(document, frozenset({"schema_version", "fallbacks"}), location, errors)

    schema_version = document.get("schema_version")
    if not _is_plain_int(schema_version) or schema_version != 1:
        errors.append(f"{location}.schema_version: expected integer 1")

    fallbacks = document.get("fallbacks")
    if not isinstance(fallbacks, list) or not fallbacks:
        errors.append(f"{location}.fallbacks: expected a non-empty array")
        return 0

    covered_topics: set[str] = set()
    for index, fallback in enumerate(fallbacks):
        item_location = f"{location}.fallbacks[{index}]"
        if not _expect_object(fallback, item_location, errors):
            continue
        _expect_exact_keys(fallback, FALLBACK_KEYS, item_location, errors)

        fallback_id = fallback.get("fallback_id")
        if _nonempty_string(fallback_id, f"{item_location}.fallback_id", errors):
            if not STABLE_ID_RE.fullmatch(fallback_id):
                errors.append(f"{item_location}.fallback_id: invalid stable identifier")
            folded = fallback_id.casefold()
            previous = id_registry.get(folded)
            if previous is not None:
                errors.append(
                    f"{item_location}.fallback_id: duplicates {previous!r} when compared case-insensitively"
                )
            else:
                id_registry[folded] = fallback_id

        speaker = fallback.get("speaker")
        if _validate_snake_case(speaker, f"{item_location}.speaker", errors) and speaker not in NPC_FILES.values():
            errors.append(f"{item_location}.speaker: expected gu_heng or ye_cheng")

        target = fallback.get("target")
        if _validate_snake_case(target, f"{item_location}.target", errors) and target not in VALID_SUBJECTS:
            errors.append(f"{item_location}.target: unknown target {target!r}")

        tags = _string_array(fallback.get("topic_tags"), f"{item_location}.topic_tags", errors)
        if tags is not None:
            covered_topics.update(tags)
            for tag_index, tag in enumerate(tags):
                _validate_snake_case(tag, f"{item_location}.topic_tags[{tag_index}]", errors)

        speech_function = fallback.get("speech_function")
        if not _nonempty_string(
            speech_function, f"{item_location}.speech_function", errors
        ):
            pass
        elif speech_function not in allowed_speech_functions:
            errors.append(
                f"{item_location}.speech_function: {speech_function!r} is not allowed by {POLICY_FILE}"
            )

        line = fallback.get("line")
        if _nonempty_string(line, f"{item_location}.line", errors):
            if len(line) > 120:
                errors.append(f"{item_location}.line: exceeds the 120-character policy limit")
            _scan_backstage_language(line, f"{item_location}.line", errors)
            placeholders = set(PLACEHOLDER_RE.findall(line))
            unknown_placeholders = sorted(placeholders - ALLOWED_FALLBACK_PLACEHOLDERS)
            if unknown_placeholders:
                errors.append(
                    f"{item_location}.line: unsupported placeholders: {', '.join(unknown_placeholders)}"
                )
            stripped = PLACEHOLDER_RE.sub("", line)
            if "{" in stripped or "}" in stripped:
                errors.append(f"{item_location}.line: malformed placeholder braces")

        references = _string_array(
            fallback.get("referenced_knowledge_ids"),
            f"{item_location}.referenced_knowledge_ids",
            errors,
        )
        for reference in references or []:
            knowledge = knowledge_by_id.get(reference)
            if knowledge is None:
                errors.append(
                    f"{item_location}.referenced_knowledge_ids: unknown knowledge ID {reference!r}"
                )
            elif isinstance(speaker, str) and knowledge.get("owner") not in {"world", speaker}:
                errors.append(
                    f"{item_location}.referenced_knowledge_ids: {reference!r} is not owned by world or {speaker}"
                )

        _validate_availability(
            fallback.get("availability"), f"{item_location}.availability", errors
        )

    missing_topics = sorted(REQUIRED_FALLBACK_TOPICS - covered_topics)
    if missing_topics:
        errors.append(
            f"{location}: fallback coverage missing topic groups: {', '.join(missing_topics)}"
        )
    return len(fallbacks)


def validate_content(content_dir: Path | str = DEFAULT_CONTENT_DIR) -> list[str]:
    content_dir = Path(content_dir)
    errors: list[str] = []
    if not content_dir.is_dir():
        return [f"{content_dir}: content directory does not exist"]

    actual_json_files = {path.name for path in content_dir.glob("*.json")}
    missing = sorted(REQUIRED_FILES - actual_json_files)
    extra = sorted(actual_json_files - REQUIRED_FILES)
    if missing:
        errors.append(f"content directory: missing files: {', '.join(missing)}")
    if extra:
        errors.append(f"content directory: unexpected JSON files: {', '.join(extra)}")
    if missing:
        return errors

    documents = {
        filename: _load_json(content_dir / filename, errors)
        for filename in sorted(REQUIRED_FILES)
    }
    if any(document is None for document in documents.values()):
        return errors

    id_registry: dict[str, str] = {}
    all_knowledge: list[dict[str, Any]] = []
    all_knowledge.extend(
        _validate_knowledge_document(
            documents[WORLD_FILE],
            WORLD_FILE,
            errors,
            id_registry,
            expected_owner="world",
            world_items=True,
        )
    )
    for filename, owner in NPC_FILES.items():
        all_knowledge.extend(
            _validate_knowledge_document(
                documents[filename],
                filename,
                errors,
                id_registry,
                expected_owner=owner,
                world_items=False,
            )
        )
    all_knowledge.extend(
        _validate_knowledge_document(
            documents[RELATIONSHIP_FILE],
            RELATIONSHIP_FILE,
            errors,
            id_registry,
            expected_owner=None,
            world_items=False,
        )
    )

    relationship_document = documents[RELATIONSHIP_FILE]
    if isinstance(relationship_document, dict):
        relationship_items = relationship_document.get("knowledge", [])
        if isinstance(relationship_items, list):
            for index, item in enumerate(relationship_items):
                if isinstance(item, dict) and item.get("owner") not in NPC_FILES.values():
                    errors.append(
                        f"{RELATIONSHIP_FILE}.knowledge[{index}].owner: "
                        "relationship knowledge must belong to an NPC"
                    )

    knowledge_by_id = {
        item["knowledge_id"]: item
        for item in all_knowledge
        if isinstance(item.get("knowledge_id"), str)
    }
    allowed_speech_functions = _validate_policy(documents[POLICY_FILE], errors)
    _validate_fallbacks(
        documents[FALLBACK_FILE],
        errors,
        id_registry,
        knowledge_by_id,
        allowed_speech_functions,
    )
    return errors


def content_counts(content_dir: Path | str = DEFAULT_CONTENT_DIR) -> dict[str, int]:
    content_dir = Path(content_dir)
    counts: dict[str, int] = {}
    for filename in (WORLD_FILE, *NPC_FILES, RELATIONSHIP_FILE):
        data = json.loads((content_dir / filename).read_text(encoding="utf-8"))
        counts[filename] = len(data["knowledge"])
    fallback_data = json.loads((content_dir / FALLBACK_FILE).read_text(encoding="utf-8"))
    counts[FALLBACK_FILE] = len(fallback_data["fallbacks"])
    return counts


def _format_counts(counts: dict[str, int]) -> str:
    return ", ".join(f"{filename}={count}" for filename, count in counts.items())


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate Whiteout Station v1.4 roleplay content")
    parser.add_argument(
        "content_dir",
        nargs="?",
        type=Path,
        default=DEFAULT_CONTENT_DIR,
        help="directory containing the six v1.4 JSON files",
    )
    args = parser.parse_args(list(argv) if argv is not None else None)

    errors = validate_content(args.content_dir)
    if errors:
        print(f"Roleplay content validation failed with {len(errors)} error(s):")
        for error in errors:
            print(f"- {error}")
        return 1

    print(f"Roleplay content valid: {_format_counts(content_counts(args.content_dir))}")
    return 0


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
