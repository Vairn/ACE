/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_MANAGERS_VIEWPORT_DUALPFBUFFER_H_
#define _ACE_MANAGERS_VIEWPORT_DUALPFBUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ACE_USE_DUAL_PF

#include <ace/types.h>
#include <ace/managers/viewport/camera.h>
#include <ace/utils/bitmap.h>
#include <ace/utils/tag.h>
#include <ace/managers/copper.h>

// Dual PF uses interleaved bitplane slots:
//   bplpt[0] = PF1 plane 0, bplpt[1] = PF2 plane 0,
//   bplpt[2] = PF1 plane 1, bplpt[3] = PF2 plane 1, ...
// On OCS both playfields share DDF/DSTRT/DSTOP; bpl1mod = odd slots (PF1), bpl2mod = even slots (PF2).

typedef enum tDualPfCreateTags {
	TAG_DUALPF_VPORT        = (TAG_USER | 1),
	TAG_DUALPF_PF1_BOUND_W  = (TAG_USER | 2),
	TAG_DUALPF_PF2_BOUND_W  = (TAG_USER | 3),
	TAG_DUALPF_BOUND_H      = (TAG_USER | 4),
	TAG_DUALPF_PF1_IS_DBL   = (TAG_USER | 5),
	TAG_DUALPF_PF2_IS_DBL   = (TAG_USER | 6),
	TAG_DUALPF_BITMAP_FLAGS = (TAG_USER | 7),
	TAG_DUALPF_PF1_FRONT    = (TAG_USER | 8),
	TAG_DUALPF_PF1_BACK     = (TAG_USER | 9),
	TAG_DUALPF_PF2_FRONT    = (TAG_USER | 10),
	TAG_DUALPF_PF2_BACK     = (TAG_USER | 11),
} tDualPfCreateTags;

#define DUALPF_FLAG_OWN_PF1_FRONT 1
#define DUALPF_FLAG_OWN_PF1_BACK  2
#define DUALPF_FLAG_OWN_PF2_FRONT 4
#define DUALPF_FLAG_OWN_PF2_BACK  8

typedef struct _tDualPfBufferManager {
	tVpManager sCommon;
	tCameraManager *pCameraPf1;
	tCameraManager *pCameraPf2;
	tBitMap *pPf1Front, *pPf1Back;
	tBitMap *pPf2Front, *pPf2Back;
	tCopBlock *pCopBlock;
	UWORD uwModuloPf1;
	UWORD uwModuloPf2;
	UWORD uwDDfStrt;
	UWORD uwDDfStop;
	UBYTE ubFlags;
	UBYTE ubDirtyCounter;
} tDualPfBufferManager;

tDualPfBufferManager *dualPfBufferCreate(void *pTags, ...);
void dualPfBufferDestroy(tDualPfBufferManager *pManager);
void dualPfBufferProcess(tDualPfBufferManager *pManager);

#endif // ACE_USE_DUAL_PF

#ifdef __cplusplus
}
#endif

#endif // _ACE_MANAGERS_VIEWPORT_DUALPFBUFFER_H_