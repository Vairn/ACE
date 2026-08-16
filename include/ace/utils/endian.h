/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_UTILS_ENDIAN_H_
#define _ACE_UTILS_ENDIAN_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  Endian conversion functions.
 *  Allows convenient converting between Little and Big Endian.
 */

#include <ace/types.h>

#if defined(AMIGA) && !defined(ACE_HOST)
#define ENDIAN_NATIVE_BIG
#else
#define ENDIAN_NATIVE_LITTLE
#endif

#if defined(ENDIAN_NATIVE_BIG)
#define endianBigToNative16(x) (x)
#define endianBigToNative32(x) (x)
#define endianLittleToNative16(x) endianSwap16(x)
#define endianLittleToNative32(x) endianSwap32(x)
#define endianNativeToBig16(x) (x)
#define endianNativeToBig32(x) (x)
#define endianNativeToLittle16(x) endianSwap16(x)
#define endianNativeToLittle32(x) endianSwap32(x)
#elif defined(ENDIAN_NATIVE_LITTLE)
#define endianBigToNative16(x) endianSwap16(x)
#define endianBigToNative32(x) endianSwap32(x)
#define endianLittleToNative16(x) (x)
#define endianLittleToNative32(x) (x)
#define endianNativeToBig16(x) endianSwap16(x)
#define endianNativeToBig32(x) endianSwap32(x)
#define endianNativeToLittle16(x) (x)
#define endianNativeToLittle32(x) (x)
#else
#error "Unknown platform endianness!"
#endif

/**
 *  @brief Converts 16-bit endian value.
 *
 *  @param uwIn 16-bit value to be converted
 *  @return Supplied value.
 *
 *  @see endianSwap32()
 */
static inline UWORD endianSwap16(UWORD uwIn) {
	// TODO: _byteswap_ushort() on msvc
	return __builtin_bswap16(uwIn);
}

/**
 *  @brief Converts 32-bit endian value.
 *
 *  @param ulIn 32-bit value to be converted
 *  @return Supplied value.
 *
 *  @see endianSwap16()
 */
static inline ULONG endianSwap32(ULONG ulIn) {
	// TODO: _byteswap_ulong() on msvc
	return __builtin_bswap32(ulIn);
}

#ifdef __cplusplus
}
#endif

#endif // _ACE_UTILS_ENDIAN_H_