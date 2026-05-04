/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <proto/exec.h> // Bartman's compiler needs this
#include <proto/dos.h> // Bartman's compiler needs this
#include <clib/exec_protos.h> // AvailMem, TypeOfMem, etc.

ULONG memAvail(ULONG ulFlags) {
	return AvailMem(ulFlags);
}

/* _memAllocRls / _memFreeRls are implemented in common/managers/memory.c */

UBYTE memIsChip(const void *pMem) {
	ULONG ulOsType = TypeOfMem((void *)pMem);
	if(ulOsType & MEMF_FAST) {
		return 0;
	}
	return 1;
}

ULONG memGetChipSize(void) {
	return AvailMem(MEMF_CHIP);
}
