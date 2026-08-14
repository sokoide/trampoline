#ifndef SAFE_HELPERS_H
#define SAFE_HELPERS_H

#include <stddef.h>
#include <unistd.h>

static inline void safe_write_str(const char* s) {
    size_t n = 0;
    while (s[n] != '\0')
        ++n;
    (void)!write(STDOUT_FILENO, s, n);
}

#endif
