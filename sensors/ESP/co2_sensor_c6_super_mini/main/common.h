#ifndef _COMMON_H
#define _COMMON_H

#define DEVICE_ID 3

#ifdef TEST
#define CONFIG_SERVER_ADDRESS_SUFFIX 200
#define HOST_IP_ADDR "192.168.178.200"
#define SEND_INTERVAL 5000 // 5 seconds
#else
#define CONFIG_SERVER_ADDRESS_SUFFIX 254
#define SEND_INTERVAL 290000 // 4 minutes 50 seconds
#define HOST_IP_ADDR "8.8.8.8"
#endif

#define PORT 0

#define BUTTON_GPIO 9
#define LED_GPIO 8
#define LED_STRIP

#define USE_CC1101

#define NO_BLE

#define TEMPERATURE_OFFSET (-1.2f)

#endif
