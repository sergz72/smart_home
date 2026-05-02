#ifndef _COMMON_H
#define _COMMON_H

#ifdef TEST
#define SEND_INTERVAL 5000 // 5 seconds
#else
#define SEND_INTERVAL 300000 // 5 minutes
#endif

#define LED_GPIO 8
#define LED_STRIP

#define CONFIG_SERVER_ADDRESS_SUFFIX 254
#define HOST_IP_ADDR "192.168.178.200"
#define PORT 59999

#define USE_SCD4X

#define TEMPERATURE_OFFSET (0.0f)

#endif
