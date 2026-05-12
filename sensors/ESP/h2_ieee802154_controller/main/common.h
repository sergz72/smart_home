#ifndef _COMMON_H
#define _COMMON_H

#define DEVICE_ID 2

#define TEST

#ifdef TEST
#define SEND_INTERVAL 10000 // 10 seconds
#else
#define SEND_INTERVAL 300000 // 5 minutes
#endif

#define BUTTON_GPIO 9
#define LED_GPIO 8
#define LED_STRIP

/* 16 Bit SPP Service UUID */
#define BLE_SVC_SPP_UUID16                                  0xABF0

/* 16 Bit SPP Service Characteristic UUID */
#define BLE_SVC_SPP_CHR_UUID16                              0xABF1

#define BLE_DEVICE_NAME "smart-home-service1"

#define NO_WIFI

#endif
