#ifndef _BOARD_H
#define _BOARD_H

#define BME280_ERROR(code) code
#define BME280_OK(code) !code

#define PD_ONEDELTA 200
#define PD_ZERODELTA 400
#define PD_WAITTIME 8500

void delayms(unsigned int);

#endif
