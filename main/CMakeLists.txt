# SPDX-FileCopyrightText: 2026 M1nt-Ch0c0
# SPDX-License-Identifier: MIT

set(entry_sources "entry.c")
if(PHOTOFRAME_ARTIFACT STREQUAL "app" AND PHOTOFRAME_ABI STREQUAL "2")
    list(APPEND entry_sources "ble_image.c" "image_sha256.c")
endif()
idf_component_register(SRCS ${entry_sources}
                       REQUIRES photoframe app_sdk)

if(PHOTOFRAME_ARTIFACT STREQUAL "app")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHOTOFRAME_APP_ENTRY=1)
endif()

if(PHOTOFRAME_ABI STREQUAL "2")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHOTOFRAME_ABI_2=1)
endif()
