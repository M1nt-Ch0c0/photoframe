# SPDX-FileCopyrightText: 2026 M1nt-Ch0c0
# SPDX-License-Identifier: MIT

idf_component_register(
    SRCS
        "src/photoframe.c"
        "src/png_decode.c"
        "src/axp2101.c"
        "src/epd_e6.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include"
    PRIV_REQUIRES
        espressif__libpng
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_spi
        freertos
        heap
)
