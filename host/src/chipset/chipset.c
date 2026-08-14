#include "chipset_priv.h"
#include "host_os.h"
#include <ace/managers/copper.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef ACE_HOST_HAS_SDL
#include <SDL.h>
#endif

struct Custom *g_pHostCustom;
tCia *g_pHostCia[2];

static int s_isPal = 1;
static int s_lines = ACE_HOST_PAL_LINES;
static volatile UWORD s_uwVpos;
static UWORD s_uwHpos;
static UWORD s_uwDmacon;
static UWORD s_uwIntena;
static UWORD s_uwIntreq;
static int s_inChipset;
static int s_skipRender;
static int s_frameReady;
static volatile int s_vblankThisTick;
static int s_vblankSync;
static int s_eClockAcc;
static int s_bplBusyRemain;
static int s_copSlotRemain;
static int s_timingLog;
static int s_timingOk = 1;
static volatile unsigned s_frameCount;
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

static ULONG s_sprPtr[8];
static int s_sprArmed[8];
static int s_sprActive[8];
static UWORD s_sprDatA[8][4], s_sprDatB[8][4];
static int s_sprX[8], s_sprY0[8], s_sprY1[8], s_sprAttach[8];
/* First sprite DMA line (HRM). Copper writes SPRxPT during vblank first. */
#define SPRITE_DMA_FIRST_LINE 25

static UWORD s_lastBltsize, s_lastCopjmp1, s_lastCopjmp2;
static UWORD s_lastDmaconW, s_lastIntenaW, s_lastIntreqW;

static int s_bplWordsThisLine;
static int s_bplModuloDone;
static int s_slotsThisLine;
static int s_linesThisFrame;
static int s_linesLastFrame;

#ifdef ACE_HOST_HAS_SDL
static SDL_mutex *s_chipMx;
static SDL_Thread *s_chipTh;
static SDL_threadID s_chipThreadId;
static SDL_threadID s_lockOwner;
static int s_lockDepth;
static volatile int s_chipRun;
static Uint64 s_paceFrameStart;
static Uint64 s_threadPaceFreq;
#endif

static void chipLock(void) {
#ifdef ACE_HOST_HAS_SDL
	SDL_threadID me;
	if(!s_chipMx) {
		return;
	}
	me = SDL_ThreadID();
	if(s_lockDepth && s_lockOwner == me) {
		s_lockDepth++;
		return;
	}
	SDL_LockMutex(s_chipMx);
	s_lockOwner = me;
	s_lockDepth = 1;
#endif
}

static void chipUnlock(void) {
#ifdef ACE_HOST_HAS_SDL
	if(!s_chipMx) {
		return;
	}
	if(--s_lockDepth == 0) {
		s_lockOwner = 0;
		SDL_UnlockMutex(s_chipMx);
	}
#endif
}

static int chipThreadIsRunning(void) {
#ifdef ACE_HOST_HAS_SDL
	return s_chipRun && s_chipTh;
#else
	return 0;
#endif
}

static int chipIsChipThread(void) {
#ifdef ACE_HOST_HAS_SDL
	return s_chipTh && SDL_ThreadID() == s_chipThreadId;
#else
	return 0;
#endif
}

static const char *hwPtrRegName(ULONG ulReg) {
	static char s_buf[20];
	int i;
	if(!g_pHostCustom) {
		return "?";
	}
	if(ulReg == (ULONG)g_pHostCustom->cop1lc) {
		return "cop1lc";
	}
	if(ulReg == (ULONG)g_pHostCustom->cop2lc) {
		return "cop2lc";
	}
	for(i = 0; i < 8; ++i) {
		if(ulReg == (ULONG)g_pHostCustom->bplpt[i]) {
			sprintf(s_buf, "bplpt[%d]", i);
			return s_buf;
		}
	}
	for(i = 0; i < 8; ++i) {
		if(ulReg == (ULONG)g_pHostCustom->sprpt[i]) {
			sprintf(s_buf, "sprpt[%d]", i);
			return s_buf;
		}
	}
	return "other";
}

static void hwPtrWarnOnce(ULONG ulReg, const char *szWhat) {
	static int s_warned;
	if(s_warned || !g_pHostCustom) {
		return;
	}
	s_warned = 1;
	fprintf(stderr,
		"[ACE_HOST] DMA pointer %08lX (%s) is not CHIP — %s "
		"(chip %p size %luK v=%u bplcon0=%04X cop1lc=%08lX cop2lc=%08lX "
		"bpl=%08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX "
		"spr=%08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX)\n",
		(unsigned long)ulReg,
		hwPtrRegName(ulReg),
		szWhat,
		(void *)aceHostBusBase(),
		(unsigned long)(aceHostChipSize() / 1024u),
		(unsigned)chipsetVpos(),
		(unsigned)g_pHostCustom->bplcon0,
		(unsigned long)g_pHostCustom->cop1lc,
		(unsigned long)g_pHostCustom->cop2lc,
		(unsigned long)g_pHostCustom->bplpt[0],
		(unsigned long)g_pHostCustom->bplpt[1],
		(unsigned long)g_pHostCustom->bplpt[2],
		(unsigned long)g_pHostCustom->bplpt[3],
		(unsigned long)g_pHostCustom->bplpt[4],
		(unsigned long)g_pHostCustom->bplpt[5],
		(unsigned long)g_pHostCustom->bplpt[6],
		(unsigned long)g_pHostCustom->bplpt[7],
		(unsigned long)g_pHostCustom->sprpt[0],
		(unsigned long)g_pHostCustom->sprpt[1],
		(unsigned long)g_pHostCustom->sprpt[2],
		(unsigned long)g_pHostCustom->sprpt[3],
		(unsigned long)g_pHostCustom->sprpt[4],
		(unsigned long)g_pHostCustom->sprpt[5],
		(unsigned long)g_pHostCustom->sprpt[6],
		(unsigned long)g_pHostCustom->sprpt[7]);
}

/* Agnus wraps CHIP fetches; host CHIP is a window at the bus base. */
static ULONG hwPtrWrapChip(ULONG ulReg) {
	UBYTE *pBus = aceHostBusBase();
	ULONG ulChip = aceHostChipSize();
	ULONG ulAddr = ulReg;
	ULONG ulBus;
	ULONG ulOffs;
	if(!pBus || !ulChip) {
		return 0;
	}
	if(ulReg < ACE_HOST_BUS_SIZE) {
		ulAddr = (ULONG)(uintptr_t)(pBus + ulReg);
	}
	ulBus = (ULONG)(uintptr_t)pBus;
	if(ulAddr < ulBus || ulAddr >= ulBus + ACE_HOST_BUS_SIZE) {
		return 0;
	}
	ulOffs = ulAddr - ulBus;
	return ulBus + (ulOffs % ulChip);
}

static ULONG hwPtr(ULONG ulReg) {
	ULONG ulWrapped;
	if(!ulReg) {
		return 0;
	}
	if(aceHostIsChipAddr(ulReg)) {
		if(ulReg < ACE_HOST_BUS_SIZE) {
			return (ULONG)(uintptr_t)aceHostBusToPtr(ulReg);
		}
		return ulReg;
	}
	/* Copper may store 16-bit halves swapped on LE if a MOVE missed the ptr path. */
	{
		ULONG ulSw = (ulReg << 16) | (ulReg >> 16);
		if(ulSw && aceHostIsChipAddr(ulSw)) {
			if(ulSw < ACE_HOST_BUS_SIZE) {
				return (ULONG)(uintptr_t)aceHostBusToPtr(ulSw);
			}
			return ulSw;
		}
	}
	ulWrapped = hwPtrWrapChip(ulReg);
	if(ulWrapped) {
		hwPtrWarnOnce(ulReg, "wrapped");
		return ulWrapped;
	}
	hwPtrWarnOnce(ulReg, "skipped");
	return 0;
}

static UWORD readChipWord(ULONG ulHost) {
	const UBYTE *b;
	if(!ulHost) {
		return 0;
	}
	b = (const UBYTE *)(uintptr_t)ulHost;
	return (UWORD)((b[0] << 8) | b[1]);
}

static void writeChipWord(ULONG ulHost, UWORD uw) {
	UBYTE *b;
	if(!ulHost) {
		return;
	}
	b = (UBYTE *)(uintptr_t)ulHost;
	b[0] = (UBYTE)(uw >> 8);
	b[1] = (UBYTE)(uw & 0xFF);
}

static void updateVposRegs(void) {
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

static int copperTick(void) {
	UWORD uwIr1, uwIr2;
	const UBYTE *p;
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

	p = (const UBYTE *)(uintptr_t)s_ulCopPc;
	if(!aceHostIsChipPtr(p)) {
		s_copHalted = 1;
		return 0;
	}
	uwIr1 = readChipWord(s_ulCopPc);
	uwIr2 = readChipWord(s_ulCopPc + 2);
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
	customWriteUword((UWORD)(uwIr1 & 0x1FE), uwIr2);
	return 1;
}

static void startBlitEcs(UWORD uwHeight, UWORD uwWidth) {
	int height = (int)(uwHeight & 0x7FFF);
	int width = (int)(uwWidth & 0x07FF);
	blitterStartWH(height ? height : 32768, width ? width : 2048);
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

static void storeColor(int reg, UWORD uwVal) {
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
		storeColor((int)((uwOffs - 0x180) / 2), uwVal);
	}

	switch(uwOffs) {
		case 0x058: /* BLTSIZE */
			blitterStart(uwVal);
			s_lastBltsize = uwVal;
			g_pHostCustom->bltsize = 0;
			break;
		case 0x05E: /* BLTSIZH */
			startBlitEcs(g_pHostCustom->bltsizv, uwVal);
			g_pHostCustom->bltsizh = 0;
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
	struct Custom *c;
	int locked = 0;
	if(!s_inChipset) {
		chipLock();
		locked = 1;
	}
	c = g_pHostCustom;
	if(c->bltsize) {
		blitterStart(c->bltsize);
		s_lastBltsize = c->bltsize;
		c->bltsize = 0;
	}
	if(c->bltsizh) {
		startBlitEcs(c->bltsizv, c->bltsizh);
		c->bltsizh = 0;
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
			if(c->color[i] != s_colorSeen[i]) {
				storeColor(i, c->color[i]);
			}
		}
	}
	updateVposRegs();
	if(locked) {
		chipUnlock();
	}
}

static void resetSpritesForFrame(void) {
	int i;
	for(i = 0; i < 8; ++i) {
		s_sprArmed[i] = 0;
		s_sprActive[i] = 0;
		s_sprPtr[i] = 0;
		s_sprX[i] = s_sprY0[i] = s_sprY1[i] = s_sprAttach[i] = 0;
		memset(s_sprDatA[i], 0, sizeof(s_sprDatA[i]));
		memset(s_sprDatB[i], 0, sizeof(s_sprDatB[i]));
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

static int inDdf(UWORD h) {
	UWORD mask = ddfMask();
	UWORD strt = (UWORD)(g_pHostCustom->ddfstrt & mask);
	UWORD stop = (UWORD)(g_pHostCustom->ddfstop & mask);
	return h >= strt && h <= stop;
}

static void endOfLineModulo(void) {
	int d = bplDepth();
	int i;
	WORD wOdd = (WORD)g_pHostCustom->bpl1mod;
	WORD wEven = (WORD)g_pHostCustom->bpl2mod;
	s_bplModuloDone = 1;
	for(i = 0; i < d; ++i) {
		WORD wMod = (i & 1) ? wEven : wOdd;
		s_bplPtr[i] = (ULONG)((LONG)s_bplPtr[i] + wMod);
		g_pHostCustom->bplpt[i] = (APTR)s_bplPtr[i];
	}
}

static int diwH0(void) { return g_pHostCustom->diwstrt & 0xFF; }
static int diwH1(void) { return (g_pHostCustom->diwstop & 0xFF) | 0x100; }
static int diwV0(void) { return (g_pHostCustom->diwstrt >> 8) & 0xFF; }
static int diwV1(void) {
	int v = (g_pHostCustom->diwstop >> 8) & 0xFF;
	if(!(v & 0x80)) {
		v |= 0x100;
	}
	return v;
}

static void plot(int x, int y, UWORD rgb) {
	int fbH = s_isPal ? ACE_HOST_FB_HEIGHT_PAL : ACE_HOST_FB_HEIGHT_NTSC;
	if((unsigned)x >= ACE_HOST_FB_WIDTH || (unsigned)y >= (unsigned)fbH) {
		return;
	}
	s_fb[y * ACE_HOST_FB_WIDTH + x] = rgb;
}

static void plotSlot(int x, int y, int hires, UWORD rgb) {
	if(hires) {
		plot(x, y, rgb);
	}
	else {
		plot(x * 2, y, rgb);
		plot(x * 2 + 1, y, rgb);
	}
}

static UWORD s_bplLine[8][64];
static int s_bplFetchIdx;

static void fetchBplIntoLine(void) {
	int d = bplDepth();
	int words = bplFetchWords();
	int w, i;
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
	int delay = even ? (con1 >> 4) & 0xF : con1 & 0xF;
#ifdef ACE_USE_AGA_FEATURES
	if(con1 & (even ? 0x4000 : 0x0400)) {
		delay |= 0x10;
	}
	if(con1 & (even ? 0x8000 : 0x0800)) {
		delay |= 0x20;
	}
#endif
	if(g_pHostCustom->bplcon0 & 0x8000) {
		delay *= 2;
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
	int ehb = bplDepth() == 6 &&
		!(g_pHostCustom->bplcon0 & 0x0800) &&
		!(g_pHostCustom->bplcon2 & 0x0200) &&
		idx >= 32;
	if(ehb) {
		idx -= 32;
	}
#ifdef ACE_USE_AGA_FEATURES
	idx &= 255;
#else
	idx &= 31;
#endif
	if(ehb) {
		return rgbFrom12((UWORD)((s_colorHi[idx] >> 1) & 0x0777), 0);
	}
	return rgbFrom12(s_colorHi[idx], s_colorLo[idx]);
}

static int sprFetchWords(void) {
#ifdef ACE_USE_AGA_FEATURES
	switch((g_pHostCustom->fmode >> 2) & 3) {
		case 1:
		case 2:
			return 2;
		case 3:
			return 4;
		default:
			return 1;
	}
#else
	return 1;
#endif
}

static void spriteFetchCtl(int ch) {
	UWORD pos, ctl;
	if(!s_sprPtr[ch]) {
		return;
	}
	/* ACE writes tHardwareSpriteHeader as native UWORDs; pixel data stays BE. */
	pos = *(const UWORD *)(uintptr_t)s_sprPtr[ch];
	ctl = *(const UWORD *)(uintptr_t)(s_sprPtr[ch] + 2);
	s_sprPtr[ch] += 4;
	g_pHostCustom->sprpt[ch] = (APTR)s_sprPtr[ch];
	s_sprY0[ch] = (pos >> 8) | ((ctl & 4) ? 0x100 : 0);
	s_sprY1[ch] = (ctl >> 8) | ((ctl & 2) ? 0x100 : 0);
	s_sprX[ch] = ((pos & 0xFF) << 1) | (ctl & 1);
	s_sprAttach[ch] = (ctl >> 7) & 1;
	s_sprArmed[ch] = 1;
	s_sprActive[ch] = 0;
}

static void spriteFetchLineData(int ch) {
	int i, nw = sprFetchWords();
	for(i = 0; i < 4; ++i) {
		s_sprDatA[ch][i] = 0;
		s_sprDatB[ch][i] = 0;
	}
	for(i = 0; i < nw; ++i) {
		s_sprDatA[ch][i] = readChipWord(s_sprPtr[ch]);
		s_sprPtr[ch] += 2;
	}
	for(i = 0; i < nw; ++i) {
		s_sprDatB[ch][i] = readChipWord(s_sprPtr[ch]);
		s_sprPtr[ch] += 2;
	}
	g_pHostCustom->sprpt[ch] = (APTR)s_sprPtr[ch];
}

static void spriteLineDma(void) {
	int ch;
	int dmaOn = (s_uwDmacon & DMAF_SPRITE) && (s_uwDmacon & DMAF_MASTER);
	if(!dmaOn || (int)s_uwVpos < SPRITE_DMA_FIRST_LINE) {
		return;
	}
	for(ch = 0; ch < 8; ++ch) {
		if(!s_sprArmed[ch]) {
			s_sprPtr[ch] = hwPtr(g_pHostCustom->sprpt[ch]);
			if(s_sprPtr[ch]) {
				spriteFetchCtl(ch);
			}
		}
		if(s_sprArmed[ch] && (int)s_uwVpos == s_sprY0[ch]) {
			s_sprActive[ch] = 1;
		}
		if(s_sprActive[ch] && (int)s_uwVpos >= s_sprY1[ch]) {
			s_sprActive[ch] = 0;
			s_sprArmed[ch] = 0;
			/* VSTOP line fetches the next sprite's control (chain / terminator). */
			s_sprPtr[ch] = hwPtr(g_pHostCustom->sprpt[ch]);
			if(s_sprPtr[ch]) {
				spriteFetchCtl(ch);
			}
		}
		if(s_sprActive[ch]) {
			spriteFetchLineData(ch);
		}
	}
}

static int spritePixel(int x, int *pColor, int *pPri) {
	int ch, bestPri = 99, found = 0, col = 0;
	int sprW = sprFetchWords() * 16;
	for(ch = 0; ch < 8; ++ch) {
		int sx, two, off, word, bit;
		if(!s_sprActive[ch] || ((ch & 1) && s_sprAttach[ch])) {
			continue;
		}
		sx = s_sprX[ch];
		if(x < sx || x >= sx + sprW) {
			continue;
		}
		off = x - sx;
		word = off / 16;
		bit = 15 - (off & 15);
		two = 0;
		if(s_sprDatA[ch][word] & (1u << bit)) {
			two |= 1;
		}
		if(s_sprDatB[ch][word] & (1u << bit)) {
			two |= 2;
		}
		if((ch + 1) < 8 && s_sprAttach[ch + 1] && s_sprActive[ch + 1]) {
			if(s_sprDatA[ch + 1][word] & (1u << bit)) {
				two |= 4;
			}
			if(s_sprDatB[ch + 1][word] & (1u << bit)) {
				two |= 8;
			}
		}
		if(two) {
			int pri = ch >> 1;
			if(pri < bestPri) {
				bestPri = pri;
				col = two;
				found = ch + 1;
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
	if(s_skipRender) {
		return;
	}
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
	int prefetch = bplFetchWords() * 16;

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
			plotSlot(xDisp, yDisp, hires, colorLookup(0));
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
			rgb = colorLookup(sprCol);
		}
		plotSlot(xDisp, yDisp, hires, rgb);
	}
}

static int spriteSlotChannel(UWORD h) {
	if(h < 0x15 || h > 0x24) {
		return -1;
	}
	return (int)((h - 0x15) / 2);
}

static void beginLine(void) {
	memset(s_lineDma, ACE_HOST_DMA_IDLE, sizeof(s_lineDma));
	s_bplFetchIdx = 0;
	s_bplWordsThisLine = 0;
	s_bplBusyRemain = 0;
	s_bplModuloDone = 0;
	s_copSlotRemain = 0;
	if(s_uwVpos == 0) {
		s_frameCount++;
		if(s_timingLog && s_linesLastFrame && (s_frameCount % 50u) == 1u) {
			fprintf(stderr,
				"[ACE_HOST] timing frame %u lines=%d (expect %d) lastCopWaitY=%u blitSlots=%u ok=%d\n",
				s_frameCount, s_linesLastFrame, s_lines, s_lastCopWaitY,
				s_blitSlotsLast, s_timingOk);
		}
		resetSpritesForFrame();
		copperJump(g_pHostCustom->cop1lc);
		s_uwIntreq |= INTF_VERTB;
		s_frameReady = 1;
		s_vblankThisTick = 1;
		s_blitSlotsLast = s_blitSlotsFrame;
		s_blitSlotsFrame = 0;
		s_copWaitHitN = 0;
		aceHostDispatchInts(s_uwIntreq);
		/* Present only on the game thread — SDL is not used from the chipset thread. */
		if((!chipThreadIsRunning() || !chipIsChipThread()) &&
			s_vblankSync && !s_skipRender) {
			aceHostOnVblank();
			s_frameReady = 0;
		}
	}
	spriteLineDma();
}

static void endLine(void) {
	if((s_uwDmacon & DMAF_RASTER) && s_bplFetchIdx && !s_bplModuloDone) {
		endOfLineModulo();
	}
	paulaLineTick(
		s_lines * (s_isPal ? 50 : 60),
		s_isPal ? 3546895u : 3579545u
	);
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
	if(!used && sprCh >= 0 && (s_uwDmacon & DMAF_SPRITE) && (s_uwDmacon & DMAF_MASTER) &&
		(int)s_uwVpos >= SPRITE_DMA_FIRST_LINE) {
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

	/* Agnus adds the modulo as the line's last fetch retires, not at the end of
	 * the scanline. Viewport managers park their BPLxPT copper MOVEs just past
	 * DDFSTOP, so a late modulo would be added on top of the next viewport's
	 * freshly loaded pointers. */
	if((s_uwDmacon & DMAF_RASTER) && s_bplFetchIdx && !s_bplModuloDone &&
		!s_bplBusyRemain && h > (UWORD)(g_pHostCustom->ddfstop & ddfMask())) {
		endOfLineModulo();
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
			s_copSlotRemain = 1;
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
	int nested;

	chipLock();
	nested = s_inChipset;
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
	s_inChipset--;
	if(!s_inChipset && !chipIsChipThread()) {
		ciaPollKbd();
	}
	chipUnlock();
}

#ifdef ACE_HOST_HAS_SDL
static int beamAtVblank(void *ctx) {
	(void)ctx;
	return s_vblankThisTick;
}

/* Do not take chipLock here — that would stall the chipset thread. */
static void chipWaitPoll(int (*done)(void *), void *ctx) {
	while(!done(ctx) && s_chipRun) {
		SDL_Delay(1);
	}
}

/* Monotonic beam position, so a wait cannot be stepped over. Testing
 * `vpos >= y` directly is only true for `lines - y` scanlines (0.8 ms at
 * y=300), which is narrower than a Sleep(1), so polls kept missing the
 * window and paying another whole frame. */
static Uint64 chipBeamAbs(void) {
	unsigned uFrame, uFrame2;
	UWORD uwLine;
	do {
		uFrame = s_frameCount;
		uwLine = s_uwVpos;
		uFrame2 = s_frameCount;
	} while(uFrame != uFrame2);
	return (Uint64)uFrame * (Uint64)s_lines + (Uint64)uwLine;
}

/* Absolute position of the next beam crossing of line uwY. isExact always
 * takes the following frame's crossing when the beam is already at or past it. */
static Uint64 chipBeamTarget(UWORD uwY, int isExact) {
	Uint64 ulNow = chipBeamAbs();
	Uint64 ulFrame = ulNow / (Uint64)s_lines;
	Uint64 ulLine = ulNow % (Uint64)s_lines;
	if(ulLine >= (Uint64)uwY) {
		return isExact ? (ulFrame + 1u) * (Uint64)s_lines + uwY : ulNow;
	}
	return ulFrame * (Uint64)s_lines + uwY;
}

/* Called after every scanline so VPOS tracks wall clock. Pacing once per frame
 * instead ran all 313 lines in a burst and parked at line 0 for the rest of the
 * period, so a wait for any line could only come true during the next burst —
 * every vPortWaitForPos cost a whole frame. Sleeps land in ~1 ms granules
 * (Sleep() cannot do the 64 us of a single line). */
static void chipThreadPace(void) {
	unsigned hz = s_isPal ? 50u : 60u;
	Uint64 period, now, target;
	if(!s_threadPaceFreq) {
		s_threadPaceFreq = SDL_GetPerformanceFrequency();
		if(!s_threadPaceFreq) {
			s_threadPaceFreq = 1000;
		}
	}
	period = (s_threadPaceFreq + (hz / 2u)) / hz;
	now = SDL_GetPerformanceCounter();
	if(!s_paceFrameStart) {
		s_paceFrameStart = now;
		return;
	}
	target = s_paceFrameStart + (Uint64)s_uwVpos * period / (Uint64)s_lines;
	if(now < target) {
		Uint64 remainMs = (target - now) * 1000u / s_threadPaceFreq;
		if(remainMs >= 1u) {
			SDL_Delay((Uint32)remainMs);
		}
	}
	if(s_uwVpos == 0) {
		s_paceFrameStart += period;
		/* Resync after a hitch instead of racing to catch up. */
		now = SDL_GetPerformanceCounter();
		if(now >= s_paceFrameStart + period) {
			s_paceFrameStart = now;
		}
	}
}

static int SDLCALL chipThreadFn(void *ud) {
	(void)ud;
	s_chipThreadId = SDL_ThreadID();
	while(s_chipRun) {
		chipsetRunSlots((unsigned)ACE_HOST_SLOTS_PER_LINE);
		chipThreadPace();
	}
	return 0;
}
#endif

void chipsetOnVposRead(void) {
	/* With the chipset thread running, VPOSR is live — except CIA-A SPMODE
	 * handshake, which needs Y to move even while the thread is asleep in
	 * chipThreadPace(). Cooperative builds always step a scanline per read. */
	UWORD uwY;
	unsigned guard;
	int spmode = g_pHostCia[0] && (g_pHostCia[0]->cra & CIACRA_SPMODE);
	if(chipThreadIsRunning() && !chipIsChipThread() && !spmode) {
		return;
	}
	uwY = s_uwVpos;
	guard = 0;
	while(s_uwVpos == uwY && guard++ < ACE_HOST_SLOTS_PER_LINE) {
		chipsetRunSlots(1);
	}
}

void chipsetWaitBlit(void) {
	/* Pixel work is already done in blitterStart; s_busy only counts DMA
	 * slots. Do not yield to the 50 Hz chipset thread — SDL_Delay(0) is a
	 * ~15 ms scheduler slice on Windows, so a checkerboard of blitRect
	 * calls (menuDrawBg) was taking 10–20 seconds. */
	chipLock();
	chipsetSyncCpuWrites();
	if(blitterBusy()) {
		blitterFinishNow();
	}
	chipUnlock();
}

void chipsetWaitVblank(void) {
	unsigned guard = 0;
	unsigned limit = (unsigned)s_lines * 2u + 2u;
	s_vblankSync = 1;
	chipsetSyncCpuWrites();
	if(chipThreadIsRunning() && !chipIsChipThread()) {
#ifdef ACE_HOST_HAS_SDL
		s_vblankThisTick = 0;
		chipWaitPoll(beamAtVblank, 0);
#else
		s_vblankThisTick = 0;
		while(!s_vblankThisTick && guard++ < 4000000u) {
		}
#endif
	}
	else {
		s_vblankThisTick = 0;
		while(!s_vblankThisTick && guard++ < limit) {
			chipsetRunSlots((unsigned)ACE_HOST_SLOTS_PER_LINE);
		}
	}
	if(s_frameReady && (!chipThreadIsRunning() || !chipIsChipThread())) {
		aceHostOnVblank();
		s_frameReady = 0;
	}
}

void aceHostTick(void) {
	s_vblankSync = 1;
	if(!s_vblankThisTick) {
		chipsetWaitVblank();
	}
	else if(s_frameReady) {
		aceHostOnVblank();
		s_frameReady = 0;
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
	if(isExact && uwY == 0) {
		chipsetWaitVblank();
		return;
	}
	if(chipThreadIsRunning() && !chipIsChipThread()) {
#ifdef ACE_HOST_HAS_SDL
		/* Wait on the frame counter, not on observing vpos < uwY: a 1 ms poll
		 * can step over that window for a viewport ending near the top and
		 * would then lose a whole frame. */
		{
			Uint64 ulTarget = chipBeamTarget(uwY, isExact);
			while(chipBeamAbs() < ulTarget && s_chipRun) {
				SDL_Delay(1);
			}
		}
#else
		if(isExact) {
			while(s_uwVpos >= uwY && guard++ < 4000000u) {
			}
		}
		guard = 0;
		while(s_uwVpos < uwY && guard++ < 4000000u) {
		}
#endif
		if(s_vblankThisTick && s_frameReady) {
			aceHostOnVblank();
			s_frameReady = 0;
		}
		return;
	}
	if(isExact) {
		/* Past the wait line: run to TOF without painting. Visible lines
		 * below must render so palette fades (and other COLOR updates)
		 * actually reach the framebuffer. */
		s_skipRender = 1;
		while(s_uwVpos >= uwY && guard++ < 4000000u) {
			chipsetRunSlots(ACE_HOST_SLOTS_PER_LINE);
		}
		s_skipRender = 0;
	}
	guard = 0;
	while(s_uwVpos < uwY && guard++ < 4000000u) {
		chipsetRunSlots(ACE_HOST_SLOTS_PER_LINE);
	}
	if(s_vblankThisTick && s_frameReady) {
		aceHostOnVblank();
		s_frameReady = 0;
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
	if(g_pHostCustom) {
		g_pHostCustom->intreqr = s_uwIntreq;
	}
	/* Paula AUDx must run the ptplayer handler as soon as the buffer ends,
	 * not at the next vblank — otherwise one-shot SFX loop for a full frame
	 * (or forever if INTENA never sees the bit). */
	if(uwMask & (INTF_AUD0 | INTF_AUD1 | INTF_AUD2 | INTF_AUD3)) {
		aceHostDispatchInts(uwMask & (INTF_AUD0 | INTF_AUD1 | INTF_AUD2 | INTF_AUD3));
	}
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
	/* Match ACE's no-OS game path: master + VERTB so timerOnInterrupt runs. */
	s_uwIntena = INTF_INTEN | INTF_VERTB | INTF_PORTS | INTF_EXTER;
	s_uwIntreq = 0;
	s_copHalted = 1;
	s_ulCopPc = 0;
	s_slotsThisLine = 0;
	s_linesThisFrame = 0;
	s_linesLastFrame = 0;
	s_timingOk = 1;
	memset(s_fb, 0, sizeof(s_fb));
	resetSpritesForFrame();
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

void chipsetStartThread(void) {
#ifdef ACE_HOST_HAS_SDL
	if(s_chipTh) {
		return;
	}
	if(!s_chipMx) {
		s_chipMx = SDL_CreateMutex();
	}
	s_chipRun = 1;
	s_paceFrameStart = 0;
	s_threadPaceFreq = 0;
	hostOsTimerHiRes(1);
	s_chipTh = SDL_CreateThread(chipThreadFn, "ace-chipset", NULL);
	if(!s_chipTh) {
		s_chipRun = 0;
		fprintf(stderr, "[ACE_HOST] chipset thread failed: %s\n", SDL_GetError());
	}
#endif
}

int chipsetThreadRunning(void) {
	return chipThreadIsRunning();
}

void chipsetShutdown(void) {
#ifdef ACE_HOST_HAS_SDL
	if(s_chipTh) {
		s_chipRun = 0;
		SDL_WaitThread(s_chipTh, NULL);
		s_chipTh = NULL;
	}
	hostOsTimerHiRes(0);
	if(s_chipMx) {
		SDL_DestroyMutex(s_chipMx);
		s_chipMx = NULL;
	}
#endif
	paulaShutdown();
}

void aceHostPump(void) {
	aceHostSdlPump();
	aceHostSdlApplyInput();
}
