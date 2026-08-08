/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/managers/viewport/dualpfbuffer.h>
#include <ace/utils/tag.h>
#include <ace/utils/extview.h>
#include <ace/utils/fetchmode.h>
#include <ace/managers/system.h>

#ifdef ACE_USE_DUAL_PF

static void dualPfBufferInitializeCopperList(tDualPfBufferManager *pManager) {
	const tVPort *pVPort = pManager->sCommon.pVPort;
	UBYTE ubBppPf1 = pVPort->ubBppPf1;
	UBYTE ubBppPf2 = pVPort->ubBppPf2;

	LONG lBplOffsPf1 = fetchModeGetInitialBplOffset(pVPort);
	LONG lBplOffsPf2 = fetchModeGetInitialBplOffset(pVPort);

	tCopList *pCopList = pVPort->pView->pCopList;
	tCopBlock *pBlock = pManager->pCopBlock;
	pBlock->uwCurrCount = 0;

	// WAIT is implicit in copBlock, start with bplcon1 (dual shift)
	copMove(pCopList, pBlock, &g_pCustom->bplcon1, 0);

	// Modulo for each field
	copMove(pCopList, pBlock, &g_pCustom->bpl1mod, pManager->uwModuloPf1); // odd slots: PF1
	copMove(pCopList, pBlock, &g_pCustom->bpl2mod, pManager->uwModuloPf2); // even slots: PF2

	// ddfstrt / ddfstop (shared, placed here for consistency with simplebuffer)
	copMove(pCopList, pBlock, &g_pCustom->ddfstrt, pManager->uwDDfStrt);
	copMove(pCopList, pBlock, &g_pCustom->ddfstop, pManager->uwDDfStop);

	// Interleaved bitplane pointers: bplpt[0]=PF1p0, bplpt[1]=PF2p0, bplpt[2]=PF1p1, bplpt[3]=PF2p1...
	// Initialize with back buffer addresses + initial offset
	UBYTE ubMax = MAX(ubBppPf1, ubBppPf2);
	for(UBYTE i = 0; i < ubMax; ++i) {
		if(i < ubBppPf1) {
			ULONG ulAddr = (ULONG)pManager->pPf1Back->Planes[i] + lBplOffsPf1;
			copMove(pCopList, pBlock, &g_pBplFetch[i * 2].uwHi, ulAddr >> 16);
			copMove(pCopList, pBlock, &g_pBplFetch[i * 2].uwLo, ulAddr & 0xFFFF);
		}
		if(i < ubBppPf2) {
			ULONG ulAddr = (ULONG)pManager->pPf2Back->Planes[i] + lBplOffsPf2;
			copMove(pCopList, pBlock, &g_pBplFetch[i * 2 + 1].uwHi, ulAddr >> 16);
			copMove(pCopList, pBlock, &g_pBplFetch[i * 2 + 1].uwLo, ulAddr & 0xFFFF);
		}
	}
}

void dualPfBufferProcess(tDualPfBufferManager *pManager) {
	const tVPort *pVPort = pManager->sCommon.pVPort;
	tCopList *pCopList = pVPort->pView->pCopList;
	UBYTE ubBppPf1 = pVPort->ubBppPf1;
	UBYTE ubBppPf2 = pVPort->ubBppPf2;

	// Compute PF1 scroll
	UWORD uwScrollXPf1 = pManager->pCameraPf1->uPos.uwX;
	UWORD uwShiftPf1 = fetchModeCalcBplShift(pVPort, uwScrollXPf1);
	ULONG ulBplAddXPf1 = fetchModeCalcBplOffsetX(pVPort, uwScrollXPf1);
	ULONG ulPlaneOffsPf1 = ulBplAddXPf1 + pManager->pPf1Back->BytesPerRow * pManager->pCameraPf1->uPos.uwY;

	// Compute PF2 scroll
	UWORD uwScrollXPf2 = pManager->pCameraPf2->uPos.uwX;
	UWORD uwShiftPf2 = fetchModeCalcBplShift(pVPort, uwScrollXPf2);
	ULONG ulBplAddXPf2 = fetchModeCalcBplOffsetX(pVPort, uwScrollXPf2);
	ULONG ulPlaneOffsPf2 = ulBplAddXPf2 + pManager->pPf2Back->BytesPerRow * pManager->pCameraPf2->uPos.uwY;

	// Assemble dual BPLCON1: PF2 shift in upper nibble, PF1 shift in lower nibble
	// fetchModeCalcBplShift already returns a value with PF1 nibbles duplicated (bits 3-0 and 7-4).
	// For dual PF, we need PF1 in bits 3-0 and PF2 in bits 7-4.
	// On OCS, the base shift per playfield is a 4-bit value. Extract and recombine.
	UWORD uwBplcon1 = ((uwShiftPf2 & 0xF0) ? ((uwShiftPf2 >> 4) & 0x0F) : (uwShiftPf2 & 0x0F));
	UWORD uwBplcon1Pf1 = (uwShiftPf1 & 0x0F);
	// Correctly encode: PF1[3:0] in bits 3-0, PF2[3:0] in bits 7-4
	// The low nibble of shift value goes to respective playfield
	UWORD uwDualShift = (uwBplcon1 << 4) | uwBplcon1Pf1;

	// Also handle AGA-style extended bits if present
	if(uwShiftPf1 & 0xF0) uwDualShift |= (uwShiftPf1 & 0xF00) >> 2; // PF1 bits 10-11
	if(uwShiftPf2 & 0xF0) uwDualShift |= (uwShiftPf2 & 0xF00) >> 2; // PF2 bits 14-15

	tCopBlock *pBlock = pManager->pCopBlock;
	pBlock->uwCurrCount = 0; // Rewind to beginning
	copMove(pCopList, pBlock, &g_pCustom->bplcon1, uwDualShift);

	// Modulos stay constant per reset, skip to plane ptrs
	pBlock->uwCurrCount += 2; // skip bpl1mod, bpl2mod
	// skip ddfstrt, ddfstop
	pBlock->uwCurrCount += 2;

	// Interleaved bitplane pointers from back buffers
	UBYTE ubMax = MAX(ubBppPf1, ubBppPf2);
	for(UBYTE i = 0; i < ubMax; ++i) {
		if(i < ubBppPf1) {
			ULONG ulAddr = (ULONG)pManager->pPf1Back->Planes[i] + ulPlaneOffsPf1;
			copMove(pCopList, pBlock, &g_pBplFetch[i * 2].uwHi, ulAddr >> 16);
			copMove(pCopList, pBlock, &g_pBplFetch[i * 2].uwLo, ulAddr & 0xFFFF);
		}
		if(i < ubBppPf2) {
			ULONG ulAddr = (ULONG)pManager->pPf2Back->Planes[i] + ulPlaneOffsPf2;
			copMove(pCopList, pBlock, &g_pBplFetch[i * 2 + 1].uwHi, ulAddr >> 16);
			copMove(pCopList, pBlock, &g_pBplFetch[i * 2 + 1].uwLo, ulAddr & 0xFFFF);
		}
	}

	// Swap buffers if double-buffered
	if(pManager->pPf1Back != pManager->pPf1Front) {
		tBitMap *pTmp = pManager->pPf1Back;
		pManager->pPf1Back = pManager->pPf1Front;
		pManager->pPf1Front = pTmp;
	}
	if(pManager->pPf2Back != pManager->pPf2Front) {
		tBitMap *pTmp = pManager->pPf2Back;
		pManager->pPf2Back = pManager->pPf2Front;
		pManager->pPf2Front = pTmp;
	}
}

tDualPfBufferManager *dualPfBufferCreate(void *pTags, ...) {
	logBlockBegin("dualPfBufferCreate(pTags: %p, ...)", pTags);

	tDualPfBufferManager *pManager = memAllocFastClear(sizeof(tDualPfBufferManager));
	pManager->sCommon.process = (tVpManagerFn)dualPfBufferProcess;
	pManager->sCommon.destroy = (tVpManagerFn)dualPfBufferDestroy;
	pManager->sCommon.ubId = VPM_SCROLL;

	va_list vaTags;
	va_start(vaTags, pTags);

	tVPort *pVPort = (tVPort*)tagGet(pTags, vaTags, TAG_DUALPF_VPORT, 0);
	if(!pVPort) {
		logWrite("ERR: No parent viewport (TAG_DUALPF_VPORT) specified\n");
		goto fail;
	}
	if(!(pVPort->eFlags & VP_FLAG_DUAL_PF)) {
		logWrite("ERR: VPort does not have VP_FLAG_DUAL_PF set\n");
		goto fail;
	}
	pManager->sCommon.pVPort = pVPort;
	logWrite("Parent VPort: %p (PF1: %hhu bpp, PF2: %hhu bpp)\n",
		pVPort, pVPort->ubBppPf1, pVPort->ubBppPf2);

	UBYTE ubBppPf1 = pVPort->ubBppPf1;
	UBYTE ubBppPf2 = pVPort->ubBppPf2;

	UWORD uwBoundWidthPf1 = tagGet(pTags, vaTags, TAG_DUALPF_PF1_BOUND_W, pVPort->uwWidth);
	UWORD uwBoundWidthPf2 = tagGet(pTags, vaTags, TAG_DUALPF_PF2_BOUND_W, pVPort->uwWidth);
	UWORD uwBoundHeight = tagGet(pTags, vaTags, TAG_DUALPF_BOUND_H, pVPort->uwHeight);
	UBYTE ubBitmapFlags = tagGet(pTags, vaTags, TAG_DUALPF_BITMAP_FLAGS, BMF_CLEAR);
	UBYTE isDblBfrPf1 = tagGet(pTags, vaTags, TAG_DUALPF_PF1_IS_DBL, 0);
	UBYTE isDblBfrPf2 = tagGet(pTags, vaTags, TAG_DUALPF_PF2_IS_DBL, 0);

	// PF1 bitmaps
	pManager->pPf1Front = (tBitMap*)tagGet(pTags, vaTags, TAG_DUALPF_PF1_FRONT, 0);
	if(pManager->pPf1Front) {
		pManager->pPf1Back = (tBitMap*)tagGet(pTags, vaTags, TAG_DUALPF_PF1_BACK, 0);
		if(!pManager->pPf1Back) pManager->pPf1Back = pManager->pPf1Front;
	} else {
		pManager->pPf1Front = bitmapCreate(uwBoundWidthPf1, uwBoundHeight, ubBppPf1, ubBitmapFlags);
		pManager->ubFlags |= DUALPF_FLAG_OWN_PF1_FRONT;
		pManager->pPf1Back = pManager->pPf1Front;
	}
	if(isDblBfrPf1 && (!pManager->pPf1Back || pManager->pPf1Back == pManager->pPf1Front)) {
		pManager->pPf1Back = bitmapCreate(uwBoundWidthPf1, uwBoundHeight, ubBppPf1, ubBitmapFlags);
		pManager->ubFlags |= DUALPF_FLAG_OWN_PF1_BACK;
	}

	// PF2 bitmaps
	pManager->pPf2Front = (tBitMap*)tagGet(pTags, vaTags, TAG_DUALPF_PF2_FRONT, 0);
	if(pManager->pPf2Front) {
		pManager->pPf2Back = (tBitMap*)tagGet(pTags, vaTags, TAG_DUALPF_PF2_BACK, 0);
		if(!pManager->pPf2Back) pManager->pPf2Back = pManager->pPf2Front;
	} else {
		pManager->pPf2Front = bitmapCreate(uwBoundWidthPf2, uwBoundHeight, ubBppPf2, ubBitmapFlags);
		pManager->ubFlags |= DUALPF_FLAG_OWN_PF2_FRONT;
		pManager->pPf2Back = pManager->pPf2Front;
	}
	if(isDblBfrPf2 && (!pManager->pPf2Back || pManager->pPf2Back == pManager->pPf2Front)) {
		pManager->pPf2Back = bitmapCreate(uwBoundWidthPf2, uwBoundHeight, ubBppPf2, ubBitmapFlags);
		pManager->ubFlags |= DUALPF_FLAG_OWN_PF2_BACK;
	}

	// Compute modulos and DDF
	UWORD uwVpWidth = pVPort->uwWidth;
	pManager->uwModuloPf1 = pManager->pPf1Front->BytesPerRow - (uwVpWidth >> 3);
	pManager->uwModuloPf2 = pManager->pPf2Front->BytesPerRow - (uwVpWidth >> 3);
	pManager->uwDDfStrt = fetchModeGetDDfStrt(pVPort);
	pManager->uwDDfStop = fetchModeGetDDfStop(pVPort);
	fetchModeApplyXScrollCopper(pVPort, &pManager->uwDDfStrt, &pManager->uwModuloPf1);

	logWrite("PF1 modulo: %u, PF2 modulo: %u, DDF: %04X-%04X\n",
		pManager->uwModuloPf1, pManager->uwModuloPf2,
		pManager->uwDDfStrt, pManager->uwDDfStop);

	// Create cameras
	pManager->pCameraPf1 = cameraCreate(pVPort, 0, 0, uwBoundWidthPf1, uwBoundHeight, isDblBfrPf1);
	pManager->pCameraPf2 = cameraCreate(pVPort, 0, 0, uwBoundWidthPf2, uwBoundHeight, isDblBfrPf2);

	// Create copper block
	UBYTE ubTotalBpp = ubBppPf1 + ubBppPf2;
	// Instructions: bplcon1 + bpl1mod + bpl2mod + ddfstrt + ddfstop + (2 * ubTotalBpp plane ptrs)
	// copBlock already includes the WAIT, so subtract 1
	UWORD uwCopInstrCount = 5 + 2 * ubTotalBpp;
	pManager->pCopBlock = copBlockCreate(
		pVPort->pView->pCopList, uwCopInstrCount - 1,
		fetchModeGetCopWaitX(pVPort),
		pVPort->uwOffsY + pVPort->pView->ubPosY - 1
	);

	dualPfBufferInitializeCopperList(pManager);

	vPortAddManager(pVPort, (tVpManager*)pManager);

	va_end(vaTags);
	logBlockEnd("dualPfBufferCreate()");
	return pManager;

fail:
	// Cleanup on failure
	if(pManager) {
		// Bitmaps
		if(pManager->ubFlags & DUALPF_FLAG_OWN_PF1_FRONT && pManager->pPf1Front)
			bitmapDestroy(pManager->pPf1Front);
		if(pManager->ubFlags & DUALPF_FLAG_OWN_PF1_BACK && pManager->pPf1Back && pManager->pPf1Back != pManager->pPf1Front)
			bitmapDestroy(pManager->pPf1Back);
		if(pManager->ubFlags & DUALPF_FLAG_OWN_PF2_FRONT && pManager->pPf2Front)
			bitmapDestroy(pManager->pPf2Front);
		if(pManager->ubFlags & DUALPF_FLAG_OWN_PF2_BACK && pManager->pPf2Back && pManager->pPf2Back != pManager->pPf2Front)
			bitmapDestroy(pManager->pPf2Back);
		memFree(pManager, sizeof(tDualPfBufferManager));
	}
	va_end(vaTags);
	logBlockEnd("dualPfBufferCreate()");
	return 0;
}

void dualPfBufferDestroy(tDualPfBufferManager *pManager) {
	logBlockBegin("dualPfBufferDestroy(pManager: %p)", pManager);

	tCopList *pCopList = pManager->sCommon.pVPort->pView->pCopList;
	copBlockDestroy(pCopList, pManager->pCopBlock);

	cameraDestroy(pManager->pCameraPf1);
	cameraDestroy(pManager->pCameraPf2);

	if(pManager->ubFlags & DUALPF_FLAG_OWN_PF1_FRONT) {
		bitmapDestroy(pManager->pPf1Front);
	}
	if((pManager->ubFlags & DUALPF_FLAG_OWN_PF1_BACK) && pManager->pPf1Back && pManager->pPf1Back != pManager->pPf1Front) {
		bitmapDestroy(pManager->pPf1Back);
	}
	if(pManager->ubFlags & DUALPF_FLAG_OWN_PF2_FRONT) {
		bitmapDestroy(pManager->pPf2Front);
	}
	if((pManager->ubFlags & DUALPF_FLAG_OWN_PF2_BACK) && pManager->pPf2Back && pManager->pPf2Back != pManager->pPf2Front) {
		bitmapDestroy(pManager->pPf2Back);
	}

	memFree(pManager, sizeof(tDualPfBufferManager));
	logBlockEnd("dualPfBufferDestroy()");
}

#endif // ACE_USE_DUAL_PF