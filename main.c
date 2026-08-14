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
        /* 注意: sleep(1) は OS スレッド (プロセス全体) をブロックする。
         * 協調スレッドでは A の sleep の間、B/C も動けない。 */
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
