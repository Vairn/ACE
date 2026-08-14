#include "chipset_priv.h"
#include <string.h>

static tCia *s_cia[2];
static UWORD s_latchA[2], s_latchB[2];
static UWORD s_timerA[2], s_timerB[2];
static UWORD s_dispA[2], s_dispB[2];
static UBYTE s_icrMask[2], s_icrData[2];

#define KBD_Q 32
static UBYTE s_kbdQ[KBD_Q];
static int s_kbdH, s_kbdT;
static int s_kbdBusy;

void ciaInit(tCia *pCiaA, tCia *pCiaB) {
	s_cia[0] = pCiaA;
	s_cia[1] = pCiaB;
	memset(pCiaA, 0, sizeof(tCia));
	memset(pCiaB, 0, sizeof(tCia));
	pCiaA->pra = 0xFF; /* fire buttons inactive (active low) */
	pCiaA->prb = 0xFF;
	pCiaB->pra = 0xFF;
	pCiaB->prb = 0xFF;
	s_latchA[0] = s_latchA[1] = 0xFFFF;
	s_latchB[0] = s_latchB[1] = 0xFFFF;
	s_timerA[0] = s_timerA[1] = 0xFFFF;
	s_timerB[0] = s_timerB[1] = 0xFFFF;
	s_dispA[0] = s_dispA[1] = 0xFFFF;
	s_dispB[0] = s_dispB[1] = 0xFFFF;
	pCiaA->talo = pCiaB->talo = 0xFF;
	pCiaA->tahi = pCiaB->tahi = 0xFF;
	pCiaA->tblo = pCiaB->tblo = 0xFF;
	pCiaA->tbhi = pCiaB->tbhi = 0xFF;
	s_kbdH = s_kbdT = 0;
	s_kbdBusy = 0;
}

static void underflowA(int i) {
	s_icrData[i] |= 0x01;
	if(s_cia[i]->cra & CIACRA_RUNMODE) {
		s_cia[i]->cra &= (UBYTE)~CIACRA_START;
	}
	else {
		s_timerA[i] = s_latchA[i];
	}
	aceHostFireCia((UBYTE)i, CIAICRB_TIMER_A);
}

static void underflowB(int i) {
	s_icrData[i] |= 0x02;
	if(s_cia[i]->crb & CIACRB_RUNMODE) {
		s_cia[i]->crb &= (UBYTE)~CIACRB_START;
	}
	else {
		s_timerB[i] = s_latchB[i];
	}
	aceHostFireCia((UBYTE)i, CIAICRB_TIMER_B);
}

/* One E-clock tick (CCK/5). Called every 5 DMA slots.
 * TALO/TAHI are both the latch (CPU writes) and the running count (CPU reads).
 * Capture a write when the visible value differs from what we last stored. */
void ciaRunEclockTick(void) {
	int i;
	for(i = 0; i < 2; ++i) {
		tCia *c = s_cia[i];
		UWORD liveA = (UWORD)((c->tahi << 8) | c->talo);
		UWORD liveB = (UWORD)((c->tbhi << 8) | c->tblo);
		if(liveA != s_dispA[i]) {
			s_latchA[i] = liveA;
		}
		if(liveB != s_dispB[i]) {
			s_latchB[i] = liveB;
		}
		if(c->cra & CIACRA_LOAD) {
			s_timerA[i] = s_latchA[i];
			c->cra &= (UBYTE)~CIACRA_LOAD;
		}
		if(c->crb & CIACRB_LOAD) {
			s_timerB[i] = s_latchB[i];
			c->crb &= (UBYTE)~CIACRB_LOAD;
		}
		if(c->cra & CIACRA_START) {
			if(s_timerA[i] > 0) {
				s_timerA[i]--;
			}
			if(s_timerA[i] == 0) {
				underflowA(i);
			}
		}
		if(c->crb & CIACRB_START) {
			if(s_timerB[i] > 0) {
				s_timerB[i]--;
			}
			if(s_timerB[i] == 0) {
				underflowB(i);
			}
		}
		s_dispA[i] = s_timerA[i];
		s_dispB[i] = s_timerB[i];
		c->talo = (UBYTE)(s_dispA[i] & 0xFF);
		c->tahi = (UBYTE)(s_dispA[i] >> 8);
		c->tblo = (UBYTE)(s_dispB[i] & 0xFF);
		c->tbhi = (UBYTE)(s_dispB[i] >> 8);
	}
}

void ciaSetPraFire(UBYTE ubKeepHigh, UBYTE ubSetLow) {
	UBYTE pra = s_cia[0]->pra;
	pra |= ubKeepHigh;
	pra &= (UBYTE)~ubSetLow;
	s_cia[0]->pra = pra;
}

void ciaWriteIcr(int cia, UBYTE val) {
	if(val & 0x80) {
		s_icrMask[cia] |= (UBYTE)(val & 0x1F);
	}
	else {
		s_icrMask[cia] &= (UBYTE)~(val & 0x1F);
	}
}

UBYTE ciaReadIcr(int cia) {
	UBYTE v = s_icrData[cia];
	s_icrData[cia] = 0;
	return v;
}

void ciaInjectKey(UBYTE ubRawKey) {
	int n = (s_kbdT + 1) % KBD_Q;
	if(n == s_kbdH) {
		return;
	}
	s_kbdQ[s_kbdT] = ubRawKey;
	s_kbdT = n;
}

void ciaPollKbd(void) {
	UBYTE raw, ser;
	if(!s_cia[0]) {
		return;
	}
	if(s_kbdBusy) {
		if(s_cia[0]->cra & CIACRA_SPMODE) {
			return;
		}
		s_kbdBusy = 0;
	}
	if(s_kbdH == s_kbdT) {
		return;
	}
	raw = s_kbdQ[s_kbdH];
	s_kbdH = (s_kbdH + 1) % KBD_Q;
	ser = (UBYTE)(((raw & 0x7F) << 1) | ((raw & 0x80) ? 1 : 0));
	s_cia[0]->sdr = (UBYTE)~ser;
	s_icrData[0] |= CIAICRF_SERIAL;
	s_kbdBusy = 1;
	aceHostFireCia(CIA_A, CIAICRB_SERIAL);
}
