#ifndef _EVENTS_H
#define _EVENTS_H

#include <stddef.h>
#include <stdint.h>

#define VALUE_TYPE_CO2       0
#define VALUE_TYPE_HUMI_INT  1
#define VALUE_TYPE_ICC       2
#define VALUE_TYPE_LUX_INT   3
#define VALUE_TYPE_PRES      4
#define VALUE_TYPE_PWR       4
#define VALUE_TYPE_TEMP_INT  5
#define VALUE_TYPE_VBAT      6
#define VALUE_TYPE_VCC       7
#define VALUE_TYPE_HUMI_EXT  8
#define VALUE_TYPE_LUX_EXT   9
#define VALUE_TYPE_TEMP_EXT  10
#define VALUE_TYPE_HUMI_EXT2 11
#define VALUE_TYPE_LUX_EXT2  12
#define VALUE_TYPE_TEMP_EXT2 13

#define MAX_VALUE_TYPES 8

typedef struct
{
  uint8_t value_types[MAX_VALUE_TYPES];
  uint32_t values[MAX_VALUE_TYPES];
} sensor_event_t;

typedef struct
{
  uint64_t timestampAndSensorId;
  sensor_event_t event;
} sensor_event_with_time_t;

typedef struct
{
  size_t start_idx;
  size_t num_events;
} events_result_t;

#ifdef __cplusplus
extern "C" {
#endif

int init_events(size_t queue_size_bytes);
int delete_events(void);
int add_event(uint32_t device_id, const uint8_t *message, int message_length);
int get_events_from(uint64_t timestamp, events_result_t *result);
size_t get_event_queue_size(void);
size_t get_event_queue_capacity(void);
sensor_event_with_time_t *get_event_queue(void);
size_t get_event_queue_top_idx(void);
size_t get_event_queue_bottom_idx(void);
sensor_event_with_time_t *get_bottom_event(void);

#ifdef __cplusplus
}
#endif

#endif
