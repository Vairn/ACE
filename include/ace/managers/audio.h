/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_MANAGERS_AUDIO_H_
#define _ACE_MANAGERS_AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ace/types.h>
#include <ace/utils/custom.h>

typedef struct _tSample {
	UWORD uwLength;
	UWORD uwPeriod;
	UBYTE *pData;
} tSample;

void INTERRUPT audioIntHandler(
	UNUSED_ARG REGARG(volatile tCustom *pCustom, "a0"),
	UNUSED_ARG REGARG(volatile void *pData, "a1")
);

void audioCreate(void);
void audioDestroy(void);

void audioPlay(
	UBYTE ubChannel, tSample *pSample, UBYTE ubVolume, BYTE bPlayCount
);
void audioStop(UBYTE ubChannel);

tSample *sampleCreate(UWORD uwLength, UWORD uwPeriod);
tSample *sampleCreateFromFile(const char *szPath, UWORD uwSampleRateHz);
void sampleDestroy(tSample *pSample);

#ifdef __cplusplus
}
#endif

#endif // _ACE_MANAGERS_AUDIO_H_
