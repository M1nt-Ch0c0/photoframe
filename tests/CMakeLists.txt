# SPDX-FileCopyrightText: 2026 M1nt-Ch0c0
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)
project(photoframe_native_tests C)

find_package(PNG REQUIRED)

add_executable(
    photoframe_native_tests
    test_photoframe.c
    ../components/photoframe/src/photoframe.c
    ../components/photoframe/src/png_decode.c
)
target_include_directories(
    photoframe_native_tests
    PRIVATE
        ../components/photoframe/include
        ../components/photoframe/private_include
)
target_link_libraries(photoframe_native_tests PRIVATE PNG::PNG ZLIB::ZLIB)
target_compile_options(photoframe_native_tests PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME photoframe_native_tests COMMAND photoframe_native_tests)

add_library(ble_echo_entry OBJECT ../examples/ble-echo/main/entry.c)
target_include_directories(ble_echo_entry PRIVATE ../components/app_sdk/include)
target_compile_definitions(ble_echo_entry PRIVATE main=app_entry)
target_compile_options(ble_echo_entry PRIVATE -Wall -Wextra -Werror)
add_executable(ble_echo_native_tests ../examples/ble-echo/test_entry.c $<TARGET_OBJECTS:ble_echo_entry>)
target_include_directories(ble_echo_native_tests PRIVATE ../components/app_sdk/include)
target_compile_options(ble_echo_native_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME ble_echo_native_tests COMMAND ble_echo_native_tests)

add_library(photoframe_entry OBJECT ../main/entry.c ../main/ble_image.c ../main/image_sha256.c)
target_include_directories(photoframe_entry PRIVATE ../components/app_sdk/include ../components/photoframe/include ../main)
target_compile_definitions(photoframe_entry PRIVATE main=app_entry PHOTOFRAME_APP_ENTRY=1 PHOTOFRAME_ABI_2=1
    malloc=image_test_malloc free=image_test_free)
target_compile_options(photoframe_entry PRIVATE -Wall -Wextra -Werror)
add_executable(photoframe_ble_tests test_ble_image.c ../components/photoframe/src/png_decode.c $<TARGET_OBJECTS:photoframe_entry>)
target_include_directories(photoframe_ble_tests PRIVATE ../components/app_sdk/include ../components/photoframe/include
    ../components/photoframe/private_include ../main)
target_compile_options(photoframe_ble_tests PRIVATE -Wall -Wextra -Werror)
target_link_libraries(photoframe_ble_tests PRIVATE PNG::PNG ZLIB::ZLIB)
add_test(NAME photoframe_ble_tests COMMAND photoframe_ble_tests)
