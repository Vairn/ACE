#include "host_chrome.h"

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

void aceHostScale2x(const UWORD *src, int w, int h, UWORD *dst) {
	int x, y;
	int dw = w * 2;
	for(y = 0; y < h; ++y) {
		for(x = 0; x < w; ++x) {
			UWORD B = px(src, w, h, x, y - 1);
			UWORD D = px(src, w, h, x - 1, y);
			UWORD E = px(src, w, h, x, y);
			UWORD F = px(src, w, h, x + 1, y);
			UWORD H = px(src, w, h, x, y + 1);
			UWORD e0 = E, e1 = E, e2 = E, e3 = E;
			if(B != H && D != F) {
				e0 = (D == B) ? D : E;
				e1 = (B == F) ? F : E;
				e2 = (D == H) ? D : E;
				e3 = (H == F) ? F : E;
			}
			dst[(y * 2) * dw + (x * 2)] = e0;
			dst[(y * 2) * dw + (x * 2) + 1] = e1;
			dst[(y * 2 + 1) * dw + (x * 2)] = e2;
			dst[(y * 2 + 1) * dw + (x * 2) + 1] = e3;
		}
	}
}
