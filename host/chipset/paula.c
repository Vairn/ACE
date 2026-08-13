#include "chipset_priv.h"
#include <string.h>
#include <stdlib.h>

#define PAULA_RATE 28867 /* PAL Paula mix rate ~ CCK/123 */
#define RING 8192

typedef struct tChan {
	ULONG ptr;
	ULONG start;
	UWORD len;
	UWORD per;
	UWORD vol;
	UWORD remain;
	int periodAcc;
	int dmaDelay;
	int16_t samp;
	int on;
} tChan;

static tChan s_ch[4];
static int16_t s_ring[RING];
static unsigned s_rHead, s_rTail;
static int s_mixAcc;

static ULONG ptrOf(APTR a) {
	return (ULONG)a ? (ULONG)(uintptr_t)aceHostBusToPtr((ULONG)a) : 0;
}

void paulaInit(void) {
	memset(s_ch, 0, sizeof(s_ch));
	s_rHead = s_rTail = 0;
	s_mixAcc = 0;
}

void paulaShutdown(void) {
}

void paulaOnDmaEnable(UWORD uwOld, UWORD uwNew) {
	int i;
	for(i = 0; i < 4; ++i) {
		UWORD bit = (UWORD)(DMAF_AUD0 << i);
		if((uwNew & bit) && !(uwOld & bit)) {
			/* ptplayer waits ~576 CIA ticks before enabling DMA. */
			s_ch[i].dmaDelay = 1;
			s_ch[i].start = ptrOf(g_pHostCustom->aud[i].ac_ptr);
			s_ch[i].ptr = s_ch[i].start;
			s_ch[i].len = g_pHostCustom->aud[i].ac_len;
			s_ch[i].remain = s_ch[i].len;
			s_ch[i].per = g_pHostCustom->aud[i].ac_per;
			s_ch[i].vol = g_pHostCustom->aud[i].ac_vol;
			s_ch[i].on = 1;
		}
		if(!(uwNew & bit)) {
			s_ch[i].on = 0;
		}
	}
}

void paulaDmaSlot(int ch) {
	tChan *p = &s_ch[ch];
	if(!p->on) {
		return;
	}
	p->per = g_pHostCustom->aud[ch].ac_per;
	p->vol = g_pHostCustom->aud[ch].ac_vol;
	if(p->dmaDelay) {
		p->dmaDelay = 0;
		return;
	}
	if(p->remain == 0) {
		p->start = ptrOf(g_pHostCustom->aud[ch].ac_ptr);
		p->len = g_pHostCustom->aud[ch].ac_len;
		p->ptr = p->start;
		p->remain = p->len;
		/* AUDx interrupt */
	}
	if(p->ptr) {
		const UBYTE *b = (const UBYTE *)(uintptr_t)p->ptr;
		p->samp = (int16_t)(int8_t)b[0];
		p->ptr++;
		if((p->remain) && (--p->remain == 0)) {
			/* loop: next DMA loc already programmed by ptplayer IRQ */
		}
	}
}

static void pushSample(int16_t s) {
	unsigned next = (s_rHead + 1) % RING;
	if(next == s_rTail) {
		return;
	}
	s_ring[s_rHead] = s;
	s_rHead = next;
}

void paulaMix(short *pOut, int nFrames) {
	int i, f;
	for(f = 0; f < nFrames; ++f) {
		int mix = 0;
		if(s_rTail != s_rHead) {
			mix = s_ring[s_rTail];
			s_rTail = (s_rTail + 1) % RING;
		}
		else {
			for(i = 0; i < 4; ++i) {
				int v = s_ch[i].vol > 64 ? 64 : (int)s_ch[i].vol;
				mix += s_ch[i].samp * v;
			}
			mix /= 4;
		}
		if(mix > 32767) {
			mix = 32767;
		}
		if(mix < -32768) {
			mix = -32768;
		}
		pOut[f * 2] = (short)mix;
		pOut[f * 2 + 1] = (short)mix;
	}
}

/* Called from chipset end-of-line to produce ~PAULA_RATE/50 samples. */
void paulaLineTick(void) {
	int i, n = PAULA_RATE / 50;
	int s;
	for(s = 0; s < n; ++s) {
		int mix = 0;
		for(i = 0; i < 4; ++i) {
			int v = s_ch[i].vol > 64 ? 64 : (int)s_ch[i].vol;
			mix += s_ch[i].samp * v;
			if(s_ch[i].on && s_ch[i].per) {
				s_ch[i].periodAcc++;
				if(s_ch[i].periodAcc >= s_ch[i].per) {
					s_ch[i].periodAcc = 0;
					/* sample already updated on DMA slot */
				}
			}
		}
		mix /= 2;
		if(mix > 32767) {
			mix = 32767;
		}
		if(mix < -32768) {
			mix = -32768;
		}
		pushSample((int16_t)mix);
	}
}
