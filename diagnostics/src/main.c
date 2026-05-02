#include <ace/managers/game.h>
#include <ace/managers/key.h>

#include "diagnostics.h"

#define GENERIC_MAIN_LOOP_CONDITION gameIsRunning() && g_pDiagStateManager->pCurrent
#include <ace/generic/main.h>

void genericCreate(void) {
	keyCreate();
	diagnosticsCreate();
	diagnosticsChangeTo(DIAG_TEST_SIMPLE_BPP_2);
}

void genericProcess(void) {
	keyProcess();
	stateProcess(g_pDiagStateManager);
}

void genericDestroy(void) {
	diagnosticsDestroy();
	keyDestroy();
}
