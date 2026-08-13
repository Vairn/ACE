#ifndef DOS_DOS_H
#define DOS_DOS_H

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *BPTR;
typedef void *BSTR;

#define DOSFALSE 0
#define DOSTRUE  (-1)
#define ACCESS_READ  -2
#define ACCESS_WRITE -1

#define SHARED_LOCK   ACCESS_READ
#define EXCLUSIVE_LOCK ACCESS_WRITE

struct FileInfoBlock {
	LONG fib_DiskKey;
	LONG fib_DirEntryType;
	char fib_FileName[108];
	LONG fib_Protection;
	LONG fib_EntryType;
	LONG fib_Size;
	LONG fib_NumBlocks;
	struct DateStamp {
		LONG ds_Days;
		LONG ds_Minute;
		LONG ds_Tick;
	} fib_Date;
	char fib_Comment[80];
	UWORD fib_OwnerUID;
	UWORD fib_OwnerGID;
	char fib_Reserved[32];
	/* Host directory iteration state (not on Amiga). */
	void *fib_HostDir;
};

#ifdef __cplusplus
}
#endif

#endif
