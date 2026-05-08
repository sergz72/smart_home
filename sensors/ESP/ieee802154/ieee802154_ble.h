#ifndef _IEEE802154_BLE_H
#define _IEEE802154_BLE_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ble_callback_write)(uint16_t conn_handle, uint64_t timestamp);
typedef uint16_t (*ble_callback_read)(uint16_t conn_handle, uint8_t **data);
typedef void (*ble_callback_connect)(uint16_t conn_handle);

esp_err_t ble_init(ble_callback_write _callback_write, ble_callback_read _callback_read,
  ble_callback_connect _callback_connect, ble_callback_connect _callback_disconnect);

#ifdef __cplusplus
}
#endif

#endif
