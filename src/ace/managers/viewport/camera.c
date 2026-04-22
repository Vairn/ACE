/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/managers/viewport/camera.h>
#include <ace/macros.h>
#ifdef ACE_CAMERA_SUBPIXEL
#include <fixmath/fix16.h>

static fix16_t cameraClampFixScalar(fix16_t fPos, fix16_t fZero, fix16_t fMax) {
	if(fPos < fZero) {
		return fZero;
	}
	if(fPos > fMax) {
		return fMax;
	}
	return fPos;
}

/**
 * Floor toward zero for nonnegative fixed values (tile / uPos sync).
 */
static UWORD cameraFixedPosToUwFloor(fix16_t fPos) {
	if(fPos < 0) {
		return 0;
	}
	return (UWORD)(fPos >> 16);
}

/**
 * Nearest UWORD pixel for copper (nonnegative).
 */
static UWORD cameraFixedPosToUwNearest(fix16_t fPos) {
	if(fPos < 0) {
		return 0;
	}
	return (UWORD)((fPos + (fix16_one >> 1)) >> 16);
}
#endif

tCameraManager *cameraCreate(
	tVPort *pVPort, UWORD uwPosX, UWORD uwPosY, UWORD uwMaxX, UWORD uwMaxY,
	UBYTE isDblBfr
) {
	logBlockBegin(
		"cameraCreate(pVPort: %p, uwPosX: %u, uwPosY: %u, uwMaxX: %u, uwMaxY: %u, isDblBfr: %hhu)",
		pVPort, uwPosX, uwPosY, uwMaxX, uwMaxY, isDblBfr
	);
	tCameraManager *pManager;

	pManager = memAllocFastClear(sizeof(tCameraManager));
	logWrite("Addr: %p\n", pManager);
	pManager->sCommon.process = (tVpManagerFn)cameraProcess;
	pManager->sCommon.destroy = (tVpManagerFn)cameraDestroy;
	pManager->sCommon.pVPort = pVPort;
	pManager->sCommon.ubId = VPM_CAMERA;

	logWrite("Resetting camera bounds...\n");
	cameraReset(pManager, uwPosX, uwPosY, uwMaxX, uwMaxY, isDblBfr);

	logWrite("Attaching camera to VPort...\n");
	vPortAddManager(pVPort, (tVpManager*)pManager);
	logBlockEnd("cameraCreate()");
	return pManager;
}

void cameraDestroy(tCameraManager *pManager) {
	logWrite("cameraManagerDestroy...");
	memFree(pManager, sizeof(tCameraManager));
	logWrite("OK! \n");
}

void cameraProcess(tCameraManager *pManager) {
#ifdef ACE_CAMERA_SUBPIXEL
	pManager->fLastPosX = pManager->fPosX;
	pManager->fLastPosY = pManager->fPosY;
#endif
	pManager->uLastPos[pManager->ubBfr].ulYX = pManager->uPos.ulYX;
	if(pManager->isDblBfr) {
		pManager->ubBfr = !pManager->ubBfr;
	}
}

void cameraReset(
	tCameraManager *pManager,
	UWORD uwStartX, UWORD uwStartY, UWORD uwWidth, UWORD uwHeight, UBYTE isDblBfr
) {
	logBlockBegin(
		"cameraReset(pManager: %p, uwStartX: %u, uwStartY: %u, uwWidth: %u, uwHeight: %u, isDblBfr: %hhu)",
		pManager, uwStartX, uwStartY, uwWidth, uwHeight, isDblBfr
	);

	pManager->uPos.uwX = uwStartX;
	pManager->uPos.uwY = uwStartY;
	pManager->uLastPos[0].uwX = uwStartX;
	pManager->uLastPos[0].uwY = uwStartY;
	pManager->uLastPos[1].uwX = uwStartX;
	pManager->uLastPos[1].uwY = uwStartY;
	pManager->isDblBfr = isDblBfr;
	pManager->ubBfr = 0;

#ifdef ACE_CAMERA_SUBPIXEL
	pManager->fPosX = fix16_from_int(uwStartX);
	pManager->fPosY = fix16_from_int(uwStartY);
	pManager->fLastPosX = pManager->fPosX;
	pManager->fLastPosY = pManager->fPosY;
#endif

	// Max camera coords based on viewport size
	pManager->uMaxPos.uwX = uwWidth - pManager->sCommon.pVPort->uwWidth;
	pManager->uMaxPos.uwY = uwHeight - pManager->sCommon.pVPort->uwHeight;
	logWrite("Camera max coord: %u,%u\n", pManager->uMaxPos.uwX, pManager->uMaxPos.uwY);

	logBlockEnd("cameraReset()");
}

void cameraSetCoord(tCameraManager *pManager, UWORD uwX, UWORD uwY) {
	pManager->uPos.uwX = uwX;
	pManager->uPos.uwY = uwY;
#ifdef ACE_CAMERA_SUBPIXEL
	pManager->fPosX = fix16_from_int(uwX);
	pManager->fPosY = fix16_from_int(uwY);
#endif
}

void cameraMoveBy(tCameraManager *pManager, WORD wDx, WORD wDy) {
#ifndef ACE_CAMERA_SUBPIXEL
	pManager->uPos.uwX = CLAMP(pManager->uPos.uwX+wDx, 0, pManager->uMaxPos.uwX);
	pManager->uPos.uwY = CLAMP(pManager->uPos.uwY+wDy, 0, pManager->uMaxPos.uwY);
#else
	{
		fix16_t fDx = fix16_from_int(wDx);
		fix16_t fDy = fix16_from_int(wDy);
		fix16_t fZero = fix16_from_int(0);
		fix16_t fMaxX = fix16_from_int((int)pManager->uMaxPos.uwX);
		fix16_t fMaxY = fix16_from_int((int)pManager->uMaxPos.uwY);
		pManager->fPosX = cameraClampFixScalar(fix16_add(pManager->fPosX, fDx), fZero, fMaxX);
		pManager->fPosY = cameraClampFixScalar(fix16_add(pManager->fPosY, fDy), fZero, fMaxY);
		pManager->uPos.uwX = cameraFixedPosToUwFloor(pManager->fPosX);
		pManager->uPos.uwY = cameraFixedPosToUwFloor(pManager->fPosY);
	}
#endif
}

void cameraCenterAt(tCameraManager *pManager, UWORD uwAvgX, UWORD uwAvgY) {
	tVPort *pVPort;

	pVPort = pManager->sCommon.pVPort;
#ifndef ACE_CAMERA_SUBPIXEL
	pManager->uPos.uwX = CLAMP(uwAvgX - (pVPort->uwWidth>>1), 0, pManager->uMaxPos.uwX);
	pManager->uPos.uwY = CLAMP(uwAvgY - (pVPort->uwHeight>>1), 0, pManager->uMaxPos.uwY);
#else
	{
		UWORD uwCx = CLAMP(uwAvgX - (pVPort->uwWidth>>1), 0, pManager->uMaxPos.uwX);
		UWORD uwCy = CLAMP(uwAvgY - (pVPort->uwHeight>>1), 0, pManager->uMaxPos.uwY);
		pManager->fPosX = fix16_from_int((int)uwCx);
		pManager->fPosY = fix16_from_int((int)uwCy);
		pManager->uPos.uwX = uwCx;
		pManager->uPos.uwY = uwCy;
	}
#endif
}

UBYTE cameraIsMoved(const tCameraManager *pManager) {
#ifdef ACE_CAMERA_SUBPIXEL
	if((pManager->fPosX != pManager->fLastPosX) || (pManager->fPosY != pManager->fLastPosY)) {
		return 1;
	}
#endif
	return pManager->uPos.ulYX != pManager->uLastPos[pManager->ubBfr].ulYX;
}

UWORD cameraGetXDiff(const tCameraManager *pManager) {
	return ABS(cameraGetDeltaX(pManager));
}

UWORD cameraGetYDiff(const tCameraManager *pManager) {
	return ABS(cameraGetDeltaY(pManager));
}

WORD cameraGetDeltaX(const tCameraManager *pManager) {
	return (pManager->uPos.uwX - pManager->uLastPos[pManager->ubBfr].uwX);
}

WORD cameraGetDeltaY(const tCameraManager *pManager) {
	return (pManager->uPos.uwY - pManager->uLastPos[pManager->ubBfr].uwY);
}

UWORD cameraGetScrollPixelX(const tCameraManager *pManager) {
#ifndef ACE_CAMERA_SUBPIXEL
	return pManager->uPos.uwX;
#else
	return cameraFixedPosToUwNearest(pManager->fPosX);
#endif
}

UWORD cameraGetScrollPixelY(const tCameraManager *pManager) {
#ifndef ACE_CAMERA_SUBPIXEL
	return pManager->uPos.uwY;
#else
	return cameraFixedPosToUwNearest(pManager->fPosY);
#endif
}

#ifdef ACE_CAMERA_SUBPIXEL
void cameraSetCoordFixed(tCameraManager *pManager, fix16_t fX, fix16_t fY) {
	fix16_t fZero = fix16_from_int(0);
	fix16_t fMaxX = fix16_from_int((int)pManager->uMaxPos.uwX);
	fix16_t fMaxY = fix16_from_int((int)pManager->uMaxPos.uwY);
	pManager->fPosX = cameraClampFixScalar(fX, fZero, fMaxX);
	pManager->fPosY = cameraClampFixScalar(fY, fZero, fMaxY);
	pManager->uPos.uwX = cameraFixedPosToUwFloor(pManager->fPosX);
	pManager->uPos.uwY = cameraFixedPosToUwFloor(pManager->fPosY);
}

void cameraMoveByFixed(tCameraManager *pManager, fix16_t fDx, fix16_t fDy) {
	fix16_t fZero = fix16_from_int(0);
	fix16_t fMaxX = fix16_from_int((int)pManager->uMaxPos.uwX);
	fix16_t fMaxY = fix16_from_int((int)pManager->uMaxPos.uwY);
	pManager->fPosX = cameraClampFixScalar(fix16_add(pManager->fPosX, fDx), fZero, fMaxX);
	pManager->fPosY = cameraClampFixScalar(fix16_add(pManager->fPosY, fDy), fZero, fMaxY);
	pManager->uPos.uwX = cameraFixedPosToUwFloor(pManager->fPosX);
	pManager->uPos.uwY = cameraFixedPosToUwFloor(pManager->fPosY);
}
#endif
