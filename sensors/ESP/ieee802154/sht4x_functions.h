#ifndef _SHT4X_FUNCTIONS_H
#define _SHT4X_FUNCTIONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void sht_event_convert(int16_t temperature, uint16_t humidity);

#ifdef __cplusplus
}
#endif

extern uint8_t sht_event[12];

#endif
