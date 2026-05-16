/* ps4_snd.c -- DMA audio backend via SceAudioOut on a dedicated thread. */

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include "../client/client.h"
#include "../client/snd_local.h"
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#define PS4_AUDIO_SAMPLE_RATE 48000
#define PS4_AUDIO_CHANNELS 2
#define PS4_AUDIO_GRANULARITY 256
#define PS4_AUDIO_BUFFER_SAMPLES (PS4_AUDIO_GRANULARITY * 4)

/* 2^21 ≈ 44s at 48kHz; must outlast shader compile (~16s) or
   S_GetSoundtime's wrap detection misses wraps and cinematic audio breaks. */
#define PS4_DMA_BUFFER_SAMPLES (1 << 21)

static int s_audioHandle = -1;
static int s_audioThreadId = -1;
static volatile qboolean s_audioRunning = qfalse;
static volatile long long s_dmaTotal = 0;
static float s_audioBuffer[PS4_AUDIO_GRANULARITY * PS4_AUDIO_CHANNELS];

/* Feeds int16 DMA samples to SceAudioOut as float [-1,1] (FLOAT_STEREO). */
static void *PS4_AudioThread(void *arg)
{
	while (s_audioRunning) {
		if (dma.buffer) {
			int samples = PS4_AUDIO_GRANULARITY * PS4_AUDIO_CHANNELS;
			int bufferSamples = dma.samples;
			int pos = (int)(s_dmaTotal % bufferSamples);
			int16_t *src = (int16_t *)dma.buffer;

			for (int i = 0; i < samples; i++) {
				s_audioBuffer[i] = (float)src[(pos + i) % bufferSamples] / 32768.0f;
			}
			s_dmaTotal += samples;
		} else {
			memset(s_audioBuffer, 0, sizeof(s_audioBuffer));
		}
		sceAudioOutOutput(s_audioHandle, s_audioBuffer);
	}

	return NULL;
}

qboolean SNDDMA_Init(void)
{
	Com_Printf("PS4 Audio: Initializing...\n");

	/* sceAudioOutInit required before Open. 0x8026000E = ALREADY_INIT (ignore). */
	{
		int ret = sceAudioOutInit();
		Com_Printf("PS4 Audio: sceAudioOutInit ret=0x%08X\n", (unsigned)ret);
		if (ret < 0 && ret != (int)0x8026000E) {
			Com_Printf("WARNING: sceAudioOutInit failed: 0x%08X\n", (unsigned)ret);
		}
	}

	Com_Printf("PS4 Audio: opening port (userId=0xFF, FLOAT_STEREO)\n");

	/* 0xFF = USER_ID_SYSTEM; accepted by PORT_TYPE_MAIN on retail FW 9.00. */
	s_audioHandle = sceAudioOutOpen(
		0xFF,
		ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,
		0,
		PS4_AUDIO_GRANULARITY,
		PS4_AUDIO_SAMPLE_RATE,
		ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO
	);

	if (s_audioHandle < 0) {
		Com_Printf("WARNING: sceAudioOutOpen failed: 0x%08X\n", (unsigned)s_audioHandle);
		return qfalse;
	}

	Com_Printf("PS4 Audio: sceAudioOutOpen handle=%d\n", s_audioHandle);

	dma.samplebits = 16;
	dma.speed = PS4_AUDIO_SAMPLE_RATE;
	dma.channels = PS4_AUDIO_CHANNELS;
	dma.samples = PS4_DMA_BUFFER_SAMPLES;
	dma.fullsamples = dma.samples / dma.channels;
	dma.submission_chunk = PS4_AUDIO_GRANULARITY;
	s_dmaTotal = 0;
	dma.buffer = (byte *)calloc(1, dma.samples * (dma.samplebits / 8));

	if (!dma.buffer) {
		Com_Printf("WARNING: Failed to allocate DMA buffer\n");
		sceAudioOutClose(s_audioHandle);
		s_audioHandle = -1;
		return qfalse;
	}

	s_audioRunning = qtrue;

	pthread_t thread;
	int ret = pthread_create(&thread, NULL, PS4_AudioThread, NULL);
	if (ret != 0) {
		Com_Printf("WARNING: Failed to create audio thread: %d\n", ret);
		s_audioRunning = qfalse;
		free(dma.buffer);
		dma.buffer = NULL;
		sceAudioOutClose(s_audioHandle);
		s_audioHandle = -1;
		return qfalse;
	}
	s_audioThreadId = 1;

	Com_Printf("PS4 Audio: %d Hz, %d channels, 16-bit DMA -> float output\n",
		dma.speed, dma.channels);

	return qtrue;
}

/* Modulo dma.samples so S_GetSoundtime's wrap detection fires correctly. */
int SNDDMA_GetDMAPos(void)
{
	return (int)(s_dmaTotal % dma.samples);
}

void SNDDMA_Shutdown(void)
{
	s_audioRunning = qfalse;
	sceKernelUsleep(50000); /* let the audio thread exit */

	if (s_audioHandle >= 0) {
		sceAudioOutClose(s_audioHandle);
		s_audioHandle = -1;
	}

	if (dma.buffer) {
		free(dma.buffer);
		dma.buffer = NULL;
	}

	Com_Printf("PS4 Audio: Shutdown complete\n");
}

/* No-ops: the audio thread submits frames directly. */
void SNDDMA_Submit(void) { }
void SNDDMA_BeginPainting(void) { }
