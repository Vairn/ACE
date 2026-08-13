#ifndef CLIB_DOS_PROTOS_H
#define CLIB_DOS_PROTOS_H

#include <dos/dos.h>

#ifdef __cplusplus
extern "C" {
#endif

BPTR Lock(CONST_STRPTR name, LONG accessMode);
void UnLock(BPTR lock);
LONG Examine(BPTR lock, struct FileInfoBlock *fib);
LONG ExNext(BPTR lock, struct FileInfoBlock *fib);
BPTR CreateDir(CONST_STRPTR name);
LONG IoErr(void);

#ifdef __cplusplus
}
#endif

#endif
