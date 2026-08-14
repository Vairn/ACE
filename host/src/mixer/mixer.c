#include <ace_host/mixer.h>
#include <ace/managers/system.h>
#include <ace/utils/custom.h>
#include <ace_host/chipset.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>
#include <stdint.h>
#include <string.h>

#ifndef ACE_HOST_MIXER_SW_CHANNELS
#define ACE_HOST_MIXER_SW_CHANNELS 3
#endif
#ifndef ACE_HOST_MIXER_PERIOD
#define ACE_HOST_MIXER_PERIOD 161
#endif
#ifndef ACE_HOST_MIXER_HW
#define ACE_HOST_MIXER_HW DMAF_AUD3
#endif

#define MIXER_PAL_CLOCK  3546895u
#define MIXER_NTSC_CLOCK 3579545u
#define MIXER_SW_CHANNELS ACE_HOST_MIXER_SW_CHANNELS

#define MIXER_CHAN_INACTIVE 0

typedef struct tMixerChannel {
	const BYTE *pSample;
	const BYTE *pLoop;
	ULONG ulRemaining;
	ULONG ulLoopLength;
	WORD wPriority;
	UWORD uwAge;
	WORD wStatus;
} tMixerChannel;

static tMixerChannel s_pCh[MIXER_SW_CHANNELS];
static BYTE *s_pEmpty;
static BYTE *s_pMix[2];
static ULONG s_ulHalf;
static UWORD s_uwPeriod;
static UWORD s_uwVolume = 64;
static UBYTE s_ubPlayIdx;
static UBYTE s_isRunning;
static UBYTE s_ownsHw;
static UBYTE s_ubHw;

static UBYTE mixerHwIndex(void) {
	UWORD uwHw = (UWORD)ACE_HOST_MIXER_HW;
	if(uwHw & DMAF_AUD3) {
		return 3;
	}
	if(uwHw & DMAF_AUD2) {
		return 2;
	}
	if(uwHw & DMAF_AUD1) {
		return 1;
	}
	return 0;
}

static UWORD mixerHwDma(void) {
	return (UWORD)(DMAF_AUD0 << s_ubHw);
}

static ULONG mixerHalfSize(UWORD uwPeriod, UWORD uwVidsys) {
	ULONG ulHalf;
	if(uwVidsys == MIX_NTSC) {
		ulHalf = (MIXER_NTSC_CLOCK / (ULONG)uwPeriod / 60u) & 65504u;
	}
	else {
		ulHalf = (MIXER_PAL_CLOCK / (ULONG)uwPeriod / 50u) & 65504u;
	}
	return ulHalf + 32u;
}

static UWORD mixerPeriodForVidsys(UWORD uwVidsys) {
	UWORD uwPeriod = (UWORD)ACE_HOST_MIXER_PERIOD;
	if(uwVidsys == MIX_NTSC) {
		return (UWORD)(((ULONG)uwPeriod * MIXER_NTSC_CLOCK) / MIXER_PAL_CLOCK);
	}
	return uwPeriod;
}

static BYTE mixerClamp(int nVal) {
	if(nVal > 127) {
		return 127;
	}
	if(nVal < -128) {
		return -128;
	}
	return (BYTE)nVal;
}

static int mixerSwIndexFromMask(UWORD uwMask) {
	if(uwMask & MIX_CH0) {
		return 0;
	}
	if(uwMask & MIX_CH1) {
		return 1;
	}
	if(uwMask & MIX_CH2) {
		return 2;
	}
	if(uwMask & MIX_CH3) {
		return 3;
	}
	return 0;
}

static void mixerProgramPaula(BYTE *pBuf) {
	g_pCustom->aud[s_ubHw].ac_ptr = (APTR)(uintptr_t)pBuf;
	g_pCustom->aud[s_ubHw].ac_len = (UWORD)(s_ulHalf / 2u);
	g_pCustom->aud[s_ubHw].ac_per = s_uwPeriod;
	g_pCustom->aud[s_ubHw].ac_vol = s_uwVolume;
	chipsetSyncCpuWrites();
}

static void mixerMixInto(BYTE *pDest) {
	int iCh;

	memset(pDest, 0, (size_t)s_ulHalf);

	for(iCh = 0; iCh < MIXER_SW_CHANNELS; ++iCh) {
		tMixerChannel *pCh = &s_pCh[iCh];
		ULONG ulLeft = s_ulHalf;
		const BYTE *pSrc;
		ULONG ulRem;

		if(pCh->wStatus == MIXER_CHAN_INACTIVE) {
			continue;
		}

		pCh->uwAge++;
		pSrc = pCh->pSample;
		ulRem = pCh->ulRemaining;

		while(ulLeft) {
			ULONG ulTake;
			ULONG j;

			if(ulRem == 0) {
				pCh->wStatus = (WORD)((UWORD)pCh->wStatus & 0xFF00);
				pSrc = pCh->pLoop;
				ulRem = pCh->ulLoopLength;
				if(pCh->wStatus == MIXER_CHAN_INACTIVE) {
					break;
				}
				if(ulRem == 0) {
					pCh->wStatus = MIXER_CHAN_INACTIVE;
					break;
				}
			}

			ulTake = ulRem < ulLeft ? ulRem : ulLeft;
			for(j = 0; j < ulTake; ++j) {
				pDest[s_ulHalf - ulLeft + j] = mixerClamp(
					(int)pDest[s_ulHalf - ulLeft + j] + (int)pSrc[j]
				);
			}
			pSrc += ulTake;
			ulRem -= ulTake;
			ulLeft -= ulTake;
		}

		pCh->pSample = pSrc;
		pCh->ulRemaining = ulRem;
	}
}

static void mixerOnAud(
	REGARG(volatile tCustom *pCustom, "a0"),
	REGARG(volatile void *pData, "a1")
) {
	BYTE *pNext;
	(void)pCustom;
	(void)pData;

	if(!s_isRunning) {
		return;
	}

	s_ubPlayIdx ^= 1;
	pNext = s_pMix[s_ubPlayIdx];
	mixerProgramPaula(pNext);
	mixerMixInto(pNext);
}

static void mixerWriteChannel(
	int iCh, const BYTE *pSample, ULONG ulLength,
	WORD wPriority, UWORD uwLoop, LONG lLoopOffset
) {
	tMixerChannel *pCh = &s_pCh[iCh];

	pCh->pSample = pSample;
	pCh->ulRemaining = ulLength;
	pCh->wPriority = wPriority;
	pCh->uwAge = 0;

	if((WORD)uwLoop < 0) {
		if(uwLoop == MIX_FX_LOOP_OFFSET) {
			ULONG ulOffs = lLoopOffset > 0 ? (ULONG)lLoopOffset : 0;
			ulOffs &= ~31u;
			if(ulOffs >= ulLength) {
				ulOffs = ulLength > 4u ? ulLength - 4u : 0;
			}
			pCh->pLoop = pSample + ulOffs;
			pCh->ulLoopLength = ulLength - ulOffs;
			if(pCh->ulLoopLength == 0) {
				pCh->pLoop = pSample;
				pCh->ulLoopLength = ulLength;
			}
		}
		else {
			pCh->pLoop = pSample;
			pCh->ulLoopLength = ulLength;
		}
	}
	else {
		pCh->pLoop = s_pEmpty;
		pCh->ulLoopLength = ulLength;
	}

	pCh->wStatus = (WORD)uwLoop;
}

static int mixerCanPlayOn(int iCh, WORD wPriority) {
	WORD wStatus = s_pCh[iCh].wStatus;
	if(wStatus == MIXER_CHAN_INACTIVE) {
		return 1;
	}
	if(wStatus < 0) {
		return 0;
	}
	return wPriority >= s_pCh[iCh].wPriority;
}

ULONG MixerGetBufferSize(void) {
	return mixerHalfSize((UWORD)ACE_HOST_MIXER_PERIOD, MIX_PAL) * 3u;
}

void MixerSetup(
	void *pBuffer, void *pPluginBuffer, void *pPluginData,
	UWORD uwVidsys, UWORD uwPluginDataLength
) {
	int i;
	BYTE *pBuf = (BYTE *)pBuffer;
	(void)pPluginBuffer;
	(void)pPluginData;
	(void)uwPluginDataLength;

	s_ubHw = mixerHwIndex();
	s_uwPeriod = mixerPeriodForVidsys(uwVidsys);
	s_ulHalf = mixerHalfSize(s_uwPeriod, uwVidsys);
	s_uwVolume = 64;
	s_isRunning = 0;
	s_ubPlayIdx = 0;

	s_pEmpty = pBuf;
	memset(s_pEmpty, 0, (size_t)s_ulHalf);
	s_pMix[0] = pBuf + s_ulHalf;
	memset(s_pMix[0], 0, (size_t)s_ulHalf);
	s_pMix[1] = pBuf + s_ulHalf * 2u;
	memset(s_pMix[1], 0, (size_t)s_ulHalf);

	for(i = 0; i < MIXER_SW_CHANNELS; ++i) {
		memset(&s_pCh[i], 0, sizeof(s_pCh[i]));
	}
}

void MixerInstallHandler(void *pVbr, UWORD uwSaveVector) {
	(void)pVbr;
	(void)uwSaveVector;
	s_ownsHw = 1;
	systemSetInt((UBYTE)(INTB_AUD0 + s_ubHw), mixerOnAud, 0);
}

void MixerRemoveHandler(void) {
	systemSetInt((UBYTE)(INTB_AUD0 + s_ubHw), 0, 0);
	s_ownsHw = 0;
}

void MixerStart(void) {
	s_ubPlayIdx = 0;
	mixerProgramPaula(s_pEmpty);
	systemSetDmaMask(mixerHwDma(), 1);
	s_isRunning = 1;
}

void MixerStop(void) {
	int i;
	s_isRunning = 0;
	g_pCustom->aud[s_ubHw].ac_vol = 0;
	systemSetDmaMask(mixerHwDma(), 0);
	chipsetSyncCpuWrites();
	for(i = 0; i < MIXER_SW_CHANNELS; ++i) {
		s_pCh[i].wStatus = MIXER_CHAN_INACTIVE;
	}
}

void MixerVolume(UWORD uwVolume) {
	if(uwVolume > 64) {
		uwVolume = 64;
	}
	s_uwVolume = uwVolume;
	if(s_isRunning) {
		g_pCustom->aud[s_ubHw].ac_vol = s_uwVolume;
		chipsetSyncCpuWrites();
	}
}

ULONG MixerPlaySample(
	void *pSample, ULONG ulHardwareChannel, LONG lLength,
	WORD wPriority, UWORD uwLoop, LONG lLoopOffset
) {
	int iCh;
	int iBest = -1;
	WORD wBestPrio = -32768;
	UWORD uwBestAge = 0;
	ULONG ulLength;
	(void)ulHardwareChannel;

	if(!pSample || lLength <= 0) {
		return (ULONG)-1;
	}

	ulLength = (ULONG)lLength & ~31u;
	if(!ulLength) {
		return (ULONG)-1;
	}

	for(iCh = 0; iCh < MIXER_SW_CHANNELS; ++iCh) {
		WORD wStatus = s_pCh[iCh].wStatus;
		if(wStatus == MIXER_CHAN_INACTIVE) {
			iBest = iCh;
			break;
		}
		if(wStatus < 0) {
			continue;
		}
		if(wPriority < s_pCh[iCh].wPriority) {
			continue;
		}
		if(iBest < 0 || s_pCh[iCh].wPriority < wBestPrio ||
			(s_pCh[iCh].wPriority == wBestPrio && s_pCh[iCh].uwAge >= uwBestAge)
		) {
			iBest = iCh;
			wBestPrio = s_pCh[iCh].wPriority;
			uwBestAge = s_pCh[iCh].uwAge;
		}
	}

	if(iBest < 0) {
		return (ULONG)-1;
	}

	mixerWriteChannel(iBest, (const BYTE *)pSample, ulLength, wPriority, uwLoop, lLoopOffset);
	return (ULONG)(mixerHwDma() | (MIX_CH0 << iBest));
}

ULONG MixerPlayChannelSample(
	void *pSample, ULONG ulMixerChannel, LONG lLength,
	WORD wPriority, UWORD uwLoop, LONG lLoopOffset
) {
	int iCh;
	ULONG ulLength;

	if(!pSample || lLength <= 0) {
		return (ULONG)-1;
	}

	ulLength = (ULONG)lLength & ~31u;
	if(!ulLength) {
		return (ULONG)-1;
	}

	iCh = mixerSwIndexFromMask((UWORD)ulMixerChannel);
	if(iCh >= MIXER_SW_CHANNELS) {
		return (ULONG)-1;
	}
	if(!mixerCanPlayOn(iCh, wPriority)) {
		return (ULONG)-1;
	}

	mixerWriteChannel(iCh, (const BYTE *)pSample, ulLength, wPriority, uwLoop, lLoopOffset);
	return (ULONG)(mixerHwDma() | (MIX_CH0 << iCh));
}

void MixerStopFX(UWORD uwMixerChannelMask) {
	int iCh;
	for(iCh = 0; iCh < MIXER_SW_CHANNELS; ++iCh) {
		if(uwMixerChannelMask & (MIX_CH0 << iCh)) {
			s_pCh[iCh].wStatus = MIXER_CHAN_INACTIVE;
		}
	}
}

ULONG MixerGetChannelStatus(UWORD uwMixerChannel) {
	int iCh;
	if(!(uwMixerChannel & (MIX_CH0 | MIX_CH1 | MIX_CH2 | MIX_CH3))) {
		iCh = 0;
	}
	else {
		iCh = mixerSwIndexFromMask(uwMixerChannel);
	}
	if(iCh < 0 || iCh >= MIXER_SW_CHANNELS) {
		return MIX_CH_FREE;
	}
	return s_pCh[iCh].wStatus == MIXER_CHAN_INACTIVE ? MIX_CH_FREE : MIX_CH_BUSY;
}

int aceHostMixerOwnsAud3(void) {
	return s_ownsHw && s_ubHw == 3;
}
