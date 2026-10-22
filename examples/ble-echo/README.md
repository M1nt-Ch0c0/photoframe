# Authenticated BLE echo (ABI 2.1)

This minimal app registers one service, echoes a written value into its readable
and notifiable characteristic, and unregisters on STOP. The host handles all
radio, authentication, event scheduling and cleanup. No networking, task or
hardware import is needed. No panel refresh occurs because its manifest has no
initial-display requirement. It requires the new ABI 2.1 host; old hosts reject
its manifest/imports before installation.

Service: `a4fe1000-1536-4c88-a2ce-32ecdaac0471`; characteristic index 0:
`a4fe1001-1536-4c88-a2ce-32ecdaac0471` (READ/WRITE/NOTIFY, at most 128 bytes).

Build without hardware using the pinned ESP-IDF and Registry elf_loader 1.3.3:

```sh
idf.py -B build-app-ble -DIDF_TARGET=esp32s3 build
# From photopainter-host:
python3 tools/module.py package --input ../photoframe/examples/ble-echo/build-app-ble/ble_echo.app.elf --output build-ble-abi/ble-echo.pkg --version 1
# From photoframe:
cmake -S tests -B tests/build-ble
cmake --build tests/build-ble
ctest --test-dir tests/build-ble --output-on-failure
```

The checked-in dependency lock keeps elf_loader at 1.3.3. The native test invokes
the actual entry for START, write/connection events and STOP, checking its copy
and completion behavior. The host separately tests UUID restrictions, bounds,
event scheduling and cleanup on unload. These tests do not prove radio behavior.

After **separate hardware authorization**, with the device token privately loaded
into the computer process environment, the expected round trip is:

```sh
ai-quota-frame ble install --device-id 001122334455 --file ble-echo.pkg
ai-quota-frame ble activate --device-id 001122334455 --app ble-echo
ai-quota-frame ble app-write --device-id 001122334455 --app ble-echo --text hello
ai-quota-frame ble app-read --device-id 001122334455 --app ble-echo
ai-quota-frame ble switch --device-id 001122334455 --app photoframe
```

The read must contain `hello`; after switching out, the echo service disappears
and the old service generation is rejected. The permanent discovery and package
service remains available. Direct writes on the custom characteristic also use
the host's signed BLE frame format, not naked text. See the host
[`docs/ble-protocol.md`](https://github.com/M1nt-Ch0c0/photopainter-host/blob/codex/framework-wiki/docs/ble-protocol.md).
