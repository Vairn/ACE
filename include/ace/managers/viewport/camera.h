/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_MANAGERS_VIEWPORT_CAMERA_H_
#define _ACE_MANAGERS_VIEWPORT_CAMERA_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 2D Camera manager
 * Keeps track of previous and current XY
 * Datasource only - scrolling etc. should be done as separate managers
 * All coords are generally used as top-left position of camera unless specified otherwise
 */

#include <ace/types.h>
#include <ace/utils/extview.h>
#ifdef ACE_CAMERA_SUBPIXEL
#include <fixmath/fix16.h>
#endif

typedef struct _tCameraManager {
	tVpManager sCommon;
	tUwCoordYX uPos;        ///< Current camera pos (floor when ACE_CAMERA_SUBPIXEL)
	tUwCoordYX uLastPos[2]; ///< Previous camera pos
	tUwCoordYX uMaxPos;     ///< Max camera pos: world W&H - camera W&H
	UBYTE ubBfr;            ///< Currently used buffer for double buffering
	UBYTE isDblBfr;
#ifdef ACE_CAMERA_SUBPIXEL
	fix16_t fPosX;          ///< Sub-pixel camera X (when ACE_CAMERA_SUBPIXEL)
	fix16_t fPosY;
	fix16_t fLastPosX;      ///< Previous frame fPos (for cameraIsMoved)
	fix16_t fLastPosY;
#endif
} tCameraManager;

tCameraManager *cameraCreate(
	tVPort *pVPort, UWORD uwPosX, UWORD uwPosY, UWORD uwMaxX, UWORD uwMaxY,
	UBYTE isDblBfr
);

void cameraDestroy(tCameraManager *pManager);
void cameraProcess(tCameraManager *pManager);

void cameraReset(
	tCameraManager *pManager,
	UWORD uwPosX, UWORD uwPosY, UWORD uwMaxX, UWORD uwMaxY, UBYTE isDblBfr
);

void cameraSetCoord(tCameraManager *pManager, UWORD uwX, UWORD uwY);

void cameraMoveBy(tCameraManager *pManager, WORD wDx, WORD wDy);

void cameraCenterAt(tCameraManager *pManager, UWORD uwAvgX, UWORD uwAvgY);

UBYTE cameraIsMoved(const tCameraManager *pManager);

UWORD cameraGetXDiff(const tCameraManager *pManager);

UWORD cameraGetYDiff(const tCameraManager *pManager);

WORD cameraGetDeltaX(const tCameraManager *pManager);

WORD cameraGetDeltaY(const tCameraManager *pManager);

/**
 * Integer pixel coordinate for copper / horizontal scroll (nearest to fractional pos).
 */
UWORD cameraGetScrollPixelX(const tCameraManager *pManager);

/**
 * Integer pixel coordinate for copper / vertical scroll (nearest to fractional pos).
 */
UWORD cameraGetScrollPixelY(const tCameraManager *pManager);

#ifdef ACE_CAMERA_SUBPIXEL
/**
 * Sets camera position in 16.16 fixed point; updates uPos to floor toward zero.
 */
void cameraSetCoordFixed(tCameraManager *pManager, fix16_t fX, fix16_t fY);

/**
 * Moves camera by fractional deltas; clamps to max bounds.
 */
void cameraMoveByFixed(tCameraManager *pManager, fix16_t fDx, fix16_t fDy);
#endif

#ifdef __cplusplus
}
#endif

#endif // _ACE_MANAGERS_VIEWPORT_CAMERA_H_
