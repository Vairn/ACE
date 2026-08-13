#ifndef GRAPHICS_VIEW_H
#define GRAPHICS_VIEW_H

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct View {
	struct ViewPort *ViewPort;
	struct cprlist *LOFCprList;
	struct cprlist *SHFCprList;
	WORD DyOffset;
	WORD DxOffset;
	UWORD Modes;
};

struct ViewPort {
	struct ViewPort *Next;
	struct ColorMap *ColorMap;
	UWORD DWidth;
	UWORD DHeight;
	WORD DxOffset;
	WORD DyOffset;
	UWORD Modes;
};

struct copinit {
	UWORD copwait[2];
};

#ifdef __cplusplus
}
#endif

#endif
