#include "diagnostics.h"
#include "tests/simple_buffer_bpp.h"

typedef struct tDiagnosticDef {
	const char *szName;
	UBYTE ubBpp;
} tDiagnosticDef;

static const tDiagnosticDef s_pDiagnostics[DIAG_TEST_COUNT] = {
	[DIAG_TEST_SIMPLE_BPP_2] = {"SimpleBuffer 2 BPP", 2},
	[DIAG_TEST_SIMPLE_BPP_3] = {"SimpleBuffer 3 BPP", 3},
	[DIAG_TEST_SIMPLE_BPP_4] = {"SimpleBuffer 4 BPP", 4},
	[DIAG_TEST_SIMPLE_BPP_5] = {"SimpleBuffer 5 BPP", 5},
};

tStateManager *g_pDiagStateManager = 0;
tState g_pDiagStates[DIAG_TEST_COUNT] = {
	[DIAG_TEST_SIMPLE_BPP_2] = {
		.cbCreate = diagSimpleBufferBppCreate,
		.cbLoop = diagSimpleBufferBppLoop,
		.cbDestroy = diagSimpleBufferBppDestroy,
	},
	[DIAG_TEST_SIMPLE_BPP_3] = {
		.cbCreate = diagSimpleBufferBppCreate,
		.cbLoop = diagSimpleBufferBppLoop,
		.cbDestroy = diagSimpleBufferBppDestroy,
	},
	[DIAG_TEST_SIMPLE_BPP_4] = {
		.cbCreate = diagSimpleBufferBppCreate,
		.cbLoop = diagSimpleBufferBppLoop,
		.cbDestroy = diagSimpleBufferBppDestroy,
	},
	[DIAG_TEST_SIMPLE_BPP_5] = {
		.cbCreate = diagSimpleBufferBppCreate,
		.cbLoop = diagSimpleBufferBppLoop,
		.cbDestroy = diagSimpleBufferBppDestroy,
	},
};

static UBYTE s_ubCurrentTest = DIAG_TEST_SIMPLE_BPP_2;

void diagnosticsCreate(void) {
	g_pDiagStateManager = stateManagerCreate();
}

void diagnosticsDestroy(void) {
	stateManagerDestroy(g_pDiagStateManager);
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

const char *diagnosticsGetCurrentName(void) {
	return s_pDiagnostics[s_ubCurrentTest].szName;
}
