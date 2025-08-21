#ifndef _COMMON_H
#define _COMMON_H

#define DEVICE_ID 4

#ifdef TEST
#define CONFIG_SERVER_ADDRESS_SUFFIX 200
#define HOST_IP_ADDR "192.168.178.200"
#define SEND_INTERVAL 5000 // 5 seconds
#else
#define CONFIG_SERVER_ADDRESS_SUFFIX 254
#define SEND_INTERVAL 290000 // 4 minutes 50 seconds
#define HOST_IP_ADDR ""
#endif

#define PORT 0

#define BUTTON_GPIO 9
#define LED_GPIO 48
#define LED_STRIP

//#define NO_BLE
#define DEVICE_NAME "co2-sensor-4"

#define TEMPERATURE_OFFSET (-1.2f)

#endif
