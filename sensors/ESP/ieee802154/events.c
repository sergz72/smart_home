#include "events.h"
#include <string.h>
#include <time.h>

static sensor_event_with_time_t event_queue[EVENT_QUEUE_SIZE];
static sensor_event_with_time_t event_search_results[EVENT_QUEUE_MAX_CLIENTS][EVENT_QUEUE_MAX_EVENTS_PER_MESSAGE];
static int event_queue_top_idx;
static int event_queue_bottom_idx;

void init_events(void)
{
  event_queue_top_idx = event_queue_bottom_idx = 0;
}

sensor_event_with_time_t *get_event_queue(void)
{
  return event_queue;
}

int get_event_queue_top_idx(void)
{
  return event_queue_top_idx;
}

int get_event_queue_bottom_idx(void)
{
  return event_queue_bottom_idx;
}

static uint64_t get_time_ms()
{
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (uint64_t)tp.tv_sec * 1000 + (uint64_t)tp.tv_nsec / 1000000;
}

int add_event(const uint32_t device_id, const uint8_t *message, const int message_length)
{
  if (message_length <= 0 || message_length > MAX_VALUE_TYPES * sizeof(uint32_t) ||
      (message_length % sizeof(uint32_t) != 0))
    return 1;
  sensor_event_with_time_t *bottom = &event_queue[event_queue_bottom_idx];
  memset(bottom, 0, sizeof(sensor_event_with_time_t));
  bottom->timestampAndSensorId = (get_time_ms() << 8) | device_id;
  memcpy(bottom->values, message, message_length);
  event_queue_bottom_idx++;
  if (event_queue_bottom_idx == EVENT_QUEUE_SIZE)
    event_queue_bottom_idx = 0;
  if (event_queue_bottom_idx == event_queue_top_idx)
    event_queue_top_idx++;
  return 0;
}

int get_event_queue_size(void)
{
  return event_queue_bottom_idx >= event_queue_top_idx ? event_queue_bottom_idx - event_queue_top_idx : EVENT_QUEUE_SIZE - 1;
}

sensor_event_with_time_t *get_bottom_event(void)
{
  return &event_queue[event_queue_bottom_idx == 0 ? EVENT_QUEUE_SIZE - 1 : event_queue_bottom_idx-1];
}

static int search_nearest(uint64_t timestamp)
{
  if (event_queue_top_idx == event_queue_bottom_idx) // empty
    return -1;
  timestamp <<= 8;
  const sensor_event_with_time_t *el = &event_queue[event_queue_top_idx];
  if (el->timestampAndSensorId >= timestamp)
    return event_queue_top_idx;
  const sensor_event_with_time_t *bottom = get_bottom_event();
  if (bottom->timestampAndSensorId < timestamp)
    return -1;
  int search_offset = get_event_queue_size() / 2;
  int search_idx = event_queue_top_idx;
  while (search_offset != 0)
  {
    if (search_idx != event_queue_top_idx && el->timestampAndSensorId >= timestamp)
    {
      const sensor_event_with_time_t *prev = &event_queue[search_idx == 0 ? EVENT_QUEUE_SIZE - 1 : search_idx - 1];
      if (prev->timestampAndSensorId < timestamp)
        return search_idx;
    }
    if (el->timestampAndSensorId < timestamp)
    {
      search_idx += search_offset;
      if (search_idx >= EVENT_QUEUE_SIZE)
        search_idx -= EVENT_QUEUE_SIZE;
    }
    else
      search_idx = search_idx >= search_offset ? search_idx - search_offset : search_idx + EVENT_QUEUE_SIZE - search_offset;
    search_offset /= 2;
    el = &event_queue[search_idx];
  }
  while (el->timestampAndSensorId < timestamp)
  {
    search_idx++;
    if (search_idx == EVENT_QUEUE_SIZE)
      search_idx = 0;
    el = &event_queue[search_idx];
  }
  while (search_idx != event_queue_top_idx && el->timestampAndSensorId >= timestamp)
  {
    const sensor_event_with_time_t *prev = &event_queue[search_idx == 0 ? EVENT_QUEUE_SIZE - 1 : search_idx - 1];
    if (prev->timestampAndSensorId < timestamp)
      return search_idx;
    search_idx--;
    el = prev;
  }
  return -1;
}

int get_events_from(const unsigned int client_id, const uint64_t timestamp, events_result_t *result)
{
  if (client_id >= EVENT_QUEUE_MAX_CLIENTS)
    return 0;
  int start_idx = search_nearest(timestamp);
  if (start_idx == -1)
    return 0;
  int num_events = event_queue_bottom_idx > start_idx ? event_queue_bottom_idx - start_idx : EVENT_QUEUE_SIZE - start_idx;
  if (num_events > EVENT_QUEUE_MAX_EVENTS_PER_MESSAGE)
    num_events = EVENT_QUEUE_MAX_EVENTS_PER_MESSAGE;
  sensor_event_with_time_t *st = event_search_results[client_id];
  memcpy(st, &event_queue[start_idx], num_events * sizeof(sensor_event_with_time_t));
  result->start = st;
  result->start_idx = start_idx;
  return num_events;
}
