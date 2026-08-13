#ifndef GRAPHICS_GFX_H
#define GRAPHICS_GFX_H

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BMF_CLEAR        (1 << 0)
#define BMF_DISPLAYABLE  (1 << 1)
#define BMF_INTERLEAVED  (1 << 2)
#define BMF_STANDARD     (1 << 3)
#define BMF_MINPLANES    (1 << 4)

struct BitMap {
	UWORD BytesPerRow;
	UWORD Rows;
	UBYTE Flags;
	UBYTE Depth;
	UWORD pad;
	PLANEPTR Planes[8];
};

#ifdef __cplusplus
}
#endif

#endif
