#ifndef TRAMPOLIN_ST_H
#define TRAMPOLIN_ST_H

/* 最小の cooperative user-thread API。スレッドは自発的な yield でだけ切り替わる。 */

typedef void* (*st_fn)(void* arg);

void st_init(void);
void st_thread_start(st_fn fn, void* arg);
void st_start(void) __attribute__((noreturn));
void st_yield(void);

#endif
