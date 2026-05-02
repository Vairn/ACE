#ifndef _DIAGNOSTICS_H_
#define _DIAGNOSTICS_H_

#include <ace/types.h>
#include <ace/managers/state.h>
#include <ace/utils/font.h>

extern tStateManager *g_pDiagStateManager;

void diagnosticsCreate(void);
void diagnosticsDestroy(void);

void diagnosticsStart(void);
void diagnosticsChangeTo(UBYTE ubTestIndex);
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
