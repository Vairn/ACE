/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/utils/file.h>
#include <ace/utils/endian.h>
#include <ace/utils/disk_file.h>
#include <ace/utils/disk_file_private.h>
#include <ace/managers/memory.h>
#include <ace/managers/system.h>
#include <ace/managers/log.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

//------------------------------------------------------------------ PRIVATE FNS

static ULONG fileReadRaw(tFile *pFile, void *pDest, ULONG ulByteCount) {
#if defined(ACE_FILE_USE_ONLY_DISK)
	return diskFileRead(pFile, pDest, ulByteCount);
#else
	return pFile->pCallbacks->cbFileRead(pFile->pData, pDest, ulByteCount);
#endif
}

static ULONG fileWriteRaw(tFile *pFile, const void *pSrc, ULONG ulByteCount) {
#if defined(ACE_FILE_USE_ONLY_DISK)
	return diskFileWrite(pFile, pSrc, ulByteCount);
#else
	return pFile->pCallbacks->cbFileWrite(pFile->pData, pSrc, ulByteCount);
#endif
}

static ULONG fileReadData(tFile *pFile, void *pDest, UBYTE ubDataSize, ULONG ulCount) {
#ifdef ACE_DEBUG
	if(!ubDataSize || !ulCount) {
		logWrite("ERR: File read size = 0!\n");
	}
#endif
	systemUse();
	systemReleaseBlitterToOs();
	ULONG ulBytes = ubDataSize * ulCount;
	ULONG ulGot = fileReadRaw(pFile, pDest, ulBytes);
	ULONG ulReadCount = (ubDataSize && ulGot >= ubDataSize) ? (ulGot / ubDataSize) : 0;
#if defined(ENDIAN_NATIVE_LITTLE)
	if(ubDataSize == sizeof(UBYTE)) {
		// no endian swap for bytes
	}
	else if(ubDataSize == sizeof(UWORD) && ulReadCount) {
		UWORD *pWords = pDest;
		for(ULONG i = 0; i < ulReadCount; ++i) {
			pWords[i] = endianBigToNative16(pWords[i]);
		}
	}
	else if(ubDataSize == sizeof(ULONG) && ulReadCount) {
		ULONG *pLongs = pDest;
		for(ULONG i = 0; i < ulReadCount; ++i) {
			pLongs[i] = endianBigToNative32(pLongs[i]);
		}
	}
	else if(ulReadCount) {
		logWrite("ERR: Unsupported data size: %hhu\n", ubDataSize);
	}
#endif
	systemGetBlitterFromOs();
	systemUnuse();

	return ulReadCount;
}

static ULONG fileWriteData(tFile *pFile, const void *pSource, UBYTE ubDataSize, ULONG ulCount) {
#ifdef ACE_DEBUG
	if(!ubDataSize || !ulCount) {
		logWrite("ERR: File read size = 0!\n");
	}
#endif
	systemUse();
	systemReleaseBlitterToOs();
	ULONG ulWriteCount = 0;
#if defined(ENDIAN_NATIVE_LITTLE)
	if(ubDataSize == sizeof(UBYTE)) {
		ulWriteCount = fileWriteRaw(pFile, pSource, ubDataSize * ulCount);
	}
	else if(ubDataSize == sizeof(UWORD)) {
		const UWORD *pWords = pSource;
		for(ULONG i = 0; i < ulCount; ++i) {
			UWORD uwReversed = endianBigToNative16(pWords[i]);
			ulWriteCount += fileWriteRaw(pFile, &uwReversed, ubDataSize);
		}
	}
	else if(ubDataSize == sizeof(ULONG)) {
		const ULONG *pLongs = pSource;
		for(ULONG i = 0; i < ulCount; ++i) {
			ULONG ulReversed = endianBigToNative32(pLongs[i]);
			ulWriteCount += fileWriteRaw(pFile, &ulReversed, ubDataSize);
		}
	}
	else {
		logWrite("ERR: Unsupported data size: %hhu\n", ubDataSize);
	}
#else
	ulWriteCount = fileWriteRaw(pFile, pSource, ubDataSize * ulCount);
#endif
	systemGetBlitterFromOs();
	systemUnuse();

	return ulWriteCount;
}

//------------------------------------------------------------------- PUBLIC FNS

LONG fileGetPathSize(const char *szPath) {
	systemUse();
	systemReleaseBlitterToOs();
	logBlockBegin("fileGetPathSize(szPath: '%s')", szPath);
	FILE *pFsFile = fopen(szPath, "rb");
	if(!pFsFile) {
		logWrite("ERR: File doesn't exist");
		logBlockEnd("fileGetPathSize()");
		systemGetBlitterFromOs();
		systemUnuse();
		return -1;
	}
	fseek(pFsFile, 0, SEEK_END);
	LONG lSize = ftell(pFsFile);
	fclose(pFsFile);

	logBlockEnd("fileGetPathSize()");
	systemGetBlitterFromOs();
	systemUnuse();

	return lSize;
}

LONG fileGetSize(tFile *pFile) {
#if defined(ACE_FILE_USE_ONLY_DISK)
	return (LONG)diskFileGetSize(pFile);
#else
	return (LONG)pFile->pCallbacks->cbFileGetSize(pFile->pData);
#endif
}

tFile *fileOpen(const char *szPath, const char *szMode) {
	UBYTE isRead = 1;
	if(szMode && (szMode[0] == 'w' || szMode[0] == 'W' || szMode[0] == 'a' || szMode[0] == 'A')) {
		isRead = 0;
	}
	return diskFileOpen(
		szPath, isRead ? DISK_FILE_MODE_READ : DISK_FILE_MODE_WRITE, 0
	);
}

void fileClose(tFile *pFile) {
#if defined(ACE_FILE_USE_ONLY_DISK)
	diskFileClose(pFile);
#else
	systemUse();
	systemReleaseBlitterToOs();
	pFile->pCallbacks->cbFileClose(pFile->pData);
	memFree(pFile, sizeof(*pFile));
	systemGetBlitterFromOs();
	systemUnuse();
#endif
}

ULONG fileRead(tFile *pFile, void *pDest, ULONG ulSize) {
	return fileReadBytes(pFile, (UBYTE *)pDest, ulSize);
}

ULONG fileWrite(tFile *pFile, const void *pSrc, ULONG ulSize) {
	return fileWriteBytes(pFile, pSrc, ulSize);
}

ULONG fileReadBytes(tFile *pFile, UBYTE *pDest, ULONG ulSize) {
	return fileReadData(pFile, pDest, sizeof(UBYTE), ulSize);
}

ULONG fileReadWords(tFile *pFile, UWORD *pDest, ULONG ulSize) {
	return fileReadData(pFile, pDest, sizeof(UWORD), ulSize);
}

ULONG fileReadLongs(tFile *pFile, ULONG *pDest, ULONG ulSize) {
	return fileReadData(pFile, pDest, sizeof(ULONG), ulSize);
}

ULONG fileWriteBytes(tFile *pFile, const void *pSrc, ULONG ulSize) {
	return fileWriteData(pFile, pSrc, sizeof(UBYTE), ulSize);
}

ULONG fileWriteWords(tFile *pFile, const void *pSrc, ULONG ulSize) {
	return fileWriteData(pFile, pSrc, sizeof(UWORD), ulSize);
}

ULONG fileWriteLongs(tFile *pFile, const void *pSrc, ULONG ulSize) {
	return fileWriteData(pFile, pSrc, sizeof(ULONG), ulSize);
}

ULONG fileSeek(tFile *pFile, LONG lPos, WORD wMode) {
	systemUse();
	systemReleaseBlitterToOs();
#if defined(ACE_FILE_USE_ONLY_DISK)
	ULONG ulResult = diskFileSeek(pFile, lPos, wMode);
#else
	ULONG ulResult = pFile->pCallbacks->cbFileSeek(pFile->pData, lPos, wMode);
#endif
	systemGetBlitterFromOs();
	systemUnuse();

	return ulResult;
}

ULONG fileGetPos(tFile *pFile) {
	systemUse();
	systemReleaseBlitterToOs();
#if defined(ACE_FILE_USE_ONLY_DISK)
	ULONG ulResult = diskFileGetPos(pFile);
#else
	ULONG ulResult = pFile->pCallbacks->cbFileGetPos(pFile->pData);
#endif
	systemGetBlitterFromOs();
	systemUnuse();

	return ulResult;
}

UBYTE fileIsEof(tFile *pFile) {
	systemUse();
	systemReleaseBlitterToOs();
#if defined(ACE_FILE_USE_ONLY_DISK)
	UBYTE ubResult = diskFileIsEof(pFile);
#else
	UBYTE ubResult = pFile->pCallbacks->cbFileIsEof(pFile->pData);
#endif
	systemGetBlitterFromOs();
	systemUnuse();

	return ubResult;
}

#if !defined(BARTMAN_GCC) // Not implemented in mini_std for now, sorry!
LONG fileVaPrintf(tFile *pFile, const char *szFmt, va_list vaArgs) {
	systemUse();
	systemReleaseBlitterToOs();
	char stackBuf[512];
	va_list copy;
	va_copy(copy, vaArgs);
	int n = vsnprintf(stackBuf, sizeof stackBuf, szFmt, copy);
	va_end(copy);
	LONG lResult;
	if(n >= 0 && n < (int)sizeof(stackBuf)) {
		lResult = (LONG)fileWriteBytes(pFile, stackBuf, (ULONG)n);
	}
	else if(n > 0) {
		char *pHeap = (char *)memAllocFast((ULONG)n + 1);
		if(!pHeap) {
			lResult = -1;
		}
		else {
			va_copy(copy, vaArgs);
			vsnprintf(pHeap, (ULONG)n + 1, szFmt, copy);
			va_end(copy);
			lResult = (LONG)fileWriteBytes(pFile, pHeap, (ULONG)n);
			memFree(pHeap, (ULONG)n + 1);
		}
	}
	else {
		lResult = -1;
	}
	fileFlush(pFile);
	systemGetBlitterFromOs();
	systemUnuse();
	return lResult;
}

LONG filePrintf(tFile *pFile, const char *szFmt, ...) {
	va_list vaArgs;
	va_start(vaArgs, szFmt);
	LONG lResult = fileVaPrintf(pFile, szFmt, vaArgs);
	va_end(vaArgs);
	return lResult;
}

LONG fileVaScanf(tFile *pFile, const char *szFmt, va_list vaArgs) {
	(void)pFile;
	(void)szFmt;
	(void)vaArgs;
	return -1;
}

LONG fileScanf(tFile *pFile, const char *szFmt, ...) {
	va_list vaArgs;
	va_start(vaArgs, szFmt);
	LONG lResult = fileVaScanf(pFile, szFmt, vaArgs);
	va_end(vaArgs);
	return lResult;
}
#endif

void fileFlush(tFile *pFile) {
	systemUse();
	systemReleaseBlitterToOs();
#if defined(ACE_FILE_USE_ONLY_DISK)
	diskFileFlush(pFile);
#else
	pFile->pCallbacks->cbFileFlush(pFile->pData);
#endif
	systemGetBlitterFromOs();
	systemUnuse();
}

void fileWriteStr(tFile *pFile, const char *szLine) {
	fileWriteBytes(pFile, szLine, strlen(szLine));
}

UBYTE fileExists(const char *szPath) {
	systemUse();
	systemReleaseBlitterToOs();
	UBYTE isExisting = 0;
	tFile *pFile = fileOpen(szPath, "rb");
	if(pFile) {
		isExisting = 1;
		fileClose(pFile);
	}
	systemGetBlitterFromOs();
	systemUnuse();

	return isExisting;
}

UBYTE fileDelete(const char *szFilePath) {
	systemUse();
	systemReleaseBlitterToOs();
	UBYTE isSuccess = remove(szFilePath);
	systemGetBlitterFromOs();
	systemUnuse();
	return isSuccess;
}

UBYTE fileMove(const char *szSource, const char *szDest) {
	systemUse();
	systemReleaseBlitterToOs();
	UBYTE isSuccess = rename(szSource, szDest);
	systemGetBlitterFromOs();
	systemUnuse();
	return isSuccess;
}
