/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/utils/custom.h>
#include <stddef.h>

#ifdef ACE_HOST
#include <ace_host/chipset.h>
#endif

#ifdef ACE_HOST

tCustom FAR REGPTR g_pCustom;
tCopperUlong FAR REGPTR g_pBplFetch;
tCopperUlong FAR REGPTR g_pSprFetch;
tCopperUlong FAR REGPTR g_pCopLc;
tCopperUlong FAR REGPTR g_pCop2Lc;
tCia FAR REGPTR g_pCia[CIA_COUNT];

void aceHostBindCustom(void) {
	UBYTE *pBus = aceHostBusBase();
	g_pCustom = (tCustom REGPTR)(pBus + ACE_HOST_CUSTOM_OFFS);
	g_pBplFetch = (tCopperUlong REGPTR)((UBYTE *)g_pCustom + offsetof(tCustom, bplpt));
	g_pSprFetch = (tCopperUlong REGPTR)((UBYTE *)g_pCustom + offsetof(tCustom, sprpt));
	g_pCopLc = (tCopperUlong REGPTR)((UBYTE *)g_pCustom + offsetof(tCustom, cop1lc));
	g_pCop2Lc = (tCopperUlong REGPTR)((UBYTE *)g_pCustom + offsetof(tCustom, cop2lc));
	g_pCia[CIA_A] = (tCia *)(pBus + ACE_HOST_CIA_A_OFFS);
	g_pCia[CIA_B] = (tCia *)(pBus + ACE_HOST_CIA_B_OFFS);
}

UWORD ciaGetTimerA(tCia REGPTR pCia) {
	chipsetSyncCpuWrites();
	{
		UBYTE ubHi, ubLo;
		do {
			ubHi = pCia->tahi;
			ubLo = pCia->talo;
		} while(ubHi != pCia->tahi);
		return (ubHi << 8) | ubLo;
	}
}

void ciaSetTimerA(tCia REGPTR pCia, UWORD uwTicks) {
	pCia->talo = uwTicks & 0xFF;
	pCia->tahi = uwTicks >> 8;
}

UWORD ciaGetTimerB(tCia REGPTR pCia) {
	chipsetSyncCpuWrites();
	{
		UBYTE ubHi, ubLo;
		do {
			ubHi = pCia->tbhi;
			ubLo = pCia->tblo;
		} while(ubHi != pCia->tbhi);
		return (ubHi << 8) | ubLo;
	}
}

void ciaSetTimerB(tCia REGPTR pCia, UWORD uwTicks) {
	pCia->tblo = uwTicks & 0xFF;
	pCia->tbhi = uwTicks >> 8;
}

tRayPos getRayPos(void) {
	UWORD uwHiY, uwLoY, uwHiY2;
	chipsetOnVposRead();
	do {
		uwHiY = g_pCustom->vposr;
		uwLoY = g_pCustom->vhposr;
		uwHiY2 = g_pCustom->vposr;
	} while(uwHiY2 != uwHiY);

	{
		tRayPos sPos;
		sPos.ulValue = ((ULONG)uwHiY2 << 16) | uwLoY;
		return sPos;
	}
}

#elif defined(AMIGA)

#define CUSTOM_BASE 0xDFF000

tCustom FAR REGPTR g_pCustom = (tCustom REGPTR)CUSTOM_BASE;

tCopperUlong FAR REGPTR g_pBplFetch = (tCopperUlong REGPTR)(
	CUSTOM_BASE + offsetof(tCustom, bplpt)
);
tCopperUlong FAR REGPTR g_pSprFetch = (tCopperUlong REGPTR)(
	CUSTOM_BASE + offsetof(tCustom, sprpt)
);
tCopperUlong FAR REGPTR g_pCopLc = (tCopperUlong REGPTR)(
	CUSTOM_BASE + offsetof(tCustom, cop1lc)
);
tCopperUlong FAR REGPTR g_pCop2Lc = (tCopperUlong REGPTR)(
	CUSTOM_BASE + offsetof(tCustom, cop2lc)
);

tCia FAR REGPTR g_pCia[CIA_COUNT] = {(tCia*)0xBFE001, (tCia*)0xBFD000};

UWORD ciaGetTimerA(tCia REGPTR pCia) {
	UBYTE ubHi, ubLo;
	do {
		ubHi = pCia->tahi;
		ubLo = pCia->talo;
	} while(ubHi != pCia->tahi);
	return (ubHi << 8) | ubLo;
}

void ciaSetTimerA(tCia REGPTR pCia, UWORD uwTicks) {
	// The latches should be loaded, low byte first, as a write access
	// to the high register causes the timer to be stopped and reloaded with the
	// latch value unless the LOAD bit in the control register is set, in which case
	// the latch value is transferred to the timers regardless of the timer state
	pCia->talo = uwTicks & 0xFF;
	pCia->tahi = uwTicks >> 8;
}

UWORD ciaGetTimerB(tCia REGPTR pCia) {
	UBYTE ubHi, ubLo;
	do {
		ubHi = pCia->tbhi;
		ubLo = pCia->tblo;
	} while(ubHi != pCia->tbhi);
	return (ubHi << 8) | ubLo;
}

void ciaSetTimerB(tCia REGPTR pCia, UWORD uwTicks) {
	// The latches should be loaded, low byte first, as a write access
	// to the high register causes the timer to be stopped and reloaded with the
	// latch value unless the LOAD bit in the control register is set, in which case
	// the latch value is transferred to the timers regardless of the timer state
	pCia->tblo = uwTicks & 0xFF;
	pCia->tbhi = uwTicks >> 8;
}

tRayPos getRayPos(void) {
	// Even the 32-bit read via mov.l is split into 16-bit reads, so whe need
	// to read it as 2 16-bit vals and check for consistency.
	UWORD uwHiY, uwLoY, uwHiY2;
	do {
		uwHiY = g_pCustom->vposr;
		uwLoY = g_pCustom->vhposr;
		uwHiY2 = g_pCustom->vposr;
	} while(uwHiY2 != uwHiY);

	tRayPos sPos = {.ulValue = (uwHiY2 << 16) | uwLoY};
	return sPos;
}

#endif // AMIGA
