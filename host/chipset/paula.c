#include "chipset_priv.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define PAULA_RATE 28867 /* PAL Paula mix rate ~ CCK/123 */
#define RING 8192

typedef struct tChan {
	ULONG ptr;
	ULONG start;
	ULONG len;
	UWORD per;
	UWORD vol;
	ULONG remain;
	uint64_t periodPhase;
	int dmaDelay;
	int16_t samp[2];
	int16_t next[2];
	int sampIdx;
	int hasCurrent;
	int hasNext;
	int needFetch;
	int on;
} tChan;

static tChan s_ch[4];
static int16_t s_ringL[RING];
static int16_t s_ringR[RING];
static unsigned s_rHead, s_rTail;
static int s_mixAcc;

static ULONG ptrOf(APTR a) {
	return (ULONG)a ? (ULONG)(uintptr_t)aceHostBusToPtr((ULONG)a) : 0;
}

static UWORD readBe16(ULONG p) {
	const UBYTE *b;
	if(!p) {
		return 0;
	}
	b = (const UBYTE *)(uintptr_t)p;
	return (UWORD)((b[0] << 8) | b[1]);
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
			s_ch[i].dmaDelay = 1;
			s_ch[i].start = ptrOf(g_pHostCustom->aud[i].ac_ptr);
			s_ch[i].ptr = s_ch[i].start;
			s_ch[i].len = g_pHostCustom->aud[i].ac_len ?
				g_pHostCustom->aud[i].ac_len : 65536u;
			s_ch[i].remain = s_ch[i].len;
			s_ch[i].per = g_pHostCustom->aud[i].ac_per;
			s_ch[i].vol = g_pHostCustom->aud[i].ac_vol;
			s_ch[i].periodPhase = 0;
			s_ch[i].sampIdx = 0;
			s_ch[i].hasCurrent = 0;
			s_ch[i].hasNext = 0;
			s_ch[i].needFetch = 1;
			s_ch[i].on = 1;
		}
		if(!(uwNew & bit)) {
			s_ch[i].on = 0;
		}
	}
}

void paulaDmaSlot(int ch) {
	tChan *p = &s_ch[ch];
	UWORD word;
	if(!p->on) {
		return;
	}
	p->per = g_pHostCustom->aud[ch].ac_per;
	p->vol = g_pHostCustom->aud[ch].ac_vol;
	if(p->dmaDelay) {
		p->dmaDelay = 0;
		return;
	}
	if(!p->needFetch) {
		return;
	}
	if(p->remain == 0) {
		p->start = ptrOf(g_pHostCustom->aud[ch].ac_ptr);
		p->len = g_pHostCustom->aud[ch].ac_len ?
			g_pHostCustom->aud[ch].ac_len : 65536u;
		p->ptr = p->start;
		p->remain = p->len;
		/* Sample DMA finished — ptplayer isChannelDone() polls this bit. */
		chipsetRaiseInt((UWORD)(INTF_AUD0 << ch));
	}
	if(!p->remain) {
		return;
	}
	word = readBe16(p->ptr);
	if(!p->hasCurrent) {
		p->samp[0] = (int16_t)(int8_t)(word >> 8);
		p->samp[1] = (int16_t)(int8_t)(word & 0xFF);
		p->sampIdx = 0;
		p->hasCurrent = 1;
		/* Paula has a second word buffer; request it at the next DMA slot. */
		p->needFetch = 1;
	}
	else {
		p->next[0] = (int16_t)(int8_t)(word >> 8);
		p->next[1] = (int16_t)(int8_t)(word & 0xFF);
		p->hasNext = 1;
		p->needFetch = 0;
	}
	p->ptr += 2;
	--p->remain;
}

static void pushSample(int16_t l, int16_t r) {
	unsigned next = (s_rHead + 1) % RING;
	if(next == s_rTail) {
		return;
	}
	s_ringL[s_rHead] = l;
	s_ringR[s_rHead] = r;
	s_rHead = next;
}

static int chanSample(int i) {
	int v = s_ch[i].vol > 64 ? 64 : (int)s_ch[i].vol;
	if(!s_ch[i].hasCurrent) {
		return 0;
	}
	int s = s_ch[i].samp[s_ch[i].sampIdx & 1];
	return s * v;
}

void paulaMix(short *pOut, int nFrames) {
	int f;
	for(f = 0; f < nFrames; ++f) {
		int l = 0, r = 0;
		if(s_rTail != s_rHead) {
			l = s_ringL[s_rTail];
			r = s_ringR[s_rTail];
			s_rTail = (s_rTail + 1) % RING;
		}
		else {
			/* 1+2 left, 0+3 right */
			l = chanSample(1) + chanSample(2);
			r = chanSample(0) + chanSample(3);
		}
		if(l > 32767) {
			l = 32767;
		}
		if(l < -32768) {
			l = -32768;
		}
		if(r > 32767) {
			r = 32767;
		}
		if(r < -32768) {
			r = -32768;
		}
		pOut[f * 2] = (short)l;
		pOut[f * 2 + 1] = (short)r;
	}
}

static void advanceChannel(tChan *p, ULONG paulaClock) {
	uint64_t threshold;
	if(!p->on || !p->per) {
		return;
	}
	threshold = (uint64_t)p->per * PAULA_RATE;
	p->periodPhase += paulaClock;
	while(p->periodPhase >= threshold) {
		p->periodPhase -= threshold;
		if(!p->hasCurrent) {
			continue;
		}
		if(p->sampIdx == 0) {
			p->sampIdx = 1;
		}
		else if(p->hasNext) {
			p->samp[0] = p->next[0];
			p->samp[1] = p->next[1];
			p->sampIdx = 0;
			p->hasNext = 0;
			p->needFetch = 1;
		}
		else {
			p->sampIdx = 0;
			p->hasCurrent = 0;
			p->needFetch = 1;
		}
	}
}

void paulaLineTick(int lineRate, ULONG paulaClock) {
	int s, n;
	int i;
	if(lineRate <= 0) {
		return;
	}
	s_mixAcc += PAULA_RATE;
	n = s_mixAcc / lineRate;
	s_mixAcc %= lineRate;
	for(s = 0; s < n; ++s) {
		int l = chanSample(1) + chanSample(2);
		int r = chanSample(0) + chanSample(3);
		for(i = 0; i < 4; ++i) {
			advanceChannel(&s_ch[i], paulaClock);
		}
		if(l > 32767) {
			l = 32767;
		}
		if(l < -32768) {
			l = -32768;
		}
		if(r > 32767) {
			r = 32767;
		}
		if(r < -32768) {
			r = -32768;
		}
		pushSample((int16_t)l, (int16_t)r);
	}
}
