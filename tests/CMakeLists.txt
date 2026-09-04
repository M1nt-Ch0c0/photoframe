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
target_link_libraries(photoframe_native_tests PRIVATE PNG::PNG)
target_compile_options(photoframe_native_tests PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME photoframe_native_tests COMMAND photoframe_native_tests)
