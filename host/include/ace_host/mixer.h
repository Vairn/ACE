#ifndef ACE_HOST_MIXER_H
#define ACE_HOST_MIXER_H

#include <ace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIX_PAL  0
#define MIX_NTSC 1

#define MIX_FX_ONCE         1
#define MIX_FX_LOOP         ((UWORD)-1)
#define MIX_FX_LOOP_OFFSET  ((UWORD)-2)

#define MIX_CH0  16
#define MIX_CH1  32
#define MIX_CH2  64
#define MIX_CH3  128

#define MIX_CH_FREE 0
#define MIX_CH_BUSY 1

ULONG MixerGetBufferSize(void);

void MixerSetup(
	void *pBuffer, void *pPluginBuffer, void *pPluginData,
	UWORD uwVidsys, UWORD uwPluginDataLength
);

void MixerInstallHandler(void *pVbr, UWORD uwSaveVector);

void MixerRemoveHandler(void);

void MixerStart(void);

void MixerStop(void);

void MixerVolume(UWORD uwVolume);

ULONG MixerPlaySample(
	void *pSample, ULONG ulHardwareChannel, LONG lLength,
	WORD wPriority, UWORD uwLoop, LONG lLoopOffset
);

ULONG MixerPlayChannelSample(
	void *pSample, ULONG ulMixerChannel, LONG lLength,
	WORD wPriority, UWORD uwLoop, LONG lLoopOffset
);

void MixerStopFX(UWORD uwMixerChannelMask);

ULONG MixerGetChannelStatus(UWORD uwMixerChannel);

int aceHostMixerOwnsAud3(void);

#ifdef __cplusplus
}
#endif

#endif
