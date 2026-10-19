# SPDX-FileCopyrightText: 2026 M1nt-Ch0c0
# SPDX-License-Identifier: MIT

idf_component_register(SRCS "entry.c"
                       REQUIRES photoframe app_sdk)

if(PHOTOFRAME_ARTIFACT STREQUAL "app")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHOTOFRAME_APP_ENTRY=1)
endif()

if(PHOTOFRAME_ABI STREQUAL "2")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHOTOFRAME_ABI_2=1)
endif()
