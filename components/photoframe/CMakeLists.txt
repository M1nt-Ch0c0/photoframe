# SPDX-License-Identifier: MIT
set(frame_sources "src/png_decode.c")
set(frame_requires espressif__libpng)
if(NOT PHOTOFRAME_ABI STREQUAL "2")
    list(APPEND frame_sources "src/photoframe.c" "src/axp2101.c" "src/epd_e6.c")
    list(APPEND frame_requires esp_driver_gpio esp_driver_i2c esp_driver_spi freertos heap)
endif()
idf_component_register(SRCS ${frame_sources} INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include" PRIV_REQUIRES ${frame_requires})
