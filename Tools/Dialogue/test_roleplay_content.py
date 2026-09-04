from __future__ import annotations

import json
import shutil
from pathlib import Path

import pytest

from Tools.Dialogue.validate_roleplay_content import (
    DEFAULT_CONTENT_DIR,
    FALLBACK_FILE,
    NPC_FILES,
    RELATIONSHIP_FILE,
    REQUIRED_FILES,
    WORLD_FILE,
    content_counts,
    validate_content,
)


def _copy_bundle(tmp_path: Path) -> Path:
    destination = tmp_path / "v1.4"
    destination.mkdir()
    for filename in REQUIRED_FILES:
        shutil.copy2(DEFAULT_CONTENT_DIR / filename, destination / filename)
    return destination


def _load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _save(path: Path, document: dict) -> None:
    path.write_text(
        json.dumps(document, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def _assert_error(content_dir: Path, fragment: str) -> None:
    errors = validate_content(content_dir)
    assert errors, "the invalid fixture unexpectedly passed"
    assert any(fragment in error for error in errors), "\n".join(errors)


def test_checked_in_content_is_valid() -> None:
    assert validate_content(DEFAULT_CONTENT_DIR) == []


def test_checked_in_content_has_required_counts() -> None:
    counts = content_counts(DEFAULT_CONTENT_DIR)
    for filename in NPC_FILES:
        assert 25 <= counts[filename] <= 35
    assert 6 <= counts[RELATIONSHIP_FILE] <= 10
    assert counts[WORLD_FILE] > 0
    assert counts[FALLBACK_FILE] >= 4


def test_rejects_case_insensitive_global_id_collision(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    npc_path = content_dir / "NPC_GuHeng.json"
    document = _load(npc_path)
    document["knowledge"][0]["knowledge_id"] = "gu_role"
    _save(npc_path, document)
    _assert_error(content_dir, "compared case-insensitively")


def test_rejects_wrong_npc_owner(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    npc_path = content_dir / "NPC_GuHeng.json"
    document = _load(npc_path)
    document["knowledge"][0]["owner"] = "ye_cheng"
    _save(npc_path, document)
    _assert_error(content_dir, "expected 'gu_heng'")


def test_rejects_unstable_or_unknown_subject(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    npc_path = content_dir / "NPC_YeCheng.json"
    document = _load(npc_path)
    document["knowledge"][0]["subject"] = "GuHeng"
    _save(npc_path, document)
    _assert_error(content_dir, "stable snake_case identifier")


def test_rejects_hidden_knowledge_available_always(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    npc_path = content_dir / "NPC_GuHeng.json"
    document = _load(npc_path)
    document["knowledge"][0]["max_disclosure"] = "hidden"
    _save(npc_path, document)
    _assert_error(content_dir, "hidden knowledge cannot be available 'always'")


def test_rejects_game_fact_creation_without_fact_id(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    npc_path = content_dir / "NPC_YeCheng.json"
    document = _load(npc_path)
    document["knowledge"][0]["creates_game_fact"] = True
    document["knowledge"][0]["game_fact_id"] = ""
    _save(npc_path, document)
    _assert_error(content_dir, "true requires a non-empty FACT_ game_fact_id")


def test_rejects_unknown_fallback_reference(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    fallback_path = content_dir / "SafeFallbacks.json"
    document = _load(fallback_path)
    document["fallbacks"][0]["referenced_knowledge_ids"] = ["DOES_NOT_EXIST"]
    _save(fallback_path, document)
    _assert_error(content_dir, "unknown knowledge ID 'DOES_NOT_EXIST'")


def test_rejects_cross_character_fallback_reference(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    fallback_path = content_dir / "SafeFallbacks.json"
    document = _load(fallback_path)
    document["fallbacks"][0]["referenced_knowledge_ids"] = ["YE_GOAL_KEEP_CREW_CAPABLE"]
    _save(fallback_path, document)
    _assert_error(content_dir, "is not owned by world or gu_heng")


@pytest.mark.parametrize(
    "forbidden_text",
    [
        "完成这件事需要 1 AP",
        "我的 Stamina 不够",
        "请读取内部 ID",
        "把内容写进 Prompt",
        "交给规则引擎结算",
        "引用 FACT_SECRET",
        "完成这件事需要一个行动点",
        "这是某个内部枚举值",
        "等待模型输出 JSON",
    ],
)
def test_rejects_backstage_language_in_knowledge(
    tmp_path: Path, forbidden_text: str
) -> None:
    content_dir = _copy_bundle(tmp_path)
    npc_path = content_dir / "NPC_GuHeng.json"
    document = _load(npc_path)
    document["knowledge"][0]["content"] = forbidden_text
    _save(npc_path, document)
    _assert_error(content_dir, "forbidden backstage language")


def test_rejects_fixed_question_answer_content(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    npc_path = content_dir / "NPC_GuHeng.json"
    document = _load(npc_path)
    document["knowledge"][0]["content"] = "玩家：你多大？顾衡：四十一。"
    _save(npc_path, document)
    _assert_error(content_dir, "looks like fixed question-and-answer dialogue")


def test_rejects_unknown_availability_token(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    npc_path = content_dir / "NPC_YeCheng.json"
    document = _load(npc_path)
    document["knowledge"][0]["availability"] = ["after_any_dialogue"]
    _save(npc_path, document)
    _assert_error(content_dir, "unsupported availability token 'after_any_dialogue'")


def test_rejects_schema_extensions(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    npc_path = content_dir / "NPC_GuHeng.json"
    document = _load(npc_path)
    document["knowledge"][0]["developer_note"] = "not part of the contract"
    _save(npc_path, document)
    _assert_error(content_dir, "unexpected fields: developer_note")


def test_reports_non_object_document_without_crashing(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    relationship_path = content_dir / RELATIONSHIP_FILE
    relationship_path.write_text("[]\n", encoding="utf-8")
    _assert_error(content_dir, "expected object")


def test_rejects_unexpected_json_file(tmp_path: Path) -> None:
    content_dir = _copy_bundle(tmp_path)
    (content_dir / "Scratch.json").write_text("{}\n", encoding="utf-8")
    _assert_error(content_dir, "unexpected JSON files: Scratch.json")
