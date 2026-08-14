#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "safe_helpers.h"
#include "st.h"

#define STEPS 5

static void* worker(void* arg) {
    const char* name = (const char*)arg;
    int i = 0;
    char line[64];
    while (1) {
        snprintf(line, sizeof(line), "[%s] step %d\n", name, i++);
        safe_write_str(line);
        sleep(1);
        st_yield();
    }
    return (void*)(intptr_t)(42 + *name - 'A');
}

int main(void) {
    if (st_init() != 0) {
        fprintf(stderr, "st_init failed\n");
        return 1;
    }

    st_thread_start(worker, "A");
    st_thread_start(worker, "B");
    st_thread_start(worker, "C");

    st_start();

    // never reaches here
    return 0;
}
