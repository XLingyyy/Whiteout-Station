from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


SCANNER_PATH = Path(__file__).with_name("scan_secrets.py")
SPEC = importlib.util.spec_from_file_location("scan_secrets", SCANNER_PATH)
assert SPEC is not None and SPEC.loader is not None
SCANNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SCANNER)


class SecretPatternTests(unittest.TestCase):
    def setUp(self) -> None:
        self.pattern = SCANNER.PATTERNS["api_key_assignment"]

    def test_detects_unquoted_assignment(self) -> None:
        self.assertIsNotNone(self.pattern.search(b"api_key = Abcdefghijklmnopqrstuvwxyz012345"))

    def test_detects_json_assignment(self) -> None:
        self.assertIsNotNone(self.pattern.search(b'\"secret_key\": \"Abcdefghijklmnopqrstuvwxyz012345\"'))

    def test_ignores_api_key_suffix_in_identifier(self) -> None:
        self.assertIsNone(self.pattern.search(b"bRequiresApiKey = IsOfficialDeepSeekEndpoint"))

    def test_ignores_environment_variable_identifier(self) -> None:
        self.assertIsNone(self.pattern.search(b"ApiKey = EnvironmentKey"))


if __name__ == "__main__":
    unittest.main()
