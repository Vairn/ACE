#include "chipset_priv.h"
#include <ace/managers/copper.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct Custom *g_pHostCustom;
tCia *g_pHostCia[2];

static int s_isPal = 1;
static int s_lines = ACE_HOST_PAL_LINES;
static UWORD s_uwVpos;
static UWORD s_uwHpos;
static UWORD s_uwDmacon;
static UWORD s_uwIntena;
static UWORD s_uwIntreq;
static int s_inChipset;
static int s_frameReady;
static int s_vblankThisTick;
static int s_blitWait;
static int s_eClockAcc;
static int s_bplBusyRemain;
static int s_copSlotRemain;
static int s_timingLog;
static int s_timingOk = 1;
static unsigned s_frameCount;
static unsigned s_blitSlotsFrame;
static unsigned s_blitSlotsLast;
static UWORD s_lastCopWaitY;
static UWORD s_copWaitHits[16];
static unsigned s_copWaitHitN;
static UWORD s_colorHi[256];
static UWORD s_colorLo[256];
static UWORD s_colorSeen[32];

static ULONG s_ulCopPc;
static int s_copHalted;
static int s_copWaiting;
static int s_copSkip;
static UWORD s_copWaitIr1;
static UWORD s_copWaitIr2;

static UWORD s_fb[ACE_HOST_FB_WIDTH * ACE_HOST_FB_HEIGHT_PAL];
static UBYTE s_lineDma[ACE_HOST_SLOTS_PER_LINE];
static ULONG s_bplPtr[8];
static UWORD s_bplDat[8];
static int s_bplShift[8];
static int s_fetchRemain;
static int s_diwOn;

static ULONG s_sprPtr[8];
static int s_sprArmed[8];
static int s_sprActive[8];
static UWORD s_sprPos[8], s_sprCtl[8];
static UWORD s_sprDatA[8], s_sprDatB[8];
static int s_sprX[8], s_sprY0[8], s_sprY1[8], s_sprAttach[8];

static UWORD s_lastBltsize, s_lastCopjmp1, s_lastCopjmp2;
static UWORD s_lastDmaconW, s_lastIntenaW, s_lastIntreqW;

static int s_bplWordsThisLine;
static int s_dispX;
static int s_slotsThisLine;
static int s_linesThisFrame;
static int s_linesLastFrame;

static ULONG hwPtr(ULONG ulReg) {
	if(!ulReg) {
		return 0;
	}
	if(aceHostIsChipAddr(ulReg)) {
		if(ulReg < ACE_HOST_BUS_SIZE) {
			return (ULONG)(uintptr_t)aceHostBusToPtr(ulReg);
		}
		return ulReg;
	}
	if(ulReg && !aceHostIsChipAddr(ulReg)) {
		static int s_warned;
		if(!s_warned) {
			fprintf(stderr, "[ACE_HOST] DMA pointer %08lX is not CHIP — skipped\n",
				(unsigned long)ulReg);
			s_warned = 1;
		}
		return 0;
	}
	return ulReg;
}

static UWORD *chipWord(ULONG ulHost) {
	if(!ulHost) {
		return 0;
	}
	return (UWORD *)(uintptr_t)ulHost;
}

static UWORD readChipWord(ULONG ulHost) {
	UWORD *p = chipWord(ulHost);
	if(!p) {
		return 0;
	}
	/* CHIP bytes are Amiga big-endian. */
	{
		const UBYTE *b = (const UBYTE *)p;
		return (UWORD)((b[0] << 8) | b[1]);
	}
}

static void writeChipWord(ULONG ulHost, UWORD uw) {
	UWORD *p = chipWord(ulHost);
	if(!p) {
		return;
	}
	{
		UBYTE *b = (UBYTE *)p;
		b[0] = (UBYTE)(uw >> 8);
		b[1] = (UBYTE)(uw & 0xFF);
	}
}

static void updateVposRegs(void) {
	/* vhposr: V7-0 in high byte, H8-1 in low byte. vposr: V8 in bit 0. */
	g_pHostCustom->vhposr = (UWORD)(((s_uwVpos & 0xFF) << 8) | (s_uwHpos & 0xFF));
	g_pHostCustom->vposr = (UWORD)((s_uwVpos >> 8) & 1);
	g_pHostCustom->dmaconr = (UWORD)(
		(s_uwDmacon & 0x03FF) |
		(blitterBusy() ? DMAF_BLTDONE : 0)
	);
	g_pHostCustom->intenar = s_uwIntena;
	g_pHostCustom->intreqr = s_uwIntreq;
}

static void applySetClr(UWORD *pState, UWORD uwWrite) {
	if(uwWrite & 0x8000) {
		*pState |= (UWORD)(uwWrite & 0x7FFF);
	}
	else {
		*pState &= (UWORD)~(uwWrite & 0x7FFF);
	}
}

static void copperJump(ULONG ulLc) {
	s_ulCopPc = hwPtr(ulLc);
	s_copHalted = (s_ulCopPc == 0);
	s_copWaiting = 0;
	s_copSkip = 0;
}

static int copperReached(UWORD uwIr1, UWORD uwIr2) {
	/* OCS: VE is VP6–VP0 (IR2 bits 14–8). VP7 cannot be masked (HRM / WinUAE). */
	UWORD vmask = (UWORD)(((uwIr2 >> 8) & 0x7F) | 0x80);
	UWORD hmask = (UWORD)(uwIr2 & 0xFE);
	UWORD vp = (UWORD)(s_uwVpos & vmask);
	UWORD hp = (UWORD)(s_uwHpos & hmask);
	UWORD vcmp = (UWORD)((uwIr1 & (uwIr2 | 0x8000u)) >> 8);
	UWORD hcmp = (UWORD)(uwIr1 & hmask);
	if(!(uwIr2 & 0x8000) && blitterBusy()) {
		return 0;
	}
	if(vp < vcmp) {
		return 0;
	}
	if(vp > vcmp) {
		return 1;
	}
	return hp >= hcmp;
}

static int isTerminator(UWORD uwIr1, UWORD uwIr2) {
	return (uwIr1 & 1) && !(uwIr2 & 1) && uwIr1 == 0xFFFF;
}

static void customWriteUword(UWORD uwOffs, UWORD uwVal);

static void copperMove(UWORD uwDest, UWORD uwVal) {
	customWriteUword(uwDest, uwVal);
}

static int copperTick(void) {
	UWORD uwIr1, uwIr2;
	if(!(s_uwDmacon & DMAF_COPPER) || !(s_uwDmacon & DMAF_MASTER) || s_copHalted) {
		return 0;
	}
	if(!s_ulCopPc) {
		return 0;
	}
	if(s_copWaiting) {
		if(copperReached(s_copWaitIr1, s_copWaitIr2)) {
			s_copWaiting = 0;
			s_lastCopWaitY = s_uwVpos;
			if(s_copWaitHitN < 16) {
				s_copWaitHits[s_copWaitHitN++] = s_uwVpos;
			}
		}
		return 0;
	}

	{
		const UBYTE *p = (const UBYTE *)(uintptr_t)s_ulCopPc;
		if(!aceHostIsChipPtr(p)) {
			s_copHalted = 1;
			return 0;
		}
		uwIr1 = readChipWord(s_ulCopPc);
		uwIr2 = readChipWord(s_ulCopPc + 2);
	}
	s_ulCopPc += 4;

	if(uwIr1 & 1) {
		if(isTerminator(uwIr1, uwIr2)) {
			s_copHalted = 1;
			return 1;
		}
		if(uwIr2 & 1) {
			if(copperReached(uwIr1, uwIr2)) {
				s_copSkip = 1;
			}
			return 1;
		}
		s_copWaitIr1 = uwIr1;
		s_copWaitIr2 = uwIr2;
		if(!copperReached(s_copWaitIr1, s_copWaitIr2)) {
			s_copWaiting = 1;
		}
		else {
			s_lastCopWaitY = s_uwVpos;
			if(s_copWaitHitN < 16) {
				s_copWaitHits[s_copWaitHitN++] = s_uwVpos;
			}
		}
		return 1;
	}

	if(s_copSkip) {
		s_copSkip = 0;
		return 1;
	}
	copperMove((UWORD)(uwIr1 & 0x1FE), uwIr2);
	return 1;
}

static void startBlit(UWORD uwSize) {
	if(!(s_uwDmacon & DMAF_BLITTER) && !(s_uwDmacon & DMAF_MASTER)) {
		/* Still run; ACE often starts blit with blitter DMA already on. */
	}
	blitterStart(uwSize);
}

static int customPtrRegBase(UWORD uwOffs, UWORD *pBase) {
	static const UWORD kPtr[] = {
		offsetof(struct Custom, dskpt),
		offsetof(struct Custom, bltcpt),
		offsetof(struct Custom, bltbpt),
		offsetof(struct Custom, bltapt),
		offsetof(struct Custom, bltdpt),
		offsetof(struct Custom, cop1lc),
		offsetof(struct Custom, cop2lc),
		offsetof(struct Custom, aud[0].ac_ptr),
		offsetof(struct Custom, aud[1].ac_ptr),
		offsetof(struct Custom, aud[2].ac_ptr),
		offsetof(struct Custom, aud[3].ac_ptr),
		offsetof(struct Custom, bplpt[0]),
		offsetof(struct Custom, bplpt[1]),
		offsetof(struct Custom, bplpt[2]),
		offsetof(struct Custom, bplpt[3]),
		offsetof(struct Custom, bplpt[4]),
		offsetof(struct Custom, bplpt[5]),
		offsetof(struct Custom, bplpt[6]),
		offsetof(struct Custom, bplpt[7]),
		offsetof(struct Custom, sprpt[0]),
		offsetof(struct Custom, sprpt[1]),
		offsetof(struct Custom, sprpt[2]),
		offsetof(struct Custom, sprpt[3]),
		offsetof(struct Custom, sprpt[4]),
		offsetof(struct Custom, sprpt[5]),
		offsetof(struct Custom, sprpt[6]),
		offsetof(struct Custom, sprpt[7])
	};
	UWORD uwAligned = (UWORD)(uwOffs & (UWORD)~2u);
	unsigned i;
	for(i = 0; i < sizeof(kPtr) / sizeof(kPtr[0]); ++i) {
		if(uwAligned == kPtr[i]) {
			*pBase = uwAligned;
			return 1;
		}
	}
	return 0;
}

static void customWriteUword(UWORD uwOffs, UWORD uwVal) {
	UWORD uwBase;
	if(uwOffs + 1 >= sizeof(struct Custom)) {
		return;
	}
	if(customPtrRegBase(uwOffs, &uwBase)) {
		ULONG *pLong = (ULONG *)((UBYTE *)g_pHostCustom + uwBase);
		ULONG ul = *pLong;
		if((uwOffs & 2) == 0) {
			ul = (ul & 0x0000FFFFu) | ((ULONG)uwVal << 16);
		}
		else {
			ul = (ul & 0xFFFF0000u) | (ULONG)uwVal;
		}
		*pLong = ul;
	}
	else {
		*(UWORD *)((UBYTE *)g_pHostCustom + uwOffs) = uwVal;
	}

	if(uwOffs >= 0x180 && uwOffs <= 0x1BE && (uwOffs & 1) == 0) {
		int reg = (int)((uwOffs - 0x180) / 2);
		int bank = 0, loct = 0, idx;
#ifdef ACE_USE_AGA_FEATURES
		bank = (g_pHostCustom->bplcon3 >> 13) & 7;
		loct = (g_pHostCustom->bplcon3 & 0x0200) != 0;
#endif
		idx = bank * 32 + (reg & 31);
		s_colorSeen[reg & 31] = uwVal;
		if(loct) {
			s_colorLo[idx] = (UWORD)(uwVal & 0x0FFF);
		}
		else {
			s_colorHi[idx] = (UWORD)(uwVal & 0x0FFF);
#ifndef ACE_USE_AGA_FEATURES
			s_colorLo[idx] = 0;
#endif
		}
	}

	switch(uwOffs) {
		case 0x058: /* BLTSIZE — write strobes a blit even if the size is unchanged */
			startBlit(uwVal);
			s_lastBltsize = uwVal;
			g_pHostCustom->bltsize = 0;
			break;
		case 0x088: /* COPJMP1 */
			copperJump(g_pHostCustom->cop1lc);
			g_pHostCustom->copjmp1 = 0;
			s_lastCopjmp1 = 0;
			break;
		case 0x08A: /* COPJMP2 */
			copperJump(g_pHostCustom->cop2lc);
			g_pHostCustom->copjmp2 = 0;
			s_lastCopjmp2 = 0;
			break;
		case 0x096: /* DMACON */
			{
				UWORD uwOld = s_uwDmacon;
				applySetClr(&s_uwDmacon, uwVal);
				if((s_uwDmacon & DMAF_MASTER) == 0) {
					s_uwDmacon &= (UWORD)~0x03FF;
					s_uwDmacon |= (UWORD)(uwVal & 0x8000 ? 0 : 0);
					applySetClr(&s_uwDmacon, uwVal);
				}
				paulaOnDmaEnable(uwOld, s_uwDmacon);
				g_pHostCustom->dmacon = 0;
				s_lastDmaconW = 0;
			}
			break;
		case 0x09A: /* INTENA */
			applySetClr(&s_uwIntena, uwVal);
			g_pHostCustom->intena = 0;
			s_lastIntenaW = 0;
			break;
		case 0x09C: /* INTREQ */
			applySetClr(&s_uwIntreq, uwVal);
			g_pHostCustom->intreq = 0;
			s_lastIntreqW = 0;
			break;
		default:
			break;
	}
}

void chipsetSyncCpuWrites(void) {
	struct Custom *c = g_pHostCustom;
	if(c->bltsize) {
		startBlit(c->bltsize);
		s_lastBltsize = c->bltsize;
		c->bltsize = 0;
	}
	if(c->copjmp1) {
		copperJump(c->cop1lc);
		c->copjmp1 = 0;
		s_lastCopjmp1 = 0;
	}
	if(c->copjmp2) {
		copperJump(c->cop2lc);
		c->copjmp2 = 0;
		s_lastCopjmp2 = 0;
	}
	if(c->dmacon) {
		UWORD uwOld = s_uwDmacon;
		applySetClr(&s_uwDmacon, c->dmacon);
		paulaOnDmaEnable(uwOld, s_uwDmacon);
		c->dmacon = 0;
	}
	if(c->intena) {
		applySetClr(&s_uwIntena, c->intena);
		c->intena = 0;
	}
	if(c->intreq) {
		applySetClr(&s_uwIntreq, c->intreq);
		c->intreq = 0;
	}
	{
		int i;
		for(i = 0; i < 32; ++i) {
			UWORD uw = c->color[i];
			if(uw != s_colorSeen[i]) {
				int bank = 0, loct = 0, idx;
#ifdef ACE_USE_AGA_FEATURES
				bank = (c->bplcon3 >> 13) & 7;
				loct = (c->bplcon3 & 0x0200) != 0;
#endif
				idx = bank * 32 + i;
				s_colorSeen[i] = uw;
				if(loct) {
					s_colorLo[idx] = (UWORD)(uw & 0x0FFF);
				}
				else {
					s_colorHi[idx] = (UWORD)(uw & 0x0FFF);
#ifndef ACE_USE_AGA_FEATURES
					s_colorLo[idx] = 0;
#endif
				}
			}
		}
	}
	updateVposRegs();
}

static void latchBplPtrs(void) {
	int i;
	for(i = 0; i < 8; ++i) {
		s_bplPtr[i] = hwPtr(g_pHostCustom->bplpt[i]);
	}
}

static void latchSprPtrs(void) {
	int i;
	for(i = 0; i < 8; ++i) {
		s_sprPtr[i] = hwPtr(g_pHostCustom->sprpt[i]);
	}
}

static int bplDepth(void) {
	int d = (g_pHostCustom->bplcon0 >> 12) & 7;
	if(g_pHostCustom->bplcon0 & 0x0010) {
		d |= 8; /* BPU3 */
	}
#ifdef ACE_USE_AGA_FEATURES
	if(d > 8) {
		d = 8;
	}
#else
	if(d > 6) {
		d = 6;
	}
#endif
	return d;
}

static int isHires(void) {
	return (g_pHostCustom->bplcon0 & 0x8000) != 0;
}

/* fetchmode.h: FMODE 0 = 16px/1 word, 1–2 = 32px/2 words, 3 = 64px/4 words.
 * HIRES halves the CCK period (4 vs 8) but FMODE still sets fetch width. */
static int fmodeShift(void) {
#ifdef ACE_USE_AGA_FEATURES
	switch(g_pHostCustom->fmode & 3) {
		case 1:
		case 2:
			return 1;
		case 3:
			return 2;
		default:
			return 0;
	}
#else
	return 0;
#endif
}

static int bplFetchWords(void) {
	return 1 << fmodeShift();
}

static int bplFetchPeriod(void) {
	return (isHires() ? 4 : 8) << fmodeShift();
}

static UWORD ddfMask(void) {
	return (UWORD)~(bplFetchPeriod() - 1);
}

static int fetchPrefetchPixels(void) {
	/* One fetch in the Denise pipeline (fetchmode scroll prefetch is 2/4/8 bytes). */
	return bplFetchWords() * 16;
}

static int inDdf(UWORD h) {
	UWORD mask = ddfMask();
	UWORD strt = (UWORD)(g_pHostCustom->ddfstrt & mask);
	UWORD stop = (UWORD)(g_pHostCustom->ddfstop & mask);
	return h >= strt && h <= stop;
}

static void fetchBplWord(void) {
	int d = bplDepth();
	int i;
	for(i = 0; i < d; ++i) {
		s_bplDat[i] = readChipWord(s_bplPtr[i]);
		s_bplPtr[i] += 2;
		g_pHostCustom->bplpt[i] = (APTR)s_bplPtr[i];
	}
	s_bplWordsThisLine++;
}

static void endOfLineModulo(void) {
	int d = bplDepth();
	int i;
	WORD wOdd = (WORD)g_pHostCustom->bpl1mod;
	WORD wEven = (WORD)g_pHostCustom->bpl2mod;
	for(i = 0; i < d; ++i) {
		WORD wMod = (i & 1) ? wEven : wOdd;
		s_bplPtr[i] = (ULONG)((LONG)s_bplPtr[i] + wMod);
		g_pHostCustom->bplpt[i] = (APTR)s_bplPtr[i];
	}
}

static UWORD palToRgb(UWORD c) {
	unsigned r = (c >> 8) & 0xF;
	unsigned g = (c >> 4) & 0xF;
	unsigned b = c & 0xF;
	r |= (unsigned)(r << 4);
	g |= (unsigned)(g << 4);
	b |= (unsigned)(b << 4);
	return (UWORD)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static int diwH0(void) { return g_pHostCustom->diwstrt & 0xFF; }
static int diwH1(void) { return (g_pHostCustom->diwstop & 0xFF) | 0x100; }
static int diwV0(void) { return (g_pHostCustom->diwstrt >> 8) & 0xFF; }
static int diwV1(void) {
	/* OCS stores only 8 bits of VSTOP; V8 is the complement of V7.
	 * A PAL 256-line window is VSTART=44 / VSTOP=300 — both 0x2C in the
	 * low 8 bits — so "stop < start => +256" leaves an empty range. */
	int v = (g_pHostCustom->diwstop >> 8) & 0xFF;
	if(!(v & 0x80)) {
		v |= 0x100;
	}
	return v;
}

static void plot(int x, int y, UWORD rgb) {
	if((unsigned)x >= ACE_HOST_FB_WIDTH || (unsigned)y >= (unsigned)(s_isPal ? ACE_HOST_FB_HEIGHT_PAL : ACE_HOST_FB_HEIGHT_NTSC)) {
		return;
	}
	s_fb[y * ACE_HOST_FB_WIDTH + x] = rgb;
}

/* Line buffer: 8 planes × 64 words (640px hires + AGA prefetch). */
static UWORD s_bplLine[8][64];
static int s_bplFetchIdx;

static void fetchBplIntoLine(void) {
	int d = bplDepth();
	int words = bplFetchWords();
	int w, i;
	/* Live BPLPT each fetch — copper MOVEs after WAIT 43,220 finish
	 * on this line before DDFSTRT, so a start-of-line latch is too early. */
	for(w = 0; w < words; ++w) {
		if(s_bplFetchIdx >= 64) {
			return;
		}
		for(i = 0; i < d && i < 8; ++i) {
			ULONG ptr = hwPtr(g_pHostCustom->bplpt[i]);
			s_bplLine[i][s_bplFetchIdx] = readChipWord(ptr);
			s_bplPtr[i] = ptr ? ptr + 2 : 0;
			g_pHostCustom->bplpt[i] = (APTR)s_bplPtr[i];
		}
		s_bplFetchIdx++;
	}
	s_bplWordsThisLine = s_bplFetchIdx;
}

static int scrollDelay(int even) {
	UWORD con1 = g_pHostCustom->bplcon1;
	int delay;
	if(even) {
		delay = (con1 >> 4) & 0xF;
#ifdef ACE_USE_AGA_FEATURES
		if(con1 & 0x4000) {
			delay |= 0x10;
		}
		if(con1 & 0x8000) {
			delay |= 0x20;
		}
#endif
	}
	else {
		delay = con1 & 0xF;
#ifdef ACE_USE_AGA_FEATURES
		if(con1 & 0x0400) {
			delay |= 0x10;
		}
		if(con1 & 0x0800) {
			delay |= 0x20;
		}
#endif
	}
	if(isHires()) {
		delay *= 2; /* BPLCON1 unit is 2 hires pixels */
	}
	return delay;
}

static int pixelColor(int pix) {
	int d = bplDepth();
	int i, idx = 0;
	for(i = 0; i < d && i < 8; ++i) {
		int delay = scrollDelay(i & 1);
		int src = pix - delay;
		if(src >= 0) {
			int word = src >> 4;
			int bit = 15 - (src & 15);
			if(word < s_bplFetchIdx && (s_bplLine[i][word] & (1u << bit))) {
				idx |= 1 << i;
			}
		}
	}
#ifdef ACE_USE_AGA_FEATURES
	idx ^= (int)(g_pHostCustom->bplcon4 & 0x00FF);
#endif
	return idx;
}

static int isEhbMode(void) {
	if(bplDepth() != 6) {
		return 0;
	}
	if(g_pHostCustom->bplcon0 & 0x0800) {
		return 0; /* HAM */
	}
	if(g_pHostCustom->bplcon2 & 0x0200) {
		return 0; /* KILLEHB */
	}
	return 1;
}

static UWORD rgbFrom12(UWORD hi, UWORD lo) {
	unsigned r = ((hi >> 8) & 0xF) << 4;
	unsigned g = ((hi >> 4) & 0xF) << 4;
	unsigned b = (hi & 0xF) << 4;
	if(lo) {
		r |= (lo >> 8) & 0xF;
		g |= (lo >> 4) & 0xF;
		b |= lo & 0xF;
	}
	else {
		r |= r >> 4;
		g |= g >> 4;
		b |= b >> 4;
	}
	return (UWORD)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static UWORD colorLookup(int idx) {
	int ehb = 0;
	if(isEhbMode() && idx >= 32) {
		ehb = 1;
		idx -= 32;
	}
#ifdef ACE_USE_AGA_FEATURES
	idx &= 255;
#else
	idx &= 31;
#endif
	if(ehb) {
		return palToRgb((UWORD)((s_colorHi[idx] >> 1) & 0x0777));
	}
	return rgbFrom12(s_colorHi[idx], s_colorLo[idx]);
}

static void spriteFetchCtl(int ch) {
	/* ACE writes POS/CTL as native UWORDs; CHIP DMA pixels stay Amiga BE. */
	UWORD *p = (UWORD *)(uintptr_t)s_sprPtr[ch];
	UWORD pos = p ? p[0] : 0;
	UWORD ctl = p ? p[1] : 0;
	s_sprPtr[ch] += 4;
	g_pHostCustom->sprpt[ch] = (APTR)s_sprPtr[ch];
	s_sprPos[ch] = pos;
	s_sprCtl[ch] = ctl;
	s_sprY0[ch] = (pos >> 8) | ((ctl & 4) ? 0x100 : 0);
	s_sprY1[ch] = (ctl >> 8) | ((ctl & 2) ? 0x100 : 0);
	s_sprX[ch] = ((pos & 0xFF) << 1) | (ctl & 1);
	s_sprAttach[ch] = (ctl >> 7) & 1;
	s_sprArmed[ch] = 1;
	s_sprActive[ch] = 0;
}

static void spriteFetchData(int ch) {
	s_sprDatA[ch] = readChipWord(s_sprPtr[ch]);
	s_sprDatB[ch] = readChipWord(s_sprPtr[ch] + 2);
	s_sprPtr[ch] += 4;
	g_pHostCustom->sprpt[ch] = (APTR)s_sprPtr[ch];
}

static int spritePixel(int x, int *pColor, int *pPri) {
	int ch, bestPri = 99, found = 0, col = 0;
	for(ch = 0; ch < 8; ++ch) {
		int sx, bit, two;
		if(!s_sprActive[ch]) {
			continue;
		}
		if((ch & 1) && s_sprAttach[ch]) {
			continue;
		}
		sx = s_sprX[ch];
		if(x < sx || x >= sx + 16) {
			continue;
		}
		bit = 15 - (x - sx);
		two = 0;
		if(s_sprDatA[ch] & (1u << bit)) {
			two |= 1;
		}
		if(s_sprDatB[ch] & (1u << bit)) {
			two |= 2;
		}
		if((ch + 1) < 8 && s_sprAttach[ch + 1] && s_sprActive[ch + 1]) {
			int t2 = 0;
			if(s_sprDatA[ch + 1] & (1u << bit)) {
				t2 |= 1;
			}
			if(s_sprDatB[ch + 1] & (1u << bit)) {
				t2 |= 2;
			}
			two |= t2 << 2;
		}
		if(two) {
			int pri = ch >> 1;
			if(pri < bestPri) {
				bestPri = pri;
				col = two;
				found = ch + 1; /* 1-based channel */
			}
		}
	}
	if(found) {
		int ch = found - 1;
#ifdef ACE_USE_AGA_FEATURES
		int bank = (ch & 1)
			? ((g_pHostCustom->bplcon4 >> 8) & 0xF)
			: ((g_pHostCustom->bplcon4 >> 12) & 0xF);
		*pColor = bank * 16 + col;
#else
		*pColor = 16 + ((ch & ~1) << 1) + col;
#endif
		*pPri = bestPri;
		return 1;
	}
	return 0;
}

static void renderSlotPixels(void) {
	int yDisp = (int)s_uwVpos - diwV0();
	int h0 = diwH0();
	int hires = isHires();
	int pxPerSlot = hires ? 4 : 2;
	int pixBase = (int)s_uwHpos * pxPerSlot;
	int i;
	int fbH = s_isPal ? ACE_HOST_FB_HEIGHT_PAL : ACE_HOST_FB_HEIGHT_NTSC;
	int inV = (int)s_uwVpos >= diwV0() && (int)s_uwVpos < diwV1();
	int colorOn = (g_pHostCustom->bplcon0 & 0x0200) != 0;
	int ddfStrt = (int)(g_pHostCustom->ddfstrt & ddfMask());
	int prefetch = fetchPrefetchPixels();

	/* COLORON off: Denise blanks the playfield. Skip the plot loop so
	 * blit-wait slot simulation during startup is not a full rasterizer. */
	if(!colorOn || !inV || yDisp < 0 || yDisp >= fbH) {
		return;
	}
	for(i = 0; i < pxPerSlot; ++i) {
		int hxLores = (int)s_uwHpos * 2 + (hires ? (i >> 1) : i);
		int xDisp = hires ? (pixBase + i - h0 * 2) : (hxLores - h0);
		int idx, sprCol, sprPri;
		UWORD rgb;
		int maxX = hires ? ACE_HOST_FB_WIDTH : ACE_HOST_FB_LORES_WIDTH;
		if(xDisp < 0 || xDisp >= maxX) {
			continue;
		}
		if(hxLores < h0 || hxLores >= diwH1()) {
			rgb = colorLookup(0);
			if(hires) {
				plot(xDisp, yDisp, rgb);
			}
			else {
				plot(xDisp * 2, yDisp, rgb);
				plot(xDisp * 2 + 1, yDisp, rgb);
			}
			continue;
		}
		idx = 0;
		if(colorOn && (s_uwDmacon & DMAF_RASTER)) {
			int ddfPix = pixBase + i - ddfStrt * pxPerSlot - prefetch;
			if(ddfPix >= 0) {
				idx = pixelColor(ddfPix);
			}
		}
		rgb = colorLookup(idx);
		if((s_uwDmacon & DMAF_SPRITE) && spritePixel(hxLores, &sprCol, &sprPri)) {
			(void)sprPri;
			if(sprCol) {
				rgb = colorLookup(sprCol);
			}
		}
		if(hires) {
			plot(xDisp, yDisp, rgb);
		}
		else {
			plot(xDisp * 2, yDisp, rgb);
			plot(xDisp * 2 + 1, yDisp, rgb);
		}
	}
}

static int spriteSlotChannel(UWORD h) {
	/* OCS: sprite DMA at 0x15,0x17,...,0x23 (2 slots each for 8 sprites). */
	if(h < 0x15 || h > 0x24) {
		return -1;
	}
	return (int)((h - 0x15) / 2);
}

static void beginLine(void) {
	int ch;
	memset(s_lineDma, ACE_HOST_DMA_IDLE, sizeof(s_lineDma));
	s_bplFetchIdx = 0;
	s_bplWordsThisLine = 0;
	s_dispX = 0;
	s_bplBusyRemain = 0;
	s_copSlotRemain = 0;
	if(s_uwVpos == 0) {
		latchSprPtrs();
		copperJump(g_pHostCustom->cop1lc);
		s_uwIntreq |= INTF_VERTB;
		s_frameReady = 1;
		s_blitSlotsLast = s_blitSlotsFrame;
		s_blitSlotsFrame = 0;
		s_copWaitHitN = 0;
		s_frameCount++;
		if(s_timingLog && s_linesLastFrame && (s_frameCount % 50u) == 1u) {
			fprintf(stderr,
				"[ACE_HOST] timing frame %u lines=%d (expect %d) lastCopWaitY=%u blitSlots=%u ok=%d\n",
				s_frameCount, s_linesLastFrame, s_lines, s_lastCopWaitY,
				s_blitSlotsLast, s_timingOk);
		}
	}
	for(ch = 0; ch < 8; ++ch) {
		if(s_sprArmed[ch] && (int)s_uwVpos == s_sprY0[ch]) {
			s_sprActive[ch] = 1;
		}
		if(s_sprActive[ch] && (int)s_uwVpos >= s_sprY1[ch]) {
			s_sprActive[ch] = 0;
			s_sprArmed[ch] = 0;
		}
	}
}

static void endLine(void) {
	if((s_uwDmacon & DMAF_RASTER) && s_bplFetchIdx) {
		endOfLineModulo();
	}
	paulaLineTick();
}

static void runOneSlot(void) {
	UWORD h = s_uwHpos;
	int used = 0;
	tAceHostDmaKind kind = ACE_HOST_DMA_IDLE;
	int sprCh;
	int period;

	if(++s_eClockAcc >= 5) {
		s_eClockAcc = 0;
		ciaRunEclockTick();
	}

	if(h <= 6 && (h & 1) == 0) {
		kind = ACE_HOST_DMA_REFRESH;
		used = 1;
	}
	else if((h == 0x0D || h == 0x0F || h == 0x11 || h == 0x13) &&
		(s_uwDmacon & DMAF_MASTER)) {
		int ch = (int)((h - 0x0D) / 2);
		if(s_uwDmacon & (DMAF_AUD0 << ch)) {
			paulaDmaSlot(ch);
			kind = ACE_HOST_DMA_AUDIO;
			used = 1;
		}
	}

	sprCh = spriteSlotChannel(h);
	if(!used && sprCh >= 0 && (s_uwDmacon & DMAF_SPRITE) && (s_uwDmacon & DMAF_MASTER)) {
		if(!s_sprArmed[sprCh] && s_sprPtr[sprCh]) {
			spriteFetchCtl(sprCh);
		}
		else if(s_sprActive[sprCh]) {
			spriteFetchData(sprCh);
		}
		kind = ACE_HOST_DMA_SPRITE;
		used = 1;
	}

	if(!used && s_bplBusyRemain > 0) {
		s_bplBusyRemain--;
		kind = ACE_HOST_DMA_BPL;
		used = 1;
	}

	period = bplFetchPeriod();
	if(!used && (s_uwDmacon & DMAF_RASTER) && (s_uwDmacon & DMAF_MASTER) &&
		inDdf(h) && ((h - (g_pHostCustom->ddfstrt & ddfMask())) % period) == 0 &&
		(int)s_uwVpos >= diwV0() && (int)s_uwVpos < diwV1()) {
		int n = bplDepth() * bplFetchWords();
		fetchBplIntoLine();
		kind = ACE_HOST_DMA_BPL;
		used = 1;
		s_bplBusyRemain = n > 1 ? n - 1 : 0;
	}

	if(!used && s_copSlotRemain > 0) {
		s_copSlotRemain--;
		kind = ACE_HOST_DMA_COPPER;
		used = 1;
	}

	if(!used) {
		int blitHog = (s_uwDmacon & DMAF_BLITHOG) && blitterBusy() &&
			(s_uwDmacon & DMAF_BLITTER) && (s_uwDmacon & DMAF_MASTER);
		if(blitHog) {
			blitterUseSlot();
			s_blitSlotsFrame++;
			kind = ACE_HOST_DMA_BLIT;
			used = 1;
		}
		else if(copperTick()) {
			kind = ACE_HOST_DMA_COPPER;
			used = 1;
			s_copSlotRemain = 1; /* 2-word copper instruction */
		}
		else if(blitterBusy() && (s_uwDmacon & DMAF_BLITTER) && (s_uwDmacon & DMAF_MASTER)) {
			blitterUseSlot();
			s_blitSlotsFrame++;
			kind = ACE_HOST_DMA_BLIT;
			used = 1;
		}
	}

	s_lineDma[h] = (UBYTE)kind;
	renderSlotPixels();

	s_uwHpos++;
	s_slotsThisLine++;
	if(s_uwHpos >= ACE_HOST_SLOTS_PER_LINE) {
		if(s_slotsThisLine != ACE_HOST_SLOTS_PER_LINE) {
			s_timingOk = 0;
		}
		s_slotsThisLine = 0;
		endLine();
		s_uwHpos = 0;
		s_uwVpos++;
		s_linesThisFrame++;
		if(s_uwVpos >= s_lines) {
			if(s_linesThisFrame != s_lines) {
				s_timingOk = 0;
			}
			s_linesLastFrame = s_linesThisFrame;
			s_linesThisFrame = 0;
			s_uwVpos = 0;
		}
		beginLine();
	}
	updateVposRegs();
}

void chipsetRunSlots(unsigned n) {
	unsigned i;
	int nested = s_inChipset;
	s_inChipset++;
	if(!nested) {
		chipsetSyncCpuWrites();
	}
	for(i = 0; i < n; ++i) {
		runOneSlot();
		if(!nested && (i & 31) == 0) {
			aceHostDispatchInts(s_uwIntreq);
		}
	}
	if(!nested) {
		aceHostDispatchInts(s_uwIntreq);
	}
	/* Present on vblank from the game loop, not from WaitBlit: a blit-wait
	 * wrap would vsync-sleep the CPU path. Leave s_frameReady for aceHostTick. */
	if(s_frameReady && !s_blitWait) {
		s_frameReady = 0;
		s_vblankThisTick = 1;
		aceHostOnVblank();
	}
	s_inChipset--;
	/* CIA-A serial after the beam step so key.c's 3-scanline SPMODE wait
	 * via getRayPos() is not nested inside endLine (which would double-count). */
	if(!s_inChipset) {
		ciaPollKbd();
	}
}

void chipsetOnVposRead(void) {
	chipsetRunSlots(1);
}

void chipsetWaitBlit(void) {
	unsigned guard = 0;
	chipsetSyncCpuWrites();
	s_blitWait++;
	while(blitterBusy() && guard++ < 4000000u) {
		chipsetRunSlots(16);
	}
	s_blitWait--;
}

void aceHostTick(void) {
	/* Real Agnus keeps scanning even if the game never WaitTOF. Without a
	 * beam wait there is no vblank, so SDL never pumps and the window freezes. */
	if(!s_vblankThisTick) {
		if(s_frameReady) {
			s_frameReady = 0;
			s_vblankThisTick = 1;
			aceHostOnVblank();
		}
		else {
			chipsetRunSlots((unsigned)s_lines * ACE_HOST_SLOTS_PER_LINE);
		}
	}
	s_vblankThisTick = 0;
}

int chipsetBlitBusyPeek(void) {
	return blitterBusy();
}

int chipsetBlitIsBusy(void) {
	chipsetSyncCpuWrites();
	return blitterBusy();
}

void chipsetRunUntilVpos(UWORD uwY, int isExact) {
	unsigned guard = 0;
	chipsetSyncCpuWrites();
	if(isExact) {
		while(s_uwVpos >= uwY && guard++ < 4000000u) {
			chipsetRunSlots(8);
		}
	}
	guard = 0;
	while(s_uwVpos < uwY && guard++ < 4000000u) {
		chipsetRunSlots(8);
	}
}

void chipsetRunUntilVposGe(UWORD uwY) {
	chipsetRunUntilVpos(uwY, 0);
}

UWORD chipsetVpos(void) { return s_uwVpos; }
UWORD chipsetHpos(void) { return s_uwHpos; }
UWORD chipsetDmacon(void) { return s_uwDmacon; }
ULONG chipsetCopperPc(void) { return s_ulCopPc; }
int chipsetIsPal(void) { return s_isPal; }

UWORD *chipsetFramebuffer(int *pWidth, int *pHeight) {
	if(pWidth) {
		*pWidth = ACE_HOST_FB_WIDTH;
	}
	if(pHeight) {
		*pHeight = s_isPal ? ACE_HOST_FB_HEIGHT_PAL : ACE_HOST_FB_HEIGHT_NTSC;
	}
	return s_fb;
}

const UBYTE *chipsetLastLineDma(void) {
	return s_lineDma;
}

void chipsetSetJoyDat(UWORD uwJoy0, UWORD uwJoy1) {
	g_pHostCustom->joy0dat = uwJoy0;
	g_pHostCustom->joy1dat = uwJoy1;
}

void chipsetSetPotinp(UWORD uwPot) {
	g_pHostCustom->potinp = uwPot;
}

void chipsetSetCiaPraFire(UBYTE ubPraMaskHigh, UBYTE ubPraMaskLow) {
	ciaSetPraFire(ubPraMaskHigh, ubPraMaskLow);
}

void chipsetInjectKey(UBYTE ubRawKey) {
	ciaInjectKey(ubRawKey);
	if(!s_inChipset) {
		ciaPollKbd();
	}
}

void chipsetRaiseInt(UWORD uwMask) {
	s_uwIntreq |= uwMask;
}

void chipsetSetTimingLog(int on) {
	s_timingLog = on ? 1 : 0;
}

int chipsetTimingLogEnabled(void) {
	return s_timingLog;
}

UWORD chipsetLastCopWaitY(void) {
	return s_lastCopWaitY;
}

unsigned chipsetBlitSlotsLastFrame(void) {
	return s_blitSlotsLast;
}

int chipsetLinesLastFrame(void) {
	return s_linesLastFrame ? s_linesLastFrame : s_lines;
}

unsigned chipsetCopWaitHits(UWORD *pOut, unsigned maxOut) {
	unsigned n = s_copWaitHitN;
	unsigned i;
	if(n > maxOut) {
		n = maxOut;
	}
	if(pOut) {
		for(i = 0; i < n; ++i) {
			pOut[i] = s_copWaitHits[i];
		}
	}
	return n;
}

int chipsetTimingOk(void) {
	return s_timingOk;
}

void chipsetCopperDisasm(char *pBuf, unsigned bufSize, unsigned maxInsns) {
	unsigned n = 0, off = 0;
	ULONG pc = s_ulCopPc;
	if(!pBuf || !bufSize) {
		return;
	}
	pBuf[0] = 0;
	while(n < maxInsns && off + 48 < bufSize && pc) {
		UWORD uwIr1, uwIr2;
		const UBYTE *p = (const UBYTE *)(uintptr_t)pc;
		if(!aceHostIsChipPtr(p)) {
			break;
		}
		uwIr1 = readChipWord(pc);
		uwIr2 = readChipWord(pc + 2);
		if(uwIr1 & 1) {
			off += (unsigned)snprintf(
				pBuf + off, bufSize - off, "%s %u,%u\n",
				(uwIr2 & 1) ? "SKIP" : "WAIT",
				((uwIr1 >> 1) & 0x7F) << 1, uwIr1 >> 8
			);
			if(isTerminator(uwIr1, uwIr2)) {
				break;
			}
		}
		else {
			off += (unsigned)snprintf(
				pBuf + off, bufSize - off, "MOVE %03X,%04X\n",
				uwIr1 & 0x1FE, uwIr2
			);
		}
		pc += 4;
		n++;
	}
}

void chipsetReset(void) {
	s_uwVpos = 0;
	s_uwHpos = 0;
	s_uwDmacon = DMAF_MASTER;
	s_uwIntena = INTF_INTEN;
	s_uwIntreq = 0;
	s_copHalted = 1;
	s_ulCopPc = 0;
	s_slotsThisLine = 0;
	s_linesThisFrame = 0;
	s_linesLastFrame = 0;
	s_timingOk = 1;
	memset(s_fb, 0, sizeof(s_fb));
	memset(s_colorHi, 0, sizeof(s_colorHi));
	memset(s_colorLo, 0, sizeof(s_colorLo));
	memset(s_colorSeen, 0, sizeof(s_colorSeen));
	g_pHostCustom->potinp = 0xFFFF;
	g_pHostCustom->joy0dat = 0;
	g_pHostCustom->joy1dat = 0;
	g_pHostCustom->serdatr = 0x2000; /* TBE */
	updateVposRegs();
}

void chipsetInit(int isPal) {
	const char *szLog;
	s_isPal = isPal;
	s_lines = isPal ? ACE_HOST_PAL_LINES : ACE_HOST_NTSC_LINES;
	g_pHostCustom = (struct Custom *)(aceHostBusBase() + ACE_HOST_CUSTOM_OFFS);
	g_pHostCia[0] = (tCia *)(aceHostBusBase() + ACE_HOST_CIA_A_OFFS);
	g_pHostCia[1] = (tCia *)(aceHostBusBase() + ACE_HOST_CIA_B_OFFS);
	memset(g_pHostCustom, 0, sizeof(struct Custom));
	ciaInit(g_pHostCia[0], g_pHostCia[1]);
	aceHostBindCustom();
	g_pHostCustom = g_pCustom;
	g_pHostCia[0] = g_pCia[0];
	g_pHostCia[1] = g_pCia[1];
	blitterInit();
	paulaInit();
	chipsetReset();
	szLog = getenv("ACE_HOST_TIMING");
	if(szLog && szLog[0] && szLog[0] != '0') {
		s_timingLog = 1;
	}
	beginLine();
}

void chipsetShutdown(void) {
	paulaShutdown();
}

static const char *s_regNames[256]; /* filled lazily in disasm via copper.c names */

void aceHostPump(void) {
	aceHostSdlPump();
	aceHostSdlApplyInput();
}
