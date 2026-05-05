#include "events.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static sensor_event_with_time_t *event_queue = nullptr;
static size_t event_queue_top_idx;
static size_t event_queue_bottom_idx;
static size_t queue_size;

int init_events(const size_t queue_size_bytes)
{
  if (event_queue != nullptr)
    return 1;
  queue_size = queue_size_bytes / sizeof(sensor_event_with_time_t);
  event_queue = (sensor_event_with_time_t *)malloc(queue_size * sizeof(sensor_event_with_time_t));
  if (event_queue == NULL)
    return 2;
  event_queue_top_idx = event_queue_bottom_idx = 0;
  return 0;
}

sensor_event_with_time_t *get_event_queue(void)
{
  return event_queue;
}

size_t get_event_queue_top_idx(void)
{
  return event_queue_top_idx;
}

size_t get_event_queue_bottom_idx(void)
{
  return event_queue_bottom_idx;
}

int delete_events(void)
{
  if (event_queue == nullptr)
    return 1;
  free(event_queue);
  event_queue = nullptr;
  return 0;
}

static uint64_t get_time_ms()
{
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (uint64_t)tp.tv_sec * 1000 + (uint64_t)tp.tv_nsec / 1000000;
}

int add_event(const uint32_t device_id, const uint8_t *message, int message_length)
{
  if (message_length <= 0 || message_length > MAX_VALUE_TYPES * (1 + sizeof(uint32_t)) ||
      (message_length % (1 + sizeof(uint32_t)) != 0))
    return 1;
  int idx = 0;
  int event_no = 0;
  sensor_event_with_time_t *bottom = &event_queue[event_queue_bottom_idx];
  memset(bottom, 0, sizeof(sensor_event_with_time_t));
  bottom->timestampAndSensorId = (get_time_ms() << 8) | device_id;
  while (message_length > 0)
  {
    bottom->event.value_types[event_no] = message[idx];
    message_length--;
    idx++;
    memcpy(&bottom->event.values[event_no], &message[idx], sizeof(uint32_t));
    idx += sizeof(uint32_t);
    message_length -= sizeof(uint32_t);
    event_no++;
  }
  event_queue_bottom_idx++;
  if (event_queue_bottom_idx == queue_size)
    event_queue_bottom_idx = 0;
  if (event_queue_bottom_idx == event_queue_top_idx)
    event_queue_top_idx++;
  return 0;
}

size_t get_event_queue_size(void)
{
  return event_queue_bottom_idx >= event_queue_top_idx ? event_queue_bottom_idx - event_queue_top_idx : queue_size;
}

size_t get_event_queue_capacity(void)
{
  return queue_size;
}

sensor_event_with_time_t *get_bottom_event(void)
{
  return &event_queue[event_queue_bottom_idx == 0 ? queue_size - 1 : event_queue_bottom_idx-1];
}

static int search_nearest(uint64_t timestamp, size_t *idx)
{
  if (event_queue_top_idx == event_queue_bottom_idx) // empty
    return 0;
  timestamp <<= 8;
  const sensor_event_with_time_t *el = &event_queue[event_queue_top_idx];
  if (el->timestampAndSensorId >= timestamp)
  {
    *idx = event_queue_top_idx;
    return 1;
  }
  const sensor_event_with_time_t *bottom = get_bottom_event();
  if (bottom->timestampAndSensorId < timestamp)
    return 0;
  size_t search_offset = get_event_queue_size() / 2;
  size_t search_idx = event_queue_top_idx;
  while (search_offset != 0)
  {
    if (search_idx != event_queue_top_idx && el->timestampAndSensorId >= timestamp)
    {
      const sensor_event_with_time_t *prev = &event_queue[search_idx == 0 ? queue_size - 1 : search_idx - 1];
      if (prev->timestampAndSensorId < timestamp)
      {
        *idx = search_idx;
        return 1;
      }
    }
    if (el->timestampAndSensorId < timestamp)
    {
      search_idx += search_offset;
      if (search_idx >= queue_size)
        search_idx -= queue_size;
    }
    else
      search_idx = search_idx >= search_offset ? search_idx - search_offset : search_idx + queue_size - search_offset;
    search_offset /= 2;
    el = &event_queue[search_idx];
  }
  while (el->timestampAndSensorId < timestamp)
  {
    search_idx++;
    if (search_idx == queue_size)
      search_idx = 0;
    el = &event_queue[search_idx];
  }
  while (search_idx != event_queue_top_idx && el->timestampAndSensorId >= timestamp)
  {
    const sensor_event_with_time_t *prev = &event_queue[search_idx == 0 ? queue_size - 1 : search_idx - 1];
    if (prev->timestampAndSensorId < timestamp)
    {
      *idx = search_idx;
      return 1;
    }
    search_idx--;
    el = prev;
  }
  return 0;
}

int get_events_from(const uint64_t timestamp, events_result_t *result)
{
  if (search_nearest(timestamp, &result->start_idx) == 0)
    return 0;
  result->num_events = event_queue_bottom_idx > result->start_idx ? event_queue_bottom_idx - result->start_idx : queue_size - result->start_idx;
  return 1;
}
