# The ordinary IDF firmware is a build-side placeholder, never a deployable host.
# Add its entry directly to the executable so it cannot extract an application's
# unresolved host imports (or libpng's pngtest main) from a component archive.
set(app_firmware_stub "${CMAKE_CURRENT_BINARY_DIR}/app_firmware_stub.c")
file(GENERATE OUTPUT "${app_firmware_stub}" CONTENT "void app_main(void) {}\n")
target_sources(${CMAKE_PROJECT_NAME}.elf PRIVATE "${app_firmware_stub}")
