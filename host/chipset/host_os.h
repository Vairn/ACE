#ifndef ACE_HOST_OS_H
#define ACE_HOST_OS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *hostOsMapLow(size_t size);
void hostOsUnmapLow(void *p, size_t size);
int hostOsIsDir(const char *path);
void *hostOsDirOpen(const char *path);
void hostOsDirClose(void *dir);
int hostOsDirNext(void *dir, char *name, unsigned nameMax, int *isDir, long *size);
int hostOsMkdir(const char *path);

#ifdef __cplusplus
}
#endif

#endif
