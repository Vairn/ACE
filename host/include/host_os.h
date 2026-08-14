#ifndef ACE_HOST_OS_H
#define ACE_HOST_OS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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
