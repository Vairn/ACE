#include "diagnostics.h"
#include "tests/simple_buffer_bpp.h"
#include "tests/tile_scroller.h"
#include <ace/managers/blit.h>
#include <ace/managers/game.h>
#include <ace/managers/key.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <stdio.h>

typedef struct tDiagnosticDef {
	const char *szName;
	UBYTE ubBpp;
	UBYTE ubFmode;
	UBYTE isEhb;
	UBYTE isAga;
	tStateCb cbCreate;
	tStateCb cbLoop;
	tStateCb cbDestroy;
} tDiagnosticDef;

#define DIAG_SIMPLE_BUFFER(szName, ubBpp, ubFmode, isEhb, isAga) { \
	szName, ubBpp, ubFmode, isEhb, isAga, \
	diagSimpleBufferBppCreate, diagSimpleBufferBppLoop, diagSimpleBufferBppDestroy \
}

static const tDiagnosticDef s_pDiagnostics[] = {
	DIAG_SIMPLE_BUFFER("SimpleBuffer 2 BPP", 2, 0, 0, 0),
	DIAG_SIMPLE_BUFFER("SimpleBuffer 3 BPP", 3, 0, 0, 0),
	DIAG_SIMPLE_BUFFER("SimpleBuffer 4 BPP", 4, 0, 0, 0),
	DIAG_SIMPLE_BUFFER("SimpleBuffer 5 BPP", 5, 0, 0, 0),
	DIAG_SIMPLE_BUFFER("SimpleBuffer 5 BPP EHB", 6, 0, 1, 0),
#ifdef ACE_USE_AGA_FEATURES
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 6 BPP FMODE 0", 6, 0, 0, 1),
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 6 BPP FMODE 1", 6, 1, 0, 1),
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 6 BPP FMODE 2", 6, 2, 0, 1),
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 6 BPP FMODE 3", 6, 3, 0, 1),
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 7 BPP FMODE 0", 7, 0, 0, 1),
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 7 BPP FMODE 1", 7, 1, 0, 1),
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 7 BPP FMODE 2", 7, 2, 0, 1),
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 7 BPP FMODE 3", 7, 3, 0, 1),
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 8 BPP FMODE 0", 8, 0, 0, 1),
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 8 BPP FMODE 1", 8, 1, 0, 1),
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 8 BPP FMODE 2", 8, 2, 0, 1),
	DIAG_SIMPLE_BUFFER("SimpleBuffer AGA 8 BPP FMODE 3", 8, 3, 0, 1),
#endif
};

tStateManager *g_pDiagStateManager = 0;
static UBYTE s_ubCurrentTest = 0;
static tFont *s_pFont;
static tTextBitMap *s_pTextBitMap;
static UBYTE s_isCurrentTestCreated = 0;
static tView *s_pMenuView;
static tVPort *s_pMenuVPort;
static tSimpleBufferManager *s_pMenuBuffer;

typedef enum tDiagnosticsMode {
	DIAGNOSTICS_MODE_MENU,
	DIAGNOSTICS_MODE_SIMPLE_BUFFER,
	DIAGNOSTICS_MODE_TILE_SCROLLER,
} tDiagnosticsMode;

static tDiagnosticsMode s_eMode = DIAGNOSTICS_MODE_MENU;

#define DIAG_TEST_COUNT (sizeof(s_pDiagnostics) / sizeof(s_pDiagnostics[0]))

static void diagnosticsRunnerCreate(void);
static void diagnosticsRunnerLoop(void);
static void diagnosticsRunnerDestroy(void);

static tState s_sDiagnosticsRunnerState = {
	.cbCreate = diagnosticsRunnerCreate,
	.cbLoop = diagnosticsRunnerLoop,
	.cbDestroy = diagnosticsRunnerDestroy,
};

static void diagnosticsMenuCreate(void) {
	s_pMenuView = viewCreate(0,
		TAG_VIEW_GLOBAL_PALETTE, 1,
	TAG_END);
	s_pMenuVPort = vPortCreate(0,
		TAG_VPORT_VIEW, s_pMenuView,
		TAG_VPORT_BPP, 4,
	TAG_END);
	s_pMenuBuffer = simpleBufferCreate(0,
		TAG_SIMPLEBUFFER_VPORT, s_pMenuVPort,
		TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_CLEAR,
	TAG_END);

	s_pMenuVPort->pPalette[0] = 0x000;
	s_pMenuVPort->pPalette[1] = 0xFFF;
	s_pMenuVPort->pPalette[2] = 0x08F;
	s_pMenuVPort->pPalette[3] = 0x0F8;

	blitRect(
		s_pMenuBuffer->pBack, 0, 0,
		s_pMenuBuffer->uBfrBounds.uwX, s_pMenuBuffer->uBfrBounds.uwY, 0
	);
	fontDrawStr(
		s_pFont, s_pMenuBuffer->pBack, 16, 24,
		"ACE DIAGNOSTICS", 1, FONT_LEFT | FONT_TOP, s_pTextBitMap
	);
	fontDrawStr(
		s_pFont, s_pMenuBuffer->pBack, 16, 48,
		"1 SIMPLE BUFFER", 2, FONT_LEFT | FONT_TOP, s_pTextBitMap
	);
	fontDrawStr(
		s_pFont, s_pMenuBuffer->pBack, 16, 64,
		"2 TILEBUFFER SCROLLER", 3, FONT_LEFT | FONT_TOP, s_pTextBitMap
	);
	fontDrawStr(
		s_pFont, s_pMenuBuffer->pBack, 16, 88,
		"ESC QUIT", 1, FONT_LEFT | FONT_TOP, s_pTextBitMap
	);

	viewLoad(s_pMenuView);
}

static void diagnosticsMenuDestroy(void) {
	viewLoad(0);
	viewDestroy(s_pMenuView);
}

static void diagnosticsMenuLoop(void) {
	if(keyUse(KEY_ESCAPE)) {
		gameExit();
		return;
	}
	if(keyUse(KEY_1)) {
		diagnosticsShowSimpleBuffer();
		return;
	}
	if(keyUse(KEY_2)) {
		diagnosticsShowTileScroller();
		return;
	}

	vPortWaitForEnd(s_pMenuVPort);
}

static void diagnosticsCreateCurrentTest(void) {
	switch(s_eMode) {
		case DIAGNOSTICS_MODE_MENU:
			diagnosticsMenuCreate();
			break;
		case DIAGNOSTICS_MODE_SIMPLE_BUFFER:
			s_pDiagnostics[s_ubCurrentTest].cbCreate();
			break;
		case DIAGNOSTICS_MODE_TILE_SCROLLER:
			diagTileScrollerCreate();
			break;
	}
	s_isCurrentTestCreated = 1;
}

static void diagnosticsDestroyCurrentTest(void) {
	if(s_isCurrentTestCreated) {
		switch(s_eMode) {
			case DIAGNOSTICS_MODE_MENU:
				diagnosticsMenuDestroy();
				break;
			case DIAGNOSTICS_MODE_SIMPLE_BUFFER:
				s_pDiagnostics[s_ubCurrentTest].cbDestroy();
				break;
			case DIAGNOSTICS_MODE_TILE_SCROLLER:
				diagTileScrollerDestroy();
				break;
		}
		s_isCurrentTestCreated = 0;
	}
}

static void diagnosticsRunnerCreate(void) {
	diagnosticsCreateCurrentTest();
}

static void diagnosticsRunnerLoop(void) {
	switch(s_eMode) {
		case DIAGNOSTICS_MODE_MENU:
			diagnosticsMenuLoop();
			break;
		case DIAGNOSTICS_MODE_SIMPLE_BUFFER:
			s_pDiagnostics[s_ubCurrentTest].cbLoop();
			break;
		case DIAGNOSTICS_MODE_TILE_SCROLLER:
			diagTileScrollerLoop();
			break;
	}
}

static void diagnosticsRunnerDestroy(void) {
	diagnosticsDestroyCurrentTest();
}

void diagnosticsCreate(void) {
	s_pFont = fontCreateFromPath("data/fonts/quaver.fnt");
	s_pTextBitMap = fontCreateTextBitMap(336, s_pFont->uwHeight);
	g_pDiagStateManager = stateManagerCreate();
}

void diagnosticsDestroy(void) {
	stateManagerDestroy(g_pDiagStateManager);
	fontDestroyTextBitMap(s_pTextBitMap);
	fontDestroy(s_pFont);
}

void diagnosticsStart(void) {
	stateChange(g_pDiagStateManager, &s_sDiagnosticsRunnerState);
}

void diagnosticsChangeTo(UBYTE ubTestIndex) {
	diagnosticsDestroyCurrentTest();
	s_ubCurrentTest = ubTestIndex % DIAG_TEST_COUNT;
	diagnosticsCreateCurrentTest();
}

void diagnosticsShowMenu(void) {
	diagnosticsDestroyCurrentTest();
	s_eMode = DIAGNOSTICS_MODE_MENU;
	diagnosticsCreateCurrentTest();
}

void diagnosticsShowSimpleBuffer(void) {
	diagnosticsDestroyCurrentTest();
	s_eMode = DIAGNOSTICS_MODE_SIMPLE_BUFFER;
	diagnosticsCreateCurrentTest();
}

void diagnosticsShowTileScroller(void) {
	diagnosticsDestroyCurrentTest();
	s_eMode = DIAGNOSTICS_MODE_TILE_SCROLLER;
	diagnosticsCreateCurrentTest();
}

void diagnosticsNextTest(void) {
	diagnosticsChangeTo((s_ubCurrentTest + 1) % DIAG_TEST_COUNT);
}

void diagnosticsPrevTest(void) {
	if(s_ubCurrentTest == 0) {
		diagnosticsChangeTo(DIAG_TEST_COUNT - 1);
	}
	else {
		diagnosticsChangeTo(s_ubCurrentTest - 1);
	}
}

UBYTE diagnosticsGetCurrentBpp(void) {
	return s_pDiagnostics[s_ubCurrentTest].ubBpp;
}

UBYTE diagnosticsIsCurrentEhb(void) {
	return s_pDiagnostics[s_ubCurrentTest].isEhb;
}

#ifdef ACE_USE_AGA_FEATURES
UBYTE diagnosticsGetCurrentFmode(void) {
	return s_pDiagnostics[s_ubCurrentTest].ubFmode;
}

UBYTE diagnosticsIsCurrentAga(void) {
	return s_pDiagnostics[s_ubCurrentTest].isAga;
}
#endif

const char *diagnosticsGetCurrentName(void) {
	return s_pDiagnostics[s_ubCurrentTest].szName;
}

tFont *diagnosticsGetFont(void) {
	return s_pFont;
}

tTextBitMap *diagnosticsGetTextBitMap(void) {
	return s_pTextBitMap;
}
