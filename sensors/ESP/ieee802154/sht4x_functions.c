#include "sht4x_functions.h"
#include <string.h>
#include "events.h"
#include "hal.h"

uint8_t sht_event[12];

void sht_event_convert(const int16_t temperature, const uint16_t humidity)
{
  int32_t value;
  value = temperature;
  memcpy(sht_event, &value, 3);
  sht_event[3] = VALUE_TYPE_TEMP_EXT;
  value = humidity;
  memcpy(sht_event+4, &value, 3);
  sht_event[7] = VALUE_TYPE_HUMI_EXT;
  value = get_vbat();
  memcpy(sht_event + 8, &value, 3);
  sht_event[11] = VALUE_TYPE_VBAT;
}
