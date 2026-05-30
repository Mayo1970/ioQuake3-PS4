/* ps4_compat.c -- shims for symbols missing when linking -lSceLibcInternal. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* FreeBSD/PS4 equivalent of __errno_location, provided by libkernel. */
extern int *__error(void);

int *__errno_location(void) {
    return __error();
}

/* SceLibcInternal provides _Assert() instead of GNU __assert_fail. */
void __assert_fail(const char *assertion, const char *file,
                   unsigned int line, const char *function) {
    printf("ASSERT FAILED: %s (%s:%u in %s)\n", assertion, file, line,
           function ? function : "?");
    abort();
}

/* umask absent on PS4; ioq3 calls it in Com_WriteCDKey. */
typedef uint16_t mode_t;
mode_t umask(mode_t mask) {
    (void)mask;
    return 0;
}

/* if_nametoindex absent on PS4; used only by IPv6 multicast path. */
unsigned int if_nametoindex(const char *ifname) {
    (void)ifname;
    return 0;
}
