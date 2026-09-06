"""Keep application/field policy out of the reusable BLE mechanism."""
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def code(path):
    return re.sub(r"//[^\n]*|/\*.*?\*/", "", (ROOT / path).read_text(), flags=re.S)


class BleBoundaries(unittest.TestCase):
    def test_session_has_no_platform_or_field_dependencies(self):
        for path in ("src/ble_session.cpp", "src/haipower/ble_session.h"):
            text = code(path)
            self.assertNotRegex(text, r"Arduino|esp_|freertos|WiFi|Json|GitHub|fieldprobe")
            self.assertNotRegex(text, r"\b(new|malloc|delay|sleep)\b")

    def test_named_reads_compose_profile_and_session_without_reversing_dependencies(self):
        for path in ("src/ble_session.cpp", "src/haipower/ble_session.h"):
            self.assertNotRegex(code(path), r"profile|signal_read|SignalId|aohai")
        for path in ("src/profile.cpp", "src/haipower/profile.h"):
            self.assertNotRegex(code(path), r"ble_session|signal_read.h|BleRead|Arduino|WiFi")
        for path in ("src/signal_read.cpp", "src/haipower/signal_read.h"):
            self.assertNotRegex(code(path), r"Arduino|esp_|WiFi|GitHub|fieldprobe")
            self.assertNotRegex(code(path), r"\b(new|malloc|delay|sleep)\b")

    def test_adapter_does_not_take_wifi_restart_or_worker_authority(self):
        text = code("src/arduino_bluedroid_link.cpp")
        self.assertNotRegex(text, r"WiFi|esp_restart|ESP\.restart|nvs_flash_erase")
        # Two read-only preflight queries are permitted, no network mutation.
        self.assertEqual(set(re.findall(r"\besp_wifi_\w+", text)),
                         {"esp_wifi_get_mode", "esp_wifi_get_ps"})
        init = text[text.index("Error BluedroidLink::initialize()"):
                    text.index("void BluedroidLink::callback(")]
        self.assertLess(init.index("checkRadioConfiguration()"), init.index("attempted_ = true"))
        self.assertLess(init.index("radio != Error::None"), init.index("xQueueCreate("))
        self.assertNotRegex(text, r"\bbtStart\s*\(")
        self.assertNotRegex(text, r"BLEClient|BLEDevice|NimBLE|xTaskCreate|vTaskDelete|delay\(")
        # The only remote write effects are the codec's bounded characteristic
        # request and the notification CCCD. No raw/configuration side channel.
        self.assertNotRegex(text, r"HTTPClient|WebSocket|HardwareSerial|PubSubClient")

    def test_metadata_versions_are_bound_to_the_same_library(self):
        version = json.loads((ROOT / "library.json").read_text())["version"]
        self.assertIn("version=" + version, (ROOT / "library.properties").read_text())
        self.assertIn('version: "' + version + '"', (ROOT / "idf_component.yml").read_text())
        self.assertIn("VERSION " + version + " LANGUAGES", (ROOT / "CMakeLists.txt").read_text())

    def test_experimental_catalogue_has_no_io_or_reverse_dependencies(self):
        for path in ("src/experimental.cpp", "src/register_value.cpp"):
            text = code(path)
            self.assertNotRegex(text, r"BLE|Arduino|esp_|WiFi|GitHub|fieldprobe|ble_session")
            self.assertNotRegex(text, r"\b(new|malloc|delay|sleep)\b")
        self.assertNotIn("experimental.h", code("src/haipower.h"))  # explicit opt-in

    def test_discovery_parser_is_portable_and_not_a_field_runner(self):
        for path in ("src/ble_discovery.cpp", "src/haipower/ble_discovery.h"):
            text = code(path)
            self.assertNotRegex(text, r"Arduino|esp_|freertos|WiFi|Json|GitHub|fieldprobe")
            self.assertNotRegex(text, r"\b(new|malloc|delay|sleep)\b")

    def test_distribution_contains_no_research_payload_or_old_parser(self):
        for path in ("docs/protocol.md", "docs/android-capture-guide.md",
                     "tools/extract_android_hci.py", "tools/test_extract_android_hci.py"):
            self.assertFalse((ROOT / path).exists(), path)
        for path in ROOT.rglob("*"):
            if any(part in (".git", ".pio", "build", "__pycache__") for part in path.parts):
                continue
            self.assertNotIn(path.suffix.lower(), (".apk", ".xapk", ".pcap", ".pcapng", ".so", ".bin"))
        self.assertNotRegex(code("src/ble_codec.cpp"), r"parse_ble_(read|page)_notification|has_exact_pkcs7_padding")
        for path in ("CMakeLists.txt", "tests/CMakeLists.txt", "library.json", "idf_component.yml",
                     ".github/workflows/ci.yml", "examples/ReadBatteryBle/platformio.ini"):
            self.assertNotRegex((ROOT / path).read_text(), r"/home/|research/|haipower-analysis|field-probe|field-data-private")

    def test_local_markdown_links_resolve(self):
        for path in [ROOT / "README.md", ROOT / "CONTRIBUTING.md", *sorted((ROOT / "docs").glob("*.md"))]:
            for target in re.findall(r"\]\(([^)]+)\)", path.read_text()):
                if "://" in target or target.startswith("#"):
                    continue
                target = target.split("#", 1)[0]
                self.assertTrue((path.parent / target).exists(), f"{path}: {target}")

    def test_common_secret_files_are_excluded_from_git_and_packages(self):
        names = ("token", "token.txt", ".env", ".env.local", "secrets.local.json",
                 "secrets.h", "secrets.local.h", "signing-private.pem", "signing.key",
                 "id_rsa", "id_ed25519")
        paths = [prefix + name for prefix in ("", "examples/example/") for name in names]
        excluded = json.loads((ROOT / "library.json").read_text())["export"]["exclude"]
        for pattern in ("**/token", "**/token.txt", "**/.env", "**/.env.*",
                        "**/secrets.local.json", "**/secrets.h", "**/secrets.local.h",
                        "**/*.pem", "**/*.key", "**/id_rsa", "**/id_ed25519"):
            self.assertIn(pattern, excluded)
        # Packaged libraries need not have .git; the export checks still run.
        if (ROOT / ".git").exists() and shutil.which("git"):
            checked = subprocess.run(["git", "check-ignore", "--no-index", "--stdin"],
                                     cwd=ROOT, input="\n".join(paths) + "\n",
                                     capture_output=True, text=True)
            self.assertEqual(checked.returncode, 0, "git ignore validation failed")
            self.assertEqual(set(checked.stdout.splitlines()), set(paths))

    def test_no_common_credential_markers_in_distributable_files(self):
        # Fast regression guard, NOT a complete history/entropy/secret audit.
        # No user-specific passwords or device identifiers belong in this test.
        patterns = {
            "GitHub credential": rb"\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,})\b",
            "private key": rb"-----BEGIN (?:[A-Z0-9]+ )?PRIVATE KEY-----",
            "AWS access key": rb"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b",
        }
        for path in ROOT.rglob("*"):
            if any(part in (".git", ".pio", "build", "__pycache__") for part in path.relative_to(ROOT).parts):
                continue
            if not path.is_file() or path.is_symlink():
                continue
            data = path.read_bytes()
            for category, pattern in patterns.items():
                if re.search(pattern, data):
                    # Never use assertRegex/assertFalse(match), which can print
                    # the matching bytes (and thus leak a credential in CI).
                    self.fail(f"Potential {category} in {path.relative_to(ROOT)}; value redacted")

    def test_readme_named_examples_compile_and_cover_every_normal_signal(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("C++11 compiler needed to check README examples")
        readme = (ROOT / "README.md").read_text()
        section = readme.split("## Read SOC — no register numbers", 1)[1].split("## Other examples", 1)[0]
        snippets = re.findall(r"```cpp\n(.*?)```", section, re.S)
        groups = [s for s in snippets if s.startswith("const sandisolar::SignalId signals[]")]
        self.assertEqual(len(groups), 5)
        # Compile the exact README fragments. The prelude supplies only the
        # connected-session/Arduino context explicitly described in the text.
        prelude = '''#include <sandisolar.h>
extern sandisolar::BleReadSession session;
sandisolar::BleReadResult result{};
const auto& profile = sandisolar::aohai_modbus_v214_profile();
const sandisolar::SignalId signals[] = {sandisolar::SignalId::BatterySoc};
unsigned long millis();
struct SerialStub {
  template<class... Args> void printf(const char*, Args...) {}
  void println(const char*) {}
} Serial;
'''
        functions = "\n".join(f"void snippet_{i}() {{\n{s}\n}}" for i, s in enumerate(snippets) if s not in groups)
        coverage = '''#include <sandisolar.h>
#include <cassert>
#include <vector>
int main() {
  const auto& profile = sandisolar::aohai_modbus_v214_profile();
  std::vector<bool> covered(profile.field_count, false);
'''
        for group in groups:
            coverage += "{\n" + group + '''
  const auto range = sandisolar::signal_read_range(profile, signals);
  assert(range && range.value.count <= sandisolar::kMaxReadRegisters);
  for (auto signal : signals) {
    for (std::size_t i = 0; i < profile.field_count; ++i)
      if (profile.fields[i].signal == signal) covered[i] = true;
  }
}\n'''
        coverage += "for (bool present : covered) assert(present);\n}\n"
        with tempfile.TemporaryDirectory(prefix="sandisolar-readme-") as directory:
            syntax = Path(directory) / "snippets.cpp"
            syntax.write_text(prelude + functions)
            flags = [compiler, "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-I", str(ROOT / "src")]
            checked = subprocess.run([*flags, "-fsyntax-only", str(syntax)], capture_output=True, text=True)
            self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)
            source = Path(directory) / "coverage.cpp"
            source.write_text(coverage)
            binary = Path(directory) / "coverage"
            built = subprocess.run([*flags, str(source), *map(str, sorted((ROOT / "src").glob("*.cpp"))),
                                    "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)


if __name__ == "__main__":
    unittest.main()
