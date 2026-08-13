#ifndef CLIB_EXEC_PROTOS_H
#define CLIB_EXEC_PROTOS_H

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>

#ifdef __cplusplus
extern "C" {
#endif

APTR AllocMem(ULONG byteSize, ULONG attributes);
void FreeMem(APTR memoryBlock, ULONG byteSize);
ULONG AvailMem(ULONG attributes);
ULONG TypeOfMem(APTR address);
void CopyMem(CONST APTR source, APTR dest, ULONG size);
void CopyMemQuick(CONST APTR source, APTR dest, ULONG size);
struct Library *OpenLibrary(CONST_STRPTR libName, ULONG version);
void CloseLibrary(struct Library *library);
APTR OpenResource(CONST_STRPTR resName);
void Forbid(void);
void Permit(void);
void Disable(void);
void Enable(void);

#ifdef __cplusplus
}
#endif

#endif
