#ifndef GRAPHICS_GFXBASE_H
#define GRAPHICS_GFXBASE_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <graphics/gfx.h>
#include <graphics/view.h>

#ifdef __cplusplus
extern "C" {
#endif

struct GfxBase {
	struct Library *gb_LibNode_pad;
	struct View *ActiView;
	struct copinit *copinit;
	UWORD *cia;
	UWORD DisplayFlags;
	BYTE NormalDisplayRows;
	BYTE NormalDisplayColumns;
};

#ifdef __cplusplus
}
#endif

#endif
