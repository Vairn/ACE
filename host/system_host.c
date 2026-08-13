#include "chipset_priv.h"
#include "host_os.h"
#include <ace/managers/system.h>
#include <ace/managers/log.h>
#include <ace/managers/timer.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/misc_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct GfxBase *GfxBase;
static struct GfxBase s_sGfxBase;
static struct copinit s_sCopInit;
static struct Library s_sMiscBase;

typedef struct tHostDir {
	void *pOs;
	char szPath[260];
} tHostDir;

struct Library *OpenLibrary(CONST_STRPTR libName, ULONG version) {
	(void)version;
	if(libName && strstr((const char *)(uintptr_t)libName, "graphics")) {
		return (struct Library *)&s_sGfxBase;
	}
	if(libName && strstr((const char *)(uintptr_t)libName, "misc")) {
		return &s_sMiscBase;
	}
	return (struct Library *)&s_sGfxBase;
}

void CloseLibrary(struct Library *library) {
	(void)library;
}

APTR OpenResource(CONST_STRPTR resName) {
	(void)resName;
	return (APTR)(uintptr_t)&s_sMiscBase;
}

void Forbid(void) {}
void Permit(void) {}
void Disable(void) {}
void Enable(void) {}

STRPTR AllocMiscResource(ULONG unitNum, CONST_STRPTR name) {
	(void)unitNum;
	(void)name;
	return 0; /* success: not owned by anyone else */
}

void FreeMiscResource(ULONG unitNum) {
	(void)unitNum;
}

struct BitMap *AllocBitMap(ULONG sizex, ULONG sizey, ULONG depth, ULONG flags, CONST struct BitMap *friend_bitmap) {
	struct BitMap *bm;
	ULONG plane = ((sizex + 15) / 16) * 2 * sizey;
	ULONG i;
	(void)friend_bitmap;
	bm = (struct BitMap *)(uintptr_t)AllocMem(sizeof(struct BitMap), MEMF_CLEAR | MEMF_PUBLIC);
	if(!bm) {
		return 0;
	}
	bm->BytesPerRow = (UWORD)(((sizex + 15) / 16) * 2);
	bm->Rows = (UWORD)sizey;
	bm->Depth = (UBYTE)depth;
	bm->Flags = (UBYTE)flags;
	for(i = 0; i < depth && i < 8; ++i) {
		bm->Planes[i] = (PLANEPTR)(uintptr_t)AllocMem(plane, MEMF_CHIP | ((flags & BMF_CLEAR) ? MEMF_CLEAR : 0));
	}
	return bm;
}

void FreeBitMap(struct BitMap *bm) {
	ULONG i;
	ULONG plane;
	if(!bm) {
		return;
	}
	plane = (ULONG)bm->BytesPerRow * bm->Rows;
	for(i = 0; i < bm->Depth && i < 8; ++i) {
		if(bm->Planes[i]) {
			FreeMem((APTR)(uintptr_t)bm->Planes[i], plane);
		}
	}
	FreeMem((APTR)(uintptr_t)bm, sizeof(struct BitMap));
}

void InitBitMap(struct BitMap *bm, LONG depth, ULONG width, ULONG height) {
	bm->BytesPerRow = (UWORD)(((width + 15) / 16) * 2);
	bm->Rows = (UWORD)height;
	bm->Depth = (UBYTE)depth;
	bm->Flags = 0;
}

LONG WaitTOF(void) {
	chipsetRunUntilVpos(0, 1);
	chipsetRunUntilVpos(1, 0);
	return 0;
}

void LoadRGB4(struct ViewPort *vp, UWORD *colors, LONG count) {
	LONG i;
	(void)vp;
	for(i = 0; i < count && i < 32; ++i) {
		g_pHostCustom->color[i] = colors[i];
	}
}

void OwnBlitter(void) {}
void DisownBlitter(void) {}
void WaitBlit(void) { chipsetWaitBlit(); }

BPTR Lock(CONST_STRPTR name, LONG accessMode) {
	tHostDir *p;
	const char *sz = (const char *)(uintptr_t)name;
	(void)accessMode;
	if(!sz || !hostOsIsDir(sz)) {
		return 0;
	}
	p = (tHostDir *)calloc(1, sizeof(tHostDir));
	if(!p) {
		return 0;
	}
	strncpy(p->szPath, sz, sizeof(p->szPath) - 1);
	p->pOs = hostOsDirOpen(sz);
	if(!p->pOs) {
		free(p);
		return 0;
	}
	return (BPTR)p;
}

void UnLock(BPTR lock) {
	tHostDir *p = (tHostDir *)lock;
	if(!p) {
		return;
	}
	hostOsDirClose(p->pOs);
	free(p);
}

LONG Examine(BPTR lock, struct FileInfoBlock *fib) {
	tHostDir *p = (tHostDir *)lock;
	if(!p || !fib) {
		return DOSFALSE;
	}
	memset(fib, 0, sizeof(*fib));
	fib->fib_DirEntryType = 1;
	strncpy(fib->fib_FileName, p->szPath, 107);
	fib->fib_HostDir = p;
	return DOSTRUE;
}

LONG ExNext(BPTR lock, struct FileInfoBlock *fib) {
	tHostDir *p = (tHostDir *)lock;
	int isDir = 0;
	long sz = 0;
	if(!p || !fib) {
		return DOSFALSE;
	}
	if(!hostOsDirNext(p->pOs, fib->fib_FileName, 108, &isDir, &sz)) {
		return DOSFALSE;
	}
	fib->fib_DirEntryType = isDir ? 1 : -1;
	fib->fib_Size = (LONG)sz;
	return DOSTRUE;
}

BPTR CreateDir(CONST_STRPTR name) {
	const char *sz = (const char *)(uintptr_t)name;
	if(hostOsMkdir(sz)) {
		return Lock(name, ACCESS_READ);
	}
	return 0;
}

LONG IoErr(void) {
	return 0;
}

void aceHostDosInitGfxBase(void) {
	memset(&s_sGfxBase, 0, sizeof(s_sGfxBase));
	s_sCopInit.copwait[0] = 0xFFFF;
	s_sCopInit.copwait[1] = 0xFFFE;
	{
		struct copinit *pChip = (struct copinit *)(uintptr_t)AllocMem(
			sizeof(struct copinit), MEMF_CHIP | MEMF_CLEAR
		);
		if(pChip) {
			pChip->copwait[0] = 0xFFFF;
			pChip->copwait[1] = 0xFFFE;
			s_sGfxBase.copinit = pChip;
		}
		else {
			s_sGfxBase.copinit = &s_sCopInit;
		}
	}
	s_sGfxBase.NormalDisplayRows = (BYTE)127;
	s_sGfxBase.NormalDisplayColumns = 40;
	GfxBase = &s_sGfxBase;
}

//---------------------------------------------------------------------- SYSTEM

typedef struct tAceInterrupt {
	tAceIntHandler pHandler;
	void *pData;
} tAceInterrupt;

static tAceInterrupt s_pAceInterrupts[16];
static tAceInterrupt s_pAceCiaInterrupts[2][5];
static tKeyInputHandler s_cbKeyInputHandler;
static WORD s_wSystemUses = 1;
static WORD s_wSystemBlitterUses = 1;
static UWORD s_uwAceDmaCon;
static int s_isPal = 1;

#ifndef ACE_HOST_MACHINE_ENUM
#define ACE_HOST_MACHINE_ENUM ACE_HOST_MACHINE_A500_512_512
#endif
#ifndef ACE_HOST_MEM_MODE_ENUM
#define ACE_HOST_MEM_MODE_ENUM ACE_HOST_MEM_STRICT
#endif
#ifndef ACE_HOST_CHIP_SIZE
#define ACE_HOST_CHIP_SIZE 0
#endif
#ifndef ACE_HOST_FAST_SIZE
#define ACE_HOST_FAST_SIZE 0
#endif

void systemCreate(void) {
	aceHostMemInit(
		(tAceHostMachine)ACE_HOST_MACHINE_ENUM,
		(tAceHostMemMode)ACE_HOST_MEM_MODE_ENUM,
		(ULONG)ACE_HOST_CHIP_SIZE,
		(ULONG)ACE_HOST_FAST_SIZE
	);
	chipsetInit(s_isPal);
	aceHostDosInitGfxBase();
	aceHostSdlInit(s_isPal);
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
	s_wSystemUses = 1;
	s_uwAceDmaCon = 0;
	g_pCustom->dmacon = DMAF_SETCLR | DMAF_MASTER;
	chipsetSyncCpuWrites();
}

void systemDestroy(void) {
	aceHostSdlShutdown();
	chipsetShutdown();
	aceHostMemShutdown();
}

void systemKill(const char *szMsg) {
	fprintf(stderr, "[ACE_HOST] KILL: %s\n", szMsg ? szMsg : "");
	systemDestroy();
	exit(1);
}

void systemUse(void) { ++s_wSystemUses; }
void systemUnuse(void) { if(s_wSystemUses) { --s_wSystemUses; } }
UBYTE systemIsUsed(void) { return s_wSystemUses > 0; }

void systemGetBlitterFromOs(void) {
	if(s_wSystemBlitterUses) {
		--s_wSystemBlitterUses;
	}
}
void systemReleaseBlitterToOs(void) { ++s_wSystemBlitterUses; }
UBYTE systemBlitterIsReleasedToOs(void) { return s_wSystemBlitterUses > 0; }

void systemDump(void) {}

void systemSetKeyInputHandler(tKeyInputHandler cbKeyInputHandler) {
	s_cbKeyInputHandler = cbKeyInputHandler;
}

tKeyInputHandler aceHostKeyHandler(void) {
	return s_cbKeyInputHandler;
}

void systemSetInt(UBYTE ubIntNumber, tAceIntHandler pHandler, void *pIntData) {
	g_pCustom->intena = BV(ubIntNumber);
	chipsetSyncCpuWrites();
	if(!pHandler) {
		s_pAceInterrupts[ubIntNumber].pHandler = 0;
	}
	else {
		s_pAceInterrupts[ubIntNumber].pHandler = pHandler;
		s_pAceInterrupts[ubIntNumber].pData = pIntData;
		g_pCustom->intena = INTF_SETCLR | BV(ubIntNumber);
		chipsetSyncCpuWrites();
	}
}

void systemSetCiaInt(UBYTE ubCia, UBYTE ubIntBit, tAceIntHandler cbHandler, void *pIntData) {
	s_pAceCiaInterrupts[ubCia][ubIntBit].pHandler = cbHandler;
	s_pAceCiaInterrupts[ubCia][ubIntBit].pData = pIntData;
}

void systemSetCiaCr(UBYTE ubCia, UBYTE isCrB, UBYTE ubCrValue) {
	if(isCrB) {
		g_pCia[ubCia]->crb = ubCrValue;
	}
	else {
		g_pCia[ubCia]->cra = ubCrValue;
	}
}

void systemSetDmaBit(UBYTE ubDmaBit, UBYTE isEnabled) {
	systemSetDmaMask((UWORD)BV(ubDmaBit), isEnabled);
}

void systemSetDmaMask(UWORD uwDmaMask, UBYTE isEnabled) {
	if(isEnabled) {
		s_uwAceDmaCon |= uwDmaMask;
		g_pCustom->dmacon = DMAF_SETCLR | uwDmaMask;
	}
	else {
		s_uwAceDmaCon &= (UWORD)~uwDmaMask;
		g_pCustom->dmacon = uwDmaMask;
	}
	chipsetSyncCpuWrites();
}

void systemSetTimer(UBYTE ubCia, UBYTE ubTimer, UWORD uwTicks) {
	if(!ubTimer) {
		ciaSetTimerA(g_pCia[ubCia], uwTicks);
	}
	else {
		ciaSetTimerB(g_pCia[ubCia], uwTicks);
	}
}

void systemIdleBegin(void) { aceHostSdlPump(); }
void systemIdleEnd(void) {}

UBYTE systemGetVerticalBlankFrequency(void) { return s_isPal ? 50 : 60; }
UBYTE systemIsPal(void) { return (UBYTE)s_isPal; }
void systemCheckStack(void) {}
UWORD systemGetVersion(void) { return 40; }
UBYTE systemIsStartVolumeWritable(void) { return 1; }
void systemDisableCpuCaches(void) {}
void systemRestoreCpuCaches(void) {}

void aceHostDispatchInts(UWORD uwPending) {
	int i;
	UWORD ena = g_pCustom->intenar;
	if(!(ena & INTF_INTEN)) {
		return;
	}
	if((uwPending & INTF_VERTB) && (ena & INTF_VERTB)) {
		timerOnInterrupt();
	}
	for(i = 0; i < 14; ++i) {
		UWORD bit = (UWORD)(1u << i);
		if((uwPending & bit) && (ena & bit) && s_pAceInterrupts[i].pHandler) {
			s_pAceInterrupts[i].pHandler(g_pCustom, s_pAceInterrupts[i].pData);
		}
	}
	/* CIA-B timer A/B → EXTER */
	if((uwPending & INTF_EXTER) || 1) {
		int b;
		for(b = 0; b < 5; ++b) {
			if(s_pAceCiaInterrupts[CIA_B][b].pHandler) {
				/* Fired from CIA underflow via dedicated path below. */
			}
		}
	}
}

void aceHostFireCia(UBYTE ubCia, UBYTE ubBit) {
	if(s_pAceCiaInterrupts[ubCia][ubBit].pHandler) {
		s_pAceCiaInterrupts[ubCia][ubBit].pHandler(
			g_pCustom, s_pAceCiaInterrupts[ubCia][ubBit].pData
		);
	}
	if(ubCia == CIA_A) {
		g_pCustom->intreq = INTF_SETCLR | INTF_PORTS;
	}
	else {
		g_pCustom->intreq = INTF_SETCLR | INTF_EXTER;
	}
	chipsetSyncCpuWrites();
}

