/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/utils/tag.h>
#include <ace/managers/log.h>

tTagValue tagGet(void *pTagListPtr, va_list vaSrcList, tTag ulTagToFind, tTagValue ulOnNotFound) {
	if(pTagListPtr) {
		// TODO
		logWrite("ERR: Unimplemented in tagFindString()");
		return ulOnNotFound;
	}

	va_list vaWorkList;
	va_copy(vaWorkList, vaSrcList);
	tTag ulTagName;
	do {
		ulTagName = va_arg(vaWorkList, tTag);
		if(ulTagName == ulTagToFind) {
			tTagValue ulOut = va_arg(vaWorkList, tTagValue);
			va_end(vaWorkList);
			return ulOut;
		}
		else if(ulTagName == TAG_SKIP) {
			// Ignore this & next
			va_arg(vaWorkList, tTagValue);
			va_arg(vaWorkList, tTagValue);
			va_arg(vaWorkList, tTagValue);
		}
		else if(ulTagName == TAG_MORE) {
			// This list is finished - parse next one
			void *pNext = va_arg(vaWorkList, void*);
			va_end(vaWorkList);
			return tagGet(pNext, 0, ulTagToFind, ulOnNotFound);
		}
		else {
			va_arg(vaWorkList, tTagValue);
		}
	} while(ulTagName != TAG_DONE);
	va_end(vaWorkList);
	return ulOnNotFound;
}
