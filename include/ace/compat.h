/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_COMPAT_H_
#define _ACE_COMPAT_H_

/**
 * Legacy ACE API aliases used by older games (e.g. GermZ) that predate the
 * FromPath / FromFd split and the Ocs/Aga palette rename.
 *
 * Include this from the game target only (not when compiling ACE itself),
 * e.g. `target_compile_options(game PRIVATE -include ace/compat.h)`.
 */

#include <string.h>
#include <ace/utils/bitmap.h>
#include <ace/utils/palette.h>
#include <ace/utils/font.h>
#include <ace/utils/disk_file.h>
#include <ace/utils/extview.h>
#include <ace/managers/ptplayer.h>

#define bitmapCreateFromFile(szPath, isFast) bitmapCreateFromPath((szPath), (isFast))
#define bitmapLoadFromFile(pBitMap, szPath, uwX, uwY) bitmapLoadFromPath((pBitMap), (szPath), (uwX), (uwY))
#define paletteLoad(szPath, pPalette, uwMax) paletteLoadFromPath((szPath), (pPalette), (uwMax))
#define paletteDim(pSrc, pDst, uwCount, ubLevel) paletteDimOcs((pSrc), (pDst), (uwCount), (ubLevel))
#define viewUpdateCLUT(pView) viewUpdateGlobalPalette(pView)
#define fontCreate(szPath) fontCreateFromPath(szPath)
#define ptplayerModCreate(szPath) ptplayerModCreateFromPath(szPath)
#define fileExists(szPath) diskFileExists(szPath)

static inline tPtplayerSfx *ptplayerSfxCreateFromFile(const char *szPath) {
	return ptplayerSfxCreateFromPath(szPath, 0);
}

static inline tFile *fileOpen(const char *szPath, const char *szMode) {
	tDiskFileMode eMode = DISK_FILE_MODE_READ;
	if(szMode && (strchr(szMode, 'w') || strchr(szMode, 'W') || strchr(szMode, 'a') || strchr(szMode, 'A'))) {
		eMode = DISK_FILE_MODE_WRITE;
	}
	return diskFileOpen(szPath, eMode, 1);
}

static inline LONG aceCompatFileGetSizePath(const char *szPath) {
	tFile *pFile = diskFileOpen(szPath, DISK_FILE_MODE_READ, 1);
	LONG lSize;
	if(!pFile) {
		return -1;
	}
	lSize = fileGetSize(pFile);
	fileClose(pFile);
	return lSize;
}

#define fileGetSize(x) aceCompatFileGetSizePath(x)

#endif
