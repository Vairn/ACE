#ifndef UTILITY_TAGITEM_H
#define UTILITY_TAGITEM_H
#include <exec/types.h>
#ifndef TAG_DONE
#define TAG_DONE   0UL
#define TAG_END    0UL
#define TAG_IGNORE 1UL
#define TAG_MORE   2UL
#define TAG_SKIP   3UL
#define TAG_USER   (1UL << 31)
#endif
#endif
