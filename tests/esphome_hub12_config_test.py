"""Exercise the external component with an installed ESPHome CLI.

Run: ESPHOME=/path/to/esphome python3 tests/esphome_hub12_config_test.py
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = (ROOT / "examples/esphome/hub12-test.yaml").read_text()
TEMPLATE = TEMPLATE.replace("../../esphome/components", str(ROOT / "esphome/components"))
TEMPLATE = TEMPLATE.replace("../../components/hub75", str(ROOT / "components/hub75"))
ESPHOME = os.environ.get("ESPHOME", "esphome")


class Hub12ConfigTest(unittest.TestCase):
    def check_config(self, text, expected, message=None):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.yaml"
            path.write_text(text)
            result = subprocess.run([ESPHOME, "config", str(path)], capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode == 0, expected, result.stdout + result.stderr)
        if message:
            self.assertIn(message, result.stdout + result.stderr)

    def test_single_panel_and_two_rows(self):
        self.check_config(TEMPLATE, True)
        self.check_config(TEMPLATE.replace(f"    library_path: {ROOT / 'components/hub75'}\n", ""), True)
        self.check_config(TEMPLATE.replace("layout_rows: 1", "layout_rows: 2").replace(
            "latch_pins: [GPIO32]", "latch_pins: [GPIO32, GPIO33]"
        ), True)
        self.check_config(TEMPLATE + "\nscript:\n  - id: turn_dark\n    then:\n"
                          "      - hub12.set_brightness:\n          id: matrix\n          brightness: 0\n", True)

    def test_rejects_unsupported_or_unsafe_configurations(self):
        s3 = TEMPLATE.replace("variant: esp32", "variant: esp32s3")
        for original, replacement in ((23, 4), (18, 5), (22, 6), (21, 7), (25, 15), (32, 16)):
            s3 = s3.replace(f"GPIO{original}", f"GPIO{replacement}")
        self.check_config(s3, False, "original ESP32")
        self.check_config(TEMPLATE.replace("layout_rows: 1", "layout_rows: 2"), False, "one GPIO per panel row")
        self.check_config(TEMPLATE.replace("data_pin: GPIO23", "data_pin: GPIO22"), False, "GPIOs must be distinct")
        self.check_config(TEMPLATE.replace(str(ROOT / "components/hub75"), "/missing/hub75"), False, "library_path")

        second = TEMPLATE.split("display:\n", 1)[1].replace("id: matrix", "id: second_matrix")
        for original, replacement in ((23, 26), (18, 27), (22, 13), (21, 14), (25, 16), (32, 17)):
            second = second.replace(f"GPIO{original}", f"GPIO{replacement}")
        self.check_config(TEMPLATE + second, False, "only one HUB12 or HUB75 display")

    def test_two_row_codegen_uses_both_latches_and_local_library(self):
        text = TEMPLATE.replace("layout_rows: 1", "layout_rows: 2").replace(
            "latch_pins: [GPIO32]", "latch_pins: [GPIO32, GPIO33]"
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.yaml"
            path.write_text(text)
            result = subprocess.run(
                [ESPHOME, "compile", "--only-generate", str(path)], capture_output=True, text=True, check=False
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            build = Path(directory) / ".esphome/build/hub12-test/src"
            main = (build / "main.cpp").read_text()
            manifest = (build / "idf_component.yml").read_text()
            self.assertIn(".latch = {32, 33}", main)
            self.assertIn(".enabled = true", main)
            self.assertIn(str(ROOT / "components/hub75"), manifest)

    def test_git_external_component_uses_library_from_same_checkout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo = root / "source"
            repo.mkdir()
            shutil.copytree(ROOT / "esphome/components/hub12", repo / "esphome/components/hub12",
                            ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
            shutil.copytree(ROOT / "components/hub75", repo / "components/hub75")
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            subprocess.run(["git", "-C", str(repo), "add", "esphome", "components"], check=True)
            subprocess.run(["git", "-C", str(repo), "-c", "user.name=Test", "-c", "user.email=test@example.com",
                            "commit", "-qm", "fixture"], check=True)
            text = TEMPLATE.replace(
                "type: local\n      path: " + str(ROOT / "esphome/components"),
                "type: git\n      url: " + repo.as_uri() + "\n      path: esphome/components",
            ).replace(f"    library_path: {ROOT / 'components/hub75'}\n", "")
            path = root / "test.yaml"
            path.write_text(text)
            result = subprocess.run([ESPHOME, "compile", "--only-generate", str(path)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            manifest = (root / ".esphome/build/hub12-test/src/idf_component.yml").read_text()
            self.assertIn("external_components", manifest)
            self.assertIn("components/hub75", manifest)
            self.assertNotIn(str(ROOT / "components/hub75"), manifest)


if __name__ == "__main__":
    unittest.main()
