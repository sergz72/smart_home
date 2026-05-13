#ifndef _EVENTS_H
#define _EVENTS_H

#include <stddef.h>
#include <stdint.h>
#include "board.h"

#define VALUE_TYPE_CO2       1
#define VALUE_TYPE_HUMI_INT  2
#define VALUE_TYPE_ICC       3
#define VALUE_TYPE_LUX_INT   4
#define VALUE_TYPE_PRES      5
#define VALUE_TYPE_PWR       6
#define VALUE_TYPE_TEMP_INT  7
#define VALUE_TYPE_VBAT      8
#define VALUE_TYPE_VCC       9
#define VALUE_TYPE_HUMI_EXT  10
#define VALUE_TYPE_LUX_EXT   11
#define VALUE_TYPE_TEMP_EXT  12
#define VALUE_TYPE_HUMI_EXT2 13
#define VALUE_TYPE_LUX_EXT2  14
#define VALUE_TYPE_TEMP_EXT2 15

#define MAX_VALUE_TYPES 8

#define EVENT_QUEUE_SIZE (EVENT_QUEUE_DAYS * EVENT_QUEUE_MAX_SENSORS * EVENT_QUEUE_MAX_EVENTS_PER_DAY)

typedef struct
{
  uint64_t timestampAndSensorId;
  uint32_t values[MAX_VALUE_TYPES];
} sensor_event_with_time_t;

typedef struct
{
  sensor_event_with_time_t *start;
  int start_idx;
} events_result_t;

#ifdef __cplusplus
extern "C" {
#endif

void init_events(void);
int add_event(uint32_t device_id, const uint8_t *message, int message_length);
int get_events_from(unsigned int client_id, uint64_t timestamp, events_result_t *result);
int get_event_queue_size(void);
sensor_event_with_time_t *get_event_queue(void);
int get_event_queue_top_idx(void);
int get_event_queue_bottom_idx(void);
sensor_event_with_time_t *get_bottom_event(void);

#ifdef __cplusplus
}
#endif

#endif
