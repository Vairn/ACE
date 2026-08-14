#include "chipset_priv.h"
#include "../sdl/host_chrome.h"
#include <stdio.h>
#include <string.h>

/* Tiny 5x7 font: 0-9 then A-Z */
static const unsigned char FONT[][7] = {
	{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /* 0 */
	{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
	{0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
	{0x0E,0x11,0x01,0x06,0x01,0x11,0x0E},
	{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
	{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
	{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
	{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
	{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
	{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, /* 9 */
	{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, /* A */
	{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
	{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
	{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
	{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
	{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
	{0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
	{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
	{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
	{0x01,0x01,0x01,0x01,0x11,0x11,0x0E},
	{0x11,0x12,0x14,0x18,0x14,0x12,0x11},
	{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
	{0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
	{0x11,0x19,0x15,0x13,0x11,0x11,0x11},
	{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
	{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
	{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
	{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
	{0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E},
	{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
	{0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
	{0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
	{0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
	{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
	{0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
	{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}  /* Z */
};

static int glyphIndex(char ch) {
	if(ch >= '0' && ch <= '9') {
		return ch - '0';
	}
	if(ch >= 'A' && ch <= 'Z') {
		return 10 + (ch - 'A');
	}
	if(ch >= 'a' && ch <= 'z') {
		return 10 + (ch - 'a');
	}
	return -1;
}

static void plotHud(UWORD *fb, int w, int h, int x, int y, UWORD c) {
	if(x < 0 || y < 0 || x >= w || y >= h) {
		return;
	}
	fb[y * w + x] = c;
}

static void drawChar(UWORD *fb, int w, int h, int x, int y, char ch, UWORD c) {
	int gx, gy, idx = glyphIndex(ch);
	unsigned char rows[7];
	memset(rows, 0, sizeof(rows));
	if(idx >= 0) {
		memcpy(rows, FONT[idx], 7);
	}
	else if(ch == ' ') {
		return;
	}
	else if(ch == ':') {
		rows[2] = 0x04; rows[4] = 0x04;
	}
	else if(ch == '/') {
		rows[1] = 0x01; rows[2] = 0x02; rows[3] = 0x04; rows[4] = 0x08; rows[5] = 0x10;
	}
	else if(ch == '+') {
		rows[2] = 0x04; rows[3] = 0x1F; rows[4] = 0x04;
	}
	else if(ch == '-') {
		rows[3] = 0x1F;
	}
	else if(ch == '=') {
		rows[2] = 0x1F; rows[4] = 0x1F;
	}
	else if(ch == '_') {
		rows[6] = 0x1F;
	}
	else if(ch == '(') {
		rows[1]=0x04; rows[2]=0x08; rows[3]=0x08; rows[4]=0x08; rows[5]=0x04;
	}
	else if(ch == ')') {
		rows[1]=0x08; rows[2]=0x04; rows[3]=0x04; rows[4]=0x04; rows[5]=0x08;
	}
	else if(ch == '.') {
		rows[5] = 0x04; rows[6] = 0x04;
	}
	else if(ch == '*') {
		rows[1]=0x15; rows[2]=0x0E; rows[3]=0x04; rows[4]=0x0E; rows[5]=0x15;
	}
	for(gy = 0; gy < 7; ++gy) {
		for(gx = 0; gx < 5; ++gx) {
			if(rows[gy] & (0x10 >> gx)) {
				plotHud(fb, w, h, x + gx, y + gy, c);
			}
		}
	}
}

void aceHostOverlayFill(UWORD *fb, int w, int h, int x, int y, int bw, int bh, UWORD c) {
	int i, j;
	for(j = 0; j < bh; ++j) {
		for(i = 0; i < bw; ++i) {
			plotHud(fb, w, h, x + i, y + j, c);
		}
	}
}

void aceHostOverlayDrawStr(UWORD *fb, int w, int h, int x, int y, const char *s, UWORD c) {
	while(*s) {
		drawChar(fb, w, h, x, y, *s, c);
		x += 6;
		s++;
	}
}

static void bar(UWORD *fb, int w, int h, int x, int y, int bw, int bh, ULONG used, ULONG budget, int over) {
	int i, fill;
	UWORD bg = 0x2104, fg = over ? 0xF800 : 0x07E0, bd = 0xFFFF;
	if(budget == 0) {
		budget = 1;
	}
	fill = (int)((used > budget ? budget : used) * (ULONG)bw / budget);
	for(i = 0; i < bw; ++i) {
		int yy;
		for(yy = 0; yy < bh; ++yy) {
			UWORD col = (i == 0 || i == bw - 1 || yy == 0 || yy == bh - 1) ? bd :
				(i < fill ? fg : bg);
			plotHud(fb, w, h, x + i, y + yy, col);
		}
	}
}

void aceHostHudDraw(UWORD *pFb, int width, int height) {
#ifdef ACE_HOST_DEBUG
	char line[128];
	UWORD white = 0xFFFF, red = 0xF800, yellow = 0xFFE0, cyan = 0x07FF;
	int y = 1;
	ULONG chipU = aceHostChipUsed(), chipB = aceHostChipBudget();
	ULONG fastU = aceHostFastUsed(), fastB = aceHostFastBudget();
	ULONG chipP = aceHostChipPeak(), fastP = aceHostFastPeak();
	ULONG chipF = aceHostChipLargestFree(), fastF = aceHostFastLargestFree();
	int over = aceHostChipOverBudget() || aceHostFastOverBudget();
	static int s_bannerFrames;

	if(aceHostOverBudgetBanner()) {
		aceHostOverlayDrawStr(pFb, width, height, 2, y, "OVER BUDGET", red);
		y += 8;
		s_bannerFrames++;
		if(s_bannerFrames > 150) {
			aceHostClearOverBudgetBanner();
			s_bannerFrames = 0;
		}
	}

	snprintf(line, sizeof(line), "%s %s",
		aceHostMachineName(),
		aceHostMemMode() == ACE_HOST_MEM_VIRTUAL ? "VIRTUAL" : "STRICT");
	aceHostOverlayDrawStr(pFb, width, height, 2, y, line, white);
	y += 8;

	snprintf(line, sizeof(line), "CHIP %lu/%luK PK%luK FR%luK",
		(unsigned long)(chipU / 1024u), (unsigned long)(chipB / 1024u),
		(unsigned long)(chipP / 1024u), (unsigned long)(chipF / 1024u));
	aceHostOverlayDrawStr(pFb, width, height, 2, y, line, over && aceHostChipOverBudget() ? red : white);
	bar(pFb, width, height, 280, y, 80, 7, chipU, chipB ? chipB : 1, aceHostChipOverBudget());
	y += 8;

	snprintf(line, sizeof(line), "FAST %lu/%luK PK%luK FR%luK",
		(unsigned long)(fastU / 1024u), (unsigned long)(fastB / 1024u),
		(unsigned long)(fastP / 1024u), (unsigned long)(fastF / 1024u));
	aceHostOverlayDrawStr(pFb, width, height, 2, y, line, aceHostFastOverBudget() ? red : white);
	bar(pFb, width, height, 280, y, 80, 7, fastU, fastB ? fastB : 1, aceHostFastOverBudget());
	y += 8;

	if(over) {
		LONG dChip = (LONG)chipU - (LONG)chipB;
		LONG dFast = (LONG)fastU - (LONG)fastB;
		LONG d = dChip > 0 ? dChip : dFast;
		snprintf(line, sizeof(line), "OVER +%ldK", (long)(d / 1024));
		aceHostOverlayDrawStr(pFb, width, height, 2, y, line, red);
		y += 8;
	}

	snprintf(line, sizeof(line), "Y%u X%u DMA%03X %s COP%08lX",
		chipsetVpos(), chipsetHpos(), chipsetDmacon(),
		chipsetBlitBusyPeek() ? "BBUSY" : "BIDLE",
		(unsigned long)chipsetCopperPc());
	aceHostOverlayDrawStr(pFb, width, height, 2, y, line, yellow);
	y += 8;
	snprintf(line, sizeof(line), "LN%u CW%u BLIT%u %s",
		chipsetLinesLastFrame(),
		chipsetLastCopWaitY(), chipsetBlitSlotsLastFrame(),
		chipsetTimingOk() ? "OK" : "BAD");
	aceHostOverlayDrawStr(pFb, width, height, 2, y, line, chipsetTimingOk() ? white : red);

	if(aceHostHudFull()) {
		const UBYTE *dma = chipsetLastLineDma();
		int i;
		unsigned nalloc, ai;
		ULONG chipSz = aceHostChipSize();
		y += 10;
		aceHostOverlayDrawStr(pFb, width, height, 2, y, "DMA", white);
		y += 8;
		for(i = 0; i < ACE_HOST_SLOTS_PER_LINE && i < width - 4; ++i) {
			UWORD col = 0x2104;
			switch(dma[i]) {
				case ACE_HOST_DMA_REFRESH: col = 0x7BEF; break;
				case ACE_HOST_DMA_AUDIO: col = 0xFD20; break;
				case ACE_HOST_DMA_SPRITE: col = 0x07FF; break;
				case ACE_HOST_DMA_BPL: col = 0x07E0; break;
				case ACE_HOST_DMA_COPPER: col = 0xF81F; break;
				case ACE_HOST_DMA_BLIT: col = 0xF800; break;
				case ACE_HOST_DMA_CPU: col = 0x001F; break;
				default: break;
			}
			plotHud(pFb, width, height, 2 + i, y, col);
			plotHud(pFb, width, height, 2 + i, y + 1, col);
		}
		y += 4;
		aceHostOverlayDrawStr(pFb, width, height, 2, y, "CHIP MAP", cyan);
		y += 8;
		{
			int bw = width - 8;
			int px;
			if(bw < 8) {
				bw = 8;
			}
			if(chipSz == 0) {
				chipSz = 1;
			}
			for(px = 0; px < bw; ++px) {
				plotHud(pFb, width, height, 4 + px, y, 0x2104);
				plotHud(pFb, width, height, 4 + px, y + 1, 0x2104);
			}
			nalloc = aceHostAllocCount();
			for(ai = 0; ai < nalloc; ++ai) {
				tAceHostAllocInfo inf;
				if(!aceHostAllocAt(ai, &inf) || !inf.isChip) {
					continue;
				}
				{
					ULONG off = inf.ulAddr - (ULONG)(uintptr_t)aceHostBusBase();
					int x0 = (int)(off * (ULONG)bw / chipSz);
					int x1 = (int)((off + inf.ulSize) * (ULONG)bw / chipSz);
					if(x0 < 0) {
						x0 = 0;
					}
					if(x1 > bw) {
						x1 = bw;
					}
					for(px = x0; px < x1; ++px) {
						plotHud(pFb, width, height, 4 + px, y, 0x07E0);
						plotHud(pFb, width, height, 4 + px, y + 1, 0x07E0);
					}
				}
			}
		}
		y += 6;
		aceHostOverlayDrawStr(pFb, width, height, 2, y, "ALLOCS", cyan);
		y += 8;
		nalloc = aceHostAllocCount();
		for(ai = 0; ai < nalloc && ai < 8 && y < height - 16; ++ai) {
			tAceHostAllocInfo inf;
			if(!aceHostAllocAt(ai, &inf)) {
				break;
			}
			snprintf(line, sizeof(line), "%s %08lX %lu",
				inf.isChip ? "C" : "F",
				(unsigned long)inf.ulAddr,
				(unsigned long)inf.ulSize);
			aceHostOverlayDrawStr(pFb, width, height, 2, y, line, white);
			y += 8;
		}
		{
			char cop[512];
			chipsetCopperDisasm(cop, sizeof(cop), 8);
			{
				char *p = cop, *nl;
				while(*p && y < height - 8) {
					nl = strchr(p, '\n');
					if(nl) {
						*nl = 0;
					}
					aceHostOverlayDrawStr(pFb, width, height, 2, y, p, white);
					y += 8;
					if(!nl) {
						break;
					}
					p = nl + 1;
				}
			}
		}
	}
#else
	(void)pFb;
	(void)width;
	(void)height;
#endif
}
