#ifndef _BOARD_H
#define _BOARD_H

#ifndef NULL
#define NULL 0
#endif

#define BME280_ERROR(code) code
#define BME280_OK(code) !code

#define PD_ONEDELTA 200
#define PD_ZERODELTA 400
#define PD_WAITTIME 8500

#define CC1101_TIMEOUT 0xFFFFF

#ifdef PIN_NUM_GDO0
#define cc1101_GD2 gpio_get_level(PIN_NUM_GDO2)
#define cc1101_GD0 gpio_get_level(PIN_NUM_GDO0)
#else
#define cc1101_GD2 0
#define cc1101_GD0 0
#endif

#define delay(x)
#define cc1101_CSN_CLR(x)
#define cc1101_CSN_SET(x)

void delayms(unsigned int);

#endif
