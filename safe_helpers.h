#ifndef SAFE_HELPERS_H
#define SAFE_HELPERS_H

#include <errno.h>
#include <stddef.h>
#include <unistd.h>

/* Write a NUL-terminated string without stdio buffering. Retry interrupted and
 * partial writes; stop on another error or zero progress. This demo ignores the
 * resulting output failure. */
static inline void write_all(const char* s) {
    size_t n = 0;
    while (s[n] != '\0')
        ++n;

    size_t written = 0;
    while (written < n) {
        ssize_t result = write(STDOUT_FILENO, s + written, n - written);
        if (result > 0) {
            written += (size_t)result;
            continue;
        }
        if (result < 0 && errno == EINTR)
            continue;
        return;
    }
}

#endif
