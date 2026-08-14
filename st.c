/* A tiny cooperative scheduler: FIFO queue + one assembly context switch. */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "internal.h"

static struct st_thread main_thread;
static struct st_thread* current = &main_thread;

/* Threads wait here until st_start() moves them to the ready queue. */
static struct st_thread* pending_head;
static struct st_thread* pending_tail;

static struct st_thread* ready_head;
static struct st_thread* ready_tail;

static void ready_push(struct st_thread* thread) {
    thread->next = NULL;
    if (ready_tail == NULL)
        ready_head = thread;
    else
        ready_tail->next = thread;
    ready_tail = thread;
}

static struct st_thread* ready_pop(void) {
    struct st_thread* thread = ready_head;
    if (thread == NULL)
        return NULL;

    ready_head = thread->next;
    if (ready_head == NULL)
        ready_tail = NULL;
    thread->next = NULL;
    return thread;
}

static void switch_to_next(void) {
    struct st_thread* next = ready_pop();
    if (next == NULL)
        _exit(1);

    struct st_thread* previous = current;
    current = next;
    st_ctx_swap(&previous->ctx, &next->ctx);
}

/* st_ctx_swap の ret が最初に到達する場所。 */
static void trampoline(void) {
    current->fn(current->arg);
    _exit(0);
}

static void setup_stack(struct st_thread* thread) {
    uint64_t* sp = (uint64_t*)(((uintptr_t)thread->stack + ST_STACK_BYTES) &
                               ~(uintptr_t)15);
    *--sp = 0;
    *--sp = (uint64_t)(uintptr_t)&trampoline;
    thread->ctx.rsp = (uint64_t)(uintptr_t)sp;
}

void st_init(void) {
    current = &main_thread;
}

void st_thread_start(st_fn fn, void* arg) {
    struct st_thread* thread = calloc(1, sizeof(*thread));
    if (thread == NULL)
        _exit(1);

    thread->stack = malloc(ST_STACK_BYTES);
    if (thread->stack == NULL)
        _exit(1);

    thread->fn = fn;
    thread->arg = arg;
    setup_stack(thread);

    thread->next = NULL;
    if (pending_tail == NULL)
        pending_head = thread;
    else
        pending_tail->next = thread;
    pending_tail = thread;
}

void st_start(void) {
    while (pending_head != NULL) {
        struct st_thread* thread = pending_head;
        pending_head = thread->next;
        ready_push(thread);
    }
    switch_to_next();
    __builtin_unreachable();
}

void st_yield(void) {
    ready_push(current);
    switch_to_next();
}
