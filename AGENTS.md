# Photoframe component agent guide

**Current development baseline (2026-10-29):** photoframe v21 / ABI 2.3 shares a pure C transaction core across legacy GATT, CoC and the generic mailbox. Phone Web GATT and physical completion/user visual confirmation passed; the historical v20/Mac CoC evidence remains scoped to that run. Source, deployment and platform limits are tracked in the [status matrix](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/09-status.md).

Read this file before changing the loadable display component. For blank-machine setup, cross-repository integration, deployment, or hardware diagnosis, also read the complete project Skill in the sibling host checkout: `../photopainter-host/.agents/skills/develop-photopainter-stack/SKILL.md`.

**2026-10-29 source contract:** pairing v2 is host-owned; v21 declares ABI 2.3 while retaining PFI1 v1 and legacy ABI layouts. Saved-bond phone authentication and image completion are verified; fresh-pair/reset acceptance is not inferred. The prior offline-only restriction was lifted for the explicitly authorized recorded deployments. Future device operations still require authorization.

## Repository role

This repository owns strict PNG decoding, six-color validation, app-owned first screens/restoration and independent `.app.elf` deliverables. Default ABI 2 uses the host board service for rotation, packing and E6/power I/O; legacy ABI 1 and `.so` retain the old driver. It must not own Wi-Fi, HTTP, authentication, scheduling, secrets, or a complete device firmware.

The host loads the app ELF from independent A/B data slots. Package and stage business updates with the sibling host tools/module.py; no host rebuild is required for ABI-compatible changes. Read ../photopainter-host/docs-module-slots.md. The `.so` remains an additional artifact, not the deployed profile. The [developer Wiki](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/README.md) is the framework entry point; application-specific input protocols and decoder behavior belong here.

The current app uses the ABI 2.3 generic mailbox and retains ABI 2.2 CoC and ABI 2.1 GATT adapters. Image reception, integrity checks, transfer state and completion receipts remain application-owned. Use the existing managed BLE/auth/display services and manager; do not extend the dedicated host PNG protocol. See docs/ble-image-input.md and docs/validation-ble-input-2026-10-23.md for the implemented protocol and hardware limits.

## Non-negotiable constraints

- Target only 800×480 Spectra 6 on the ESP32-S3 PhotoPainter.
- Keep SCLK 10, MOSI 11, DC 8, CS 9, RST 12, and BUSY 13.
- Keep the Waveshare-derived E6 initialization, refresh, and shutdown timing traceable to the documented upstream commit and retain its MIT notice.
- Resolve `espressif/elf_loader: ^1.3.3` and `espressif/libpng` from the Component Registry. Never fork or edit managed components.
- Accept only non-interlaced 800×480 PNG whose pixels are fully opaque exact black, white, yellow, red, blue, or green.
- Fully decode and validate before AXP2101, GPIO, SPI, or panel I/O.
- Return success only after final POWER_OFF and BUSY completion.
- Preserve the published C ABI, host bridge symbols, and numeric result codes unless the host is updated atomically.
- Do not own the network/BLE stack or add direct system networking, filesystem, SD, WebUI, album, self-OTA, Home Assistant, or secret storage. Application-level protocols through ABI 2.1 GATT, ABI 2.2 CoC and ABI 2.3 generic host messages are supported; installation remains the host's job.

## Build and test

Use ESP-IDF commit `5e6f53cdb31fe5708eae3f55af9737be2822db22` for ESP artifacts.

```bash
cmake -S tests -B tests/build
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure

. /path/to/esp-idf/export.sh
idf.py -B build-app -DIDF_TARGET=esp32s3 \
  -DPHOTOFRAME_ARTIFACT=app build
idf.py -B build-so -DIDF_TARGET=esp32s3 \
  -DPHOTOFRAME_ARTIFACT=so build
```

For ABI 2 use the mirrored `components/app_sdk` header and the host `docs-runtime-v2.md` contract. Never import background tasks or board APIs into ABI 2 apps. Keep the SDK copy byte-identical to host.

After legacy driver changes, inspect both artifacts and test module staging/activation/rollback. Rebuild `../photopainter-host` only when changing its fixed ABI or framework. Do not claim real display success without HTTP 200 or an authenticated BLE image-completion receipt, plus human confirmation of content, orientation, and colors. The old host image path reuses PNG INPUT for compatibility; the app-owned path requires the matching GATT/CoC service and its own tested receive protocol.

## Secrets and hardware

This repository needs no secrets. Never add credentials, NVS images, device backups, or captured token-bearing requests. Do not run a hardware push merely to validate a decoder change when native tests can prove the behavior.
