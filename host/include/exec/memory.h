#ifndef EXEC_MEMORY_H
#define EXEC_MEMORY_H

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMF_ANY           (0L)
#define MEMF_PUBLIC        (1L << 0)
#define MEMF_CHIP          (1L << 1)
#define MEMF_FAST          (1L << 2)
#define MEMF_LOCAL         (1L << 8)
#define MEMF_24BITDMA      (1L << 9)
#define MEMF_KICK          (1L << 10)
#define MEMF_CLEAR         (1L << 16)
#define MEMF_LARGEST       (1L << 17)
#define MEMF_REVERSE       (1L << 18)
#define MEMF_TOTAL         (1L << 19)
#define MEMF_NO_EXPUNGE    (1L << 31)

#ifdef __cplusplus
}
#endif

#endif
