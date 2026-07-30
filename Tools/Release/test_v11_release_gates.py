from __future__ import annotations

import hashlib
import json
import subprocess
from pathlib import Path

import pytest

try:
    from .create_release_manifest_v11 import create_manifest
    from .run_v11_input_smoke import (
        _validate_antenna_event_log,
        _validate_antenna_log,
    )
    from .v11_gate_common import (
        AGENT_RUNTIME_REL,
        DISTRIBUTION_CLASS,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        PACKAGED_AGENT_RUNTIME_REL,
        PACKAGED_RULES_REL,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES,
        RULES_REL,
        GateError,
        sha256_file,
    )
    from .validate_source_v11 import (
        scan_tracked_secrets,
        validate_protected_assets,
        validate_source,
        validate_versions,
        validate_worktree_state,
    )
except ImportError:
    from create_release_manifest_v11 import create_manifest
    from run_v11_input_smoke import (
        _validate_antenna_event_log,
        _validate_antenna_log,
    )
    from v11_gate_common import (
        AGENT_RUNTIME_REL,
        DISTRIBUTION_CLASS,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        PACKAGED_AGENT_RUNTIME_REL,
        PACKAGED_RULES_REL,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES,
        RULES_REL,
        GateError,
        sha256_file,
    )
    from validate_source_v11 import (
        scan_tracked_secrets,
        validate_protected_assets,
        validate_source,
        validate_versions,
        validate_worktree_state,
    )


MAP_REL = "WhiteoutStation/Content/WindStation/World/MVP_StationMap.umap"
NPC_ROOT = "WhiteoutStation/Content/WindStation/Art/AnimeNPC"
PRESENTATION_ROOT = "WhiteoutStation/Content/WindStation/Presentation/Characters"
PROTECTED_MANIFEST_REL = "docs/PROTECTED_CHARACTER_ASSETS_v1.0.json"
BUILD_TIMESTAMP = "2026-07-31T12:00:00Z"


def git(repo: Path, *arguments: str) -> str:
    completed = subprocess.run(
        ["git", *arguments],
        cwd=repo,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return completed.stdout.strip()


def git_bytes(repo: Path, *arguments: str) -> bytes:
    completed = subprocess.run(
        ["git", *arguments],
        cwd=repo,
        check=True,
        capture_output=True,
    )
    return completed.stdout


def write_text(repo: Path, relative_path: str, value: str) -> None:
    path = repo / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8")


def write_bytes(repo: Path, relative_path: str, value: bytes) -> None:
    path = repo / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(value)


def make_repository(tmp_path: Path) -> tuple[Path, str, str]:
    repo = tmp_path / "repo"
    repo.mkdir()
    git(repo, "init", "-b", "main")
    git(repo, "config", "user.name", "Release Gate Test")
    git(repo, "config", "user.email", "release-gate@example.invalid")

    write_text(
        repo,
        PROJECT_CONFIG_REL,
        "[/Script/EngineSettings.GeneralProjectSettings]\n"
        f"ProjectVersion={PROJECT_VERSION}\n",
    )
    write_text(
        repo,
        RULES_REL,
        json.dumps(
            {
                "schema_version": 4,
                "rules_version": PROJECT_VERSION,
                "gameplay": {
                    "phases": ["morning", "afternoon", "dusk"],
                    "action_points_per_phase": 4,
                },
                "routes": {
                    "success_a": {"expected_success": True},
                    "success_b": {"expected_success": True},
                    "success_c": {"expected_success": True},
                    "failure_a": {"expected_success": False},
                    "failure_b": {"expected_success": False},
                },
            },
            indent=2,
        )
        + "\n",
    )
    write_text(
        repo,
        AGENT_RUNTIME_REL,
        json.dumps(
            {
                "schema_version": 4,
                "runtime_version": PROJECT_VERSION,
                "provider_name": "deepseek",
                "model": "deepseek-chat",
                "endpoint": "https://api.deepseek.com",
                "llm_enabled": False,
                "transport": "openai_chat_completions",
            },
            indent=2,
        )
        + "\n",
    )
    write_text(
        repo,
        "WhiteoutStation/Source/WhiteoutStation/Private/State/"
        "WindStationStateSubsystem.cpp",
        f'const char* Rules = "{Path(RULES_REL).name}";\n',
    )
    write_text(
        repo,
        "WhiteoutStation/Source/WhiteoutStation/Private/Agents/" "WSAgentGateway.cpp",
        f'const char* Runtime = "{Path(AGENT_RUNTIME_REL).name}";\n',
    )
    write_text(
        repo,
        "Tools/Rules/whiteout_rules_v11.py",
        f'RULES_FILE = "{Path(RULES_REL).name}"\n',
    )
    write_bytes(repo, MAP_REL, b"approved-user-map-checkpoint\n")
    write_text(repo, f"{NPC_ROOT}/GuHeng/model.txt", "approved-model\n")
    write_text(
        repo,
        f"{PRESENTATION_ROOT}/GuHeng/material.txt",
        "approved-material\n",
    )
    git(repo, "add", "--", ".")
    git(repo, "commit", "-m", "approved source and protected baseline")
    baseline_commit = git(repo, "rev-parse", "HEAD")

    protected_objects = []
    for relative_path in (MAP_REL, NPC_ROOT, PRESENTATION_ROOT):
        protected_objects.append(
            {
                "path": relative_path,
                "type": "blob" if relative_path == MAP_REL else "tree",
                "object_id": git(
                    repo,
                    "rev-parse",
                    f"HEAD:{relative_path}",
                ),
            }
        )
    map_sha = hashlib.sha256((repo / MAP_REL).read_bytes()).hexdigest()
    write_text(
        repo,
        PROTECTED_MANIFEST_REL,
        json.dumps(
            {
                "schema": "whiteout.protected-assets.v3",
                "version": "1.0.0",
                "baseline_commit": baseline_commit,
                "protected_roots": [MAP_REL, NPC_ROOT, PRESENTATION_ROOT],
                "allowed_additions": [],
                "protected_git_objects": protected_objects,
                "map_baseline": {
                    "path": MAP_REL,
                    "sha256": map_sha,
                    "policy": "fixture",
                },
            },
            indent=2,
        )
        + "\n",
    )
    git(repo, "add", "--", PROTECTED_MANIFEST_REL)
    git(repo, "commit", "-m", "record protected assets")
    return (
        repo,
        git(repo, "rev-parse", "HEAD"),
        git(repo, "rev-parse", "HEAD^{tree}"),
    )


def make_artifact(
    tmp_path: Path,
    repo: Path,
    source_commit: str,
) -> tuple[Path, str]:
    run_id = f"20260731T120000Z-{source_commit[:8]}-test"
    artifact_root = tmp_path / "artifacts" / f"WhiteoutStation-v1.1-Win64-{run_id}"
    artifact_root.mkdir(parents=True)

    payloads = {
        PACKAGED_RULES_REL: git_bytes(
            repo,
            "show",
            f"{source_commit}:{RULES_REL}",
        ),
        PACKAGED_AGENT_RUNTIME_REL: git_bytes(
            repo,
            "show",
            f"{source_commit}:{AGENT_RUNTIME_REL}",
        ),
        "README_v1.1.txt": b"Whiteout Station v1.1 local review build\n",
        "ASSET_LICENSES.md": b"# Asset licenses\nFixture only.\n",
        "Validation/InputSmokeV11/input_smoke_summary.json": (
            b'{"schema":"whiteout.v1.1.real-input-smoke.v1","passed":true}\n'
        ),
        "Validation/ShippingSmokeV11/shipping_smoke_summary.json": (
            b'{"schema":"whiteout.v1.1.shipping-smoke.v1","passed":true}\n'
        ),
    }
    for relative_path in REQUIRED_PACKAGE_FILES:
        write_bytes(
            artifact_root,
            relative_path,
            payloads.get(relative_path, b"fixture-binary\n"),
        )
    return artifact_root, run_id


def create_valid_manifest(
    repo: Path,
    artifact_root: Path,
    run_id: str,
) -> Path:
    return create_manifest(
        repo,
        artifact_root,
        run_id=run_id,
        source_ref="HEAD",
        build_timestamp_utc=BUILD_TIMESTAMP,
    )


def test_shipping_antenna_evidence_uses_committed_event_without_log(
    tmp_path: Path,
) -> None:
    missing_log = tmp_path / "Shipping.log"
    assert _validate_antenna_log(missing_log) == {"shipping_log_available": False}
    event_log = tmp_path / "WhiteoutStation_EventLog.json"
    event_log.write_text(
        json.dumps(
            {
                "rules_version": PROJECT_VERSION,
                "events": [
                    {
                        "action_id": "calibrate_antenna",
                        "reason_code": "Committed",
                        "ap_before": 4,
                        "ap_after": 2,
                    }
                ],
            }
        ),
        encoding="utf-8",
    )
    evidence = _validate_antenna_event_log(event_log)
    assert evidence["reason_code"] == "Committed"
    assert evidence["ap_before"] == 4
    assert evidence["ap_after"] == 2


def test_valid_v11_source_and_manifest_contract_passes(tmp_path: Path) -> None:
    repo, commit, tree = make_repository(tmp_path)
    assert validate_versions(repo).passed
    assert validate_worktree_state(repo).passed
    assert scan_tracked_secrets(repo).passed
    assert validate_protected_assets(repo, require_tracked=True).passed
    source_report = validate_source(repo, final=True, check_lfs=False)
    assert source_report.passed, source_report.errors

    artifact_root, run_id = make_artifact(tmp_path, repo, commit)
    manifest_path = create_valid_manifest(repo, artifact_root, run_id)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert manifest["schema"] == MANIFEST_SCHEMA
    assert manifest["version"] == PROJECT_VERSION
    assert manifest["distribution_class"] == DISTRIBUTION_CLASS
    assert manifest["source_commit"] == commit
    assert manifest["source_tree"] == tree
    assert manifest["source_branch"] == "main"
    assert manifest["source_dirty"] is False
    assert MANIFEST_REL not in manifest["checksums"]
    assert set(REQUIRED_PACKAGE_FILES).issubset(manifest["checksums"])
    assert all(
        checksum == sha256_file(artifact_root / relative_path)
        for relative_path, checksum in manifest["checksums"].items()
    )


@pytest.mark.parametrize(
    ("target", "mutator", "expected"),
    (
        (
            "project",
            lambda value: value.replace("ProjectVersion=1.1.0", "ProjectVersion=1.0.0"),
            "ProjectVersion=1.1.0",
        ),
        (
            "rules",
            lambda value: {**value, "schema_version": 3},
            "schema_version must be 4",
        ),
        (
            "agent",
            lambda value: {**value, "schema_version": 3},
            "schema_version must be 4",
        ),
        (
            "agent",
            lambda value: {**value, "llm_enabled": True},
            "llm_enabled=false",
        ),
    ),
)
def test_v11_version_contract_rejects_mismatch(
    tmp_path: Path,
    target: str,
    mutator,
    expected: str,
) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    if target == "project":
        path = repo / PROJECT_CONFIG_REL
        path.write_text(
            mutator(path.read_text(encoding="utf-8")),
            encoding="utf-8",
        )
    else:
        relative_path = RULES_REL if target == "rules" else AGENT_RUNTIME_REL
        path = repo / relative_path
        payload = json.loads(path.read_text(encoding="utf-8"))
        path.write_text(
            json.dumps(mutator(payload), indent=2) + "\n",
            encoding="utf-8",
        )
    report = validate_versions(repo)
    assert not report.passed
    assert any(expected in error for error in report.errors)


def test_legacy_runtime_file_and_fallback_remain_compatible(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    write_text(
        repo,
        "WhiteoutStation/Content/Agents/AgentRuntime.v1.0.json",
        "{}\n",
    )
    gateway = (
        repo / "WhiteoutStation/Source/WhiteoutStation/Private/Agents/"
        "WSAgentGateway.cpp"
    )
    gateway.write_text(
        gateway.read_text(encoding="utf-8")
        + 'const char* Legacy = "AgentRuntime.v1.0.json";\n',
        encoding="utf-8",
    )
    report = validate_versions(repo)
    assert report.passed, report.errors


def test_release_source_requires_clean_main(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    git(repo, "switch", "-c", "feature/release")
    branch_report = validate_worktree_state(repo)
    assert not branch_report.passed
    assert any("branch main" in error for error in branch_report.errors)

    git(repo, "switch", "main")
    write_text(repo, "untracked.txt", "dirty\n")
    dirty_report = validate_worktree_state(repo)
    assert not dirty_report.passed
    assert any("clean main" in error for error in dirty_report.errors)


def test_tracked_secret_is_rejected_without_echo(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    secret = b"s" + b"k-" + b"A" * 24
    write_bytes(repo, "leak.txt", b"credential=" + secret + b"\n")
    git(repo, "add", "--", "leak.txt")
    report = scan_tracked_secrets(repo)
    assert not report.passed
    joined = "\n".join(report.errors)
    assert "suspicious_sk_key" in joined
    assert secret.decode() not in joined


def test_protected_map_worktree_change_is_rejected(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    write_bytes(repo, MAP_REL, b"changed-user-map\n")
    report = validate_protected_assets(repo, require_tracked=True)
    assert not report.passed
    assert any(
        "worktree/index changes" in error or "SHA-256 mismatch" in error
        for error in report.errors
    )


def test_protected_committed_asset_change_is_rejected(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    write_text(repo, f"{NPC_ROOT}/GuHeng/model.txt", "replacement-model\n")
    git(repo, "add", "--", NPC_ROOT)
    git(repo, "commit", "-m", "unauthorized model replacement")
    report = validate_protected_assets(repo, require_tracked=True)
    assert not report.passed
    assert any(
        "Protected baseline file changed" in error or "Protected path changed" in error
        for error in report.errors
    )


def test_manifest_rejects_multiple_v11_artifacts(tmp_path: Path) -> None:
    repo, commit, _tree = make_repository(tmp_path)
    artifact_root, run_id = make_artifact(tmp_path, repo, commit)
    sibling = artifact_root.parent / (
        f"WhiteoutStation-v1.1-Win64-20260731T120001Z-{commit[:8]}-other"
    )
    sibling.mkdir()
    with pytest.raises(GateError, match="Exactly one"):
        create_valid_manifest(repo, artifact_root, run_id)


def test_manifest_rejects_dirty_or_non_main_source(tmp_path: Path) -> None:
    repo, commit, _tree = make_repository(tmp_path)
    artifact_root, run_id = make_artifact(tmp_path, repo, commit)
    write_text(repo, "dirty.txt", "dirty\n")
    with pytest.raises(GateError, match="clean main"):
        create_valid_manifest(repo, artifact_root, run_id)

    (repo / "dirty.txt").unlink()
    git(repo, "switch", "-c", "release-candidate")
    with pytest.raises(GateError, match="branch main"):
        create_valid_manifest(repo, artifact_root, run_id)


def test_manifest_rejects_packaged_rules_not_from_source_commit(
    tmp_path: Path,
) -> None:
    repo, commit, _tree = make_repository(tmp_path)
    artifact_root, run_id = make_artifact(tmp_path, repo, commit)
    rules_path = artifact_root / PACKAGED_RULES_REL
    rules = json.loads(rules_path.read_text(encoding="utf-8"))
    rules["tampered"] = True
    rules_path.write_text(json.dumps(rules) + "\n", encoding="utf-8")
    with pytest.raises(GateError, match="does not match"):
        create_valid_manifest(repo, artifact_root, run_id)


def test_manifest_allows_legacy_runtime_alongside_v11(tmp_path: Path) -> None:
    repo, commit, _tree = make_repository(tmp_path)
    artifact_root, run_id = make_artifact(tmp_path, repo, commit)
    write_text(
        artifact_root,
        "Windows/WhiteoutStation/Content/Agents/AgentRuntime.v1.0.json",
        "{}\n",
    )
    manifest_path = create_valid_manifest(repo, artifact_root, run_id)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert (
        "Windows/WhiteoutStation/Content/Agents/AgentRuntime.v1.0.json"
        in manifest["checksums"]
    )


def test_manifest_rejects_artifact_secret_without_echo(tmp_path: Path) -> None:
    repo, commit, _tree = make_repository(tmp_path)
    artifact_root, run_id = make_artifact(tmp_path, repo, commit)
    secret = b"s" + b"k-" + b"B" * 24
    write_bytes(
        artifact_root,
        "README_v1.1.txt",
        b"credential=" + secret + b"\n",
    )
    with pytest.raises(GateError) as captured:
        create_valid_manifest(repo, artifact_root, run_id)
    message = str(captured.value)
    assert "credential-like material" in message
    assert secret.decode() not in message
