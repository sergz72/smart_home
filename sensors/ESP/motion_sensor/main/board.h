#ifndef _BOARD_H
#define _BOARD_H

#define BME280_ERROR(code) code
#define BME280_OK(code) !code

void delayms(unsigned int);
unsigned long long int get_time_ms(void);

#endif
