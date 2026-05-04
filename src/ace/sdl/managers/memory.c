/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/managers/memory.h>

ULONG memAvail(ULONG ulFlags) {
	(void)ulFlags;
	return 0; // TODO implement
}

UBYTE memIsChip(UNUSED_ARG const void *pMem) {
	return 0;
}

ULONG memGetChipSize(void) {
	return 2 * 1024 * 1024;
}
