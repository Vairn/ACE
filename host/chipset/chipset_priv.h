#ifndef ACE_HOST_CHIPSET_PRIV_H
#define ACE_HOST_CHIPSET_PRIV_H

#include <ace_host/chipset.h>
#include <ace/utils/custom.h>
#include <ace/managers/system.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>

struct Custom;
struct _tCia;

extern struct Custom *g_pHostCustom;
extern tCia *g_pHostCia[2];

void blitterInit(void);
void blitterStart(UWORD uwBltSize);
void blitterUseSlot(void);
int blitterBusy(void);
void blitterFinishNow(void);

void ciaInit(tCia *pCiaA, tCia *pCiaB);
void ciaRunEclockTick(void);
void ciaSetPraFire(UBYTE ubKeepHigh, UBYTE ubSetLow);
void ciaWriteIcr(int cia, UBYTE val);
UBYTE ciaReadIcr(int cia);
void ciaInjectKey(UBYTE ubRawKey);
void ciaPollKbd(void);

void paulaInit(void);
void paulaShutdown(void);
void paulaDmaSlot(int ch);
void paulaMix(short *pOut, int nFrames);
void paulaOnDmaEnable(UWORD uwOld, UWORD uwNew);
void paulaLineTick(void);

void aceHostDispatchInts(UWORD uwPending);
void aceHostFireCia(UBYTE ubCia, UBYTE ubBit);
tKeyInputHandler aceHostKeyHandler(void);
void aceHostOnVblank(void);
int aceHostPollQuit(void);

void aceHostHudDraw(UWORD *pFb, int width, int height);
void aceHostSdlInit(int isPal);
void aceHostSdlShutdown(void);
void aceHostSdlPresent(const UWORD *pFb, int width, int height);
void aceHostSdlPump(void);
void aceHostSdlApplyInput(void);
int aceHostHudFull(void);
int aceHostHudEnabled(void);

#endif
