from __future__ import annotations

import json
from pathlib import Path

from Tools.Release.validate_source_v12 import validate_versions
from Tools.Release import run_v11_shipping_smoke as v11_smoke
from Tools.Release.run_v12_shipping_smoke import configure_v12_contract


def _write_fixture(root: Path) -> None:
    files = {
        "WhiteoutStation/Config/DefaultGame.ini": "ProjectVersion=1.2.0\n",
        "WhiteoutStation/Content/Rules/WhiteoutStationRules.v1.1.json": json.dumps(
            {"rules_version": "1.1.0", "schema_version": 4}
        ),
        "WhiteoutStation/Content/Agents/AgentRuntime.v1.2.json": json.dumps(
            {
                "runtime_version": "1.2.0",
                "schema_version": 5,
                "protocol_version": "dialogue_grounding_v2",
                "prompt_mode": "semantic_spine_plus_persona_tail",
                "max_tail_chars": 48,
                "max_tail_tokens": 128,
                "transport": "openai_chat_completions",
                "llm_enabled": False,
                "endpoint": "https://api.deepseek.com",
            }
        ),
        "README.md": "当前版本为 v1.2\ndocs/BUILD_AND_PLAY_v1.2.md\n",
        "docs/BUILD_AND_PLAY_v1.2.md": (
            "AgentRuntime.v1.2.json Whiteout.DialogueDebug 1 "
            "WhiteoutStation_OfferAudit.jsonl"
        ),
        "WhiteoutStation/Source/WhiteoutStation/Private/Agents/"
        "WSAgentGateway.cpp": "AgentRuntime.v1.2.json semantic_spine persona_tail",
        "WhiteoutStation/Source/WhiteoutStation/Private/State/"
        "WindStationStateSubsystem.cpp": (
            "WhiteoutStation_Autosave_v1_2 WhiteoutStation_Autosave_v1_1 "
            "WhiteoutStation_OfferAudit.jsonl"
        ),
        "WhiteoutStation/Source/WhiteoutStation/Public/Save/"
        "WindStationSaveGame.h": 'TEXT("1.2.0")',
    }
    for relative_path, content in files.items():
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")


def test_v12_version_contract_accepts_dialogue_layer_over_v11_rules(
    tmp_path: Path,
) -> None:
    _write_fixture(tmp_path)
    assert validate_versions(tmp_path).passed


def test_v12_version_contract_rejects_stale_project_version(tmp_path: Path) -> None:
    _write_fixture(tmp_path)
    config = tmp_path / "WhiteoutStation/Config/DefaultGame.ini"
    config.write_text("ProjectVersion=1.1.0\n", encoding="utf-8")
    assert not validate_versions(tmp_path).passed


def test_v12_version_contract_rejects_full_answer_prompt_mode(tmp_path: Path) -> None:
    _write_fixture(tmp_path)
    runtime = tmp_path / ("WhiteoutStation/Content/Agents/AgentRuntime.v1.2.json")
    payload = json.loads(runtime.read_text(encoding="utf-8"))
    payload["prompt_mode"] = "rewrite_full_answer"
    runtime.write_text(json.dumps(payload), encoding="utf-8")
    assert not validate_versions(tmp_path).passed


def test_v12_version_contract_rejects_credential_fields(tmp_path: Path) -> None:
    _write_fixture(tmp_path)
    runtime = tmp_path / ("WhiteoutStation/Content/Agents/AgentRuntime.v1.2.json")
    payload = json.loads(runtime.read_text(encoding="utf-8"))
    payload["api_key"] = "fixture"
    runtime.write_text(json.dumps(payload), encoding="utf-8")
    assert not validate_versions(tmp_path).passed


def test_v12_shipping_contract_covers_grounded_actions_and_failures() -> None:
    configure_v12_contract()
    modes = {scenario.llm_mode for scenario in v11_smoke.SCENARIOS}
    assert v11_smoke.AGENT_RUNTIME_VERSION == "1.2.0"
    assert v11_smoke.AGENT_SCHEMA_VERSION == 5
    assert "repair_generator" in v11_smoke.EXPRESSION_ACTION_IDS
    assert v11_smoke.PERFORMANCE_VALIDATION_REASON == "persona_tail_accepted"
    assert {"provider_rejected", "loopback_mock"} <= modes
    assert v11_smoke.RUN_TIMEOUT_PROBE is True
