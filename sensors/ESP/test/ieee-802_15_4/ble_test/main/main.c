/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_log.h"
#include "nvs_flash.h"
/* BLE */
#include <ieee802154_ble.h>
#include "esp_nimble_cfg.h"
#include "modlog/modlog.h"

static uint64_t conn_timestamps[CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1];

void callback_write(const uint16_t conn_handle, const uint64_t timestamp)
{
    conn_timestamps[conn_handle] = timestamp;
}

uint16_t callback_read(const uint16_t conn_handle, uint8_t **data)
{
    *data = (uint8_t*)&conn_timestamps[conn_handle];
    return sizeof(uint64_t);
}

void callback_connect(const uint16_t conn_handle)
{
    conn_timestamps[conn_handle] = 0;
}

void callback_disconnect(const uint16_t conn_handle)
{
    conn_timestamps[conn_handle] = 0;
}

void app_main(void)
{
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = ble_init(callback_write, callback_read, callback_connect, callback_disconnect);
    if (ret != ESP_OK) {
        MODLOG_DFLT(ERROR, "Failed to initialize BLE: %d", ret);
    }
}
