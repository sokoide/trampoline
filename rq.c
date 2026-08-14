#include "rq.h"

void rq_init(struct st_run_queue* rq) {
    rq->head = NULL;
    rq->tail = NULL;
}

void rq_push_back(struct st_run_queue* rq, st_thread* t) {
    t->rq_next = NULL;
    if (rq->tail == NULL) {
        rq->head = t;
        rq->tail = t;
    } else {
        rq->tail->rq_next = t;
        rq->tail = t;
    }
}

st_thread* rq_pop_front(struct st_run_queue* rq) {
    st_thread* t = rq->head;
    if (t == NULL)
        return NULL;

    rq->head = t->rq_next;
    if (rq->head == NULL)
        rq->tail = NULL;
    t->rq_next = NULL;
    return t;
}

size_t rq_count(const struct st_run_queue* rq) {
    size_t count = 0;
    for (const st_thread* t = rq->head; t != NULL; t = t->rq_next) ++count;
    return count;
}
