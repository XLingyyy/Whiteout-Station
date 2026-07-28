from __future__ import annotations

import hashlib
import json
import subprocess
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path

try:
    from .v07_gate_common import (
        AGENT_RUNTIME_REL,
        DISTRIBUTION_CLASS,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES,
        RULES_REL,
        sha256_file,
    )
    from .validate_release_v07 import validate_release_artifact
    from .validate_source_v07 import (
        AGENT_BASENAME,
        RULES_BASENAME,
        scan_tracked_secrets,
        validate_protected_assets,
        validate_versions,
    )
    from .run_v07_shipping_smoke import EXPECTED_ROUTES, SCENARIOS
except ImportError:
    from v07_gate_common import (
        AGENT_RUNTIME_REL,
        DISTRIBUTION_CLASS,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES,
        RULES_REL,
        sha256_file,
    )
    from validate_release_v07 import validate_release_artifact
    from validate_source_v07 import (
        AGENT_BASENAME,
        RULES_BASENAME,
        scan_tracked_secrets,
        validate_protected_assets,
        validate_versions,
    )
    from run_v07_shipping_smoke import EXPECTED_ROUTES, SCENARIOS


PROTECTED_PATHS = (
    "WhiteoutStation/Content/WindStation/Art/Characters",
    "SourceAssets/MakeHuman/Characters",
)
DIFF_ONLY_ROOT = "WhiteoutStation/Content/WindStation/Art/AnimeNPC"
ALLOWED_ADDITION = f"{DIFF_ONLY_ROOT}/GuHeng/AnimationsV07"


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


def write_text(repo: Path, relative_path: str, value: str) -> None:
    path = repo / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8")


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
                "schema_version": 2,
                "rules_version": PROJECT_VERSION,
                "limits": {"model_call_hard_limit": 10},
                "notes": "x" * 180,
            }
        ),
    )
    write_text(
        repo,
        AGENT_RUNTIME_REL,
        json.dumps(
            {
                "schema_version": 3,
                "runtime_version": PROJECT_VERSION,
                "provider_name": "deepseek",
                "model": "deepseek-v4-flash",
                "endpoint": "https://api.deepseek.com/chat/completions",
                "llm_enabled": True,
                "performance_contract": {
                    "movement_intents": [
                        "stay",
                        "step_closer",
                        "step_back",
                        "return_to_post",
                    ],
                    "reaction_actions": [
                        "neutral",
                        "acknowledge",
                        "consider",
                        "reassure",
                        "reject",
                        "alarmed",
                    ],
                    "movement_policy": "Dialogue-only local constrained movement.",
                },
                "notes": "x" * 120,
            }
        ),
    )
    write_text(
        repo,
        "WhiteoutStation/Source/WhiteoutStation/Private/State/"
        "WindStationStateSubsystem.cpp",
        f'const char* Rules = "{RULES_BASENAME}";\n',
    )
    write_text(
        repo,
        "WhiteoutStation/Source/WhiteoutStation/Private/Tests/"
        "WhiteoutRulesTests.cpp",
        f'const char* Rules = "{RULES_BASENAME}";\n',
    )
    write_text(
        repo,
        "Tools/Rules/whiteout_rules.py",
        f'RULES_FILE = "{RULES_BASENAME}"\n',
    )
    write_text(
        repo,
        "WhiteoutStation/Source/WhiteoutStation/Private/Agents/"
        "WSAgentGateway.cpp",
        f'const char* AgentRuntime = "{AGENT_BASENAME}";\n',
    )
    for index, relative_path in enumerate(PROTECTED_PATHS):
        write_text(repo, f"{relative_path}/protected_{index}.txt", f"asset-{index}\n")
    write_text(repo, f"{DIFF_ONLY_ROOT}/GuHeng/model.txt", "user-model\n")
    write_text(
        repo,
        ".gitattributes",
        "*.uasset filter=lfs diff=lfs merge=lfs -text\n"
        "*.umap filter=lfs diff=lfs merge=lfs -text\n",
    )

    git(repo, "add", "--", ".")
    git(repo, "commit", "-m", "baseline")
    baseline_commit = git(repo, "rev-parse", "HEAD")
    objects = [
        {
            "path": relative_path,
            "type": "tree",
            "object_id": git(repo, "rev-parse", f"HEAD:{relative_path}"),
        }
        for relative_path in PROTECTED_PATHS
    ]
    write_text(
        repo,
        "docs/PROTECTED_CHARACTER_ASSETS_v0.7.json",
        json.dumps(
            {
                "version": "0.7",
                "baseline_commit": baseline_commit,
                "protected_roots": [*PROTECTED_PATHS, DIFF_ONLY_ROOT],
                "allowed_additions": [ALLOWED_ADDITION],
                "protected_git_objects": objects,
            },
            indent=2,
        )
        + "\n",
    )
    git(repo, "add", "--", "docs/PROTECTED_CHARACTER_ASSETS_v0.7.json")
    git(repo, "commit", "-m", "record protected assets")
    return repo, git(repo, "rev-parse", "HEAD"), git(repo, "rev-parse", "HEAD^{tree}")


def write_sized_file(path: Path, size: int, prefix: bytes = b"") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as handle:
        handle.write(prefix)
        if size > len(prefix):
            handle.seek(size - 1)
            handle.write(b"\0")


def make_valid_artifact(
    tmp_path: Path,
    repo: Path,
    source_commit: str,
    source_tree: str,
) -> tuple[Path, Path, datetime]:
    now = datetime.now(timezone.utc)
    run_id = f"{now.strftime('%Y%m%dT%H%M%SZ')}-{source_commit[:8]}-test"
    artifact_root = (
        tmp_path / "artifacts" / f"WhiteoutStation-v0.7-Win64-{run_id}"
    )
    artifact_root.mkdir(parents=True)

    executable_paths = {
        "Windows/WhiteoutStation.exe",
        "Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe",
    }
    json_payloads = {
        "Windows/WhiteoutStation/Content/Rules/"
        "WhiteoutStationRules.v0.7.json": git(
            repo,
            "show",
            f"{source_commit}:{RULES_REL}",
        ).encode(),
        "Windows/WhiteoutStation/Content/Agents/"
        "AgentRuntime.v0.7.json": git(
            repo,
            "show",
            f"{source_commit}:{AGENT_RUNTIME_REL}",
        ).encode(),
        "README_v0.7.txt": (
            b"Whiteout Station v0.7\n"
            b"LOCAL REVIEW BUILD - DO NOT REDISTRIBUTE\n"
        ),
        "ASSET_LICENSES.md": b"# Asset licenses\nTest fixture only.\n",
    }
    for relative_path in REQUIRED_PACKAGE_FILES:
        path = artifact_root / relative_path
        if relative_path in json_payloads:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(json_payloads[relative_path])
            continue
        minimum = {
            "Windows/WhiteoutStation.exe": 100_000,
            "Windows/WhiteoutStation/Binaries/Win64/"
            "WhiteoutStation-Win64-Shipping.exe": 1_000_000,
            "Windows/WhiteoutStation/Content/Paks/"
            "WhiteoutStation-Windows.pak": 100_000,
            "Windows/WhiteoutStation/Content/Paks/"
            "WhiteoutStation-Windows.ucas": 1_000_000,
            "Windows/WhiteoutStation/Content/Paks/"
            "WhiteoutStation-Windows.utoc": 10_000,
        }[relative_path]
        write_sized_file(
            path,
            minimum,
            b"MZ" if relative_path in executable_paths else b"fixture",
        )

    checksums = {
        path.relative_to(artifact_root).as_posix(): sha256_file(path)
        for path in artifact_root.rglob("*")
        if path.is_file()
    }
    manifest_path = artifact_root / MANIFEST_REL
    manifest_path.parent.mkdir(parents=True)
    manifest = {
        "schema": MANIFEST_SCHEMA,
        "version": PROJECT_VERSION,
        "distribution_class": DISTRIBUTION_CLASS,
        "run_id": run_id,
        "artifact_root_name": artifact_root.name,
        "source_commit": source_commit,
        "source_tree": source_tree,
        "source_dirty": False,
        "build_timestamp_utc": now.isoformat().replace("+00:00", "Z"),
        "engine_version": "5.8.0",
        "python_version": ".".join(map(str, sys.version_info[:3])),
        "checksums": checksums,
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return artifact_root, manifest_path, now + timedelta(minutes=1)


def test_valid_version_and_release_fixture_passes(tmp_path: Path) -> None:
    repo, commit, tree = make_repository(tmp_path)
    assert validate_versions(repo).passed
    assert validate_protected_assets(repo).passed
    artifact_root, _manifest_path, validation_time = make_valid_artifact(
        tmp_path,
        repo,
        commit,
        tree,
    )
    report = validate_release_artifact(
        repo,
        artifact_root,
        now=validation_time,
    )
    assert report.passed, report.errors


def test_rules_tool_legacy_fallback_is_rejected(
    tmp_path: Path,
) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    write_text(
        repo,
        "Tools/Rules/whiteout_rules.py",
        f'DEFAULT_RULES_PATH = "{RULES_BASENAME}"\n'
        'LEGACY_RULES_PATH = "WhiteoutStationRules.v0.1.json"\n'
        "SELECTED = DEFAULT_RULES_PATH if True else LEGACY_RULES_PATH\n",
    )
    report = validate_versions(repo)
    assert not report.passed
    assert any("legacy runtime file" in error for error in report.errors)


def test_stale_v04_false_green_artifact_is_rejected(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    stale_root = tmp_path / "WhiteoutStation-v0.4-Win64"
    stale_manifest = stale_root / "Validation" / "gate_manifest.json"
    stale_manifest.parent.mkdir(parents=True)
    stale_manifest.write_text(
        json.dumps(
            {
                "schema": "whiteout.v0.4.release-manifest.v1",
                "version": "0.4.0",
                "run_id": "20260722T000000Z-00000000-test",
                "artifact_root_name": stale_root.name,
                "checksums": {},
            }
        ),
        encoding="utf-8",
    )
    report = validate_release_artifact(repo, stale_root)
    assert not report.passed
    assert any("v0.7" in error or "v0.4" in error for error in report.errors)


def test_manifest_provenance_mismatch_is_rejected(tmp_path: Path) -> None:
    repo, commit, tree = make_repository(tmp_path)
    artifact_root, manifest_path, validation_time = make_valid_artifact(
        tmp_path,
        repo,
        commit,
        tree,
    )
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["source_tree"] = "0" * len(tree)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    report = validate_release_artifact(
        repo,
        artifact_root,
        now=validation_time,
    )
    assert not report.passed
    assert any("source_tree mismatch" in error for error in report.errors)


def test_packaged_rules_must_match_declared_source_commit(tmp_path: Path) -> None:
    repo, commit, tree = make_repository(tmp_path)
    artifact_root, manifest_path, validation_time = make_valid_artifact(
        tmp_path,
        repo,
        commit,
        tree,
    )
    rules_rel = (
        "Windows/WhiteoutStation/Content/Rules/"
        "WhiteoutStationRules.v0.7.json"
    )
    rules_path = artifact_root / rules_rel
    rules = json.loads(rules_path.read_text(encoding="utf-8"))
    rules["tampered_after_build"] = True
    rules_path.write_text(json.dumps(rules), encoding="utf-8")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["checksums"][rules_rel] = sha256_file(rules_path)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    report = validate_release_artifact(
        repo,
        artifact_root,
        now=validation_time,
    )
    assert not report.passed
    assert any(
        "rules JSON does not match the declared source_commit" in error
        for error in report.errors
    )


def test_packaged_agent_runtime_must_match_declared_source_commit(
    tmp_path: Path,
) -> None:
    repo, commit, tree = make_repository(tmp_path)
    artifact_root, manifest_path, validation_time = make_valid_artifact(
        tmp_path,
        repo,
        commit,
        tree,
    )
    agent_rel = (
        "Windows/WhiteoutStation/Content/Agents/"
        "AgentRuntime.v0.7.json"
    )
    agent_path = artifact_root / agent_rel
    agent = json.loads(agent_path.read_text(encoding="utf-8"))
    agent["tampered_after_build"] = True
    agent_path.write_text(json.dumps(agent), encoding="utf-8")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["checksums"][agent_rel] = sha256_file(agent_path)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    report = validate_release_artifact(
        repo,
        artifact_root,
        now=validation_time,
    )
    assert not report.passed
    assert any(
        "Agent runtime JSON does not match the declared source_commit" in error
        for error in report.errors
    )


def test_stale_v07_build_timestamp_is_rejected(tmp_path: Path) -> None:
    repo, commit, tree = make_repository(tmp_path)
    artifact_root, manifest_path, validation_time = make_valid_artifact(
        tmp_path,
        repo,
        commit,
        tree,
    )
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["build_timestamp_utc"] = (
        validation_time - timedelta(days=7)
    ).isoformat().replace("+00:00", "Z")
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    report = validate_release_artifact(
        repo,
        artifact_root,
        now=validation_time,
    )
    assert not report.passed
    assert any("Artifact is stale" in error for error in report.errors)


def test_artifact_checksum_mismatch_is_rejected(tmp_path: Path) -> None:
    repo, commit, tree = make_repository(tmp_path)
    artifact_root, _manifest_path, validation_time = make_valid_artifact(
        tmp_path,
        repo,
        commit,
        tree,
    )
    package = (
        artifact_root
        / "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.pak"
    )
    with package.open("r+b") as handle:
        handle.seek(16)
        handle.write(b"tampered")
    report = validate_release_artifact(
        repo,
        artifact_root,
        now=validation_time,
    )
    assert not report.passed
    assert any("checksum mismatch" in error for error in report.errors)


def test_protected_worktree_path_mismatch_is_rejected(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    protected_file = (
        repo
        / "WhiteoutStation/Content/WindStation/Art/Characters/protected_0.txt"
    )
    protected_file.write_text("changed without approval\n", encoding="utf-8")
    report = validate_protected_assets(repo)
    assert not report.passed
    assert any("worktree/index changes" in error for error in report.errors)


def test_protected_committed_tree_mismatch_is_rejected(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    protected_file = repo / "SourceAssets/MakeHuman/Characters/protected_1.txt"
    protected_file.write_text("changed and committed\n", encoding="utf-8")
    git(repo, "add", "--", protected_file.relative_to(repo).as_posix())
    git(repo, "commit", "-m", "unauthorized protected change")
    report = validate_protected_assets(repo)
    assert not report.passed
    assert any("changed in current HEAD" in error for error in report.errors)


def test_allowlisted_animation_addition_passes(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    write_text(repo, f"{ALLOWED_ADDITION}/walk.uasset", "animation\n")
    git(repo, "add", "--", ALLOWED_ADDITION)
    git(repo, "commit", "-m", "add allowlisted animation")
    report = validate_protected_assets(repo)
    assert report.passed, report.errors


def test_unexpected_addition_below_protected_root_is_rejected(
    tmp_path: Path,
) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    unexpected = f"{DIFF_ONLY_ROOT}/GuHeng/replacement_model.uasset"
    write_text(repo, unexpected, "replacement\n")
    git(repo, "add", "--", unexpected)
    git(repo, "commit", "-m", "add unauthorized model")
    report = validate_protected_assets(repo)
    assert not report.passed
    assert any("Unexpected addition" in error for error in report.errors)


def test_modified_file_below_diff_only_root_is_rejected(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    model = repo / DIFF_ONLY_ROOT / "GuHeng/model.txt"
    model.write_text("modified model\n", encoding="utf-8")
    git(repo, "add", "--", model.relative_to(repo).as_posix())
    git(repo, "commit", "-m", "modify user model")
    report = validate_protected_assets(repo)
    assert not report.passed
    assert any("baseline file changed" in error for error in report.errors)


def test_missing_protected_manifest_fails_closed(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    manifest = repo / "docs/PROTECTED_CHARACTER_ASSETS_v0.7.json"
    manifest.unlink()
    report = validate_protected_assets(repo)
    assert not report.passed
    assert any("Missing protected" in error for error in report.errors)


def test_untracked_protection_manifest_warns_until_final_mode(
    tmp_path: Path,
) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    relative_path = "docs/PROTECTED_CHARACTER_ASSETS_v0.7.json"
    git(repo, "rm", "--cached", "--", relative_path)
    development = validate_protected_assets(repo)
    final = validate_protected_assets(repo, require_tracked=True)
    assert development.passed
    assert any("not tracked" in warning for warning in development.warnings)
    assert not final.passed
    assert any("not tracked" in error for error in final.errors)


def test_tracked_suspicious_key_is_rejected_without_echo(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    secret = ("s" + "k-" + "A" * 24).encode()
    leak_path = repo / "leak.txt"
    leak_path.write_bytes(b"credential=" + secret + b"\n")
    git(repo, "add", "--", "leak.txt")
    report = scan_tracked_secrets(repo)
    assert not report.passed
    joined = "\n".join(report.errors)
    assert "suspicious_sk_key" in joined
    assert secret.decode() not in joined


def test_obvious_test_placeholder_is_safely_allowlisted(tmp_path: Path) -> None:
    repo, _commit, _tree = make_repository(tmp_path)
    placeholder = "s" + "k-test-placeholder-not-a-secret"
    write_text(
        repo,
        "Tools/Release/test_placeholder_fixture.py",
        f'PLACEHOLDER = "{placeholder}"\n',
    )
    git(repo, "add", "--", "Tools/Release/test_placeholder_fixture.py")
    report = scan_tracked_secrets(repo)
    assert report.passed, report.errors


def test_shipping_matrix_covers_endings_and_ai_modes() -> None:
    assert {
        route["ending"]
        for route in EXPECTED_ROUTES.values()
    } == {
        "TaskSuccess",
        "SurvivalWait",
        "CostUncontrolled",
        "TotalCollapse",
    }
    assert {
        scenario.llm_mode
        for scenario in SCENARIOS
    } == {
        "default_missing_key",
        "explicit_offline",
        "loopback_mock",
        "unreachable_endpoint",
    }
    assert len(SCENARIOS) == 9
