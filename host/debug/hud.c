#include "chipset_priv.h"
#include <stdio.h>
#include <string.h>

/* Tiny 5x7 font for 0-9 A-Z space /:+- */
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
};

static int glyphIndex(char ch) {
	if(ch >= '0' && ch <= '9') {
		return ch - '0';
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
	else if(ch == 'K') {
		rows[0]=0x11; rows[1]=0x12; rows[2]=0x14; rows[3]=0x18; rows[4]=0x14; rows[5]=0x12; rows[6]=0x11;
	}
	else if(ch == 'M') {
		rows[0]=0x11; rows[1]=0x1B; rows[2]=0x15; rows[3]=0x15; rows[4]=0x11; rows[5]=0x11; rows[6]=0x11;
	}
	else if(ch == 'C') {
		rows[0]=0x0E; rows[1]=0x11; rows[2]=0x10; rows[3]=0x10; rows[4]=0x10; rows[5]=0x11; rows[6]=0x0E;
	}
	else if(ch == 'H') {
		rows[0]=0x11; rows[1]=0x11; rows[2]=0x11; rows[3]=0x1F; rows[4]=0x11; rows[5]=0x11; rows[6]=0x11;
	}
	else if(ch == 'I') {
		rows[0]=0x0E; rows[1]=0x04; rows[2]=0x04; rows[3]=0x04; rows[4]=0x04; rows[5]=0x04; rows[6]=0x0E;
	}
	else if(ch == 'P') {
		rows[0]=0x1E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x1E; rows[4]=0x10; rows[5]=0x10; rows[6]=0x10;
	}
	else if(ch == 'F') {
		rows[0]=0x1F; rows[1]=0x10; rows[2]=0x10; rows[3]=0x1E; rows[4]=0x10; rows[5]=0x10; rows[6]=0x10;
	}
	else if(ch == 'A') {
		rows[0]=0x0E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x1F; rows[4]=0x11; rows[5]=0x11; rows[6]=0x11;
	}
	else if(ch == 'S') {
		rows[0]=0x0E; rows[1]=0x11; rows[2]=0x10; rows[3]=0x0E; rows[4]=0x01; rows[5]=0x11; rows[6]=0x0E;
	}
	else if(ch == 'T') {
		rows[0]=0x1F; rows[1]=0x04; rows[2]=0x04; rows[3]=0x04; rows[4]=0x04; rows[5]=0x04; rows[6]=0x04;
	}
	else if(ch == 'R') {
		rows[0]=0x1E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x1E; rows[4]=0x14; rows[5]=0x12; rows[6]=0x11;
	}
	else if(ch == 'O') {
		rows[0]=0x0E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x11; rows[4]=0x11; rows[5]=0x11; rows[6]=0x0E;
	}
	else if(ch == 'V') {
		rows[0]=0x11; rows[1]=0x11; rows[2]=0x11; rows[3]=0x11; rows[4]=0x11; rows[5]=0x0A; rows[6]=0x04;
	}
	else if(ch == 'B') {
		rows[0]=0x1E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x1E; rows[4]=0x11; rows[5]=0x11; rows[6]=0x1E;
	}
	else if(ch == 'Y') {
		rows[0]=0x11; rows[1]=0x11; rows[2]=0x0A; rows[3]=0x04; rows[4]=0x04; rows[5]=0x04; rows[6]=0x04;
	}
	else if(ch == 'X') {
		rows[0]=0x11; rows[1]=0x11; rows[2]=0x0A; rows[3]=0x04; rows[4]=0x0A; rows[5]=0x11; rows[6]=0x11;
	}
	else if(ch == 'D') {
		rows[0]=0x1E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x11; rows[4]=0x11; rows[5]=0x11; rows[6]=0x1E;
	}
	else if(ch == 'E') {
		rows[0]=0x1F; rows[1]=0x10; rows[2]=0x10; rows[3]=0x1E; rows[4]=0x10; rows[5]=0x10; rows[6]=0x1F;
	}
	else if(ch == 'N') {
		rows[0]=0x11; rows[1]=0x19; rows[2]=0x15; rows[3]=0x13; rows[4]=0x11; rows[5]=0x11; rows[6]=0x11;
	}
	else if(ch == 'G') {
		rows[0]=0x0E; rows[1]=0x11; rows[2]=0x10; rows[3]=0x17; rows[4]=0x11; rows[5]=0x11; rows[6]=0x0E;
	}
	else if(ch == 'U') {
		rows[0]=0x11; rows[1]=0x11; rows[2]=0x11; rows[3]=0x11; rows[4]=0x11; rows[5]=0x11; rows[6]=0x0E;
	}
	else if(ch == 'L') {
		rows[0]=0x10; rows[1]=0x10; rows[2]=0x10; rows[3]=0x10; rows[4]=0x10; rows[5]=0x10; rows[6]=0x1F;
	}
	else if(ch == 'W') {
		rows[0]=0x11; rows[1]=0x11; rows[2]=0x11; rows[3]=0x15; rows[4]=0x15; rows[5]=0x1B; rows[6]=0x11;
	}
	else if(ch == '.') {
		rows[5] = 0x04; rows[6] = 0x04;
	}
	for(gy = 0; gy < 7; ++gy) {
		for(gx = 0; gx < 5; ++gx) {
			if(rows[gy] & (0x10 >> gx)) {
				plotHud(fb, w, h, x + gx, y + gy, c);
			}
		}
	}
}

static void drawStr(UWORD *fb, int w, int h, int x, int y, const char *s, UWORD c) {
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
	char line[128];
	UWORD white = 0xFFFF, red = 0xF800, yellow = 0xFFE0;
	int y = 1;
	ULONG chipU = aceHostChipUsed(), chipB = aceHostChipBudget();
	ULONG fastU = aceHostFastUsed(), fastB = aceHostFastBudget();
	int over = aceHostChipOverBudget() || aceHostFastOverBudget();

	snprintf(line, sizeof(line), "%s %s",
		aceHostMachineName(),
		aceHostMemMode() == ACE_HOST_MEM_VIRTUAL ? "VIRTUAL" : "STRICT");
	drawStr(pFb, width, height, 2, y, line, white);
	y += 8;

	snprintf(line, sizeof(line), "CHIP %lu/%luK",
		(unsigned long)(chipU / 1024u), (unsigned long)(chipB / 1024u));
	drawStr(pFb, width, height, 2, y, line, over && aceHostChipOverBudget() ? red : white);
	bar(pFb, width, height, 140, y, 80, 7, chipU, chipB ? chipB : 1, aceHostChipOverBudget());
	y += 8;

	snprintf(line, sizeof(line), "FAST %lu/%luK",
		(unsigned long)(fastU / 1024u), (unsigned long)(fastB / 1024u));
	drawStr(pFb, width, height, 2, y, line, aceHostFastOverBudget() ? red : white);
	bar(pFb, width, height, 140, y, 80, 7, fastU, fastB ? fastB : 1, aceHostFastOverBudget());
	y += 8;

	if(over) {
		LONG dChip = (LONG)chipU - (LONG)chipB;
		snprintf(line, sizeof(line), "OVER +%ldK", (long)(dChip > 0 ? dChip / 1024 : ((LONG)fastU - (LONG)fastB) / 1024));
		drawStr(pFb, width, height, 2, y, line, red);
		y += 8;
	}

	snprintf(line, sizeof(line), "Y%u X%u DMA%03X %s COP%08lX",
		chipsetVpos(), chipsetHpos(), chipsetDmacon(),
		chipsetBlitBusyPeek() ? "BBUSY" : "BIDLE",
		(unsigned long)chipsetCopperPc());
	drawStr(pFb, width, height, 2, y, line, yellow);
	y += 8;
	snprintf(line, sizeof(line), "LN%u CW%u BLIT%u %s",
		chipsetLinesLastFrame(),
		chipsetLastCopWaitY(), chipsetBlitSlotsLastFrame(),
		chipsetTimingOk() ? "OK" : "BAD");
	drawStr(pFb, width, height, 2, y, line, chipsetTimingOk() ? white : red);

	if(aceHostHudFull()) {
		const UBYTE *dma = chipsetLastLineDma();
		int i;
		y += 10;
		drawStr(pFb, width, height, 2, y, "DMA", white);
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
				default: break;
			}
			plotHud(pFb, width, height, 2 + i, y, col);
			plotHud(pFb, width, height, 2 + i, y + 1, col);
		}
		{
			char cop[512];
			chipsetCopperDisasm(cop, sizeof(cop), 8);
			y += 4;
			{
				char *p = cop, *nl;
				while(*p && y < height - 8) {
					nl = strchr(p, '\n');
					if(nl) {
						*nl = 0;
					}
					drawStr(pFb, width, height, 2, y, p, white);
					y += 8;
					if(!nl) {
						break;
					}
					p = nl + 1;
				}
			}
		}
	}
}
