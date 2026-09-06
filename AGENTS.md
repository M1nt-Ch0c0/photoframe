# Photoframe component agent guide

Read this file before changing the loadable display component. For blank-machine setup, cross-repository integration, deployment, or hardware diagnosis, also read the complete project Skill in the sibling host checkout: `../photopainter-host/.agents/skills/develop-photopainter-stack/SKILL.md`.

## Repository role

This repository owns strict PNG decoding, six-color validation, 180-degree pixel packing, AXP2101 setup, GPIO/SPI access, official Spectra 6 E6 timing, and the `.app.elf` and `.so` deliverables. It must not own Wi-Fi, HTTP, authentication, scheduling, secrets, or a complete device firmware.

The host loads the app ELF from independent A/B data slots. Package and stage business updates with the sibling host tools/module.py; no host rebuild is required for ABI-compatible changes. Read ../photopainter-host/docs-module-slots.md. The `.so` remains an additional artifact, not the deployed profile.

## Non-negotiable constraints

- Target only 800×480 Spectra 6 on the ESP32-S3 PhotoPainter.
- Keep SCLK 10, MOSI 11, DC 8, CS 9, RST 12, and BUSY 13.
- Keep the Waveshare-derived E6 initialization, refresh, and shutdown timing traceable to the documented upstream commit and retain its MIT notice.
- Resolve `espressif/elf_loader: ^1.3.3` and `espressif/libpng` from the Component Registry. Never fork or edit managed components.
- Accept only non-interlaced 800×480 PNG whose pixels are fully opaque exact black, white, yellow, red, blue, or green.
- Fully decode and validate before AXP2101, GPIO, SPI, or panel I/O.
- Return success only after final POWER_OFF and BUSY completion.
- Preserve the published C ABI, host bridge symbols, and numeric result codes unless the host is updated atomically.
- Do not add networking, filesystem, SD, WebUI, album, OTA, Home Assistant, or secret storage.

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

After driver changes, inspect both artifacts and test module staging/activation/rollback. Rebuild `../photopainter-host` only when changing its fixed ABI or framework. Do not claim real display success without an authenticated request returning 200 and human confirmation of content, orientation, and colors.

## Secrets and hardware

This repository needs no secrets. Never add credentials, NVS images, device backups, or captured token-bearing requests. Do not run a hardware push merely to validate a decoder change when native tests can prove the behavior.
