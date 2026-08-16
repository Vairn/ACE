#ifndef ACE_HOST_OS_H
#define ACE_HOST_OS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Monotonic high-resolution clock, stable across the host build. These are
 * used by the frame pacers ("Sleep(1)" has ~1 ms granularity even with
 * timeBeginPeriod(1), which makes the emulated beam land early/late by a
 * millisecond or worse per frame). */
uint64_t hostOsClockFreq(void);
uint64_t hostOsClockTicks(void);
/* Busy-park until the given deadline. Returns when the clock is at/after it,
 * spinning the last <=2 ms so a shortened timer quantum cannot steal it. */
void hostOsSleepUntilTicks(uint64_t deadline);

void *hostOsMapLow(size_t size);
void hostOsUnmapLow(void *p, size_t size);

/* Real OS heap — not CHIP/FAST. Used by the allocator slab fallback, SDL, and
 * CRT malloc before aceHostMemInit(). */
void *hostOsHeapMalloc(size_t n);
void *hostOsHeapCalloc(size_t nmemb, size_t size);
void *hostOsHeapRealloc(void *p, size_t n);
void hostOsHeapFree(void *p);
int hostOsIsDir(const char *path);
void *hostOsDirOpen(const char *path);
void hostOsDirClose(void *dir);
int hostOsDirNext(void *dir, char *name, unsigned nameMax, int *isDir, long *size);
int hostOsMkdir(const char *path);
void hostOsTimerHiRes(int on);

#ifdef __cplusplus
}
#endif

#endif
