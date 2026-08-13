#ifndef EXEC_LIBRARIES_H
#define EXEC_LIBRARIES_H

#include <exec/types.h>
#include <exec/lists.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Library {
	struct Node lib_Node;
	UBYTE lib_Flags;
	UBYTE lib_pad;
	UWORD lib_NegSize;
	UWORD lib_PosSize;
	UWORD lib_Version;
	UWORD lib_Revision;
	APTR lib_IdString;
	ULONG lib_Sum;
	UWORD lib_OpenCnt;
};

#ifdef __cplusplus
}
#endif

#endif
