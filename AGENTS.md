# photoframe agent guide

Read this before changing the photoframe app, its decoder, or the examples. Project-wide documentation lives in the [esp32s3 docs](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/README.md). For new-machine setup, cross-repository work, or hardware diagnosis, also read the sibling host skill at `../photopainter-host/.agents/skills/develop-photopainter-stack/SKILL.md`.

## Repository role

This repository owns:

- the photoframe app (`main/`): first screen and cache restore, the app-owned image transaction protocol (`docs/ble-image-input.md`), and its three BLE adapters (ABI 2.1 GATT, ABI 2.2 CoC, ABI 2.3 generic channel);
- strict PNG decoding and six-color validation (`components/photoframe/`);
- the legacy ABI 1 app and `photoframe.so`, which carry their own E6 panel driver;
- example apps under `examples/`;
- a copy of the app SDK in `components/app_sdk/`.

The host firmware (`../photopainter-host`) owns the BLE stack, pairing and authentication, app installation, A/B slots, and, for ABI 2 apps, rotation, packing and all panel/power I/O. Apps never own networking, BLE stack control, Wi-Fi, scheduling tasks, filesystems, self-update, or secrets.

## Constraints

- Target only the 800×480 Spectra 6 ESP32-S3 PhotoPainter.
- Accept only non-interlaced 800×480 PNG, at most 5 MiB, whose pixels are fully opaque exact black, white, yellow, red, blue or green, with no trailing data.
- Fully decode and validate before any display call or any AXP2101, GPIO, SPI or panel I/O.
- Report display success only after the final POWER_OFF/BUSY wait. A completion receipt must never be produced before the host display call returns.
- Keep the PFI1 command and 64-byte receipt format, the opcode/state/error numbers, the GATT service UUIDs, the legacy C ABI, host bridge symbols and `PHOTOFRAME_ERR_*` codes stable unless the host and clients are updated together. Update `docs/ble-image-input.md` in the same change.
- Keep `components/app_sdk/` byte-identical to `../photopainter-host/sdk/`.
- Legacy driver: keep SCLK 10, MOSI 11, DC 8, CS 9, RST 12, BUSY 13; keep the E6 timing traceable to the documented Waveshare commit and retain `LICENSES/Waveshare-PhotoPainter-MIT.txt`.
- Resolve `espressif/elf_loader` (locked 1.3.3) and `espressif/libpng` from the Component Registry. Never fork, vendor, or edit `managed_components/`.
- Extend app communication through the existing host services. Do not extend the host's built-in PNG opcodes.

## Build and test

ESP artifacts require ESP-IDF commit `5e6f53cdb31fe5708eae3f55af9737be2822db22` (v6.0.3); the CMake files enforce it.

```bash
# Native tests (no device, no IDF)
cmake -S tests -B tests/build
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
python3 -m unittest discover -s examples/color-test -p 'test_*.py'

# ESP artifacts
. /path/to/esp-idf/export.sh
idf.py -B build-app -DIDF_TARGET=esp32s3 -DPHOTOFRAME_ARTIFACT=app build
idf.py -B build-so  -DIDF_TARGET=esp32s3 -DPHOTOFRAME_ARTIFACT=so build
```

Deploy only `*.app.elf`, packaged with `../photopainter-host/tools/module.py package`, then installed with `ai-quota-frame ble install` / `ble activate` or the web plugin's developer tools. Never flash the `.bin` files in build directories. Rebuild the host only when its ABI or framework changes.

## Hardware verification

Native tests use display stubs. Prefer native tests over hardware pushes for decoder changes. A real display counts as verified only with an authenticated completion receipt (complete, error 0, matching ID/length/SHA-256) plus human confirmation of content, orientation and colors. Device operations require the user's explicit authorization.

## Secrets

This repository needs no secrets. Never commit credentials, NVS images, device backups, or captured authenticated requests.
