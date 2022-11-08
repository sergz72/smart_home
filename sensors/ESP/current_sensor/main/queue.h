#ifndef MAIN_QUEUE_H
#define MAIN_QUEUE_H

void queue_init(void);
void *queue_peek(void);
void queue_pop(void);
void queue_push(void *data);

#endif //MAIN_QUEUE_H
