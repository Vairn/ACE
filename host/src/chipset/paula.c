#include "chipset_priv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* Mix at the SDL device rate (set by paulaSetOutputRate). 28867 Hz was the
 * Paula DMA ceiling, not the host output rate — the ring underran every frame.
 * The ring is a producer(emulator)/consumer(SDL callback) SPSC queue. It must
 * be large enough to ride out the bursty per-frame production and the
 * clock drift between the emu's 50 Hz and the host device clock; on underrun
 * we hold the last sample instead of writing digital silence (clicks). */
#define RING 32768
#define VOL_SCALE 4 /* 8-bit * vol(0..64) * 4 → near 16-bit, two chans still sat */

#define TARGET_FILL (RING / 3)   /* steady-state slack we aim to preserve   */
#define UNDERRUN_SLACK 512       /* hold sample for this many frames before 0 */

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
	int idle;
} tChan;

static tChan s_ch[4];
static int16_t s_ringL[RING];
static int16_t s_ringR[RING];
static volatile unsigned s_rHead, s_rTail;
static int s_mixAcc;
static int s_outRate = 44100;
static int s_hostVol = 10;
static int s_lastL, s_lastR;      /* last produced sample */
static int s_holdL, s_holdR;       /* underrun hold (consumer side) */
static unsigned s_underrunFrames;  /* consecutive held frames for stats */

static unsigned fill(void) {
	unsigned h = s_rHead, t = s_rTail;
	return (unsigned)((h + RING - t) % RING);
}

static ULONG ptrOf(APTR a) {
	ULONG p = (ULONG)a;
	if(!p || !aceHostIsChipAddr(p)) {
		return 0;
	}
	return (ULONG)(uintptr_t)aceHostBusToPtr(p);
}

static UWORD readBe16(ULONG p) {
	const UBYTE *b;
	if(!p) {
		return 0;
	}
	b = (const UBYTE *)(uintptr_t)p;
	return (UWORD)((b[0] << 8) | b[1]);
}

static ULONG lenWords(UWORD uwLen) {
	return uwLen ? (ULONG)uwLen : 65536u;
}

void paulaInit(void) {
	memset(s_ch, 0, sizeof(s_ch));
	s_rHead = s_rTail = 0;
	s_mixAcc = 0;
}

void paulaShutdown(void) {
}

void paulaSetOutputRate(int hz) {
	if(hz > 0) {
		s_outRate = hz;
	}
}

void paulaSetHostVolume(int vol0to10) {
	if(vol0to10 < 0) {
		vol0to10 = 0;
	}
	if(vol0to10 > 10) {
		vol0to10 = 10;
	}
	s_hostVol = vol0to10;
}

void paulaOnDmaEnable(UWORD uwOld, UWORD uwNew) {
	int i;
	if(!(uwNew & DMAF_MASTER)) {
		uwNew &= (UWORD)~0x03FF;
	}
	if(!(uwOld & DMAF_MASTER)) {
		uwOld &= (UWORD)~0x03FF;
	}
	for(i = 0; i < 4; ++i) {
		UWORD bit = (UWORD)(DMAF_AUD0 << i);
		if((uwNew & bit) && !(uwOld & bit)) {
			s_ch[i].dmaDelay = 2;
			s_ch[i].start = ptrOf(g_pHostCustom->aud[i].ac_ptr);
			s_ch[i].ptr = s_ch[i].start;
			s_ch[i].len = lenWords(g_pHostCustom->aud[i].ac_len);
			s_ch[i].remain = s_ch[i].len;
			s_ch[i].per = g_pHostCustom->aud[i].ac_per;
			s_ch[i].vol = g_pHostCustom->aud[i].ac_vol;
			s_ch[i].periodPhase = 0;
			s_ch[i].sampIdx = 0;
			s_ch[i].hasCurrent = 0;
			s_ch[i].hasNext = 0;
			s_ch[i].needFetch = 1;
			s_ch[i].on = 1;
			s_ch[i].idle = 0;
		}
		if(!(uwNew & bit)) {
			s_ch[i].on = 0;
			s_ch[i].hasCurrent = 0;
			s_ch[i].hasNext = 0;
			s_ch[i].needFetch = 0;
			s_ch[i].idle = 0;
		}
	}
}

void paulaDmaSlot(int ch) {
	tChan *p = &s_ch[ch];
	UWORD word;
	p->per = g_pHostCustom->aud[ch].ac_per;
	p->vol = g_pHostCustom->aud[ch].ac_vol;
	if(!p->on) {
		return;
	}
	if(p->dmaDelay) {
		p->dmaDelay--;
		return;
	}
	if(!p->needFetch) {
		return;
	}
	if(p->remain == 0) {
		UWORD uwLen = g_pHostCustom->aud[ch].ac_len;
		p->start = ptrOf(g_pHostCustom->aud[ch].ac_ptr);
		p->len = lenWords(uwLen);
		p->ptr = p->start;
		p->remain = p->len;
		/* Sample DMA finished. A 1-word loop is Paula idle (two zeros);
		 * keep AUDx pending so ptplayer's poll path sees the channel done. */
		chipsetRaiseInt((UWORD)(INTF_AUD0 << ch));
		p->idle = (uwLen == 1);
	}
	else if(p->idle) {
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
	unsigned head = s_rHead;
	unsigned next = (head + 1u) % RING;
	s_lastL = l;
	s_lastR = r;
	if(next == s_rTail) {
		/* Full: the device is not keeping up. Drop the oldest sample so the
		 * ring can never livelock; the consumer will then see a gap. */
		s_rTail = (s_rTail + 1u) % RING;
	}
	s_ringL[head] = l;
	s_ringR[head] = r;
	s_rHead = next;
}

static int chanSample(int i) {
	int v, s;
	if(!s_ch[i].on || !s_ch[i].hasCurrent) {
		return 0;
	}
	v = s_ch[i].vol > 64 ? 64 : (int)s_ch[i].vol;
	s = s_ch[i].samp[s_ch[i].sampIdx & 1];
	return s * v * VOL_SCALE;
}

static int sat16(int v) {
	if(v > 32767) {
		return 32767;
	}
	if(v < -32768) {
		return -32768;
	}
	return v;
}

void paulaMix(short *pOut, int nFrames) {
	int f;
	int underrun = 0;
	for(f = 0; f < nFrames; ++f) {
		int l = s_holdL, r = s_holdR;
		unsigned tail = s_rTail;
		if(tail != s_rHead) {
			l = s_ringL[tail];
			r = s_ringR[tail];
			s_rTail = (tail + 1u) % RING;
			s_holdL = l;
			s_holdR = r;
		}
		else if(s_underrunFrames < UNDERRUN_SLACK) {
			/* Ring empty: hold the last sample, not digital silence. */
			++s_underrunFrames;
		}
		else {
			underrun = 1;
			break;
		}
		pOut[f * 2] = (short)((l * s_hostVol) / 10);
		pOut[f * 2 + 1] = (short)((r * s_hostVol) / 10);
	}
	if(underrun) {
		int i;
		fprintf(stderr, "[ACE_HOST] audio underrun at fill=%u\n", fill());
		for(i = f; i < nFrames; ++i) {
			pOut[i * 2] = (short)((s_holdL * s_hostVol) / 10);
			pOut[i * 2 + 1] = (short)((s_holdR * s_hostVol) / 10);
		}
		s_underrunFrames = 0;
	}
}

static void advanceChannel(tChan *p, ULONG paulaClock) {
	uint64_t threshold;
	if(!p->on || !p->per) {
		return;
	}
	threshold = (uint64_t)p->per * (uint64_t)s_outRate;
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
	if(lineRate <= 0 || s_outRate <= 0) {
		return;
	}
	s_mixAcc += s_outRate;
	n = s_mixAcc / lineRate;
	s_mixAcc %= lineRate;
	for(s = 0; s < n; ++s) {
		int l, r;
		for(i = 0; i < 4; ++i) {
			s_ch[i].per = g_pHostCustom->aud[i].ac_per;
			s_ch[i].vol = g_pHostCustom->aud[i].ac_vol;
		}
		l = chanSample(1) + chanSample(2);
		r = chanSample(0) + chanSample(3);
		for(i = 0; i < 4; ++i) {
			advanceChannel(&s_ch[i], paulaClock);
		}
		pushSample((int16_t)sat16(l), (int16_t)sat16(r));
	}
	/* Drift compensation: if the device has drained the ring below the
	 * steady-state target (device clock a hair faster than the emu's), top it
	 * back up so the underrun path is never exercised in steady state.
	 * Repeating the last produced sample is a one-sample ghost, inaudible. */
	while(fill() < TARGET_FILL) {
		pushSample((int16_t)sat16(s_lastL), (int16_t)sat16(s_lastR));
	}
}
