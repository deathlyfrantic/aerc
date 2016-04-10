#ifndef _AQUEUE_H
#define _AQUEUE_H

#include <stdbool.h>

/* Lock-free single-producer/single-consumer asyncronous queue */

typedef struct aqueue aqueue_t;

aqueue_t *aqueue_new();
void aqueue_free(aqueue_t *queue);
void aqueue_enqueue(aqueue_t *q, void *val);
bool aqueue_dequeue(aqueue_t *q, void **val);

#endif
