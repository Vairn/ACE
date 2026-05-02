#ifndef _DIAGNOSTICS_H_
#define _DIAGNOSTICS_H_

#include <ace/types.h>
#include <ace/managers/state.h>

typedef enum tDiagnosticTest {
	DIAG_TEST_SIMPLE_BPP_2,
	DIAG_TEST_SIMPLE_BPP_3,
	DIAG_TEST_SIMPLE_BPP_4,
	DIAG_TEST_SIMPLE_BPP_5,
	DIAG_TEST_SIMPLE_BPP_5_EHB,
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
UBYTE diagnosticsIsCurrentEhb(void);
const char *diagnosticsGetCurrentName(void);

#endif // _DIAGNOSTICS_H_
