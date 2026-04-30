/*
 * ps4_compat.c - Compatibility shims for SceLibcInternal
 *
 * When linking with -lSceLibcInternal instead of musl -lc,
 * some GNU/musl-specific symbols are missing. This file provides
 * BSD/PS4-compatible replacements.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* __error() is the FreeBSD/PS4 equivalent of __errno_location().
 * It's provided by libkernel and returns int*.
 */
extern int *__error(void);

int *__errno_location(void) {
    return __error();
}

/* __assert_fail is GNU libc's assert handler.
 * SceLibcInternal provides _Assert() instead.
 * Signature: void __assert_fail(const char *assertion, const char *file,
 *                                unsigned int line, const char *function)
 */
void __assert_fail(const char *assertion, const char *file,
                   unsigned int line, const char *function) {
    printf("ASSERT FAILED: %s (%s:%u in %s)\n", assertion, file, line,
           function ? function : "?");
    abort();
}

/* umask() is not available on PS4 (no traditional Unix permission model).
 * ioq3 calls it in Com_WriteCDKey; return a dummy value.
 */
typedef uint16_t mode_t;
mode_t umask(mode_t mask) {
    (void)mask;
    return 0;
}

/* if_nametoindex() is not available on PS4.
 * Used by NET_SetMulticast6 for IPv6 multicast (not relevant on PS4).
 */
unsigned int if_nametoindex(const char *ifname) {
    (void)ifname;
    return 0;
}
