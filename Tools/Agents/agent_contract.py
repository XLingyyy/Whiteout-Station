"""Shared v1.3 DeepSeek request and response contract for agent tools."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from enum import Enum
from typing import Any, Iterable, Mapping, Sequence
from urllib.parse import urlsplit


MODEL = "deepseek-v4-flash"
OFFICIAL_ENDPOINT = "https://api.deepseek.com/chat/completions"
PROTOCOL_VERSION = "dialogue_epistemic_v3"
PROMPT_MODE = "semantic_atoms_full_line"
MAX_RESPONSE_BYTES = 64 * 1024
MAX_LINE_CHARACTERS = 96
MAX_SENTENCES = 2
MAX_OUTPUT_TOKENS = 256

NPC_RESPONSE_FIELDS = frozenset(
    {
        "npc_line",
        "realized_atom_ids",
        "disclosed_fact_ids",
        "emotion",
        "movement_intent",
        "reaction_action",
    }
)
EMOTIONS = frozenset(
    {
        "neutral",
        "focused",
        "firm",
        "strained",
        "steadier",
        "reserved",
        "measured",
        "defiant",
        "withdrawn",
        "wary",
        "cornered",
        "defensive",
        "controlled",
        "guarded",
        "uneasy",
        "clinical",
        "grim",
        "alarmed",
        "urgent",
        "relieved",
    }
)
MOVEMENT_INTENTS = frozenset(
    {"stay", "step_closer", "step_back", "return_to_post"}
)
REACTION_ACTIONS = frozenset(
    {"neutral", "acknowledge", "consider", "reassure", "reject", "alarmed"}
)

DEFAULT_FORBIDDEN_PHRASES = (
    "AP",
    "Stamina",
    "至少两点",
    "至少2点",
    "条件ID",
    "不会单独否决",
    "否决",
    "阈值",
    "修正值",
    "+1",
)

FACT_SURFACE_FORMS: Mapping[str, tuple[str, ...]] = {
    "FACT_GENERATOR_PROTECTION_STOP": ("保护停机", "protection stop"),
    "FACT_BURNT_RELAY": ("继电器烧", "烧毁继电器", "触点熔", "burnt relay"),
    "FACT_HAND_INJURY": (
        "手伤",
        "伤手",
        "右手受伤",
        "手部受伤",
        "右手使不上力",
        "手还使不上力",
        "手上的伤口",
        "手上伤口",
        "伤口又裂",
        "hand injury",
        "injured hand",
    ),
    "FACT_HEAT_PACK": ("保温包", "暖袋", "热敷袋", "heat pack"),
    "FACT_RELAY_COMPATIBILITY": (
        "规格能对上",
        "继电器能替",
        "替代继电器",
        "可靠替代件",
        "厨房加热器",
        "正好能装上",
        "零件能装上",
        "compatible relay",
    ),
    "FACT_FORCED_RESTART_SUSPICION": (
        "手动旁路",
        "强制重启",
        "forced restart",
        "manual bypass",
    ),
    "FACT_FORCED_RESTART_CONFIRMED": (
        "越过保护",
        "绕过保护",
        "bypassed protection",
    ),
    "FACT_MEDICAL_DIAGNOSIS": (
        "完整诊断",
        "诊断结论",
        "诊断结果",
        "medical diagnosis",
    ),
}

# Domain nouns can appear only when a realized atom or disclosed fact authorizes them.
# This catches newly invented tasks/resources even when the output lists valid IDs.
KNOWN_DOMAIN_CONCEPTS: Mapping[str, frozenset[str]] = {
    "天线": frozenset({"ANTENNA_REPAIR_NEEDED", "FACT_ANTENNA_FAILURE"}),
    "食物": frozenset({"FOOD_RESOURCE_AVAILABLE", "FACT_FOOD_STOCK"}),
    "药品": frozenset({"MEDICINE_RESOURCE_AVAILABLE", "FACT_MEDICINE_STOCK"}),
    "厨房": frozenset({"FACT_RELAY_COMPATIBILITY"}),
    "保温包": frozenset({"HEAT_PACK_AVAILABLE", "FACT_HEAT_PACK"}),
    "暖袋": frozenset({"HEAT_PACK_AVAILABLE", "FACT_HEAT_PACK"}),
    "继电器": frozenset({"FACT_BURNT_RELAY", "FACT_RELAY_COMPATIBILITY"}),
    "手伤": frozenset({"HAND_INJURY_AFFECTS_FINE_WORK", "FACT_HAND_INJURY"}),
    "伤手": frozenset({"HAND_INJURY_AFFECTS_FINE_WORK", "FACT_HAND_INJURY"}),
    "诊断": frozenset({"HAND_INJURY_AFFECTS_FINE_WORK", "FACT_MEDICAL_DIAGNOSIS"}),
    "供暖": frozenset({"HEAT_PRIORITY_DECISION", "REPAIR_ROOM_SHOULD_BE_WARM"}),
    "维修间": frozenset({"REPAIR_ROOM_SHOULD_BE_WARM"}),
    "备用电": frozenset({"BACKUP_POWER_DECLINING"}),
    "暴雪": frozenset({"BLIZZARD_WINDOW_SHRINKING"}),
}

SYSTEM_JARGON_PATTERNS = tuple(
    re.compile(pattern, re.IGNORECASE)
    for pattern in (
        r"\bAP\b",
        r"\bStamina\b",
        r"条件\s*ID",
        r"至少\s*(?:两|2)\s*点",
        r"不会单独否决",
        r"否决",
        r"阈值",
        r"修正值",
        r"(?<![\w])\+\s*\d+",
        r"\b(?:FACT|REQ|COND|CONDITION|REQUIREMENT)_[A-Z0-9_]+\b",
        r"\b(?:gu_heng_available|player_collaboration|repair_room_heated|"
        r"gu_heng_stamina_ready|replacement_relay_available|right_hand_injury_risk|"
        r"technical_alternative_unconfirmed|unknown_fine_work_risk)\b",
    )
)


class EndpointKind(str, Enum):
    OFFICIAL = "official"
    LOOPBACK = "loopback"


class ContractError(ValueError):
    """Validation failure with a stable, non-sensitive error code."""

    def __init__(self, code: str) -> None:
        super().__init__(code)
        self.code = code


@dataclass(frozen=True)
class EndpointPolicy:
    kind: EndpointKind
    endpoint: str
    requires_api_key: bool


@dataclass(frozen=True)
class SemanticAtom:
    atom_id: str
    fallback: str
    surface_tokens: tuple[str, ...]
    related_fact_ids: tuple[str, ...] = ()


@dataclass(frozen=True)
class NPCResponse:
    npc_line: str
    realized_atom_ids: tuple[str, ...]
    disclosed_fact_ids: tuple[str, ...]
    emotion: str
    movement_intent: str
    reaction_action: str


PROBE_MUST_ATOMS = (
    SemanticAtom(
        "PROBE_ACKNOWLEDGED",
        "联调响应正常。",
        ("联调", "响应正常"),
    ),
)


def classify_endpoint(endpoint: str) -> EndpointPolicy:
    """Allow the official HTTPS API and local development mocks only."""

    clean_endpoint = endpoint.strip()
    try:
        parsed = urlsplit(clean_endpoint)
        port = parsed.port
    except ValueError as exc:
        raise ContractError("endpoint_invalid") from exc

    if (
        not clean_endpoint
        or not parsed.scheme
        or not parsed.hostname
        or parsed.username is not None
        or parsed.password is not None
        or parsed.query
        or parsed.fragment
    ):
        raise ContractError("endpoint_invalid")

    scheme = parsed.scheme.lower()
    hostname = parsed.hostname.lower()
    path = parsed.path.rstrip("/")
    if (
        scheme == "https"
        and hostname == "api.deepseek.com"
        and port in (None, 443)
        and path in ("/chat/completions", "/v1/chat/completions")
    ):
        return EndpointPolicy(EndpointKind.OFFICIAL, clean_endpoint, True)

    if (
        scheme in ("http", "https")
        and hostname in ("localhost", "127.0.0.1", "::1")
    ):
        return EndpointPolicy(EndpointKind.LOOPBACK, clean_endpoint, False)

    raise ContractError("endpoint_not_allowed")


def _stable_id(value: Any, *, code: str) -> str:
    if not isinstance(value, str):
        raise ContractError(code)
    clean = value.strip()
    if not clean or len(clean) > 96 or any(character.isspace() for character in clean):
        raise ContractError(code)
    return clean


def _string_tuple(value: Any, *, array_code: str, item_code: str) -> tuple[str, ...]:
    if not isinstance(value, (list, tuple)):
        raise ContractError(array_code)
    result: list[str] = []
    for item in value:
        if not isinstance(item, str) or not item.strip() or len(item.strip()) > 96:
            raise ContractError(item_code)
        clean = item.strip()
        if clean in result:
            raise ContractError(f"{item_code}_duplicate")
        result.append(clean)
    return tuple(result)


def normalize_atom(value: SemanticAtom | Mapping[str, Any]) -> SemanticAtom:
    if isinstance(value, SemanticAtom):
        atom = value
    elif isinstance(value, Mapping):
        atom_id = value.get("id", value.get("atom_id"))
        fallback = value.get("fallback", value.get("natural_fallback", ""))
        surface_tokens = value.get(
            "required_surface_tokens",
            value.get(
                "required_concept_tokens",
                value.get("surface_tokens", ()),
            ),
        )
        related_facts = value.get("related_fact_ids", ())
        atom = SemanticAtom(
            _stable_id(atom_id, code="atom_id_invalid"),
            fallback.strip() if isinstance(fallback, str) else "",
            _string_tuple(
                surface_tokens,
                array_code="atom_surface_tokens_not_array",
                item_code="atom_surface_token_invalid",
            ),
            _string_tuple(
                related_facts,
                array_code="atom_related_facts_not_array",
                item_code="atom_related_fact_invalid",
            ),
        )
    else:
        raise ContractError("atom_invalid")

    atom_id = _stable_id(atom.atom_id, code="atom_id_invalid")
    fallback = atom.fallback.strip()
    if not fallback or len(fallback) > MAX_LINE_CHARACTERS:
        raise ContractError("atom_fallback_invalid")
    surface_tokens = tuple(token.strip() for token in atom.surface_tokens if token.strip())
    if not surface_tokens or len(surface_tokens) != len(set(surface_tokens)):
        raise ContractError("atom_surface_tokens_invalid")
    if any(len(token) > 48 for token in surface_tokens):
        raise ContractError("atom_surface_token_invalid")
    related_fact_ids = tuple(
        _stable_id(fact, code="atom_related_fact_invalid")
        for fact in atom.related_fact_ids
    )
    if len(related_fact_ids) != len(set(related_fact_ids)):
        raise ContractError("atom_related_fact_duplicate")
    return SemanticAtom(atom_id, fallback, surface_tokens, related_fact_ids)


def normalize_atoms(
    values: Iterable[SemanticAtom | Mapping[str, Any]],
    *,
    may: bool = False,
) -> tuple[SemanticAtom, ...]:
    atoms = tuple(normalize_atom(value) for value in values)
    atom_ids = tuple(atom.atom_id for atom in atoms)
    if len(atom_ids) != len(set(atom_ids)):
        raise ContractError("atom_id_duplicate")
    if may and any(atom.related_fact_ids for atom in atoms):
        raise ContractError("may_atom_has_related_fact")
    return atoms


def atom_to_payload(atom: SemanticAtom) -> dict[str, Any]:
    return {
        "id": atom.atom_id,
        "fallback": atom.fallback,
        "required_surface_tokens": list(atom.surface_tokens),
        "related_fact_ids": list(atom.related_fact_ids),
    }


def build_request_payload(action_id: str = "probe_availability") -> dict[str, Any]:
    """Build the exact v1.3 non-thinking JSON request used by the probe."""

    clean_action_id = _stable_id(action_id, code="action_id_invalid")
    example = {
        "npc_line": "联调响应正常。",
        "realized_atom_ids": ["PROBE_ACKNOWLEDGED"],
        "disclosed_fact_ids": [],
        "emotion": "focused",
        "movement_intent": "stay",
        "reaction_action": "neutral",
    }
    system_prompt = (
        "你是游戏 NPC 的中文台词实现器。输入中的 must_realize 是必须表达的意思，"
        "may_realize 是可选意思；forbidden_fact_ids 与 forbidden_phrases 绝不能出现。"
        "只能依据输入原子组织完整自然台词，不得新增条件、事实、资源、承诺、任务结果或系统数值。"
        "台词最多两句、96 个字符、不得换行。严格返回一个 JSON 对象且不返回 Markdown。"
        "JSON 必须恰好包含 npc_line、realized_atom_ids、disclosed_fact_ids、emotion、"
        "movement_intent、reaction_action 六个字段。Example JSON: "
        + json.dumps(example, ensure_ascii=False, separators=(",", ":"))
    )
    user_prompt = json.dumps(
        {
            "protocol_version": PROTOCOL_VERSION,
            "prompt_mode": PROMPT_MODE,
            "action_id": clean_action_id,
            "speaker": "联调 NPC",
            "player_said": "请确认自然台词协议可用。",
            "stance": "focused_acknowledgement",
            "must_realize": [atom_to_payload(atom) for atom in PROBE_MUST_ATOMS],
            "may_realize": [],
            "allowed_fact_ids": [],
            "planned_disclosure_fact_ids": [],
            "forbidden_fact_ids": [],
            "forbidden_phrases": list(DEFAULT_FORBIDDEN_PHRASES),
            "persona": {
                "style": "简短、直接、自然",
                "max_sentences": MAX_SENTENCES,
                "max_characters": MAX_LINE_CHARACTERS,
            },
            "allowed_emotions": ["focused"],
            "allowed_movement_intents": ["stay"],
            "allowed_reaction_actions": ["neutral"],
        },
        ensure_ascii=False,
        separators=(",", ":"),
    )
    return {
        "model": MODEL,
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_prompt},
        ],
        "temperature": 0,
        "max_tokens": MAX_OUTPUT_TOKENS,
        "stream": False,
        "thinking": {"type": "disabled"},
        "response_format": {"type": "json_object"},
    }


def encode_request_payload(action_id: str = "probe_availability") -> bytes:
    return json.dumps(
        build_request_payload(action_id),
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")


def make_completion_envelope(
    content: Mapping[str, Any],
    *,
    model: str = MODEL,
    finish_reason: str = "stop",
    empty_content: bool = False,
) -> dict[str, Any]:
    """Create a minimal OpenAI-compatible non-streaming response envelope."""

    encoded_content = (
        ""
        if empty_content
        else json.dumps(content, ensure_ascii=False, separators=(",", ":"))
    )
    return {
        "id": "whiteout-mock-v13",
        "object": "chat.completion",
        "created": 0,
        "model": model,
        "choices": [
            {
                "index": 0,
                "message": {"role": "assistant", "content": encoded_content},
                "finish_reason": finish_reason,
            }
        ],
        "usage": {
            "prompt_tokens": 0,
            "completion_tokens": 0,
            "total_tokens": 0,
        },
    }


def _validate_id_array(value: Any, *, field: str) -> tuple[str, ...]:
    if not isinstance(value, list):
        raise ContractError(f"{field}_not_array")
    result: list[str] = []
    for item in value:
        clean = _stable_id(item, code=f"{field}_item_invalid")
        if clean in result:
            raise ContractError(f"{field}_duplicate")
        result.append(clean)
    return tuple(result)


def _validate_provider_envelope(payload: bytes) -> Mapping[str, Any]:
    if len(payload) > MAX_RESPONSE_BYTES:
        raise ContractError("response_too_large")
    try:
        root = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ContractError("provider_invalid_json") from exc
    if not isinstance(root, dict):
        raise ContractError("provider_invalid_envelope")

    choices = root.get("choices")
    if not isinstance(choices, list) or not choices or not isinstance(choices[0], dict):
        raise ContractError("provider_invalid_envelope")
    choice = choices[0]
    finish_reason = choice.get("finish_reason")
    if not isinstance(finish_reason, str):
        raise ContractError("provider_missing_finish_reason")
    if finish_reason != "stop":
        known_reasons = {
            "length",
            "content_filter",
            "tool_calls",
            "insufficient_system_resource",
        }
        suffix = finish_reason if finish_reason in known_reasons else "other"
        raise ContractError(f"provider_finish_{suffix}")

    message = choice.get("message")
    if not isinstance(message, dict):
        raise ContractError("provider_invalid_message")
    content = message.get("content")
    if not isinstance(content, str) or not content.strip():
        raise ContractError("provider_empty_content")
    try:
        response = json.loads(content)
    except json.JSONDecodeError as exc:
        raise ContractError("content_invalid_json") from exc
    if not isinstance(response, dict):
        raise ContractError("content_not_object")
    return response


def _sentence_count(line: str) -> int:
    segments = [segment.strip() for segment in re.split(r"[。！？.!?]+", line) if segment.strip()]
    return max(1, len(segments))


def _contains(text: str, token: str) -> bool:
    return token.casefold() in text.casefold()


def _validate_line(
    line: Any,
    *,
    realized_atoms: Sequence[SemanticAtom],
    disclosed_fact_ids: Sequence[str],
    forbidden_fact_ids: frozenset[str],
    forbidden_phrases: Sequence[str],
    fact_surface_forms: Mapping[str, Sequence[str]],
) -> str:
    if not isinstance(line, str):
        raise ContractError("npc_line_invalid")
    clean = line.strip()
    if (
        not clean
        or len(clean) > MAX_LINE_CHARACTERS
        or "\n" in clean
        or "\r" in clean
    ):
        raise ContractError("npc_line_invalid_length")
    if _sentence_count(clean) > MAX_SENTENCES:
        raise ContractError("npc_line_too_many_sentences")

    for pattern in SYSTEM_JARGON_PATTERNS:
        if pattern.search(clean):
            raise ContractError("npc_line_system_jargon")
    for phrase in forbidden_phrases:
        if phrase and _contains(clean, phrase):
            raise ContractError("npc_line_forbidden_phrase")
    for fact_id in forbidden_fact_ids:
        for phrase in fact_surface_forms.get(fact_id, ()):
            if phrase and _contains(clean, phrase):
                raise ContractError("npc_line_forbidden_fact")

    disclosed_ids = frozenset(disclosed_fact_ids)
    for fact_id, surface_forms in fact_surface_forms.items():
        if fact_id in disclosed_ids:
            continue
        if any(phrase and _contains(clean, phrase) for phrase in surface_forms):
            raise ContractError("npc_line_undisclosed_fact")

    realized_ids = frozenset(atom.atom_id for atom in realized_atoms)
    for atom in realized_atoms:
        if not any(_contains(clean, token) for token in atom.surface_tokens):
            raise ContractError("realized_atom_surface_missing")

    authorized_ids = realized_ids | disclosed_ids
    authorized_surface = tuple(
        token
        for atom in realized_atoms
        for token in (atom.fallback, *atom.surface_tokens)
    )
    for concept, concept_ids in KNOWN_DOMAIN_CONCEPTS.items():
        if not _contains(clean, concept):
            continue
        if concept_ids & authorized_ids:
            continue
        if any(_contains(surface, concept) for surface in authorized_surface):
            continue
        raise ContractError("npc_line_added_domain_concept")
    return clean


def validate_completion(
    payload: bytes,
    *,
    must_atoms: Iterable[SemanticAtom | Mapping[str, Any]] = PROBE_MUST_ATOMS,
    may_atoms: Iterable[SemanticAtom | Mapping[str, Any]] = (),
    planned_disclosed_fact_ids: Iterable[str] = (),
    allowed_fact_ids: Iterable[str] = (),
    forbidden_fact_ids: Iterable[str] = (),
    forbidden_phrases: Iterable[str] = DEFAULT_FORBIDDEN_PHRASES,
    allowed_emotions: Iterable[str] = EMOTIONS,
    allowed_movement_intents: Iterable[str] = MOVEMENT_INTENTS,
    allowed_reaction_actions: Iterable[str] = REACTION_ACTIONS,
    fact_surface_forms: Mapping[str, Sequence[str]] = FACT_SURFACE_FORMS,
) -> NPCResponse:
    """Validate the provider envelope and the exact v1.3 realization object."""

    must = normalize_atoms(must_atoms)
    may = normalize_atoms(may_atoms, may=True)
    must_ids = tuple(atom.atom_id for atom in must)
    may_ids = tuple(atom.atom_id for atom in may)
    if set(must_ids) & set(may_ids):
        raise ContractError("atom_id_duplicate")
    atom_by_id = {atom.atom_id: atom for atom in (*must, *may)}

    allowed_facts = frozenset(
        _stable_id(value, code="allowed_fact_id_invalid")
        for value in allowed_fact_ids
    )
    planned_facts = tuple(
        _stable_id(value, code="planned_fact_id_invalid")
        for value in planned_disclosed_fact_ids
    )
    if len(planned_facts) != len(set(planned_facts)):
        raise ContractError("planned_fact_id_duplicate")
    forbidden_facts = frozenset(
        _stable_id(value, code="forbidden_fact_id_invalid")
        for value in forbidden_fact_ids
    )
    if not set(planned_facts) <= allowed_facts:
        raise ContractError("planned_fact_not_allowed")
    if set(planned_facts) & forbidden_facts:
        raise ContractError("planned_fact_forbidden")

    response = _validate_provider_envelope(payload)
    keys = frozenset(response)
    if keys != NPC_RESPONSE_FIELDS:
        if NPC_RESPONSE_FIELDS - keys:
            raise ContractError("content_missing_field")
        raise ContractError("content_unknown_field")

    realized_atom_ids = _validate_id_array(
        response["realized_atom_ids"],
        field="realized_atom_ids",
    )
    realized_set = set(realized_atom_ids)
    if not set(must_ids) <= realized_set:
        raise ContractError("must_atom_missing")
    if not realized_set <= set(atom_by_id):
        raise ContractError("realized_atom_unknown")

    disclosed_fact_ids = _validate_id_array(
        response["disclosed_fact_ids"],
        field="disclosed_fact_ids",
    )
    disclosed_set = set(disclosed_fact_ids)
    if not disclosed_set <= allowed_facts:
        raise ContractError("disclosed_fact_not_allowed")
    if disclosed_set & forbidden_facts:
        raise ContractError("disclosed_fact_forbidden")
    if disclosed_set != set(planned_facts):
        raise ContractError("disclosed_fact_plan_mismatch")

    emotion = response["emotion"]
    movement_intent = response["movement_intent"]
    reaction_action = response["reaction_action"]
    if not isinstance(emotion, str) or emotion not in frozenset(allowed_emotions):
        raise ContractError("emotion_invalid")
    if (
        not isinstance(movement_intent, str)
        or movement_intent not in frozenset(allowed_movement_intents)
    ):
        raise ContractError("movement_intent_invalid")
    if (
        not isinstance(reaction_action, str)
        or reaction_action not in frozenset(allowed_reaction_actions)
    ):
        raise ContractError("reaction_action_invalid")

    realized_atoms = tuple(atom_by_id[atom_id] for atom_id in realized_atom_ids)
    npc_line = _validate_line(
        response["npc_line"],
        realized_atoms=realized_atoms,
        disclosed_fact_ids=disclosed_fact_ids,
        forbidden_fact_ids=forbidden_facts,
        forbidden_phrases=tuple(forbidden_phrases),
        fact_surface_forms=fact_surface_forms,
    )
    return NPCResponse(
        npc_line=npc_line,
        realized_atom_ids=realized_atom_ids,
        disclosed_fact_ids=disclosed_fact_ids,
        emotion=emotion,
        movement_intent=movement_intent,
        reaction_action=reaction_action,
    )
