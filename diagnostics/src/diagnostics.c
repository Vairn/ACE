#include "diagnostics.h"
#include "tests/simple_buffer_bpp.h"

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

#define DIAG_TEST_COUNT (sizeof(s_pDiagnostics) / sizeof(s_pDiagnostics[0]))

static void diagnosticsRunnerCreate(void);
static void diagnosticsRunnerLoop(void);
static void diagnosticsRunnerDestroy(void);

static tState s_sDiagnosticsRunnerState = {
	.cbCreate = diagnosticsRunnerCreate,
	.cbLoop = diagnosticsRunnerLoop,
	.cbDestroy = diagnosticsRunnerDestroy,
};

static void diagnosticsCreateCurrentTest(void) {
	s_pDiagnostics[s_ubCurrentTest].cbCreate();
	s_isCurrentTestCreated = 1;
}

static void diagnosticsDestroyCurrentTest(void) {
	if(s_isCurrentTestCreated) {
		s_pDiagnostics[s_ubCurrentTest].cbDestroy();
		s_isCurrentTestCreated = 0;
	}
}

static void diagnosticsRunnerCreate(void) {
	diagnosticsCreateCurrentTest();
}

static void diagnosticsRunnerLoop(void) {
	s_pDiagnostics[s_ubCurrentTest].cbLoop();
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
