#include "chipset_priv.h"
#include <hardware/blit.h>
#include <string.h>
#include <stdlib.h>

static int s_busy;
static int s_slotsLeft;

static LONG chipAddr(APTR a) {
	ULONG p = (ULONG)(uintptr_t)a;
	if(aceHostIsChipAddr(p)) {
		p = (ULONG)(uintptr_t)aceHostBusToPtr(p);
	}
	return (LONG)(p & ~1ul);
}

static UWORD r16(LONG p) {
	const UBYTE *b;
	if(!p) {
		return 0;
	}
	b = (const UBYTE *)(uintptr_t)(ULONG)p;
	return (UWORD)((b[0] << 8) | b[1]);
}

static void w16(LONG p, UWORD v) {
	UBYTE *b;
	if(!p) {
		return;
	}
	b = (UBYTE *)(uintptr_t)(ULONG)p;
	b[0] = (UBYTE)(v >> 8);
	b[1] = (UBYTE)(v & 0xFF);
}

static UWORD minterm(UWORD a, UWORD b, UWORD c, UBYTE mt) {
	UWORD na = (UWORD)~a;
	UWORD nb = (UWORD)~b;
	UWORD nc = (UWORD)~c;
	UWORD r = 0;
	if(mt & 0x01) {
		r |= na & nb & nc;
	}
	if(mt & 0x02) {
		r |= na & nb & c;
	}
	if(mt & 0x04) {
		r |= na & b & nc;
	}
	if(mt & 0x08) {
		r |= na & b & c;
	}
	if(mt & 0x10) {
		r |= a & nb & nc;
	}
	if(mt & 0x20) {
		r |= a & nb & c;
	}
	if(mt & 0x40) {
		r |= a & b & nc;
	}
	if(mt & 0x80) {
		r |= a & b & c;
	}
	return r;
}

static void blitStandard(int width, int height) {
	struct Custom *c = g_pHostCustom;
	int useA = c->bltcon0 & 0x0800;
	int useB = c->bltcon0 & 0x0400;
	int useC = c->bltcon0 & 0x0200;
	int useD = c->bltcon0 & 0x0100;
	int desc = c->bltcon1 & 2;
	int fill = (c->bltcon1 >> 2) & 3;
	int ashift = (c->bltcon0 >> 12) & 0xF;
	int bshift = (c->bltcon1 >> 12) & 0xF;
	UBYTE mt = (UBYTE)(c->bltcon0 & 0xFF);
	int step = desc ? -2 : 2;
	int ms = desc ? -1 : 1;
	LONG pa = chipAddr(c->bltapt);
	LONG pb = chipAddr(c->bltbpt);
	LONG pc = chipAddr(c->bltcpt);
	LONG pd = chipAddr(c->bltdpt);
	WORD amod = (WORD)c->bltamod;
	WORD bmod = (WORD)c->bltbmod;
	WORD cmod = (WORD)c->bltcmod;
	WORD dmod = (WORD)c->bltdmod;
	UWORD afwm = c->bltafwm;
	UWORD alwm = c->bltalwm;
	UWORD ahold = 0, bhold = 0;
	int y, x, nch;
	if(!width) {
		width = 64;
	}
	if(!height) {
		height = 1024;
	}
	nch = !!useA + !!useB + !!useC + !!useD;
	if(!nch) {
		s_slotsLeft = height * width;
		return;
	}

	for(y = 0; y < height; ++y) {
		LONG ra = pa, rb = pb, rc = pc, rd = pd;
		int fillCarry = (c->bltcon1 & 0x04) != 0;
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
				ra += step;
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
				rb += step;
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
				rc += step;
			}
			d = minterm(a, b, cw, mt);
			if(fill) {
				UWORD out = 0;
				int bit, carry = fillCarry;
				int inclusive = fill >= 2;
				for(bit = 0; bit < 16; ++bit) {
					int bidx = desc ? 15 - bit : bit;
					int din = (d >> bidx) & 1;
					int dout;
					if(inclusive) {
						dout = din | carry;
					}
					carry ^= din;
					if(!inclusive) {
						dout = carry;
					}
					if(dout) {
						out |= (UWORD)(1 << bidx);
					}
				}
				fillCarry = carry;
				d = out;
			}
			if(useD) {
				w16(rd, d);
				rd += step;
			}
		}
		if(useA) {
			pa = ra + ms * amod;
		}
		if(useB) {
			pb = rb + ms * bmod;
		}
		if(useC) {
			pc = rc + ms * cmod;
		}
		if(useD) {
			pd = rd + ms * dmod;
		}
	}
	c->bltapt = (APTR)(ULONG)pa;
	c->bltbpt = (APTR)(ULONG)pb;
	c->bltcpt = (APTR)(ULONG)pc;
	c->bltdpt = (APTR)(ULONG)pd;
	s_slotsLeft = height * width * nch;
}

static void blitLine(int width, int height) {
	struct Custom *c = g_pHostCustom;
	int ashift = (c->bltcon0 >> 12) & 0xF;
	int bshift = (c->bltcon1 >> 12) & 0xF;
	int sign = (c->bltcon1 & SIGNFLAG) != 0;
	int sing = c->bltcon1 & SING;
	int sud = c->bltcon1 & SUD;
	int sul = c->bltcon1 & SUL;
	int aul = c->bltcon1 & AUL;
	int useA = c->bltcon0 & 0x0800;
	int useC = c->bltcon0 & 0x0200;
	UBYTE mt = (UBYTE)(c->bltcon0 & 0xFF);
	UWORD afwm = c->bltafwm;
	UWORD alwm = c->bltalwm;
	UWORD adat = c->bltadat;
	UWORD bdat = c->bltbdat;
	WORD amod = (WORD)c->bltamod;
	WORD bmod = (WORD)c->bltbmod;
	WORD cmod = (WORD)c->bltcmod;
	LONG pc = chipAddr(c->bltcpt);
	LONG pd = chipAddr(c->bltdpt);
	LONG apt = (WORD)(ULONG)c->bltapt;
	UWORD blineb;
	int onedot = 0;
	int i;

	if(!height) {
		height = 1024;
	}
	if(!width) {
		width = 64;
	}

	blineb = ror16(bdat, (UBYTE)bshift);
	for(i = 0; i < height; ++i) {
		int plot = !sing || !onedot;
		int oldSign = sign;
		UWORD mask, ahold, bhold, chold, d;
		int ovf = 0;

		onedot = 1;
		if(useA) {
			apt += oldSign ? bmod : amod;
		}

		mask = afwm;
		if(width <= 1) {
			mask &= alwm;
		}
		ahold = (UWORD)((adat & mask) >> ashift);
		bhold = (blineb & 1) ? 0xFFFF : 0;
		chold = useC ? r16(pc) : 0;
		d = minterm(ahold, bhold, chold, mt);

		if(width > 1 && (sud || !oldSign)) {
			int left = sud ? aul : sul;
			if(left) {
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
		ashift = (ashift + ovf) & 15;

		if(width >= 2 && (!sud || !oldSign)) {
			int up = sud ? sul : aul;
			pc += up ? -cmod : cmod;
			onedot = 0;
		}

		sign = (WORD)apt < 0;
		bshift = (bshift - 1) & 15;
		blineb = ror16(bdat, (UBYTE)bshift);
		if(plot && useC) {
			w16(pd, d);
		}
		pd = pc;
	}

	c->bltcpt = (APTR)(ULONG)pc;
	c->bltdpt = (APTR)(ULONG)pd;
	c->bltapt = (APTR)(ULONG)(UWORD)apt;
	c->bltcon0 = (UWORD)((c->bltcon0 & 0x0FFF) | (ashift << 12));
	c->bltcon1 = (UWORD)((c->bltcon1 & 0x0FFF & (UWORD)~SIGNFLAG) |
		(bshift << 12) | (sign ? SIGNFLAG : 0));
	s_slotsLeft = height * 2;
}

void blitterInit(void) {
	s_busy = 0;
	s_slotsLeft = 0;
}

void blitterStart(UWORD uwBltSize) {
	blitterStartWH((int)(uwBltSize >> 6), (int)(uwBltSize & 0x3F));
}

void blitterStartWH(int height, int width) {
	/* Queued blits (BLITHOG tile draw strobes BLTSIZE without waiting) arrive
	 * while the previous one still owes DMA slots. The pixel work is already
	 * done, but its bus cost is not — keep it so BBUSY covers both. */
	int pendingSlots = s_busy ? s_slotsLeft : 0;

	s_busy = 1;
	if(g_pHostCustom->bltcon1 & 1) {
		blitLine(width, height);
	}
	else {
		blitStandard(width, height);
	}
	if(s_slotsLeft < 1) {
		s_slotsLeft = 1;
	}
	s_slotsLeft += pendingSlots;
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
	if(s_busy) {
		s_busy = 0;
		s_slotsLeft = 0;
		chipsetRaiseInt(INTF_BLIT);
	}
}
