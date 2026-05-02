#include "tests/simple_buffer_bpp.h"
#include <ace/managers/blit.h>
#include <ace/managers/game.h>
#include <ace/managers/key.h>
#include <ace/managers/system.h>
#include <ace/managers/timer.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <stdio.h>

#include "diagnostics.h"

static tView *s_pView;
static tVPort *s_pVPort;
static tSimpleBufferManager *s_pBuffer;
static UBYTE s_isFallbackScreen;
static ULONG s_ulAutoAdvanceStart;

static UWORD makePaletteColor(UWORD uwIndex, UWORD uwColorCount) {
	UBYTE ubStep = uwColorCount > 1 ? (15 * uwIndex) / (uwColorCount - 1) : 0;
	UBYTE ubR = ubStep;
	UBYTE ubG = (uwIndex * 5) & 0x0F;
	UBYTE ubB = 15 - ubStep;

	return (ubR << 8) | (ubG << 4) | ubB;
}

static ULONG makePaletteColorAga(UWORD uwIndex, UWORD uwColorCount) {
	UBYTE ubStep = uwColorCount > 1 ? (255 * uwIndex) / (uwColorCount - 1) : 0;
	UBYTE ubR = ubStep;
	UBYTE ubG = (uwIndex * 37) & 0xFF;
	UBYTE ubB = 255 - ubStep;

	return ((ULONG)ubR << 16) | ((ULONG)ubG << 8) | ubB;
}

static void setupPalette(UBYTE ubBpp, UBYTE isAgaMode) {
	UWORD uwColorCount = diagnosticsIsCurrentEhb() ? 32 : (1 << ubBpp);

	if(isAgaMode) {
		ULONG *pPalette = (ULONG *)s_pVPort->pPalette;

		pPalette[0] = 0x000000;
		for(UWORD i = 1; i < uwColorCount; ++i) {
			pPalette[i] = makePaletteColorAga(i, uwColorCount);
		}
		pPalette[uwColorCount - 1] = 0xFFFFFF;
		return;
	}

	s_pVPort->pPalette[0] = 0x000;
	for(UWORD i = 1; i < uwColorCount; ++i) {
		s_pVPort->pPalette[i] = makePaletteColor(i, uwColorCount);
	}
	s_pVPort->pPalette[uwColorCount - 1] = 0xFFF;
}

static void drawPattern(UBYTE ubBpp) {
	UWORD uwColorCount = 1 << ubBpp;
	UWORD uwWidth = s_pBuffer->uBfrBounds.uwX;
	UWORD uwHeight = s_pBuffer->uBfrBounds.uwY;
	UWORD uwStripeHeight = 12;
	UWORD uwBlockSize = 24;
	UBYTE ubBright = diagnosticsIsCurrentEhb() ? 31 : uwColorCount - 1;

	blitRect(s_pBuffer->pBack, 0, 0, uwWidth, uwHeight, 0);

	for(UWORD y = 0; y < uwHeight; y += uwStripeHeight) {
		UBYTE ubColor = 1 + ((y / uwStripeHeight) % (uwColorCount - 1));
		blitRect(s_pBuffer->pBack, 0, y, uwWidth, uwStripeHeight, ubColor);
	}

	for(UWORD y = 64; y < uwHeight; y += uwBlockSize) {
		for(UWORD x = 0; x < uwWidth; x += uwBlockSize) {
			UBYTE ubColor = (x / uwBlockSize + y / uwBlockSize) % uwColorCount;
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

static void drawFallbackScreen(void) {
	blitRect(
		s_pBuffer->pBack, 0, 0,
		s_pBuffer->uBfrBounds.uwX, s_pBuffer->uBfrBounds.uwY, 0
	);
}

static void drawPaletteSwatches(UBYTE ubBpp) {
	UWORD uwColorCount = 1 << ubBpp;
	UWORD uwWidth = s_pBuffer->uBfrBounds.uwX;
	UWORD uwHeight = s_pBuffer->uBfrBounds.uwY;
	UWORD uwSquare = 6;
	UWORD uwStep = 8;
	UWORD uwMaxCols = (uwWidth - 8) / uwStep;
	UWORD uwCols = uwColorCount < uwMaxCols ? uwColorCount : uwMaxCols;
	UWORD uwRows = (uwColorCount + uwCols - 1) / uwCols;
	UWORD uwStartX = 4;
	UWORD uwStartY = uwHeight - 4 - uwRows * uwStep;
	UWORD uwBackWidth = uwCols * uwStep + 4;
	UWORD uwBackHeight = uwRows * uwStep + 4;

	blitRect(s_pBuffer->pBack, 2, uwStartY - 2, uwBackWidth, uwBackHeight, 0);

	for(UWORD i = 0; i < uwColorCount; ++i) {
		UWORD uwX = uwStartX + (i % uwCols) * uwStep;
		UWORD uwY = uwStartY + (i / uwCols) * uwStep;

		blitRect(s_pBuffer->pBack, uwX, uwY, uwSquare, uwSquare, i);
	}
}

static void drawHeaderLine(UWORD uwY, const char *szText, UBYTE ubTextColor) {
	blitRect(s_pBuffer->pBack, 0, uwY, s_pBuffer->uBfrBounds.uwX, 9, 0);
	fontDrawStr(
		diagnosticsGetFont(), s_pBuffer->pBack, 4, uwY + 1,
		szText, ubTextColor, FONT_LEFT | FONT_TOP, diagnosticsGetTextBitMap()
	);
}

static void drawHeader(UBYTE ubBpp) {
	char szTitle[64];
	char szChipset[64];
	char szFmode[32];
	UBYTE ubTextColor = s_isFallbackScreen ? 15 : (
		diagnosticsIsCurrentEhb() ? 31 : (1 << ubBpp) - 1
	);

	sprintf(szTitle, "DIAG: %s", diagnosticsGetCurrentName());
	drawHeaderLine(4, szTitle, ubTextColor);
	drawHeaderLine(13, "SPACE next  BACKSPACE prev  ESC quit", ubTextColor);
	if(s_isFallbackScreen) {
		drawHeaderLine(22, "AGA chipset not detected", ubTextColor);
		drawHeaderLine(31, "This is a fallback screen", ubTextColor);
		sprintf(
			szChipset, "GfxBase=%p ChipRevBits0=$%02X",
			GfxBase, GfxBase ? GfxBase->ChipRevBits0 : 0
		);
		drawHeaderLine(40, szChipset, ubTextColor);
		sprintf(szFmode, "Requested FMODE %u", diagnosticsGetCurrentFmode());
		drawHeaderLine(49, szFmode, ubTextColor);
	}
	else if(diagnosticsIsCurrentAga()) {
		sprintf(szFmode, "FMODE %u", diagnosticsGetCurrentFmode());
		drawHeaderLine(22, szFmode, ubTextColor);
	}
	else if(diagnosticsIsCurrentEhb()) {
		drawHeaderLine(22, "EHB: colors 32-63 are half-brite", ubTextColor);
	}
}

void diagSimpleBufferBppCreate(void) {
	UBYTE ubRequestedBpp = diagnosticsGetCurrentBpp();
	UBYTE isAgaMode = diagnosticsIsCurrentAga() && systemIsAga();
	UBYTE ubDisplayBpp = isAgaMode ? ubRequestedBpp : 4;

	if(!diagnosticsIsCurrentAga()) {
		ubDisplayBpp = ubRequestedBpp;
	}
	s_isFallbackScreen = diagnosticsIsCurrentAga() && !isAgaMode;

	if(isAgaMode) {
		s_pView = viewCreate(0,
			TAG_VIEW_GLOBAL_PALETTE, 1,
			TAG_VIEW_USES_AGA, 1,
		TAG_END);
		s_pVPort = vPortCreate(0,
			TAG_VPORT_VIEW, s_pView,
			TAG_VPORT_BPP, ubDisplayBpp,
			TAG_VPORT_USES_AGA, 1,
			TAG_VPORT_FMODE, diagnosticsGetCurrentFmode(),
		TAG_END);
	}
	else {
		s_pView = viewCreate(0,
			TAG_VIEW_GLOBAL_PALETTE, 1,
		TAG_END);
		s_pVPort = vPortCreate(0,
			TAG_VPORT_VIEW, s_pView,
			TAG_VPORT_BPP, ubDisplayBpp,
		TAG_END);
	}
	s_pBuffer = simpleBufferCreate(0,
		TAG_SIMPLEBUFFER_VPORT, s_pVPort,
		TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_CLEAR,
	TAG_END);

	setupPalette(ubDisplayBpp, isAgaMode);

	s_ulAutoAdvanceStart = timerGet();

	if(s_isFallbackScreen) {
		drawFallbackScreen();
		drawHeader(ubRequestedBpp);
	}
	else {
		drawPattern(ubDisplayBpp);
		drawPaletteSwatches(ubDisplayBpp);
		drawHeader(ubRequestedBpp);
	}

	viewLoad(s_pView);
}

void diagSimpleBufferBppLoop(void) {
	if(timerGetDelta(s_ulAutoAdvanceStart, timerGet()) >= systemGetVerticalBlankFrequency() * 2) {
		diagnosticsNextTest();
		return;
	}

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
	viewLoad(0);
	viewDestroy(s_pView);
}
