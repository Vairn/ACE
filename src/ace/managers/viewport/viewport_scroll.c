/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/managers/viewport/viewport_scroll.h>

/**
 * Pack single 6-bit horizontal scroll delay for AGA into BPLCON1 with equal
 * PF1/PF2 delay (single display layer). Layout per AGA HRM: low nibble in
 * bits 15-12 / 11-8, high pair in bits 7-6 / 5-4.
 */
static UWORD viewportPackBplcon1Aga6(UWORD uwFine6) {
	UWORD uwLow = uwFine6 & 0xFU;
	UWORD uwHigh = (uwFine6 >> 4) & 3U;
	return (UWORD)((uwLow << 12) | (uwLow << 8) | (uwHigh << 6) | (uwHigh << 4));
}

UWORD viewportCalcDdfStep(const tView *pView, UBYTE ubFmode) {
	UWORD uwQuad = (pView->uwWidth / 16) - 1;
	UWORD uwDDFStep = uwQuad * 8;
#ifdef ACE_USE_AGA_FEATURES
	switch(ubFmode & ACE_BITPLANE_FMODE_MASK) {
	case ACE_BITPLANE_FMODE_8BYTE:
		uwDDFStep = uwQuad * 6;
		break;
	case ACE_BITPLANE_FMODE_2BYTE:
	case ACE_BITPLANE_FMODE_BPL32:
	case ACE_BITPLANE_FMODE_BPAGE:
	default:
		uwDDFStep = uwQuad * 8;
		break;
	}
#else
	(void)ubFmode;
#endif
	return uwDDFStep;
}

void viewportCalcBplScrollX(
	const tVPort *pVPort, UWORD uwScrollXPixels,
	UWORD *pUwBplcon1, ULONG *pUlBplByteOffs
) {
#ifdef ACE_USE_AGA_FEATURES
	UBYTE ubAga64 =
		((pVPort->ubFmode & ACE_BITPLANE_FMODE_MASK) == ACE_BITPLANE_FMODE_8BYTE) &&
		!(pVPort->eFlags & VP_FLAG_HIRES);
#else
	UBYTE ubAga64 = 0;
#endif

	if(ubAga64) {
		ULONG wx = uwScrollXPixels;
		UWORD uwFine = (UWORD)((64UL - (wx & 63UL)) & 63UL);
		*pUlBplByteOffs = (wx >> 6) * 8;
		*pUwBplcon1 = viewportPackBplcon1Aga6(uwFine);
		return;
	}

	// ECS / OCS, AGA non-64-fetch, or hires (including FMODE=3 hires): ACE legacy
	UWORD uwShift = (16 - (uwScrollXPixels & 0xF)) & 0xF;
	ULONG ulBplAddX;
	if(uwScrollXPixels == 0) {
		ulBplAddX = 0;
	}
	else {
		ulBplAddX = ((ULONG)(uwScrollXPixels - 1) >> 4) << 1;
	}
	if(pVPort->eFlags & VP_FLAG_HIRES) {
		uwShift >>= 1;
		ulBplAddX -= 2;
	}
	*pUwBplcon1 = (uwShift << 4) | uwShift;
	*pUlBplByteOffs = ulBplAddX;
}
