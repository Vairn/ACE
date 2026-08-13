#ifndef EXEC_TYPES_H
#define EXEC_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t  UBYTE;
typedef uint16_t UWORD;
typedef uint32_t ULONG;
typedef int8_t   BYTE;
typedef int16_t  WORD;
typedef int32_t  LONG;
typedef uint32_t FLOAT_PAD;
typedef int16_t  BOOL;
typedef char     TEXT;

/* 32-bit chip-bus pointers so struct Custom matches Amiga register offsets.
 * Allocations live in a low-4GB window; CPU pointer writes truncate safely. */
typedef uint32_t APTR;
typedef uint32_t CPTR;
typedef char *STRPTR;
typedef const char *CONST_STRPTR;
typedef UBYTE *PLANEPTR;

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef CONST
#define CONST const
#endif

#ifndef VOID
#define VOID void
#endif

#ifndef FAR
#define FAR
#endif

#ifndef CHIP
#define CHIP
#endif

#ifndef REGISTER
#define REGISTER
#endif

#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
