#include <ace_host/chipset.h>
#include "host_os.h"
#include <exec/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN8(n) (((n) + 7u) & ~7u)
#define MAX_SPANS 4096

typedef struct tSpan {
	ULONG ulOffs;
	ULONG ulSize;
	ULONG ulFlags;
	struct tSpan *pNext;
} tSpan;

typedef struct tPool {
	UBYTE *pBase;
	ULONG ulMapped;
	ULONG ulSize;
	ULONG ulBudget;
	ULONG ulUsed;
	ULONG ulPeak;
	int isChip;
	int isDma;
	tSpan *pFree;
	tSpan *pAlloc;
} tPool;

static UBYTE *s_pBus;
static tPool s_sChip;
static tPool s_sFast;
static tAceHostMachine s_eMachine;
static tAceHostMemMode s_eMode;
static const char *s_szMachine = "A500_512_512";
static int s_isOverBanner;
static int s_isChipOver;
static int s_isFastOver;

static tSpan s_spans[MAX_SPANS];
static tSpan *s_spanFree;
static int s_spanInited;

static void spanPoolInit(void) {
	unsigned i;
	for(i = 0; i < MAX_SPANS - 1; ++i) {
		s_spans[i].pNext = &s_spans[i + 1];
	}
	s_spans[MAX_SPANS - 1].pNext = 0;
	s_spanFree = s_spans;
	s_spanInited = 1;
}

static void spanRelease(tSpan *p) {
	if(!p) {
		return;
	}
	p->pNext = s_spanFree;
	s_spanFree = p;
}

static tSpan *spanAlloc(ULONG ulOffs, ULONG ulSize, ULONG ulFlags) {
	tSpan *p;
	if(!s_spanInited) {
		spanPoolInit();
	}
	p = s_spanFree;
	if(!p) {
		fprintf(stderr, "[ACE_HOST] ERR: allocator span slab exhausted\n");
		return 0;
	}
	s_spanFree = p->pNext;
	p->ulOffs = ulOffs;
	p->ulSize = ulSize;
	p->ulFlags = ulFlags;
	p->pNext = 0;
	return p;
}

static void spanFreeList(tSpan **pp) {
	while(*pp) {
		tSpan *p = *pp;
		*pp = p->pNext;
		spanRelease(p);
	}
}

static void insertFreeSorted(tPool *pPool, tSpan *pNew) {
	tSpan **pp = &pPool->pFree;
	tSpan *pPrev = 0;
	while(*pp && (*pp)->ulOffs < pNew->ulOffs) {
		pPrev = *pp;
		pp = &(*pp)->pNext;
	}
	pNew->pNext = *pp;
	*pp = pNew;
	if(pNew->pNext && pNew->ulOffs + pNew->ulSize == pNew->pNext->ulOffs) {
		tSpan *pN = pNew->pNext;
		pNew->ulSize += pN->ulSize;
		pNew->pNext = pN->pNext;
		spanRelease(pN);
	}
	if(pPrev && pPrev->ulOffs + pPrev->ulSize == pNew->ulOffs) {
		pPrev->ulSize += pNew->ulSize;
		pPrev->pNext = pNew->pNext;
		spanRelease(pNew);
	}
}

static ULONG largestFree(const tPool *pPool) {
	ULONG ulBest = 0;
	for(tSpan *p = pPool->pFree; p; p = p->pNext) {
		if(p->ulSize > ulBest) {
			ulBest = p->ulSize;
		}
	}
	return ulBest;
}

static void poolInit(tPool *pPool, UBYTE *pBase, ULONG ulSize, ULONG ulBudget, int isChip, int isDma) {
	memset(pPool, 0, sizeof(*pPool));
	pPool->pBase = pBase;
	pPool->ulMapped = ulSize;
	pPool->ulSize = ulSize;
	pPool->ulBudget = ulBudget;
	pPool->isChip = isChip;
	pPool->isDma = isDma;
	pPool->pFree = spanAlloc(0, ulSize, 0);
	if(!pPool->pFree) {
		fprintf(stderr, "[ACE_HOST] ERR: pool init failed\n");
		exit(1);
	}
}

static int growPool(tPool *pPool, ULONG ulNeed) {
	ULONG ulNew = pPool->ulSize + ALIGN8(ulNeed);
	if(ulNew > pPool->ulMapped) {
		ulNew = pPool->ulMapped;
	}
	if(pPool->isChip && ulNew > ACE_HOST_AGNUS_CHIP_MAX) {
		ulNew = ACE_HOST_AGNUS_CHIP_MAX;
	}
	if(ulNew <= pPool->ulSize) {
		return 0;
	}
	{
		tSpan *pNew = spanAlloc(pPool->ulSize, ulNew - pPool->ulSize, 0);
		if(!pNew) {
			return 0;
		}
		insertFreeSorted(pPool, pNew);
	}
	pPool->ulSize = ulNew;
	return 1;
}

static void *poolAlloc(tPool *pPool, ULONG ulSize, ULONG ulFlags) {
	ULONG ulNeed = ALIGN8(ulSize);
	tSpan **pp = &pPool->pFree;
	while(*pp && (*pp)->ulSize < ulNeed) {
		pp = &(*pp)->pNext;
	}
	if(!*pp) {
		if(s_eMode != ACE_HOST_MEM_VIRTUAL || !growPool(pPool, ulNeed)) {
			return 0;
		}
		pp = &pPool->pFree;
		while(*pp && (*pp)->ulSize < ulNeed) {
			pp = &(*pp)->pNext;
		}
		if(!*pp) {
			return 0;
		}
	}
	{
		tSpan *p = *pp;
		ULONG ulOffs = p->ulOffs;
		if(p->ulSize == ulNeed) {
			*pp = p->pNext;
			spanRelease(p);
		}
		else {
			p->ulOffs += ulNeed;
			p->ulSize -= ulNeed;
		}
		{
			tSpan *pA = spanAlloc(ulOffs, ulNeed, ulFlags);
			if(!pA) {
				return 0;
			}
			pA->pNext = pPool->pAlloc;
			pPool->pAlloc = pA;
		}
		pPool->ulUsed += ulNeed;
		if(pPool->ulUsed > pPool->ulPeak) {
			pPool->ulPeak = pPool->ulUsed;
		}
		if(pPool->ulUsed > pPool->ulBudget) {
			if(pPool->isChip) {
				s_isChipOver = 1;
			}
			else {
				s_isFastOver = 1;
			}
			if(!s_isOverBanner) {
				s_isOverBanner = 1;
				fprintf(stderr,
					"[ACE_HOST] OVER-BUDGET %s: used %lu / budget %lu (%s, VIRTUAL)\n",
					pPool->isChip ? "CHIP" : "FAST",
					(unsigned long)pPool->ulUsed,
					(unsigned long)pPool->ulBudget,
					s_szMachine
				);
			}
		}
		if(ulFlags & MEMF_CLEAR) {
			memset(pPool->pBase + ulOffs, 0, ulNeed);
		}
		return pPool->pBase + ulOffs;
	}
}

static void poolFree(tPool *pPool, void *pMem, ULONG ulSize) {
	UBYTE *p = (UBYTE *)pMem;
	ULONG ulOffs;
	tSpan **pp;
	(void)ulSize;
	if(p < pPool->pBase) {
		return;
	}
	ulOffs = (ULONG)(p - pPool->pBase);
	pp = &pPool->pAlloc;
	while(*pp && (*pp)->ulOffs != ulOffs) {
		pp = &(*pp)->pNext;
	}
	if(!*pp) {
		return;
	}
	{
		tSpan *pA = *pp;
		*pp = pA->pNext;
		pPool->ulUsed -= pA->ulSize;
		pA->pNext = 0;
		insertFreeSorted(pPool, pA);
	}
}

static const char *machineName(tAceHostMachine e) {
	switch(e) {
		case ACE_HOST_MACHINE_A500: return "A500";
		case ACE_HOST_MACHINE_A500_512_512: return "A500_512_512";
		case ACE_HOST_MACHINE_A500_1MB: return "A500_1MB";
		case ACE_HOST_MACHINE_A600: return "A600";
		case ACE_HOST_MACHINE_A1200: return "A1200";
		case ACE_HOST_MACHINE_A1200_4MB: return "A1200_4MB";
		case ACE_HOST_MACHINE_A1200_8MB: return "A1200_8MB";
		case ACE_HOST_MACHINE_A4000: return "A4000";
		default: return "CUSTOM";
	}
}

void aceHostMemInit(
	tAceHostMachine eMachine, tAceHostMemMode eMode, ULONG ulChipBytes, ULONG ulFastBytes
) {
	ULONG ulChip = ulChipBytes;
	ULONG ulFast = ulFastBytes;
	int fastInWindow = 0;

	s_eMachine = eMachine;
	s_eMode = eMode;
	s_szMachine = machineName(eMachine);
	s_isOverBanner = 0;
	s_isChipOver = 0;
	s_isFastOver = 0;
	spanPoolInit();

	switch(eMachine) {
		case ACE_HOST_MACHINE_A500:
			ulChip = 512u * 1024u; ulFast = 0; break;
		case ACE_HOST_MACHINE_A500_512_512:
			ulChip = 512u * 1024u; ulFast = 512u * 1024u; fastInWindow = 1; break;
		case ACE_HOST_MACHINE_A500_1MB:
			ulChip = 1024u * 1024u; ulFast = 0; break;
		case ACE_HOST_MACHINE_A600:
			ulChip = 1024u * 1024u; ulFast = 0; break;
		case ACE_HOST_MACHINE_A1200:
			ulChip = 2048u * 1024u; ulFast = 0; break;
		case ACE_HOST_MACHINE_A1200_4MB:
			ulChip = 2048u * 1024u; ulFast = 4096u * 1024u; break;
		case ACE_HOST_MACHINE_A1200_8MB:
			ulChip = 2048u * 1024u; ulFast = 8192u * 1024u; break;
		case ACE_HOST_MACHINE_A4000:
			ulChip = 2048u * 1024u; ulFast = 4096u * 1024u; break;
		case ACE_HOST_MACHINE_CUSTOM:
		default:
			if(!ulChip) {
				ulChip = 512u * 1024u;
			}
			/* Trapdoor FAST at 0xC00000 fits under custom ($DFF000). */
			if(ulFast && ulFast <= (ACE_HOST_CUSTOM_OFFS - ACE_HOST_A500_FAST_OFFS)) {
				fastInWindow = 1;
			}
			break;
	}

	s_pBus = (UBYTE *)hostOsMapLow(ACE_HOST_BUS_SIZE);
	if(!s_pBus) {
		fprintf(stderr, "[ACE_HOST] ERR: failed to map 16MiB chip bus\n");
		exit(1);
	}
	memset(s_pBus, 0, ACE_HOST_BUS_SIZE);

	poolInit(&s_sChip, s_pBus, ulChip, ulChip, 1, 1);
	s_sChip.ulMapped = ACE_HOST_AGNUS_CHIP_MAX;

	if(ulFast || eMode == ACE_HOST_MEM_VIRTUAL) {
		ULONG ulFastMap = ulFast;
		if(eMode == ACE_HOST_MEM_VIRTUAL) {
			ulFastMap = ulFast > (16u * 1024u * 1024u) ? ulFast : (16u * 1024u * 1024u);
		}
		if(!ulFastMap) {
			ulFastMap = 16u * 1024u * 1024u;
		}
		if(fastInWindow && eMode != ACE_HOST_MEM_VIRTUAL) {
			poolInit(
				&s_sFast, s_pBus + ACE_HOST_A500_FAST_OFFS, ulFast, ulFast, 0, 0
			);
			s_sFast.ulMapped = ulFast;
		}
		else {
			void *pFast = hostOsMapLow(ulFastMap);
			if(!pFast) {
				fprintf(stderr, "[ACE_HOST] ERR: FAST pool mapping failed\n");
				exit(1);
			}
			memset(pFast, 0, ulFastMap);
			poolInit(&s_sFast, (UBYTE *)pFast, ulFast ? ulFast : 0, ulFast, 0, 0);
			s_sFast.ulMapped = ulFastMap;
			if(!ulFast && eMode == ACE_HOST_MEM_VIRTUAL) {
				/* No FAST budget; keep empty free list until first virtual grow. */
				spanFreeList(&s_sFast.pFree);
				s_sFast.ulSize = 0;
			}
		}
	}
	else {
		memset(&s_sFast, 0, sizeof(s_sFast));
	}

	fprintf(stderr,
		"[ACE_HOST] machine %s  CHIP %luK  FAST %luK  %s  bus=%p\n",
		s_szMachine,
		(unsigned long)(ulChip / 1024u),
		(unsigned long)(ulFast / 1024u),
		eMode == ACE_HOST_MEM_VIRTUAL ? "VIRTUAL" : "STRICT",
		(void *)s_pBus
	);
}

void aceHostMemShutdown(void) {
	spanFreeList(&s_sChip.pFree);
	spanFreeList(&s_sChip.pAlloc);
	spanFreeList(&s_sFast.pFree);
	spanFreeList(&s_sFast.pAlloc);
	if(s_sFast.pBase && (s_sFast.pBase < s_pBus || s_sFast.pBase >= s_pBus + ACE_HOST_BUS_SIZE)) {
		hostOsUnmapLow(s_sFast.pBase, s_sFast.ulSize);
	}
	hostOsUnmapLow(s_pBus, ACE_HOST_BUS_SIZE);
	s_pBus = 0;
}

UBYTE *aceHostBusBase(void) {
	return s_pBus;
}

ULONG aceHostPtrToBus(const void *p) {
	if(!p || !s_pBus) {
		return 0;
	}
	if((const UBYTE *)p >= s_pBus && (const UBYTE *)p < s_pBus + ACE_HOST_BUS_SIZE) {
		return (ULONG)((const UBYTE *)p - s_pBus);
	}
	return (ULONG)(uintptr_t)p;
}

void *aceHostBusToPtr(ULONG ulBus) {
	if(!s_pBus) {
		return 0;
	}
	if(ulBus < ACE_HOST_BUS_SIZE) {
		return s_pBus + ulBus;
	}
	return (void *)(uintptr_t)ulBus;
}

int aceHostIsChipAddr(ULONG ulAddr) {
	void *p;
	if(!ulAddr || !s_pBus) {
		return 0;
	}
	/* CPU writes store truncated host pointers; copper may store either. */
	if(ulAddr < s_sChip.ulSize) {
		return 1;
	}
	p = (void *)(uintptr_t)ulAddr;
	return aceHostIsChipPtr(p);
}

int aceHostIsChipPtr(const void *p) {
	return p && s_sChip.pBase &&
		(const UBYTE *)p >= s_sChip.pBase &&
		(const UBYTE *)p < s_sChip.pBase + s_sChip.ulSize;
}

const char *aceHostMachineName(void) { return s_szMachine; }
tAceHostMachine aceHostMachine(void) { return s_eMachine; }
tAceHostMemMode aceHostMemMode(void) { return s_eMode; }
ULONG aceHostChipBudget(void) { return s_sChip.ulBudget; }
ULONG aceHostFastBudget(void) { return s_sFast.ulBudget; }
ULONG aceHostChipSize(void) { return s_sChip.ulSize; }
ULONG aceHostFastSize(void) { return s_sFast.ulSize; }
ULONG aceHostChipUsed(void) { return s_sChip.ulUsed; }
ULONG aceHostFastUsed(void) { return s_sFast.ulUsed; }
ULONG aceHostChipPeak(void) { return s_sChip.ulPeak; }
ULONG aceHostFastPeak(void) { return s_sFast.ulPeak; }
ULONG aceHostChipLargestFree(void) { return largestFree(&s_sChip); }
ULONG aceHostFastLargestFree(void) { return largestFree(&s_sFast); }
int aceHostChipOverBudget(void) { return s_isChipOver; }
int aceHostFastOverBudget(void) { return s_isFastOver; }
int aceHostOverBudgetBanner(void) { return s_isOverBanner; }
void aceHostClearOverBudgetBanner(void) { s_isOverBanner = 0; }

unsigned aceHostAllocCount(void) {
	unsigned n = 0;
	for(tSpan *p = s_sChip.pAlloc; p; p = p->pNext) { ++n; }
	for(tSpan *p = s_sFast.pAlloc; p; p = p->pNext) { ++n; }
	return n;
}

int aceHostAllocAt(unsigned idx, tAceHostAllocInfo *pOut) {
	unsigned n = 0;
	tSpan *p;
	for(p = s_sChip.pAlloc; p; p = p->pNext, ++n) {
		if(n == idx) {
			pOut->ulAddr = (ULONG)(uintptr_t)(s_sChip.pBase + p->ulOffs);
			pOut->ulSize = p->ulSize;
			pOut->ulFlags = p->ulFlags | MEMF_CHIP;
			pOut->isChip = 1;
			return 1;
		}
	}
	for(p = s_sFast.pAlloc; p; p = p->pNext, ++n) {
		if(n == idx) {
			pOut->ulAddr = (ULONG)(uintptr_t)(s_sFast.pBase + p->ulOffs);
			pOut->ulSize = p->ulSize;
			pOut->ulFlags = p->ulFlags | MEMF_FAST;
			pOut->isChip = 0;
			return 1;
		}
	}
	return 0;
}

APTR AllocMem(ULONG byteSize, ULONG attributes) {
	void *p = 0;
	if(!byteSize) {
		return 0;
	}
	if(attributes & MEMF_CHIP) {
		p = poolAlloc(&s_sChip, byteSize, attributes);
	}
	else if((attributes & MEMF_FAST) || s_sFast.pBase) {
		p = poolAlloc(&s_sFast, byteSize, attributes);
		if(!p && !(attributes & MEMF_FAST)) {
			p = poolAlloc(&s_sChip, byteSize, attributes);
		}
	}
	else {
		p = poolAlloc(&s_sChip, byteSize, attributes);
	}
	return (APTR)(uintptr_t)p;
}

void FreeMem(APTR memoryBlock, ULONG byteSize) {
	void *p = (void *)(uintptr_t)memoryBlock;
	if(!p) {
		return;
	}
	if(aceHostIsChipPtr(p)) {
		poolFree(&s_sChip, p, byteSize);
	}
	else {
		poolFree(&s_sFast, p, byteSize);
	}
}

ULONG AvailMem(ULONG attributes) {
	if(attributes & MEMF_LARGEST) {
		if(attributes & MEMF_CHIP) {
			return largestFree(&s_sChip);
		}
		if(s_sFast.pBase) {
			return largestFree(&s_sFast);
		}
		return largestFree(&s_sChip);
	}
	if(attributes & MEMF_CHIP) {
		return s_sChip.ulSize - s_sChip.ulUsed;
	}
	if(s_sFast.pBase) {
		return s_sFast.ulSize - s_sFast.ulUsed;
	}
	return s_sChip.ulSize - s_sChip.ulUsed;
}

ULONG TypeOfMem(APTR address) {
	void *p = (void *)(uintptr_t)address;
	if(aceHostIsChipPtr(p)) {
		return MEMF_CHIP;
	}
	if(s_sFast.pBase && (UBYTE *)p >= s_sFast.pBase &&
		(UBYTE *)p < s_sFast.pBase + s_sFast.ulSize) {
		return MEMF_FAST;
	}
	return MEMF_CHIP;
}

void CopyMem(CONST APTR source, APTR dest, ULONG size) {
	memcpy((void *)(uintptr_t)dest, (const void *)(uintptr_t)source, size);
}

void CopyMemQuick(CONST APTR source, APTR dest, ULONG size) {
	CopyMem(source, dest, size);
}
