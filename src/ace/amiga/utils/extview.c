/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/utils/extview.h>
#include <ace/managers/system.h>
#include <ace/managers/copper.h>
#include <ace/managers/log.h>
#include <ace/utils/custom.h>

UBYTE viewIsLoaded(const tView *pView) {
	UBYTE isLoaded = (g_sCopManager.pCopList == pView->pCopList);
	return isLoaded;
}

/**
 *  @todo bplcon0 BPP is set up globally - make it only when all vports
 *        are truly of same BPP.
 */
void viewLoad(tView *pView) {
	logBlockBegin("viewLoad(pView: %p)", pView);

	UBYTE isPAL = systemIsPal();
	UWORD uwWaitPos = isPAL ? 300 : 260;
	while(getRayPos().bfPosY < uwWaitPos) {
	}
#if defined(AMIGA)
	if(!pView) {
		g_sCopManager.pCopList = g_sCopManager.pBlankList;
		g_pCustom->bplcon0 = 0;
#ifdef ACE_USE_AGA_FEATURES
		g_pCustom->bplcon3 = 0;
		g_pCustom->fmode = 0;
#else
		g_pCustom->bplcon3 = 0;
		g_pCustom->fmode = 0;
#endif
		for(UBYTE i = 0; i < 8; ++i) {
			g_pCustom->bplpt[i] = 0;
		}
		g_pCustom->bpl1mod = 0;
		g_pCustom->bpl2mod = 0;
	}
	else {
#if defined(ACE_DEBUG)
		{
			tVPort *pVp = pView->pFirstVPort;
			while(pVp->pNext) {
				pVp = pVp->pNext;
			}
			if(pVp->uwOffsY + pVp->uwHeight != pView->uwHeight) {
				logWrite(
					"ERR: View height %hu doesn't match the total VPort area: %hu",
					pView->uwHeight, pVp->uwOffsY + pVp->uwHeight
				);
			}
		}
#endif
		pView->uwBplCon0 = 0;
		if(pView->uwFlags & VIEW_FLAG_GLOBAL_BPP) {
			pView->uwBplCon0 |= pView->pFirstVPort->ubBpp << 12;
		}
		if(pView->uwFlags & VIEW_FLAG_GLOBAL_HRES) {
			pView->uwBplCon0 |= ((pView->pFirstVPort->eFlags & VP_FLAG_HIRES) != 0) << 15;
		}
		pView->uwBplCon0 |= BV(9);

		g_sCopManager.pCopList = pView->pCopList;
#ifdef ACE_USE_AGA_FEATURES
		if(pView->pFirstVPort->eFlags & VP_FLAG_AGA) {
			g_pCustom->bplcon0 = ((0x07 & pView->pFirstVPort->ubBpp) << 12) | BV(9);
			if(pView->pFirstVPort->ubBpp & 0x08) {
				g_pCustom->bplcon0 |= BV(4);
			}
			if(pView->pFirstVPort->ubBpp == 6) {
				g_pCustom->bplcon2 = BV(9);
			}
		}
		else {
			g_pCustom->bplcon0 = (pView->pFirstVPort->ubBpp << 12) | BV(9);
			g_pCustom->bplcon2 = 0;
		}
		g_pCustom->fmode = pView->pFirstVPort->ubFmode;
		g_pCustom->bplcon3 = 0;
#else
		g_pCustom->bplcon0 = (pView->pFirstVPort->ubBpp << 12) | BV(9);
		g_pCustom->bplcon2 = 0;
		g_pCustom->fmode = 0;
		g_pCustom->bplcon3 = 0;
#endif
		g_pCustom->diwstrt = (pView->ubPosY << 8) | 0x81;
		g_pCustom->bplcon4 = 0x0011;
		UWORD uwDiwStartX = pView->ubPosX;
		UWORD uwDiwStopX = uwDiwStartX + pView->uwWidth - 256;
		UWORD uwDiwStopY = pView->ubPosY + pView->uwHeight;

		if(BTST(uwDiwStopY, 8) == BTST(uwDiwStopY, 7)) {
			logWrite(
				"ERR: DiwStopY (%hu) bit 8 (%hhu) must be different than bit 7 (%hhu)\n",
				uwDiwStopY, BTST(uwDiwStopY, 8), BTST(uwDiwStopY, 7)
			);
		}
		g_pCustom->diwstrt = (pView->ubPosY << 8) | uwDiwStartX;
		g_pCustom->diwstop = ((uwDiwStopY & 0xFF) << 8) | uwDiwStopX;
		viewUpdateGlobalPalette(pView);
	}
	copProcessBlocks();
	g_pCustom->copjmp1 = 1;
	systemSetDmaBit(DMAB_RASTER, pView != 0);

	while(getRayPos().bfPosY < uwWaitPos) {
	}

#endif
	logBlockEnd("viewLoad()");
}

void vPortWaitForPos(const tVPort *pVPort, UWORD uwPosY, UBYTE isExact) {
#ifdef AMIGA
	UWORD uwEndPos = pVPort->uwOffsY + uwPosY;
	uwEndPos += pVPort->pView->ubPosY;
#if defined(ACE_DEBUG)
	UWORD yPos = systemIsPal() ? 312 : 272;
	if(uwEndPos >= yPos) {
		logWrite("ERR: vPortWaitForPos - too big wait pos: %04hx (%hu)\n", uwEndPos, uwEndPos);
		logWrite("\tVPort offs: %hu, pos: %hu\n", pVPort->uwOffsY, uwPosY);
	}
#endif

	if(isExact) {
		while(getRayPos().bfPosY >= uwEndPos) {
		}
	}
	while(getRayPos().bfPosY < uwEndPos) {
	}

#endif
}
