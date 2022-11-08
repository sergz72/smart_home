#include <string.h>
#include "queue.h"
#include "stddef.h"
#include "common.h"

static unsigned char queue[QUEUE_SIZE][ENCRYPTED_LENGTH];
static int current, first;

void queue_init(void)
{
  current = first = 0;
}

void *queue_peek(void)
{
  if (current != first)
    return queue[first];
  return NULL;
}

void queue_pop(void)
{
  if (first != current)
  {
    first++;
    if (first >= QUEUE_SIZE)
      first = 0;
  }
}

void queue_push(void *data)
{
  memcpy(queue[current++], data, ENCRYPTED_LENGTH);
  if (current >= QUEUE_SIZE)
    current = 0;
  if (current == first)
  {
    first++;
    if (first >= QUEUE_SIZE)
      first = 0;
  }
}
