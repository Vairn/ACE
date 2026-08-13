#ifndef EXEC_EXECBASE_H
#define EXEC_EXECBASE_H

#include <exec/types.h>
#include <exec/lists.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ExecBase {
	UWORD SoftVer;
	WORD LowMemChkSum;
	ULONG ChkBase;
	APTR ColdCapture;
	APTR CoolCapture;
	APTR WarmCapture;
	APTR SysStkUpper;
	APTR SysStkLower;
	ULONG MaxLocMem;
	APTR DebugEntry;
	APTR DebugData;
	APTR AlertData;
	APTR MaxExtMem;
	UWORD ChkSum;
	UWORD Version;
	UWORD Revision;
};

#ifdef __cplusplus
}
#endif

#endif
