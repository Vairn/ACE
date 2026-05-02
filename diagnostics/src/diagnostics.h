#ifndef _DIAGNOSTICS_H_
#define _DIAGNOSTICS_H_

#include <ace/types.h>
#include <ace/managers/state.h>
#include <ace/utils/font.h>

typedef enum tDiagnosticTest {
	DIAG_TEST_SIMPLE_BPP_2,
	DIAG_TEST_SIMPLE_BPP_3,
	DIAG_TEST_SIMPLE_BPP_4,
	DIAG_TEST_SIMPLE_BPP_5,
	DIAG_TEST_SIMPLE_BPP_5_EHB,
	DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_0,
	DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_1,
	DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_2,
	DIAG_TEST_SIMPLE_AGA_BPP_6_FMODE_3,
	DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_0,
	DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_1,
	DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_2,
	DIAG_TEST_SIMPLE_AGA_BPP_7_FMODE_3,
	DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_0,
	DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_1,
	DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_2,
	DIAG_TEST_SIMPLE_AGA_BPP_8_FMODE_3,
	DIAG_TEST_COUNT
} tDiagnosticTest;

extern tStateManager *g_pDiagStateManager;
extern tState g_pDiagStates[DIAG_TEST_COUNT];

void diagnosticsCreate(void);
void diagnosticsDestroy(void);

void diagnosticsChangeTo(UBYTE ubTest);
void diagnosticsNextTest(void);
void diagnosticsPrevTest(void);

UBYTE diagnosticsGetCurrentBpp(void);
UBYTE diagnosticsGetCurrentFmode(void);
UBYTE diagnosticsIsCurrentEhb(void);
UBYTE diagnosticsIsCurrentAga(void);
const char *diagnosticsGetCurrentName(void);
tFont *diagnosticsGetFont(void);
tTextBitMap *diagnosticsGetTextBitMap(void);

#endif // _DIAGNOSTICS_H_
