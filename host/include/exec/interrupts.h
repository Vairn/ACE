#ifndef EXEC_INTERRUPTS_H
#define EXEC_INTERRUPTS_H

#include <exec/types.h>
#include <exec/lists.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Interrupt {
	struct Node is_Node;
	APTR is_Data;
	void (*is_Code)(void);
};

#ifdef __cplusplus
}
#endif

#endif
