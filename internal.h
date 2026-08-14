#ifndef TRAMPOLIN_INTERNAL_H
#define TRAMPOLIN_INTERNAL_H

#include "ctx.h"
#include "st.h"

#define ST_STACK_BYTES (64 * 1024)

struct st_thread {
    struct st_ctx ctx;
    void* stack;
    st_fn fn;
    void* arg;
    struct st_thread* next;
};

#endif
