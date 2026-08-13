#ifndef CLIB_GRAPHICS_PROTOS_H
#define CLIB_GRAPHICS_PROTOS_H

#include <graphics/gfx.h>
#include <graphics/view.h>

#ifdef __cplusplus
extern "C" {
#endif

struct BitMap *AllocBitMap(ULONG sizex, ULONG sizey, ULONG depth, ULONG flags, CONST struct BitMap *friend_bitmap);
void FreeBitMap(struct BitMap *bm);
void InitBitMap(struct BitMap *bm, LONG depth, ULONG width, ULONG height);
LONG WaitTOF(void);
void LoadRGB4(struct ViewPort *vp, UWORD *colors, LONG count);
void OwnBlitter(void);
void DisownBlitter(void);
void WaitBlit(void);

#ifdef __cplusplus
}
#endif

#endif
