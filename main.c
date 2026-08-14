#include <stdio.h>
#include <unistd.h>

#include "safe_helpers.h"
#include "st.h"

static void* worker(void* arg) {
    const char* name = (const char*)arg;
    int i = 0;
    char line[64];
    while (1) {
        snprintf(line, sizeof(line), "[%s] step %d\n", name, i++);
        safe_write_str(line);
        /* sleep(1) blocks the OS thread, so B and C also stop while A sleeps. */
        sleep(1);
        st_yield();
    }
    return NULL;
}

int main(void) {
    st_init();

    st_thread_create(worker, "A");
    st_thread_create(worker, "B");
    st_thread_create(worker, "C");

    st_start();

    __builtin_unreachable();
}
