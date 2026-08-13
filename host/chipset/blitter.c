#include "chipset_priv.h"
#include <hardware/blit.h>
#include <string.h>
#include <stdlib.h>

static int s_busy;
static int s_slotsLeft;
static int s_lineMode;

static ULONG ptrOf(APTR a) {
	ULONG p = aceHostIsChipAddr((ULONG)a)
		? (ULONG)(uintptr_t)aceHostBusToPtr((ULONG)a)
		: (ULONG)a;
	return p & ~(ULONG)1;
}

static UWORD r16(ULONG p) {
	const UBYTE *b;
	if(!p) {
		return 0;
	}
	b = (const UBYTE *)(uintptr_t)p;
	return (UWORD)((b[0] << 8) | b[1]);
}

static void w16(ULONG p, UWORD v) {
	UBYTE *b;
	if(!p) {
		return;
	}
	b = (UBYTE *)(uintptr_t)p;
	b[0] = (UBYTE)(v >> 8);
	b[1] = (UBYTE)(v & 0xFF);
}

static UWORD minterm(UWORD a, UWORD b, UWORD c, UBYTE mt) {
	UWORD out = 0;
	int bit;
	for(bit = 0; bit < 16; ++bit) {
		int ia = (a >> bit) & 1;
		int ib = (b >> bit) & 1;
		int ic = (c >> bit) & 1;
		int idx = (ia << 2) | (ib << 1) | ic;
		if(mt & (1 << idx)) {
			out |= (UWORD)(1u << bit);
		}
	}
	return out;
}

static void blitStandard(int width, int height) {
	struct Custom *c = g_pHostCustom;
	int useA = (c->bltcon0 & 0x0800) != 0;
	int useB = (c->bltcon0 & 0x0400) != 0;
	int useC = (c->bltcon0 & 0x0200) != 0;
	int useD = (c->bltcon0 & 0x0100) != 0;
	int desc = (c->bltcon1 & 2) != 0;
	int fill = (c->bltcon1 >> 2) & 3;
	int ashift = (c->bltcon0 >> 12) & 0xF;
	int bshift = (c->bltcon1 >> 12) & 0xF;
	UBYTE mt = (UBYTE)(c->bltcon0 & 0xFF);
	int step = desc ? -2 : 2;
	ULONG pa = ptrOf(c->bltapt);
	ULONG pb = ptrOf(c->bltbpt);
	ULONG pc = ptrOf(c->bltcpt);
	ULONG pd = ptrOf(c->bltdpt);
	WORD amod = (WORD)c->bltamod;
	WORD bmod = (WORD)c->bltbmod;
	WORD cmod = (WORD)c->bltcmod;
	WORD dmod = (WORD)c->bltdmod;
	UWORD afwm = c->bltafwm;
	UWORD alwm = c->bltalwm;
	UWORD ahold = 0, bhold = 0;
	int y, x;
	int fillCarry = 0;
	int channels = (useA ? 1 : 0) + (useB ? 1 : 0) + (useC ? 1 : 0) + (useD ? 1 : 0);
	LONG ms;
	if(width == 0) {
		width = 64;
	}
	if(height == 0) {
		height = 1024;
	}
	if(!useA && !useB && !useC && !useD) {
		s_slotsLeft = height * width;
		return;
	}

	ms = desc ? -1 : 1;
	for(y = 0; y < height; ++y) {
		ULONG ra = pa, rb = pb, rc = pc, rd = pd;
		fillCarry = (c->bltcon1 & 0x04) ? 1 : 0;
		for(x = 0; x < width; ++x) {
			UWORD a = 0, b = 0, cw = 0, d, rawA, rawB;
			UWORD mask = 0xFFFF;
			if(x == 0) {
				mask &= afwm;
			}
			if(x == width - 1) {
				mask &= alwm;
			}
			if(useA) {
				rawA = r16(ra);
				ra = (ULONG)((LONG)ra + step);
			}
			else {
				rawA = c->bltadat;
			}
			rawA &= mask;
			if(desc) {
				a = (UWORD)(((((ULONG)rawA << 16) | ahold) >> (16 - ashift)) & 0xFFFF);
			}
			else {
				a = (UWORD)(((((ULONG)ahold << 16) | rawA) >> ashift) & 0xFFFF);
			}
			ahold = rawA;
			if(useB) {
				rawB = r16(rb);
				rb = (ULONG)((LONG)rb + step);
			}
			else {
				rawB = c->bltbdat;
			}
			if(desc) {
				b = (UWORD)(((((ULONG)rawB << 16) | bhold) >> (16 - bshift)) & 0xFFFF);
			}
			else {
				b = (UWORD)(((((ULONG)bhold << 16) | rawB) >> bshift) & 0xFFFF);
			}
			bhold = rawB;
			if(useC) {
				cw = r16(rc);
				rc = (ULONG)((LONG)rc + step);
			}
			d = minterm(a, b, cw, mt);
			if(fill) {
				UWORD out = 0;
				int bit, carry = fillCarry;
				int inclusive = (fill == 2 || fill == 3);
				for(bit = 0; bit < 16; ++bit) {
					int bidx = desc ? (15 - bit) : bit;
					int din = (d >> bidx) & 1;
					int dout;
					if(inclusive) {
						dout = din | carry;
						if(din) {
							carry ^= 1;
						}
					}
					else {
						if(din) {
							carry ^= 1;
						}
						dout = carry;
					}
					if(dout) {
						out |= (UWORD)(1u << bidx);
					}
				}
				fillCarry = carry;
				d = out;
			}
			if(useD) {
				w16(rd, d);
				rd = (ULONG)((LONG)rd + step);
			}
		}
		if(useA) {
			pa = (ULONG)((LONG)ra + ms * (LONG)amod);
		}
		if(useB) {
			pb = (ULONG)((LONG)rb + ms * (LONG)bmod);
		}
		if(useC) {
			pc = (ULONG)((LONG)rc + ms * (LONG)cmod);
		}
		if(useD) {
			pd = (ULONG)((LONG)rd + ms * (LONG)dmod);
		}
	}
	c->bltapt = (APTR)pa;
	c->bltbpt = (APTR)pb;
	c->bltcpt = (APTR)pc;
	c->bltdpt = (APTR)pd;
	s_slotsLeft = height * width * (channels ? channels : 1);
}

static void blitLine(int width, int height) {
	struct Custom *c = g_pHostCustom;
	int ashift = (c->bltcon0 >> 12) & 0xF;
	int bshift = (c->bltcon1 >> 12) & 0xF;
	int sign = (c->bltcon1 & SIGNFLAG) != 0;
	int sing = (c->bltcon1 & SING) != 0;
	int sud = (c->bltcon1 & SUD) != 0;
	int sul = (c->bltcon1 & SUL) != 0;
	int aul = (c->bltcon1 & AUL) != 0;
	int useA = (c->bltcon0 & 0x0800) != 0;
	int useC = (c->bltcon0 & 0x0200) != 0;
	UBYTE mt = (UBYTE)(c->bltcon0 & 0xFF);
	UWORD afwm = c->bltafwm;
	UWORD alwm = c->bltalwm;
	UWORD adat = c->bltadat;
	UWORD bdat = c->bltbdat;
	WORD amod = (WORD)c->bltamod;
	WORD bmod = (WORD)c->bltbmod;
	WORD cmod = (WORD)c->bltcmod;
	ULONG pc = ptrOf(c->bltcpt);
	ULONG pd = ptrOf(c->bltdpt);
	LONG apt = (LONG)(WORD)(UWORD)((ULONG)c->bltapt & 0xFFFFu);
	UWORD blineb;
	int onedot = 0;
	int ovf = 0;
	int i;

	if(height == 0) {
		height = 1024;
	}
	if(width == 0) {
		width = 64;
	}

	blineb = (UWORD)((bdat >> bshift) | (bdat << ((16 - bshift) & 15)));
	if(bshift == 0) {
		blineb = bdat;
	}

	for(i = 0; i < height; ++i) {
		int plot = !sing || !onedot;
		UWORD mask, ahold, bhold, chold, d;
		int oldSign = sign;

		onedot = 1;

		if(useA) {
			apt += oldSign ? (LONG)bmod : (LONG)amod;
		}

		mask = afwm;
		if(width <= 1) {
			mask &= alwm;
		}
		ahold = (UWORD)((adat & mask) >> ashift);
		bhold = (UWORD)((blineb & 1u) ? 0xFFFFu : 0);
		chold = useC ? r16(pc) : 0;
		d = minterm(ahold, bhold, chold, mt);

		if(width > 1) {
			if(!oldSign && !sud) {
				if(sul) {
					if(ashift == 0) {
						pc -= 2;
					}
					ovf = -1;
				}
				else {
					if(ashift == 15) {
						pc += 2;
					}
					ovf = 1;
				}
			}
			if(sud) {
				if(aul) {
					if(ashift == 0) {
						pc -= 2;
					}
					ovf = -1;
				}
				else {
					if(ashift == 15) {
						pc += 2;
					}
					ovf = 1;
				}
			}
		}

		ashift = (ashift + ovf) & 15;
		ovf = 0;

		if(width >= 2) {
			if(!oldSign && sud) {
				pc = (ULONG)((LONG)pc + (sul ? -(LONG)cmod : (LONG)cmod));
				onedot = 0;
			}
			if(!sud) {
				pc = (ULONG)((LONG)pc + (aul ? -(LONG)cmod : (LONG)cmod));
				onedot = 0;
			}
		}

		sign = (WORD)apt < 0;
		bshift = (bshift - 1) & 15;
		blineb = (UWORD)((bdat >> bshift) | (bdat << ((16 - bshift) & 15)));
		if(bshift == 0) {
			blineb = bdat;
		}

		if(plot && useC) {
			w16(pd, d);
		}
		pd = pc;
	}

	c->bltcpt = (APTR)pc;
	c->bltdpt = (APTR)pd;
	c->bltapt = (APTR)(ULONG)(UWORD)apt;
	c->bltcon0 = (UWORD)((c->bltcon0 & 0x0FFF) | (ashift << 12));
	c->bltcon1 = (UWORD)((c->bltcon1 & 0x0FFF & (UWORD)~SIGNFLAG) |
		(bshift << 12) | (sign ? SIGNFLAG : 0));
	s_slotsLeft = height * 2;
}

void blitterInit(void) {
	s_busy = 0;
	s_slotsLeft = 0;
	s_lineMode = 0;
}

void blitterStart(UWORD uwBltSize) {
	blitterStartWH((int)(uwBltSize >> 6), (int)(uwBltSize & 0x3F));
}

void blitterStartWH(int height, int width) {
	s_lineMode = (g_pHostCustom->bltcon1 & 1) != 0;
	s_busy = 1;
	if(s_lineMode) {
		blitLine(width, height);
	}
	else {
		blitStandard(width, height);
	}
	if(s_slotsLeft < 1) {
		s_slotsLeft = 1;
	}
}

void blitterUseSlot(void) {
	if(!s_busy) {
		return;
	}
	if(--s_slotsLeft <= 0) {
		s_busy = 0;
		chipsetRaiseInt(INTF_BLIT);
	}
}

int blitterBusy(void) {
	return s_busy;
}

void blitterFinishNow(void) {
	s_busy = 0;
	s_slotsLeft = 0;
}
