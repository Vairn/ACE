/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/utils/sprite.h>
#include <ace/utils/custom.h>

tCopBlock *spriteDisableInCopBlockMode(tCopList *pList, tSpriteMask eSpriteMask, ULONG pBlankSprite[1]) {
	// TODO: move to sprite manager?
	UBYTE ubCmdCount = 0;
	tSpriteMask eMask = eSpriteMask;

	// Determine instruction count
	for(UBYTE i = HARDWARE_SPRITE_CHANNEL_COUNT; i--;) {
		if(eMask & 1) {
			ubCmdCount += 2;
		}
		eMask >>= 1;
	}

	// Set instructions
	ULONG ulBlank = (ULONG)pBlankSprite;
	tCopBlock *pBlock = copBlockCreate(pList, ubCmdCount, 0, 0);
	eMask = eSpriteMask;
	for(UBYTE i = 0; i < HARDWARE_SPRITE_CHANNEL_COUNT; ++i) {
		if(eMask & 1) {
			copMove(pList, pBlock, &g_pSprFetch[i].uwHi, ulBlank >> 16);
			copMove(pList, pBlock, &g_pSprFetch[i].uwLo, ulBlank & 0xFFFF);
		}
		eMask >>= 1;
	}
	return pBlock;
}

UBYTE spriteDisableInCopRawMode(
	tCopList *pList, tSpriteMask eSpriteMask, UWORD uwCmdOffs, ULONG pBlankSprite[1]
) {
	// TODO: move to sprite
	UBYTE ubCmdCount = 0;
	ULONG ulBlank = (ULONG)pBlankSprite;

	// No WAIT - could be done earlier by other stuff
	tCopMoveCmd *pCmd = &pList->pBackBfr->pList[uwCmdOffs].sMove;
	for(UBYTE i = 0; i < HARDWARE_SPRITE_CHANNEL_COUNT; ++i) {
		if(eSpriteMask & 1) {
			copSetMove(pCmd++, &g_pSprFetch[i].uwHi, ulBlank >> 16);
			copSetMove(pCmd++, &g_pSprFetch[i].uwLo, ulBlank & 0xFFFF);
			ubCmdCount += 2;
		}
		eSpriteMask >>= 1;
	}

	// Copy to front buffer
	for(UWORD i = uwCmdOffs; i < uwCmdOffs + ubCmdCount; ++i) {
		pList->pFrontBfr->pList[i].ulCode = pList->pBackBfr->pList[i].ulCode;
	}

	return ubCmdCount;
}

#ifdef ACE_USE_AGA_FEATURES
/**
 * BPLCON4: bits 15-12 ESPRM (even sprites), 11-8 OSPRM (odd sprites),
 * 7-0 BPLAM (playfield XOR). Preserve XOR and the other sprite bank.
 */
void spriteSetOddColorPaletteBank(UBYTE ubIndex) {
	UWORD uw = g_pCustom->bplcon4;

	g_pCustom->bplcon4 = (UWORD)((uw & 0xF0FF) | (((UWORD)(ubIndex & 0x0F)) << 8));
}

void spriteSetEvenColorPaletteBank(UBYTE ubIndex) {
	UWORD uw = g_pCustom->bplcon4;

	g_pCustom->bplcon4 = (UWORD)((uw & 0x0FFF) | (((UWORD)(ubIndex & 0x0F)) << 12));
}
#endif