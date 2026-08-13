#ifndef ACE_HOST_CHIPSET_H
#define ACE_HOST_CHIPSET_H

#include <ace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ACE_HOST_BUS_SIZE 0x1000000u
#define ACE_HOST_CUSTOM_OFFS 0x00DFF000u
#define ACE_HOST_CIA_A_OFFS  0x00BFE001u
#define ACE_HOST_CIA_B_OFFS  0x00BFD000u
#define ACE_HOST_A500_FAST_OFFS 0x00C00000u
#define ACE_HOST_AGNUS_CHIP_MAX (2u * 1024u * 1024u)

#define ACE_HOST_SLOTS_PER_LINE 227
#define ACE_HOST_PAL_LINES 313
#define ACE_HOST_NTSC_LINES 263
#define ACE_HOST_FB_WIDTH 640
#define ACE_HOST_FB_LORES_WIDTH 320
#define ACE_HOST_FB_HEIGHT_PAL 256
#define ACE_HOST_FB_HEIGHT_NTSC 200

typedef enum tAceHostMachine {
	ACE_HOST_MACHINE_A500 = 0,
	ACE_HOST_MACHINE_A500_512_512,
	ACE_HOST_MACHINE_A500_1MB,
	ACE_HOST_MACHINE_A600,
	ACE_HOST_MACHINE_A1200,
	ACE_HOST_MACHINE_A1200_4MB,
	ACE_HOST_MACHINE_A1200_8MB,
	ACE_HOST_MACHINE_A4000,
	ACE_HOST_MACHINE_CUSTOM
} tAceHostMachine;

typedef enum tAceHostMemMode {
	ACE_HOST_MEM_STRICT = 0,
	ACE_HOST_MEM_VIRTUAL
} tAceHostMemMode;

typedef enum tAceHostDmaKind {
	ACE_HOST_DMA_IDLE = 0,
	ACE_HOST_DMA_REFRESH,
	ACE_HOST_DMA_DISK,
	ACE_HOST_DMA_AUDIO,
	ACE_HOST_DMA_SPRITE,
	ACE_HOST_DMA_BPL,
	ACE_HOST_DMA_COPPER,
	ACE_HOST_DMA_BLIT
} tAceHostDmaKind;

typedef struct tAceHostAllocInfo {
	ULONG ulAddr;
	ULONG ulSize;
	ULONG ulFlags;
	int isChip;
} tAceHostAllocInfo;

void aceHostMemInit(tAceHostMachine eMachine, tAceHostMemMode eMode, ULONG ulChipBytes, ULONG ulFastBytes);
void aceHostMemShutdown(void);
UBYTE *aceHostBusBase(void);
ULONG aceHostPtrToBus(const void *p);
void *aceHostBusToPtr(ULONG ulBus);
int aceHostIsChipAddr(ULONG ulAddr);
int aceHostIsChipPtr(const void *p);

const char *aceHostMachineName(void);
tAceHostMachine aceHostMachine(void);
tAceHostMemMode aceHostMemMode(void);
ULONG aceHostChipBudget(void);
ULONG aceHostFastBudget(void);
ULONG aceHostChipSize(void);
ULONG aceHostFastSize(void);
ULONG aceHostChipUsed(void);
ULONG aceHostFastUsed(void);
ULONG aceHostChipPeak(void);
ULONG aceHostFastPeak(void);
ULONG aceHostChipLargestFree(void);
ULONG aceHostFastLargestFree(void);
int aceHostChipOverBudget(void);
int aceHostFastOverBudget(void);
int aceHostOverBudgetBanner(void);
void aceHostClearOverBudgetBanner(void);

unsigned aceHostAllocCount(void);
int aceHostAllocAt(unsigned idx, tAceHostAllocInfo *pOut);

void aceHostBindCustom(void);
void chipsetInit(int isPal);
void chipsetShutdown(void);
void chipsetReset(void);
void chipsetSyncCpuWrites(void);
void chipsetOnVposRead(void);
void chipsetRunSlots(unsigned n);
void chipsetWaitBlit(void);
int chipsetBlitIsBusy(void);
int chipsetBlitBusyPeek(void);
void chipsetRunUntilVpos(UWORD uwY, int isExact);
void chipsetRunUntilVposGe(UWORD uwY);

UWORD chipsetVpos(void);
UWORD chipsetHpos(void);
UWORD chipsetDmacon(void);
ULONG chipsetCopperPc(void);
int chipsetIsPal(void);

UWORD *chipsetFramebuffer(int *pWidth, int *pHeight);
const UBYTE *chipsetLastLineDma(void);

void chipsetSetJoyDat(UWORD uwJoy0, UWORD uwJoy1);
void chipsetSetPotinp(UWORD uwPot);
void chipsetSetCiaPraFire(UBYTE ubPraMaskHigh, UBYTE ubPraMaskLow);
void chipsetInjectKey(UBYTE ubRawKey);
void chipsetRaiseInt(UWORD uwMask);

void chipsetCopperDisasm(char *pBuf, unsigned bufSize, unsigned maxInsns);

/* Host timing self-check / HUD. Lines per PAL frame should be 313. */
void chipsetSetTimingLog(int on);
int chipsetTimingLogEnabled(void);
UWORD chipsetLastCopWaitY(void);
unsigned chipsetBlitSlotsLastFrame(void);
int chipsetLinesLastFrame(void);
unsigned chipsetCopWaitHits(UWORD *pOut, unsigned maxOut);
int chipsetTimingOk(void);

void aceHostFireInts(void);
void aceHostPump(void);
void aceHostTick(void);
void aceHostCrashInit(void);

#ifdef __cplusplus
}
#endif

#endif
