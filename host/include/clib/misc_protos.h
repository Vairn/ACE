#ifndef CLIB_MISC_PROTOS_H
#define CLIB_MISC_PROTOS_H

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif

STRPTR AllocMiscResource(ULONG unitNum, CONST_STRPTR name);
void FreeMiscResource(ULONG unitNum);

#ifdef __cplusplus
}
#endif

#endif
