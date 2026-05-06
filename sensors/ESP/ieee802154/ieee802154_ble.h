#ifndef _IEEE802154_BLE_H
#define _IEEE802154_BLE_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*ble_callback_func)(uint8_t *message, size_t message_length, uint8_t **response, size_t *response_length);

esp_err_t ble_init(ble_callback_func callback);

#ifdef __cplusplus
}
#endif

#endif
