#ifndef EXEC_LISTS_H
#define EXEC_LISTS_H

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Node {
	struct Node *ln_Succ;
	struct Node *ln_Pred;
	UBYTE ln_Type;
	BYTE ln_Pri;
	char *ln_Name;
};

struct List {
	struct Node *lh_Head;
	struct Node *lh_Tail;
	struct Node *lh_TailPred;
	UBYTE lh_Type;
	UBYTE lh_pad;
};

#ifdef __cplusplus
}
#endif

#endif
