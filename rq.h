#ifndef COOP_RQ_H
#define COOP_RQ_H

#include <stddef.h>

#include "internal.h"

/* A FIFO queue defines the scheduler's ready-set policy. */
struct st_run_queue {
    st_thread* head;
    st_thread* tail;
};

void rq_init(struct st_run_queue* rq);
void rq_push_back(struct st_run_queue* rq, st_thread* t);
st_thread* rq_pop_front(struct st_run_queue* rq);
size_t rq_count(const struct st_run_queue* rq);

#endif /* COOP_RQ_H */
