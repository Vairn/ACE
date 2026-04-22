/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_MANAGERS_VIEWPORT_VIEWPORT_SCROLL_H_
#define _ACE_MANAGERS_VIEWPORT_VIEWPORT_SCROLL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ace/types.h>
#include <ace/utils/extview.h>

/**
 * Distance (in low-res fetch clock units) from uwDDfStrt to uwDDfStop for the
 * display window described by pView. Matches simplebuffer / scrollbuffer DDF.
 */
UWORD viewportCalcDdfStep(const tView *pView, UBYTE ubFmode);

/**
 * Horizontal scroll: fills BPLCON1 word (both playfields same shift) and
 * bitplane byte offset for one row from the left edge of scrollable bitmap.
 *
 * @param pVPort          Active viewport (resolution, optional AGA FMODE).
 * @param uwScrollXPixels Camera X in pixels (integer or rounded subpixel).
 * @param pUwBplcon1      OUT: value for custom.bplcon1.
 * @param pUlBplByteOffs  OUT: byte offset added to each bitplane ptr.
 */
void viewportCalcBplScrollX(
	const tVPort *pVPort, UWORD uwScrollXPixels,
	UWORD *pUwBplcon1, ULONG *pUlBplByteOffs
);

#ifdef __cplusplus
}
#endif

#endif // _ACE_MANAGERS_VIEWPORT_VIEWPORT_SCROLL_H_
