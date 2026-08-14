#ifndef TRAMPOLINE_ST_H
#define TRAMPOLINE_ST_H

/* A minimal cooperative user-thread API. Threads switch only when they yield. */

typedef void* (*st_fn)(void* arg);

void st_init(void);
void st_thread_create(st_fn fn, void* arg);
void st_start(void) __attribute__((noreturn));
void st_yield(void);

#endif
