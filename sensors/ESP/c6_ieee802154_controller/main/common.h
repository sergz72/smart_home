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

#endif
