#include "host_chrome.h"

/* Compact HQ2x/HQ3x: YUV-threshold neighbour mask + 3:1 / 1:1 RGB565 blends.
 * Same 3x3 pattern idea as Maxim Stepin's HQX, small enough to live in-tree. */

static UWORD px(const UWORD *s, int w, int h, int x, int y) {
	if(x < 0) {
		x = 0;
	}
	else if(x >= w) {
		x = w - 1;
	}
	if(y < 0) {
		y = 0;
	}
	else if(y >= h) {
		y = h - 1;
	}
	return s[y * w + x];
}

static int yuvDiff(UWORD a, UWORD b) {
	int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
	int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
	int y1 = ar * 76 + ag * 150 + ab * 29;
	int y2 = br * 76 + bg * 150 + bb * 29;
	int u1 = ab * 128 - ar * 43 - ag * 85;
	int u2 = bb * 128 - br * 43 - bg * 85;
	int v1 = ar * 128 - ag * 107 - ab * 21;
	int v2 = br * 128 - bg * 107 - bb * 21;
	int dy = y1 - y2;
	int du = u1 - u2;
	int dv = v1 - v2;
	if(dy < 0) {
		dy = -dy;
	}
	if(du < 0) {
		du = -du;
	}
	if(dv < 0) {
		dv = -dv;
	}
	return dy > 48 * 255 || du > 7 * 128 || dv > 6 * 128;
}

static UWORD mix2(UWORD a, UWORD b) {
	return (UWORD)(((a & 0xF7DE) >> 1) + ((b & 0xF7DE) >> 1) + (a & b & 0x0821));
}

static UWORD mix3a(UWORD a, UWORD b) {
	/* 3:1 */
	return mix2(mix2(a, a), b);
}

static void neighborhood(const UWORD *src, int w, int h, int x, int y, UWORD n[9]) {
	n[0] = px(src, w, h, x - 1, y - 1);
	n[1] = px(src, w, h, x, y - 1);
	n[2] = px(src, w, h, x + 1, y - 1);
	n[3] = px(src, w, h, x - 1, y);
	n[4] = px(src, w, h, x, y);
	n[5] = px(src, w, h, x + 1, y);
	n[6] = px(src, w, h, x - 1, y + 1);
	n[7] = px(src, w, h, x, y + 1);
	n[8] = px(src, w, h, x + 1, y + 1);
}

static int pattern(const UWORD n[9]) {
	int p = 0;
	UWORD e = n[4];
	if(yuvDiff(e, n[0])) {
		p |= 1;
	}
	if(yuvDiff(e, n[1])) {
		p |= 2;
	}
	if(yuvDiff(e, n[2])) {
		p |= 4;
	}
	if(yuvDiff(e, n[3])) {
		p |= 8;
	}
	if(yuvDiff(e, n[5])) {
		p |= 16;
	}
	if(yuvDiff(e, n[6])) {
		p |= 32;
	}
	if(yuvDiff(e, n[7])) {
		p |= 64;
	}
	if(yuvDiff(e, n[8])) {
		p |= 128;
	}
	return p;
}

void aceHostHq2x(const UWORD *src, int w, int h, UWORD *dst) {
	int x, y;
	int dw = w * 2;
	for(y = 0; y < h; ++y) {
		for(x = 0; x < w; ++x) {
			UWORD n[9];
			int pat;
			UWORD e, e0, e1, e2, e3;
			neighborhood(src, w, h, x, y, n);
			pat = pattern(n);
			e = n[4];
			e0 = e;
			e1 = e;
			e2 = e;
			e3 = e;
			if(!(pat & 0x0A) && (pat & 0x05)) {
				e0 = mix3a(e, n[3]);
			}
			else if((n[1] == n[3]) && (pat & 0x0A) == 0) {
				e0 = n[1];
			}
			else if(!(pat & 2) && (pat & 8)) {
				e0 = mix3a(e, n[1]);
			}
			else if(!(pat & 8) && (pat & 2)) {
				e0 = mix3a(e, n[3]);
			}

			if(!(pat & 0x12) && (pat & 0x04)) {
				e1 = mix3a(e, n[5]);
			}
			else if((n[1] == n[5]) && (pat & 0x12) == 0) {
				e1 = n[1];
			}
			else if(!(pat & 2) && (pat & 16)) {
				e1 = mix3a(e, n[1]);
			}
			else if(!(pat & 16) && (pat & 2)) {
				e1 = mix3a(e, n[5]);
			}

			if(!(pat & 0x48) && (pat & 0x20)) {
				e2 = mix3a(e, n[3]);
			}
			else if((n[7] == n[3]) && (pat & 0x48) == 0) {
				e2 = n[7];
			}
			else if(!(pat & 64) && (pat & 8)) {
				e2 = mix3a(e, n[7]);
			}
			else if(!(pat & 8) && (pat & 64)) {
				e2 = mix3a(e, n[3]);
			}

			if(!(pat & 0x50) && (pat & 0x80)) {
				e3 = mix3a(e, n[5]);
			}
			else if((n[7] == n[5]) && (pat & 0x50) == 0) {
				e3 = n[7];
			}
			else if(!(pat & 64) && (pat & 16)) {
				e3 = mix3a(e, n[7]);
			}
			else if(!(pat & 16) && (pat & 64)) {
				e3 = mix3a(e, n[5]);
			}

			dst[(y * 2) * dw + x * 2] = e0;
			dst[(y * 2) * dw + x * 2 + 1] = e1;
			dst[(y * 2 + 1) * dw + x * 2] = e2;
			dst[(y * 2 + 1) * dw + x * 2 + 1] = e3;
		}
	}
}

void aceHostHq3x(const UWORD *src, int w, int h, UWORD *dst) {
	int x, y;
	int dw = w * 3;
	for(y = 0; y < h; ++y) {
		for(x = 0; x < w; ++x) {
			UWORD n[9];
			int pat;
			UWORD e, o[9];
			int i;
			neighborhood(src, w, h, x, y, n);
			pat = pattern(n);
			e = n[4];
			for(i = 0; i < 9; ++i) {
				o[i] = e;
			}
			if((n[1] == n[3]) && n[1] != n[7] && n[3] != n[5]) {
				o[0] = n[1];
				o[1] = mix2(n[1], e);
				o[3] = mix2(n[3], e);
			}
			else {
				if(!(pat & 2)) {
					o[1] = mix3a(e, n[1]);
				}
				if(!(pat & 8)) {
					o[3] = mix3a(e, n[3]);
				}
				if(!(pat & 10)) {
					o[0] = mix3a(e, mix2(n[1], n[3]));
				}
			}
			if((n[1] == n[5]) && n[1] != n[7] && n[5] != n[3]) {
				o[2] = n[1];
				o[1] = mix2(n[1], e);
				o[5] = mix2(n[5], e);
			}
			else {
				if(!(pat & 2)) {
					o[1] = mix3a(e, n[1]);
				}
				if(!(pat & 16)) {
					o[5] = mix3a(e, n[5]);
				}
				if(!(pat & 18)) {
					o[2] = mix3a(e, mix2(n[1], n[5]));
				}
			}
			if((n[7] == n[3]) && n[7] != n[1] && n[3] != n[5]) {
				o[6] = n[7];
				o[7] = mix2(n[7], e);
				o[3] = mix2(n[3], e);
			}
			else {
				if(!(pat & 64)) {
					o[7] = mix3a(e, n[7]);
				}
				if(!(pat & 8)) {
					o[3] = mix3a(e, n[3]);
				}
				if(!(pat & 72)) {
					o[6] = mix3a(e, mix2(n[7], n[3]));
				}
			}
			if((n[7] == n[5]) && n[7] != n[1] && n[5] != n[3]) {
				o[8] = n[7];
				o[7] = mix2(n[7], e);
				o[5] = mix2(n[5], e);
			}
			else {
				if(!(pat & 64)) {
					o[7] = mix3a(e, n[7]);
				}
				if(!(pat & 16)) {
					o[5] = mix3a(e, n[5]);
				}
				if(!(pat & 80)) {
					o[8] = mix3a(e, mix2(n[7], n[5]));
				}
			}
			{
				int oy, ox;
				for(oy = 0; oy < 3; ++oy) {
					for(ox = 0; ox < 3; ++ox) {
						dst[(y * 3 + oy) * dw + x * 3 + ox] = o[oy * 3 + ox];
					}
				}
			}
		}
	}
}
