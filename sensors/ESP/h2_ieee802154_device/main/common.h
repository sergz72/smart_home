#ifndef _COMMON_H
#define _COMMON_H

#define TEST

#ifdef TEST
#define SEND_INTERVAL 5000 // 5 seconds
#else
#define SEND_INTERVAL 300000 // 5 minutes
#endif

#define LED_GPIO 8
#define LED_STRIP

//#define USE_SHT4X
#define USE_SCD4X

#endif
