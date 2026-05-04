/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_UTILS_FILE_H_
#define _ACE_UTILS_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <ace/types.h>

#define FILE_SEEK_CURRENT SEEK_CUR
#define FILE_SEEK_SET SEEK_SET
#define FILE_SEEK_END SEEK_END

#if defined(ACE_FILE_USE_ONLY_DISK)
typedef void *tFile;
#else
typedef void (*tCbFileClose)(void *pData);
typedef ULONG (*tCbFileRead)(void *pData, void *pDest, ULONG ulSize);
typedef ULONG (*tCbFileWrite)(void *pData, const void *pSrc, ULONG ulSize);
typedef ULONG (*tCbFileSeek)(void *pData, LONG lPos, WORD wMode);
typedef ULONG (*tCbFileGetPos)(void *pData);
typedef ULONG (*tCbFileGetSize)(void *pData);
typedef UBYTE (*tCbFileIsEof)(void *pData);
typedef void (*tCbFileFlush)(void *pData);

typedef struct tFileCallbacks {
	tCbFileClose cbFileClose;
	tCbFileRead cbFileRead;
	tCbFileWrite cbFileWrite;
	tCbFileSeek cbFileSeek;
	tCbFileGetPos cbFileGetPos;
	tCbFileGetSize cbFileGetSize;
	tCbFileIsEof cbFileIsEof;
	tCbFileFlush cbFileFlush;
} tFileCallbacks;

typedef struct tFile {
	const tFileCallbacks *pCallbacks;
	void *pData;
} tFile;
#endif

/**
 * @brief Opens a file on the host filesystem (Amiga: DOS via disk layer).
 */
tFile *fileOpen(const char *szPath, const char *szMode);

void fileClose(tFile *pFile);

/**
 * @brief Read bytes from an open tFile (alias of fileReadBytes for FILE-like use).
 */
ULONG fileRead(tFile *pFile, void *pDest, ULONG ulSize);

/**
 * @brief Write bytes to an open tFile.
 */
ULONG fileWrite(tFile *pFile, const void *pSrc, ULONG ulSize);

ULONG fileReadBytes(tFile *pFile, UBYTE *pDest, ULONG ulSize);

ULONG fileReadWords(tFile *pFile, UWORD *pDest, ULONG ulSize);

ULONG fileReadLongs(tFile *pFile, ULONG *pDest, ULONG ulSize);

ULONG fileWriteBytes(tFile *pFile, const void *pSrc, ULONG ulSize);

ULONG fileWriteWords(tFile *pFile, const void *pSrc, ULONG ulSize);

ULONG fileWriteLongs(tFile *pFile, const void *pSrc, ULONG ulSize);


ULONG fileSeek(tFile *pFile, LONG lPos, WORD wMode);

ULONG fileGetPos(tFile *pFile);

UBYTE fileIsEof(tFile *pFile);

void fileFlush(tFile *pFile);

/**
 * @brief Returns file size of file, in bytes.
 *
 * @param pFile File handle.
 * @return On fail -1, otherwise file size in bytes.
 */
LONG fileGetSize(tFile *pFile);

/**
 * @brief Returns file size by filesystem path (does not keep a handle open).
 */
LONG fileGetPathSize(const char *szPath);

void fileWriteStr(tFile *pFile, const char *szLine);

#ifdef __cplusplus
}
#endif

#endif // _ACE_UTILS_FILE_H_
