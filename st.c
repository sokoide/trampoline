/* A tiny cooperative scheduler: FIFO queue + one assembly context switch. */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "internal.h"

static struct st_thread main_thread;
static struct st_thread* current = &main_thread;

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

/* Pick the next thread. If none is ready, stop with an error.
 * exit() runs atexit handlers and stdio cleanup. This small runtime does not
 * define how those interact with its stacks, so it uses _exit() instead. */
static void switch_to_next(void) {
    struct st_thread* next = ready_pop();
    if (next == NULL)
        _exit(1);

    /* Set current before st_ctx_swap. On a new thread, ret starts the
     * trampoline without setting up arguments. Caller-saved registers such as
     * rdi may contain anything, and this small context has no saved argument
     * slots. The trampoline finds itself through current. */
    struct st_thread* previous = current;
    current = next;
    st_ctx_swap(&previous->ctx, &next->ctx);
}

/* Entry point for a new thread. st_ctx_swap reaches it with ret.
 * ret does not set arguments, so use the global current to find self and
 * call fn(arg). This small API has no thread-exit operation: if fn returns,
 * terminate the entire process with _exit(0). */
static void trampoline(void) {
    current->fn(current->arg);
    _exit(0);
}

/* Build the first ret frame for a new thread.
 *
 *   high address (stack + ST_STACK_BYTES)
 *     ... round down to a 16-byte boundary ...
 *     [top- 8] = 0            padding for ABI alignment; invalid return target
 *     [top-16] = &trampoline  first ret target of st_ctx_swap
 *                 ^
 *                 thread->ctx.rsp
 *
 * ctx.rsp is top-16, so it is 16-byte aligned. After ret, trampoline starts
 * with rsp = top-8, which gives the System V AMD64 entry alignment rsp % 16 == 8. */
static void setup_stack(struct st_thread* thread) {
    uint64_t* sp = (uint64_t*)(((uintptr_t)thread->stack + ST_STACK_BYTES) &
                               ~(uintptr_t)15);
    *--sp = 0;
    *--sp = (uint64_t)(uintptr_t)&trampoline;
    thread->ctx.rsp = (uint64_t)(uintptr_t)sp;

    /* A new logical thread starts with the creator's floating-point control
     * state. st_ctx_swap subsequently keeps this state per thread. */
    __asm__ volatile("stmxcsr %0" : "=m"(thread->ctx.mxcsr));
    __asm__ volatile("fnstcw %0" : "=m"(thread->ctx.x87_cw));
}

void st_init(void) {
    current = &main_thread;
}

/* Create a TCB and private stack, then append it to the ready queue.
 * No OS thread is created. Execution starts at the first switch in st_start(). */
void st_thread_create(st_fn fn, void* arg) {
    if (fn == NULL)
        _exit(1);

    struct st_thread* thread = calloc(1, sizeof(*thread));
    if (thread == NULL)
        _exit(1);

    thread->stack = malloc(ST_STACK_BYTES);
    if (thread->stack == NULL)
        _exit(1);

    thread->fn = fn;
    thread->arg = arg;
    setup_stack(thread);

    ready_push(thread);
}

/* Leave main and switch to the first thread in the ready queue.
 * main_thread is never queued, so control never returns to main. */
void st_start(void) {
    switch_to_next();
    __builtin_unreachable();
}

void st_yield(void) {
    ready_push(current);
    switch_to_next();
}
