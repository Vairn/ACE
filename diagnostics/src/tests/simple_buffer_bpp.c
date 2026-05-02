#include "tests/simple_buffer_bpp.h"
#include <ace/managers/blit.h>
#include <ace/managers/game.h>
#include <ace/managers/key.h>
#include <ace/managers/system.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/utils/font.h>
#include <stdio.h>

#include "diagnostics.h"

static tView *s_pView;
static tVPort *s_pVPort;
static tSimpleBufferManager *s_pBuffer;
static tFont *s_pFont;
static tTextBitMap *s_pTextBitMap;

static UWORD makePaletteColor(UBYTE ubIndex, UBYTE ubColorCount) {
	UBYTE ubStep = ubColorCount > 1 ? (15 * ubIndex) / (ubColorCount - 1) : 0;
	UBYTE ubR = ubStep;
	UBYTE ubG = (ubIndex * 5) & 0x0F;
	UBYTE ubB = 15 - ubStep;

	return (ubR << 8) | (ubG << 4) | ubB;
}

static void setupPalette(UBYTE ubBpp) {
	UBYTE ubColorCount = 1 << ubBpp;

	s_pVPort->pPalette[0] = 0x000;
	for(UBYTE i = 1; i < ubColorCount; ++i) {
		s_pVPort->pPalette[i] = makePaletteColor(i, ubColorCount);
	}
	s_pVPort->pPalette[ubColorCount - 1] = 0xFFF;
}

static void drawPattern(UBYTE ubBpp) {
	UBYTE ubColorCount = 1 << ubBpp;
	UWORD uwWidth = s_pBuffer->uBfrBounds.uwX;
	UWORD uwHeight = s_pBuffer->uBfrBounds.uwY;
	UWORD uwStripeHeight = 12;
	UWORD uwBlockSize = 24;
	UBYTE ubBright = ubColorCount - 1;

	blitRect(s_pBuffer->pBack, 0, 0, uwWidth, uwHeight, 0);

	for(UWORD y = 0; y < uwHeight; y += uwStripeHeight) {
		UBYTE ubColor = 1 + ((y / uwStripeHeight) % (ubColorCount - 1));
		blitRect(s_pBuffer->pBack, 0, y, uwWidth, uwStripeHeight, ubColor);
	}

	for(UWORD y = 64; y < uwHeight; y += uwBlockSize) {
		for(UWORD x = 0; x < uwWidth; x += uwBlockSize) {
			UBYTE ubColor = (x / uwBlockSize + y / uwBlockSize) % ubColorCount;
			blitRect(s_pBuffer->pBack, x, y, uwBlockSize / 2, uwBlockSize / 2, ubColor);
		}
	}

	for(UWORD x = 0; x < uwWidth; x += 32) {
		blitRect(s_pBuffer->pBack, x, 0, 1, uwHeight, ubBright);
	}
	for(UWORD y = 0; y < uwHeight; y += 32) {
		blitRect(s_pBuffer->pBack, 0, y, uwWidth, 1, ubBright);
	}

	blitRect(s_pBuffer->pBack, 0, 0, uwWidth, 1, ubBright);
	blitRect(s_pBuffer->pBack, 0, uwHeight - 1, uwWidth, 1, ubBright);
	blitRect(s_pBuffer->pBack, 0, 0, 1, uwHeight, ubBright);
	blitRect(s_pBuffer->pBack, uwWidth - 1, 0, 1, uwHeight, ubBright);
}

static void drawHeader(UBYTE ubBpp) {
	char szTitle[64];
	UBYTE ubTextColor = (1 << ubBpp) - 1;

	sprintf(szTitle, "DIAG: %s", diagnosticsGetCurrentName());
	fontDrawStr(
		s_pFont, s_pBuffer->pBack, 4, 4,
		szTitle, ubTextColor, FONT_LEFT | FONT_TOP, s_pTextBitMap
	);
	fontDrawStr(
		s_pFont, s_pBuffer->pBack, 4, 14,
		"SPACE next  BACKSPACE prev  ESC quit",
		ubTextColor, FONT_LEFT | FONT_TOP, s_pTextBitMap
	);
}

void diagSimpleBufferBppCreate(void) {
	UBYTE ubBpp = diagnosticsGetCurrentBpp();

	s_pView = viewCreate(0,
		TAG_VIEW_GLOBAL_PALETTE, 1,
	TAG_END);
	s_pVPort = vPortCreate(0,
		TAG_VPORT_VIEW, s_pView,
		TAG_VPORT_BPP, ubBpp,
	TAG_END);
	s_pBuffer = simpleBufferCreate(0,
		TAG_SIMPLEBUFFER_VPORT, s_pVPort,
		TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_CLEAR,
	TAG_END);

	setupPalette(ubBpp);

	s_pFont = fontCreateFromPath("data/fonts/quaver.fnt");
	s_pTextBitMap = fontCreateTextBitMap(336, s_pFont->uwHeight);

	drawPattern(ubBpp);
	drawHeader(ubBpp);

	systemUnuse();
	viewLoad(s_pView);
}

void diagSimpleBufferBppLoop(void) {
	if(keyUse(KEY_ESCAPE)) {
		gameExit();
		return;
	}
	if(keyUse(KEY_SPACE)) {
		diagnosticsNextTest();
		return;
	}
	if(keyUse(KEY_BACKSPACE)) {
		diagnosticsPrevTest();
		return;
	}

	vPortWaitForEnd(s_pVPort);
}

void diagSimpleBufferBppDestroy(void) {
	systemUse();
	viewDestroy(s_pView);

	fontDestroyTextBitMap(s_pTextBitMap);
	fontDestroy(s_pFont);
}
