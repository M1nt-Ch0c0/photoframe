# SPDX-FileCopyrightText: 2026 M1nt-Ch0c0
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

set(PHOTOFRAME_REQUIRED_IDF_COMMIT
    "5e6f53cdb31fe5708eae3f55af9737be2822db22")
execute_process(
    COMMAND git -C "$ENV{IDF_PATH}" rev-parse HEAD
    RESULT_VARIABLE PHOTOFRAME_IDF_GIT_RESULT
    OUTPUT_VARIABLE PHOTOFRAME_IDF_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT PHOTOFRAME_IDF_GIT_RESULT EQUAL 0 OR
   NOT PHOTOFRAME_IDF_COMMIT STREQUAL PHOTOFRAME_REQUIRED_IDF_COMMIT)
    message(FATAL_ERROR
        "photoframe requires ESP-IDF commit ${PHOTOFRAME_REQUIRED_IDF_COMMIT}")
endif()

set(PHOTOFRAME_ARTIFACT "app" CACHE STRING "ELF-loader artifact to build: app or so")
set_property(CACHE PHOTOFRAME_ARTIFACT PROPERTY STRINGS app so)
set(PHOTOFRAME_ABI "2" CACHE STRING "Application ABI: 1 (legacy) or 2")
if(NOT PHOTOFRAME_ABI MATCHES "^[12]$")
    message(FATAL_ERROR "PHOTOFRAME_ABI must be 1 or 2")
endif()
if(PHOTOFRAME_ARTIFACT STREQUAL "so")
    set(PHOTOFRAME_ABI "1")
endif()

if(NOT PHOTOFRAME_ARTIFACT STREQUAL "app" AND
   NOT PHOTOFRAME_ARTIFACT STREQUAL "so")
    message(FATAL_ERROR "PHOTOFRAME_ARTIFACT must be 'app' or 'so'")
endif()

# Keep each profile's generated sdkconfig inside its build directory.  This
# prevents an app build and an SO build from silently sharing incompatible
# CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT values.
if(NOT DEFINED SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()

if(PHOTOFRAME_ARTIFACT STREQUAL "so")
    set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.so.defaults")
else()
    set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.app.defaults")
endif()

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(photoframe)

if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "photoframe supports only IDF_TARGET=esp32s3")
endif()

# The SO profile still participates in ESP-IDF's ordinary firmware link, whose
# generated startup code requires app_main.  Put a no-op implementation in the
# build directory and attach it only to the normal main component target.
# project_so() deliberately excludes build-directory sources, so this stub is
# not part of photoframe.so and does not expand the module ABI.
if(PHOTOFRAME_ARTIFACT STREQUAL "so")
    set(PHOTOFRAME_SO_APP_STUB
        "${CMAKE_CURRENT_BINARY_DIR}/photoframe_so_app_stub.c")
    file(GENERATE OUTPUT "${PHOTOFRAME_SO_APP_STUB}"
        CONTENT "void app_main(void) {}\n")
    idf_component_get_property(PHOTOFRAME_MAIN_LIB main COMPONENT_LIB)
    target_sources(${PHOTOFRAME_MAIN_LIB} PRIVATE "${PHOTOFRAME_SO_APP_STUB}")
endif()

# project_so() recursively recompiles every C file below each ELF_COMPONENTS
# directory without ESP-IDF's normal toolchain response file.  Keep the SO
# list empty: Registry components contain non-library sources, and even the
# project component must retain ESP-IDF's selected libc/toolchain flags.  The
# app-ELF path consumes registered archives directly and is unaffected.
if(PHOTOFRAME_ARTIFACT STREQUAL "so")
    set(ELF_COMPONENTS "")
else()
    set(ELF_COMPONENTS
        photoframe
        espressif__libpng
        espressif__zlib
    )
endif()

# elf_loader 1.3.3's project_so() recompiles component sources with -fPIC and
# reads the legacy COMPILE_INCLUDE_DIRECTORIES build property.  ESP-IDF 6 no
# longer populates that property, so collect the registered components' target
# usage here.  This is a project-side compatibility shim; the Registry
# component itself remains untouched.
if(PHOTOFRAME_ARTIFACT STREQUAL "so")
    # Link the implementation, libpng, and zlib from the exact archives
    # produced by their registered component targets.  Compile those archives
    # as PIC so they remain valid shared-object input; Registry sources stay
    # untouched.
    set(ELF_LIBS "")
    set(PHOTOFRAME_SO_ARCHIVE_TARGETS "")
    foreach(PHOTOFRAME_PIC_COMPONENT
            photoframe
            espressif__libpng
            espressif__zlib)
        idf_component_get_property(PHOTOFRAME_PIC_COMPONENT_LIB
            ${PHOTOFRAME_PIC_COMPONENT} COMPONENT_LIB)
        # project_so() links these already-compiled archives.  Build them PIC
        # and hide their implementation symbols so --gc-sections retains only
        # the public photoframe ABI and its transitive decoder path.
        target_compile_options(${PHOTOFRAME_PIC_COMPONENT_LIB}
            PRIVATE -fPIC -fvisibility=hidden)
        list(APPEND PHOTOFRAME_SO_ARCHIVE_TARGETS
            ${PHOTOFRAME_PIC_COMPONENT_LIB})

        get_target_property(PHOTOFRAME_PIC_COMPONENT_BINARY_DIR
            ${PHOTOFRAME_PIC_COMPONENT_LIB} BINARY_DIR)
        get_target_property(PHOTOFRAME_PIC_COMPONENT_OUTPUT_NAME
            ${PHOTOFRAME_PIC_COMPONENT_LIB} OUTPUT_NAME)
        if(NOT PHOTOFRAME_PIC_COMPONENT_BINARY_DIR OR
           NOT PHOTOFRAME_PIC_COMPONENT_OUTPUT_NAME)
            message(FATAL_ERROR
                "Unable to locate archive for ${PHOTOFRAME_PIC_COMPONENT}")
        endif()
        set(PHOTOFRAME_PIC_COMPONENT_ARCHIVE
            "${PHOTOFRAME_PIC_COMPONENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${PHOTOFRAME_PIC_COMPONENT_OUTPUT_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX}")
        list(APPEND ELF_LIBS "${PHOTOFRAME_PIC_COMPONENT_ARCHIVE}")
    endforeach()

    # A normal archive contributes objects only after an unresolved reference.
    # This tiny generated translation unit makes the public entry point such a
    # reference.  project_so() compiles it alongside main; the implementation
    # itself still comes from ESP-IDF's correctly configured PIC archive.
    set(PHOTOFRAME_SO_LINK_ANCHOR
        "${CMAKE_CURRENT_BINARY_DIR}/photoframe_so_link_anchor.c")
    file(GENERATE OUTPUT "${PHOTOFRAME_SO_LINK_ANCHOR}"
        CONTENT [=[
extern int photoframe_render_png(const unsigned char *data,
                                 unsigned int size);

/* Keep an externally visible relocation so the archive member containing the
 * documented entry point is retained even with --gc-sections. */
__attribute__((used, visibility("hidden")))
int photoframe_so_link_anchor(const unsigned char *data, unsigned int size)
{
    return photoframe_render_png(data, size);
}
]=])
    list(PREPEND ELF_LIBS "${PHOTOFRAME_SO_LINK_ANCHOR}")

    idf_build_get_property(PHOTOFRAME_BUILD_COMPONENTS BUILD_COMPONENTS)
    set(PHOTOFRAME_SO_INCLUDE_DIRS "")
    foreach(PHOTOFRAME_BUILD_COMPONENT IN LISTS PHOTOFRAME_BUILD_COMPONENTS)
        idf_component_get_property(PHOTOFRAME_COMPONENT_LIB
            ${PHOTOFRAME_BUILD_COMPONENT} COMPONENT_LIB)
        foreach(PHOTOFRAME_INCLUDE_PROPERTY
                INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES)
            get_target_property(PHOTOFRAME_COMPONENT_INCLUDE_DIRS
                ${PHOTOFRAME_COMPONENT_LIB} ${PHOTOFRAME_INCLUDE_PROPERTY})
            if(PHOTOFRAME_COMPONENT_INCLUDE_DIRS)
                list(APPEND PHOTOFRAME_SO_INCLUDE_DIRS
                    ${PHOTOFRAME_COMPONENT_INCLUDE_DIRS})
            endif()
        endforeach()
    endforeach()
    list(FILTER PHOTOFRAME_SO_INCLUDE_DIRS EXCLUDE REGEX "-NOTFOUND$")
    list(REMOVE_DUPLICATES PHOTOFRAME_SO_INCLUDE_DIRS)
    idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES
        "${PHOTOFRAME_SO_INCLUDE_DIRS}")
endif()

include(elf_loader)
if(PHOTOFRAME_ARTIFACT STREQUAL "so")
    project_so(photoframe)
    # project_so() only adds build dependencies for ELF_COMPONENTS.  These
    # project-owned PIC archives arrive through ELF_LIBS, so make their order
    # dependency explicit as well.
    add_dependencies(so ${PHOTOFRAME_SO_ARCHIVE_TARGETS})
else()
    include("${CMAKE_CURRENT_LIST_DIR}/components/app_sdk/app_elf.cmake")
    project_elf(photoframe)
endif()
