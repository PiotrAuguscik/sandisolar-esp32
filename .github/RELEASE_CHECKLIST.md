# Public release checklist

- [x] Set the GitHub account in `library.properties`, `library.json` and
      `idf_component.yml`.
- [x] Document GitHub private vulnerability reporting in `SECURITY.md`.
- [x] Confirm the public name: `sandisolar-esp32` (SANDISOLAR ESP32).
- [x] Confirm the GitHub repository: `PiotrAuguscik/sandisolar-esp32`, branch `main`.
- [ ] Review the exact snapshot and Git refs selected for publication.
- [ ] Run host tests in GitHub Actions.
- [ ] Compile affected Arduino examples with the supported ESP32 core version.
- [ ] Inspect the packaged file list: code, tests, examples and API docs only;
      no APK, private capture, credentials, research notes or build artifacts.
- [ ] Confirm that no capture contains serial numbers, credentials or site data.
- [ ] Re-scan working files, staged content, all Git refs/history and the actual
      package for credentials before every publication. Do not print matches.
      Ignore rules alone are not evidence that history or a package is clean.
- [ ] Verify upstream app/register-map links and preserve image attribution;
      externally linked material is not relicensed under this project's MIT.
- [ ] Publish a release/tag only as experimental while scanner/session hardware
      qualification is pending. Do not imply RS-485 physical testing.
- [ ] Before promoting BLE support: test real discovery, sustained reads,
      reconnects, heap stability, Wi-Fi coexistence and SOC below 100%.
