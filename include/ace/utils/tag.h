/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_UTILS_TAG_H_
#define _ACE_UTILS_TAG_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Util for handling AmigaOS tag list pattern.
 */

#include <stdarg.h>
#include <ace/types.h>
#ifdef ACE_HOST
#include <stdint.h>
typedef uintptr_t tTagValue;
typedef uintptr_t tTag;
#else
typedef ULONG tTagValue;
typedef ULONG tTag;
#endif

// This is implemented on AOS 2.0+ (utility/tagitem.h) and I could include it
// But I can't detect if we're building for 1.3. Even if I could do that
// I'm overriding type name for ACE convention, so I just define needed stuff.
// Ifdef is needed because of e.g. utils/bitmap.h includes graphics_protos.h
// which may (or may not on KS 1.3) in turn include tag defines.
#ifndef TAG_DONE
#define TAG_DONE   ((tTag)0)
#define TAG_END    ((tTag)0)
#define TAG_IGNORE ((tTag)1)
#define TAG_MORE   ((tTag)2)
#define TAG_SKIP   ((tTag)3)
#define TAG_USER   ((tTag)BV(31))
#endif // TAG_DONE

/**
 *  Finds and returns value of specified tag name from tag list.
 *  Tag list may be supplied as list or va_list.
 *  TODO: 1st arg doesn't work yet
 *  @param pTagListPtr  Pointer to tag list.
 *  @param vaSrcList    va_list containing alternating tags and values.
 *  @param ulTagToFind  Tag name, of which value should be returned.
 *  @param ulOnNotFound Value to be returned if tag was not found on list.
 *  @return ulOnNotFound if tag was not found, otherwise tag value.
 */
tTagValue tagGet(
	void *pTagListPtr, va_list vaSrcList, tTag ulTagToFind, tTagValue ulOnNotFound
);

#ifdef __cplusplus
}
#endif

#endif // _ACE_UTILS_TAG_H_
