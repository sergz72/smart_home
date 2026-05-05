#include <stdio.h>
#include <events.h>
#include <time.h>
#include <sys/random.h>

static struct timespec two_ms = {0, 2000000};

static uint8_t message[40];

static int build_message(uint32_t sensor_id)
{
  int length = 0;
  while (sensor_id > 0)
  {
    message[length++] = sensor_id--;
    getrandom(&message[length], 4, 0);
    length += 4;
  }
  return length;
}

static bool check_num_events(const events_result_t *result, const uint32_t step_no)
{
  const size_t bottom_idx = get_event_queue_bottom_idx();
  const size_t cap = get_event_queue_capacity();
  const bool ok = result->num_events == (result->start_idx > bottom_idx ? cap : bottom_idx) - result->start_idx;
  if (!ok)
    printf("Wrong search result.num_events - step %d\n", step_no);
  return ok;
}


static int run_tests(const uint32_t step_no)
{
  const size_t top_idx = get_event_queue_top_idx();
  const sensor_event_with_time_t *event_queue = get_event_queue();
  events_result_t result;

  const sensor_event_with_time_t *top = &event_queue[top_idx];
  const uint64_t top_timestamp = top->timestampAndSensorId >> 8;
  if (get_events_from(top_timestamp, &result) == 0)
  {
    printf("Wrong search result with top timestamp - step %d\n", step_no);
    return 1;
  }
  if (result.start_idx != top_idx)
  {
    printf("Wrong search result.start with top timestamp - step %d\n", step_no);
    return 2;
  }
  if (!check_num_events(&result, step_no))
    return 100;

  const sensor_event_with_time_t *bottom = get_bottom_event();
  const uint64_t bottom_timestamp = bottom->timestampAndSensorId >> 8;
  if (get_events_from(bottom_timestamp + 1, &result) != 0)
  {
    printf("Wrong search result with bottom timestamp+1 - step %d\n", step_no);
    return 4;
  }

  const uint64_t diff = bottom_timestamp - top_timestamp;
  if (diff == 0)
    return 0;

  uint64_t random_offset;
  getrandom(&random_offset, sizeof(random_offset), 0);
  random_offset %= diff;
  const uint64_t random_timestamp = top_timestamp + random_offset;
  if (step_no == 600)
    puts("step 600");
  if (get_events_from(random_timestamp, &result) == 0)
  {
    printf("Wrong search result with random timestamp - step %d\n", step_no);
    return 5;
  }
  const sensor_event_with_time_t *el = &event_queue[result.start_idx];
  const uint64_t result_timestamp = el->timestampAndSensorId >> 8;
  if (result_timestamp < random_timestamp)
  {
    printf("Wrong search result.timestampAndSensorId with random timestamp - step %d\n", step_no);
    return 6;
  }
  if (result.start_idx != top_idx)
  {
    const size_t prev_idx = result.start_idx == 0 ? get_event_queue_capacity() - 1 : result.start_idx - 1;
    if ((event_queue[prev_idx].timestampAndSensorId >> 8) >= random_timestamp)
    {
      printf("Wrong search result prev timestampAndSensorId with random timestamp - step %d\n", step_no);
      return 6;
    }
  }
  if (!check_num_events(&result, step_no))
    return 101;

  return 0;
}

int main(void)
{
  events_result_t result;

  init_events(30000);
  if (get_events_from(0, &result) != 0)
  {
    puts("Wrong search result on empty queue");
    return 1;
  }
  size_t expected_size = 0;
  for (uint32_t i = 0; i < 1000; i++)
  {
    if (get_event_queue_size() != expected_size)
    {
      puts("Wrong queue size");
      return 2;
    }
    if (expected_size < get_event_queue_capacity())
      expected_size++;
    const uint32_t sensor_id = i % 5 + 1;
    const int message_length = build_message(sensor_id);
    add_event(sensor_id, message, message_length);
    const int rc = run_tests(i);
    if (rc)
      return rc;
    nanosleep(&two_ms, nullptr);
  }
  delete_events();
  return 0;
}
