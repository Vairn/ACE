/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/utils/file.h>
#include <stdarg.h>
#include <ace/managers/system.h>
#include <ace/managers/log.h>
#include <ace/utils/endian.h>

#if defined(ACE_FILE_USE_ONLY_DISK)
#include <ace/utils/disk_file_private.h>

ALWAYS_INLINE
inline static ULONG fileReadData(tFile *pFile, void *pDest, ULONG ulSize) {
	return diskFileRead(pFile->pData, pDest, ulSize);
}

ALWAYS_INLINE
inline static ULONG fileWriteData(
	tFile *pFile, const void *pSrc, ULONG ulSize
) {
	return diskFileWrite(pFile->pData, pSrc, ulSize);
}

void fileClose(tFile *pFile) {
	diskFileClose(pFile);
}

ULONG fileSeek(tFile *pFile, LONG lPos, WORD wMode) {
	return diskFileSeek(pFile, lPos, wMode);
}

ULONG fileGetPos(tFile *pFile) {
	return diskFileGetPos(pFile);
}

LONG fileGetSize(tFile *pFile) {
	return diskFileGetSize(pFile);
}

UBYTE fileIsEof(tFile *pFile) {
	return diskFileIsEof(pFile);
}

void fileFlush(tFile *pFile) {
	diskFileFlush(pFile);
}

#else
ALWAYS_INLINE
inline static ULONG fileReadData(tFile *pFile, void *pDest, ULONG ulSize) {
	return pFile->pCallbacks->cbFileRead(pFile->pData, pDest, ulSize);
}

ALWAYS_INLINE
inline static ULONG fileWriteData(tFile *pFile, const void *pSrc, ULONG ulSize) {
	return pFile->pCallbacks->cbFileWrite(pFile->pData, pSrc, ulSize);
}

void fileClose(tFile *pFile) {
	logWrite("Closing file %p\n", pFile);
	if(!pFile) {
		logWrite("ERR: Null file handle\n");
		return;
	}
	pFile->pCallbacks->cbFileClose(pFile->pData);
	memFree(pFile, sizeof(*pFile));
}

ULONG fileSeek(tFile *pFile, LONG lPos, WORD wMode) {
	if(!pFile) {
		logWrite("ERR: Null file handle\n");
	}
	return pFile->pCallbacks->cbFileSeek(pFile->pData, lPos, wMode);
}

ULONG fileGetPos(tFile *pFile) {
	if(!pFile) {
		logWrite("ERR: Null file handle\n");
	}
	return pFile->pCallbacks->cbFileGetPos(pFile->pData);
}

LONG fileGetSize(tFile *pFile) {
	if(!pFile) {
		logWrite("ERR: Null file handle\n");
	}
	return pFile->pCallbacks->cbFileGetSize(pFile->pData);
}

UBYTE fileIsEof(tFile *pFile) {
	if(!pFile) {
		logWrite("ERR: Null file handle\n");
	}
	return pFile->pCallbacks->cbFileIsEof(pFile->pData);
}

void fileFlush(tFile *pFile) {
	if(!pFile) {
		logWrite("ERR: Null file handle\n");
	}
	pFile->pCallbacks->cbFileFlush(pFile->pData);
}
#endif

ULONG fileReadBytes(tFile *pFile, UBYTE *pDest, ULONG ulCount) {
	if(!pFile) {
		logWrite("ERR: Null file handle\n");
	}

	ULONG ulReadCount = fileReadData(pFile, pDest, ulCount);
	return ulReadCount;
}

ULONG fileReadWords(tFile *pFile, UWORD *pDest, ULONG ulCount) {
	ULONG ulReadCount = fileReadData(pFile, (UBYTE*)pDest, ulCount * sizeof(UWORD)) / sizeof(UWORD);

#if defined(ENDIAN_NATIVE_LITTLE)
	for(ULONG i = ulReadCount; i--;) {
		pDest[i] = endianBigToNative16(pDest[i]);
	}
#endif

	return ulReadCount;
}

ULONG fileReadLongs(tFile *pFile, ULONG *pDest, ULONG ulCount) {
	ULONG ulReadCount = fileReadData(pFile, (UBYTE*)pDest, ulCount * sizeof(ULONG)) / sizeof(ULONG);

#if defined(ENDIAN_NATIVE_LITTLE)
	for(ULONG i = ulReadCount; i--;) {
		pDest[i] = endianBigToNative32(pDest[i]);
	}
#endif

	return ulReadCount;
}

ULONG fileWriteBytes(tFile *pFile, const UBYTE *pSrc, ULONG ulCount) {
	if(!pFile) {
		logWrite("ERR: Null file handle\n");
	}

	ULONG ulWritten = fileWriteData(pFile, pSrc, ulCount);
	return ulWritten;
}

ULONG fileWriteWords(tFile *pFile, const UWORD *pSrc, ULONG ulCount) {
#if defined(ENDIAN_NATIVE_LITTLE)
	if(!pFile) {
		logWrite("ERR: Null file handle\n");
	}
	// Byte-by-byte write avoids temporary allocation; the disk buffering
	// makes few-byte writes acceptable on host.
	ULONG ulWriteCount = 0;
	for(ULONG i = 0; i < ulCount; ++i) {
		UWORD uwSwapped = endianNativeToBig16(pSrc[i]);
		if(fileWriteData(pFile, &uwSwapped, sizeof(UWORD))) {
			++ulWriteCount;
		}
	}
	return ulWriteCount;
#else
	return fileWriteData(pFile, (const UBYTE*)pSrc, ulCount * sizeof(UWORD)) / sizeof(UWORD);
#endif
}

ULONG fileWriteLongs(tFile *pFile, const ULONG *pSrc, ULONG ulCount) {
#if defined(ENDIAN_NATIVE_LITTLE)
	if(!pFile) {
		logWrite("ERR: Null file handle\n");
	}
	ULONG ulWriteCount = 0;
	for(ULONG i = 0; i < ulCount; ++i) {
		ULONG ulSwapped = endianNativeToBig32(pSrc[i]);
		if(fileWriteData(pFile, &ulSwapped, sizeof(ULONG))) {
			++ulWriteCount;
		}
	}
	return ulWriteCount;
#else
	return fileWriteData(pFile, (const UBYTE*)pSrc, ulCount * sizeof(ULONG)) / sizeof(ULONG);
#endif
}

void fileWriteStr(tFile *pFile, const char *szLine) {
	fileWriteBytes(pFile, (const UBYTE*)szLine, strlen(szLine));
}