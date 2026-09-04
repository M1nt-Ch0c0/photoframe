/*
 * SPDX-FileCopyrightText: 2026 M1nt-Ch0c0
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

#include "photoframe.h"
#include "photoframe_internal.h"

#define AXP2101_I2C_PORT I2C_NUM_0
#define AXP2101_PIN_SCL GPIO_NUM_48
#define AXP2101_PIN_SDA GPIO_NUM_47
#define AXP2101_I2C_ADDRESS 0x34u
#define AXP2101_I2C_CLOCK_HZ 100000u
#define AXP2101_TRANSFER_TIMEOUT_MS 1000

#define AXP2101_REG_CHIP_ID 0x03u
#define AXP2101_CHIP_ID 0x4au
#define AXP2101_REG_ALDO_ENABLE 0x90u
#define AXP2101_REG_ALDO3_VOLTAGE 0x94u

static esp_err_t axp2101_read_register(i2c_master_dev_handle_t device,
                                       uint8_t reg,
                                       uint8_t *value)
{
    return i2c_master_transmit_receive(device,
                                       &reg,
                                       sizeof(reg),
                                       value,
                                       sizeof(*value),
                                       AXP2101_TRANSFER_TIMEOUT_MS);
}

static esp_err_t axp2101_write_register(i2c_master_dev_handle_t device,
                                        uint8_t reg,
                                        uint8_t value)
{
    const uint8_t transfer[] = {reg, value};
    return i2c_master_transmit(device,
                               transfer,
                               sizeof(transfer),
                               AXP2101_TRANSFER_TIMEOUT_MS);
}

static esp_err_t axp2101_write_and_verify(i2c_master_dev_handle_t device,
                                          uint8_t reg,
                                          uint8_t value)
{
    esp_err_t err = axp2101_write_register(device, reg, value);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t readback = 0;
    err = axp2101_read_register(device, reg, &readback);
    if (err != ESP_OK) {
        return err;
    }
    return readback == value ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

int photoframe_axp2101_enable_epd(void)
{
    i2c_master_bus_handle_t bus = NULL;
    i2c_master_dev_handle_t device = NULL;
    int result = PHOTOFRAME_ERR_IO;

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = AXP2101_I2C_PORT,
        .sda_io_num = AXP2101_PIN_SDA,
        .scl_io_num = AXP2101_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&bus_config, &bus) != ESP_OK) {
        goto cleanup;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_I2C_ADDRESS,
        .scl_speed_hz = AXP2101_I2C_CLOCK_HZ,
    };
    if (i2c_master_bus_add_device(bus, &device_config, &device) != ESP_OK) {
        goto cleanup;
    }

    uint8_t value = 0;
    if (axp2101_read_register(device, AXP2101_REG_CHIP_ID, &value) !=
            ESP_OK ||
        value != AXP2101_CHIP_ID) {
        goto cleanup;
    }

    if (axp2101_read_register(device,
                              AXP2101_REG_ALDO3_VOLTAGE,
                              &value) != ESP_OK) {
        goto cleanup;
    }
    value = photoframe_axp2101_aldo3_voltage_value(value);
    if (axp2101_write_and_verify(device,
                                 AXP2101_REG_ALDO3_VOLTAGE,
                                 value) != ESP_OK) {
        goto cleanup;
    }

    if (axp2101_read_register(device,
                              AXP2101_REG_ALDO_ENABLE,
                              &value) != ESP_OK) {
        goto cleanup;
    }
    value = photoframe_axp2101_aldo3_enable_value(value);
    if (axp2101_write_and_verify(device,
                                 AXP2101_REG_ALDO_ENABLE,
                                 value) != ESP_OK) {
        goto cleanup;
    }

    result = PHOTOFRAME_OK;

cleanup:
    if (device != NULL && i2c_master_bus_rm_device(device) != ESP_OK) {
        result = PHOTOFRAME_ERR_IO;
    }
    if (bus != NULL && i2c_del_master_bus(bus) != ESP_OK) {
        result = PHOTOFRAME_ERR_IO;
    }
    return result;
}
