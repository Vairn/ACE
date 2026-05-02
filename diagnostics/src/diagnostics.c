#include "diagnostics.h"
#include "tests/simple_buffer_bpp.h"

typedef struct tDiagnosticDef {
	const char *szName;
	UBYTE ubBpp;
	UBYTE ubFmode;
	UBYTE isEhb;
	UBYTE isAga;
} tDiagnosticDef;

static const tDiagnosticDef s_pDiagnostics[DIAG_TEST_COUNT] = {
	[DIAG_TEST_SIMPLE_BPP_2] = {"SimpleBuffer 2 BPP", 2, 0, 0, 0},
	[DIAG_TEST_SIMPLE_BPP_3] = {"SimpleBuffer 3 BPP", 3, 0, 0, 0},
	[DIAG_TEST_SIMPLE_BPP_4] = {"SimpleBuffer 4 BPP", 4, 0, 0, 0},
	[DIAG_TEST_SIMPLE_BPP_5] = {"SimpleBuffer 5 BPP", 5, 0, 0, 0},
	[DIAG_TEST_SIMPLE_BPP_5_EHB] = {"SimpleBuffer 5 BPP EHB", 6, 0, 1, 0},
#ifdef ACE_USE_AGA_FEATURES
	[DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_0] = {"SimpleBuffer AGA 6 BPP FMODE 0", 6, 0, 0, 1},
	[DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_1] = {"SimpleBuffer AGA 6 BPP FMODE 1", 6, 1, 0, 1},
	[DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_2] = {"SimpleBuffer AGA 6 BPP FMODE 2", 6, 2, 0, 1},
	[DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_3] = {"SimpleBuffer AGA 6 BPP FMODE 3", 6, 3, 0, 1},
	[DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_0] = {"SimpleBuffer AGA 7 BPP FMODE 0", 7, 0, 0, 1},
	[DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_1] = {"SimpleBuffer AGA 7 BPP FMODE 1", 7, 1, 0, 1},
	[DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_2] = {"SimpleBuffer AGA 7 BPP FMODE 2", 7, 2, 0, 1},
	[DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_3] = {"SimpleBuffer AGA 7 BPP FMODE 3", 7, 3, 0, 1},
	[DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_0] = {"SimpleBuffer AGA 8 BPP FMODE 0", 8, 0, 0, 1},
	[DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_1] = {"SimpleBuffer AGA 8 BPP FMODE 1", 8, 1, 0, 1},
	[DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_2] = {"SimpleBuffer AGA 8 BPP FMODE 2", 8, 2, 0, 1},
	[DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_3] = {"SimpleBuffer AGA 8 BPP FMODE 3", 8, 3, 0, 1},
#endif
};

#define DIAG_SIMPLE_STATE { \
	.cbCreate = diagSimpleBufferBppCreate, \
	.cbLoop = diagSimpleBufferBppLoop, \
	.cbDestroy = diagSimpleBufferBppDestroy, \
}

tStateManager *g_pDiagStateManager = 0;
tState g_pDiagStates[DIAG_TEST_COUNT] = {
	[DIAG_TEST_SIMPLE_BPP_2] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_BPP_3] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_BPP_4] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_BPP_5] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_BPP_5_EHB] = DIAG_SIMPLE_STATE,
#ifdef ACE_USE_AGA_FEATURES
	[DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_0] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_1] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_2] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_3] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_0] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_1] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_2] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_3] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_0] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_1] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_2] = DIAG_SIMPLE_STATE,
	[DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_3] = DIAG_SIMPLE_STATE,
#endif
};

static UBYTE s_ubCurrentTest = DIAG_TEST_SIMPLE_BPP_2;
static tFont *s_pFont;
static tTextBitMap *s_pTextBitMap;

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

void diagnosticsChangeTo(UBYTE ubTest) {
	s_ubCurrentTest = ubTest % DIAG_TEST_COUNT;
	stateChange(g_pDiagStateManager, &g_pDiagStates[s_ubCurrentTest]);
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
