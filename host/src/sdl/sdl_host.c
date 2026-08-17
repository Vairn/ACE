#include "chipset_priv.h"
#include "host_os.h"
#include "host_chrome.h"
#include <ace/managers/key.h>
#include <ace/managers/game.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef ACE_HOST_HAS_SDL
#include <SDL.h>
#endif

static int s_hudOn = 1;
static int s_hudFull;
static int s_quit;
static int s_isPal = 1;
static int s_envNoPace;
static int s_headless;
static int s_autoKeyFrame = -1;
static int s_autoKeyUpFrame = -1;
static int s_quitAfter = 0;
static int s_vbl;
static UBYTE s_autoKey;
static UWORD s_joy0, s_joy1;
static UWORD s_pot = 0xFFFF;
static UBYTE s_fireMask;
static UBYTE s_mouseX, s_mouseY;
static char s_iniPath[1024];

#ifdef ACE_HOST_HAS_SDL
static SDL_Window *s_win;
static SDL_Renderer *s_ren;
static SDL_Texture *s_tex;
static SDL_AudioDeviceID s_audio;
static int s_texW, s_texH;
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
static SDL_GameController *s_pad;
#endif
static Uint64 s_paceFreq;
static Uint64 s_nextPace;
static int s_paceReady;

/* Present does scale + SDL_UpdateTexture/RenderCopy/RenderPresent on a
 * dedicated thread so the game thread never blocks on vsync or on software
 * filters (hq3x @ scale 3 etc). The game thread only copies the framebuffer
 * into the next free ring slot; the ring lets us drop a frame instead of
 * stalling the emulation when a display hiccup happens. The present thread
 * parks in an SDL_semaphore channel. */
#define ACE_HOST_PRESENT_SLOTS 8
static UWORD s_presentRing[ACE_HOST_PRESENT_SLOTS][ACE_HOST_FB_WIDTH * ACE_HOST_FB_HEIGHT_PAL];
static UWORD s_presentTmp[ACE_HOST_FB_WIDTH * ACE_HOST_FB_HEIGHT_PAL];
static UWORD s_presentScale[ACE_HOST_FB_WIDTH * 3 * ACE_HOST_FB_HEIGHT_PAL * 3];
static volatile int s_presentRun;
static SDL_Thread *s_presentTh;
static SDL_sem *s_presentSem;     /* posted for each queued frame */
static SDL_mutex *s_presentMx;    /* guards ring head/tail + pending chrome */
static volatile int s_presentHead;   /* next slot to draw into (write side) */
static volatile int s_presentTail;   /* next slot to present (read side) */
static int s_presentChrome;       /* pending aceHostChromeConsumeApply bits */
static volatile int s_presentDrops;   /* ring overflow: frames dropped */

/* Called by applyChrome() while the present thread owns all SDL render state.
 * Queues the chrome change instead of mutating SDL objects on the game thread. */
static void presentQueueChrome(int bits);
static int sdlPresentThread(void *ud);

static void audioCb(void *ud, Uint8 *stream, int len) {
	(void)ud;
	paulaMix((short *)stream, len / 4);
}

#define KEY_UNMAPPED 0xFF

static void initKeyMap(UBYTE *m) {
	memset(m, KEY_UNMAPPED, 512);
	m[SDL_SCANCODE_GRAVE] = KEY_ACCENT;
	m[SDL_SCANCODE_1] = KEY_1;
	m[SDL_SCANCODE_2] = KEY_2;
	m[SDL_SCANCODE_3] = KEY_3;
	m[SDL_SCANCODE_4] = KEY_4;
	m[SDL_SCANCODE_5] = KEY_5;
	m[SDL_SCANCODE_6] = KEY_6;
	m[SDL_SCANCODE_7] = KEY_7;
	m[SDL_SCANCODE_8] = KEY_8;
	m[SDL_SCANCODE_9] = KEY_9;
	m[SDL_SCANCODE_0] = KEY_0;
	m[SDL_SCANCODE_MINUS] = KEY_MINUS;
	m[SDL_SCANCODE_EQUALS] = KEY_EQUALS;
	m[SDL_SCANCODE_BACKSLASH] = KEY_BACKSLASH;
	m[SDL_SCANCODE_Q] = KEY_Q;
	m[SDL_SCANCODE_W] = KEY_W;
	m[SDL_SCANCODE_E] = KEY_E;
	m[SDL_SCANCODE_R] = KEY_R;
	m[SDL_SCANCODE_T] = KEY_T;
	m[SDL_SCANCODE_Y] = KEY_Y;
	m[SDL_SCANCODE_U] = KEY_U;
	m[SDL_SCANCODE_I] = KEY_I;
	m[SDL_SCANCODE_O] = KEY_O;
	m[SDL_SCANCODE_P] = KEY_P;
	m[SDL_SCANCODE_LEFTBRACKET] = KEY_LBRACKET;
	m[SDL_SCANCODE_RIGHTBRACKET] = KEY_RBRACKET;
	m[SDL_SCANCODE_A] = KEY_A;
	m[SDL_SCANCODE_S] = KEY_S;
	m[SDL_SCANCODE_D] = KEY_D;
	m[SDL_SCANCODE_F] = KEY_F;
	m[SDL_SCANCODE_G] = KEY_G;
	m[SDL_SCANCODE_H] = KEY_H;
	m[SDL_SCANCODE_J] = KEY_J;
	m[SDL_SCANCODE_K] = KEY_K;
	m[SDL_SCANCODE_L] = KEY_L;
	m[SDL_SCANCODE_SEMICOLON] = KEY_SEMICOLON;
	m[SDL_SCANCODE_APOSTROPHE] = KEY_APOSTROPHE;
	m[SDL_SCANCODE_Z] = KEY_Z;
	m[SDL_SCANCODE_X] = KEY_X;
	m[SDL_SCANCODE_C] = KEY_C;
	m[SDL_SCANCODE_V] = KEY_V;
	m[SDL_SCANCODE_B] = KEY_B;
	m[SDL_SCANCODE_N] = KEY_N;
	m[SDL_SCANCODE_M] = KEY_M;
	m[SDL_SCANCODE_COMMA] = KEY_COMMA;
	m[SDL_SCANCODE_PERIOD] = KEY_PERIOD;
	m[SDL_SCANCODE_SLASH] = KEY_SLASH;
	m[SDL_SCANCODE_SPACE] = KEY_SPACE;
	m[SDL_SCANCODE_RETURN] = KEY_RETURN;
	m[SDL_SCANCODE_ESCAPE] = KEY_ESCAPE;
	m[SDL_SCANCODE_BACKSPACE] = KEY_BACKSPACE;
	m[SDL_SCANCODE_TAB] = KEY_TAB;
	m[SDL_SCANCODE_F1] = KEY_F1;
	m[SDL_SCANCODE_F2] = KEY_F2;
	m[SDL_SCANCODE_F3] = KEY_F3;
	m[SDL_SCANCODE_F4] = KEY_F4;
	m[SDL_SCANCODE_F5] = KEY_F5;
	m[SDL_SCANCODE_F6] = KEY_F6;
	m[SDL_SCANCODE_F7] = KEY_F7;
	m[SDL_SCANCODE_F8] = KEY_F8;
	m[SDL_SCANCODE_F9] = KEY_F9;
	m[SDL_SCANCODE_UP] = KEY_UP;
	m[SDL_SCANCODE_DOWN] = KEY_DOWN;
	m[SDL_SCANCODE_LEFT] = KEY_LEFT;
	m[SDL_SCANCODE_RIGHT] = KEY_RIGHT;
	m[SDL_SCANCODE_LSHIFT] = KEY_LSHIFT;
	m[SDL_SCANCODE_RSHIFT] = KEY_RSHIFT;
	m[SDL_SCANCODE_LCTRL] = KEY_CONTROL;
	m[SDL_SCANCODE_RCTRL] = KEY_CONTROL;
	m[SDL_SCANCODE_LALT] = KEY_LALT;
	m[SDL_SCANCODE_RALT] = KEY_RALT;
}

static UBYTE amiKey(SDL_Scancode sc) {
	static int once;
	static UBYTE map[512];
	if(!once) {
		initKeyMap(map);
		once = 1;
	}
	if((unsigned)sc < 512) {
		return map[sc];
	}
	return KEY_UNMAPPED;
}

#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
static int isVirtualJoyKey(SDL_Scancode sc);
#endif

/* Write ACE pStates from SDL's live matrix. CIA SDR inject + the 3-line
 * SPMODE wait drops KEYUPs (and can decode the wrong raw key), so Return
 * stays ACTIVE and Up looks like Enter. keyUse() still sees a fresh press
 * only on NACTIVE -> ACTIVE. */
static void syncKeysFromSdl(void) {
	const Uint8 *ks = SDL_GetKeyboardState(NULL);
	UBYTE down[KEY_COUNT];
	int sc;
	UBYTE i;

	if(!ks) {
		return;
	}
	memset(down, 0, sizeof(down));
	if(!aceHostMenuIsOpen()) {
		int alt = ks[SDL_SCANCODE_LALT] || ks[SDL_SCANCODE_RALT];
		for(sc = 0; sc < SDL_NUM_SCANCODES; ++sc) {
			UBYTE ami;
			if(!ks[sc]) {
				continue;
			}
			if(sc == SDL_SCANCODE_F10 || sc == SDL_SCANCODE_F11 || sc == SDL_SCANCODE_F12) {
				continue;
			}
			if(alt) {
				continue;
			}
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
			if(aceHostSettings()->virtualJoy && isVirtualJoyKey((SDL_Scancode)sc)) {
				continue;
			}
#endif
			ami = amiKey((SDL_Scancode)sc);
			if(ami < KEY_COUNT) {
				down[ami] = 1;
			}
		}
	}
	for(i = 0; i < KEY_COUNT; ++i) {
		if(down[i]) {
			if(g_sKeyManager.pStates[i] == KEY_NACTIVE) {
				keySetState(i, KEY_ACTIVE);
			}
		}
		else if(g_sKeyManager.pStates[i] != KEY_NACTIVE) {
			keySetState(i, KEY_NACTIVE);
		}
	}
}

#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
static int isVirtualJoyKey(SDL_Scancode sc) {
	return sc == SDL_SCANCODE_KP_8 || sc == SDL_SCANCODE_KP_2 ||
		sc == SDL_SCANCODE_KP_4 || sc == SDL_SCANCODE_KP_6 ||
		sc == SDL_SCANCODE_KP_5 || sc == SDL_SCANCODE_RCTRL;
}

/* Amiga JOYxDAT: up = bit8 XOR bit9, down = bit0 XOR bit1, left = bit9, right = bit1. */
static UWORD joyDatFromDirs(int up, int down, int left, int right) {
	UWORD dat = 0;
	if(left) {
		dat |= (UWORD)(1u << 9);
	}
	if(right) {
		dat |= (UWORD)(1u << 1);
	}
	if(up ^ left) {
		dat |= (UWORD)(1u << 8);
	}
	if(down ^ right) {
		dat |= (UWORD)(1u << 0);
	}
	return dat;
}

static void closePad(void) {
	if(s_pad) {
		SDL_GameControllerClose(s_pad);
		s_pad = 0;
	}
}

static SDL_JoystickID padInstanceId(void) {
	SDL_Joystick *js;
	if(!s_pad) {
		return -1;
	}
	js = SDL_GameControllerGetJoystick(s_pad);
	return js ? SDL_JoystickInstanceID(js) : (SDL_JoystickID)-1;
}

static void openPadAt(int index) {
	const char *name;
	if(s_pad || index < 0 || !SDL_IsGameController(index)) {
		return;
	}
	s_pad = SDL_GameControllerOpen(index);
	if(!s_pad) {
		fprintf(stderr, "[ACE_HOST] gamepad open failed: %s\n", SDL_GetError());
		return;
	}
	name = SDL_GameControllerName(s_pad);
	fprintf(stderr, "[ACE_HOST] gamepad connected: %s\n", name ? name : "?");
}

static void openFirstPad(void) {
	int i, n;
	if(s_pad) {
		return;
	}
	n = SDL_NumJoysticks();
	for(i = 0; i < n; ++i) {
		openPadAt(i);
		if(s_pad) {
			return;
		}
	}
}
#endif

static int fbHeight(void) {
	return s_isPal ? ACE_HOST_FB_HEIGHT_PAL : ACE_HOST_FB_HEIGHT_NTSC;
}

static int noPaceNow(void) {
	return s_envNoPace || !aceHostSettings()->pace;
}

static void aspectUnit(int *uw, int *uh) {
	tAceHostSettings *s = aceHostSettings();
	*uw = ACE_HOST_FB_WIDTH;
	if(s->aspect == ACE_HOST_ASPECT_SQUARE) {
		*uh = fbHeight();
	}
	else {
		*uh = ACE_HOST_FB_WIDTH * 3 / 4;
	}
}

static int filterInteger(void) {
	tAceHostSettings *s = aceHostSettings();
	if(aceHostMenuIsOpen()) {
		return 1;
	}
	if(s->filter == ACE_HOST_FILTER_HQ3X) {
		return 3;
	}
	if(s->filter == ACE_HOST_FILTER_SCALE2X || s->filter == ACE_HOST_FILTER_HQ2X) {
		return 2;
	}
	if(s->filter == ACE_HOST_FILTER_NEAREST && s->scanlines) {
		return 2;
	}
	return 1;
}

static void createHostTexture(void) {
	tAceHostSettings *s = aceHostSettings();
	int n = filterInteger();
	int nativeH = fbHeight();
	if(s_tex) {
		SDL_DestroyTexture(s_tex);
		s_tex = 0;
	}
	s_texW = ACE_HOST_FB_WIDTH * n;
	s_texH = nativeH * n;
	if(!s_ren) {
		return;
	}
	s_tex = SDL_CreateTexture(
		s_ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
		s_texW, s_texH
	);
#if SDL_VERSION_ATLEAST(2, 0, 12)
	if(s_tex) {
		SDL_ScaleMode mode = (!aceHostMenuIsOpen() && s->filter == ACE_HOST_FILTER_LINEAR) ?
			SDL_ScaleModeLinear : SDL_ScaleModeNearest;
		SDL_SetTextureScaleMode(s_tex, mode);
	}
#endif
}

static void recreateHostRenderer(void) {
	tAceHostSettings *s = aceHostSettings();
	Uint32 flags = SDL_RENDERER_ACCELERATED;
	if(s->vsync) {
		flags |= SDL_RENDERER_PRESENTVSYNC;
	}
	if(s_tex) {
		SDL_DestroyTexture(s_tex);
		s_tex = 0;
	}
	if(s_ren) {
		SDL_DestroyRenderer(s_ren);
		s_ren = 0;
	}
	if(!s_win) {
		return;
	}
	s_ren = SDL_CreateRenderer(s_win, -1, flags);
	if(!s_ren) {
		s_ren = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_ACCELERATED);
	}
	if(!s_ren) {
		s_ren = SDL_CreateRenderer(s_win, -1, 0);
	}
	createHostTexture();
}

static void applyWindowSize(void) {
	tAceHostSettings *s = aceHostSettings();
	int uw, uh;
	if(!s_win || s->fullscreen != ACE_HOST_FS_OFF) {
		return;
	}
	aspectUnit(&uw, &uh);
	SDL_SetWindowSize(s_win, uw * s->scale, uh * s->scale);
	SDL_SetWindowPosition(s_win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

static void applyFullscreen(void) {
	tAceHostSettings *s = aceHostSettings();
	Uint32 flag = 0;
	if(!s_win) {
		return;
	}
	if(s->fullscreen == ACE_HOST_FS_BORDERLESS) {
		flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	else if(s->fullscreen == ACE_HOST_FS_EXCLUSIVE) {
		flag = SDL_WINDOW_FULLSCREEN;
	}
	SDL_SetWindowFullscreen(s_win, flag);
	if(s->fullscreen == ACE_HOST_FS_OFF) {
		applyWindowSize();
	}
}

static void applyChrome(int bits) {
	/* All SDL render state (s_ren/s_tex/window flags) is owned by the present
	 * thread. Mutating it here on the game thread while the present thread is
	 * mid-render would be a data race, so we hand the bits to the present
	 * thread's queue and wake it. Settings are saved here (game thread) so a
	 * killed process still persists choices like before. */
	if(!bits) {
		return;
	}
	if(s_iniPath[0]) {
		aceHostSettingsSave(s_iniPath);
	}
	presentQueueChrome(bits);
}

static void presentQueueChrome(int bits) {
	if(!bits || !s_presentMx || !s_presentSem) {
		return;
	}
	SDL_LockMutex(s_presentMx);
	s_presentChrome |= bits;
	SDL_UnlockMutex(s_presentMx);
	SDL_SemPost(s_presentSem);
}

static void resolveIniPath(void) {
	char *base = SDL_GetBasePath();
	s_iniPath[0] = 0;
	if(base) {
		snprintf(s_iniPath, sizeof(s_iniPath), "%sace_host.ini", base);
		SDL_free(base);
	}
}
#endif

void aceHostSdlInit(int isPal) {
	s_isPal = isPal;
	{
		const char *sz;
		sz = getenv("ACE_HOST_NOPACE");
		s_envNoPace = (sz && sz[0] && sz[0] != '0');
		sz = getenv("ACE_HOST_HEADLESS");
		if(sz && sz[0] && sz[0] != '0') {
			s_headless = 1;
			s_envNoPace = 1;
		}
		sz = getenv("ACE_HOST_QUIT_AFTER");
		if(sz && sz[0]) {
			s_quitAfter = atoi(sz);
		}
		sz = getenv("ACE_HOST_AUTO_KEY");
		if(sz && sz[0]) {
			if(!_stricmp(sz, "return") || !_stricmp(sz, "enter")) {
				s_autoKey = KEY_RETURN;
			}
			else if(!_stricmp(sz, "space")) {
				s_autoKey = KEY_SPACE;
			}
			else if(!_stricmp(sz, "escape") || !_stricmp(sz, "esc")) {
				s_autoKey = KEY_ESCAPE;
			}
			else {
				s_autoKey = (UBYTE)strtoul(sz, 0, 0);
			}
			s_autoKeyFrame = 10;
			sz = getenv("ACE_HOST_AUTO_KEY_FRAME");
			if(sz && sz[0]) {
				s_autoKeyFrame = atoi(sz);
			}
			if(s_autoKeyFrame < 1) {
				s_autoKeyFrame = 1;
			}
			s_autoKeyUpFrame = s_autoKeyFrame + 2;
			fprintf(stderr, "[ACE_HOST] auto-key 0x%02X on vblank %d\n",
				s_autoKey, s_autoKeyFrame);
		}
	}
	if(s_headless) {
		fprintf(stderr, "[ACE_HOST] headless (no SDL window)\n");
		return;
	}
#ifdef ACE_HOST_HAS_SDL
	{
		SDL_AudioSpec want, have;
		tAceHostSettings *st;
		int uw, uh;
#if SDL_VERSION_ATLEAST(2, 0, 16)
		SDL_SetMemoryFunctions(hostOsHeapMalloc, hostOsHeapCalloc, hostOsHeapRealloc, hostOsHeapFree);
#endif
#if SDL_VERSION_ATLEAST(2, 24, 0)
		SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
#ifdef SDL_HINT_WINDOWS_DPI_SCALING
		SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "0");
#endif
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
#ifdef SDL_HINT_TIMER_RESOLUTION
		SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");
#endif
		if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
			| SDL_INIT_GAMECONTROLLER
#endif
			) != 0) {
			fprintf(stderr, "[ACE_HOST] SDL_Init: %s\n", SDL_GetError());
			return;
		}
		resolveIniPath();
		aceHostSettingsLoad(s_iniPath);
		st = aceHostSettings();
		aspectUnit(&uw, &uh);
		s_win = SDL_CreateWindow(
			"ACE host",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			uw * st->scale, uh * st->scale,
			SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
		);
		/* Renderer/texture are owned by the present thread from here on. */
		paulaSetHostVolume(st->volume);
		memset(&want, 0, sizeof(want));
		want.freq = 44100;
		want.format = AUDIO_S16SYS;
		want.channels = 2;
		/* 2048-frame device buffer: the audio callback runs ~21x/sec instead
		 * of ~43x, so the producer burst (882/16.6ms) has room to accumulate
		 * between pulls instead of the callback catching the ring dry. */
		want.samples = 2048;
		want.callback = audioCb;
		s_audio = SDL_OpenAudioDevice(
			NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE
		);
		if(s_audio) {
			paulaSetOutputRate(have.freq);
			SDL_PauseAudioDevice(s_audio, 0);
			fprintf(stderr, "[ACE_HOST] audio %d Hz S16 stereo, vblank sync %u Hz\n",
				have.freq, isPal ? 50u : 60u);
		}
		else {
			fprintf(stderr, "[ACE_HOST] SDL_OpenAudioDevice: %s\n", SDL_GetError());
		}
		SDL_SetRelativeMouseMode(SDL_TRUE);
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
		openFirstPad();
		fprintf(stderr, "[ACE_HOST] virtual joystick %s (numpad 8462, fire 5/RCtrl → JOY1DAT)\n",
			st->virtualJoy ? "ON" : "off");
#endif
		s_paceFreq = SDL_GetPerformanceFrequency();
		if(!s_paceFreq) {
			s_paceFreq = 1000;
		}
		s_paceReady = 0;
		fprintf(stderr, "[ACE_HOST] video scale=%d aspect=%d filter=%d fs=%d ini=%s\n",
			st->scale, st->aspect, st->filter, st->fullscreen,
			s_iniPath[0] ? s_iniPath : "(none)");
		s_presentRun = 1;
		s_presentHead = 0;
		s_presentTail = 0;
		s_presentChrome = 0;
		s_presentSem = SDL_CreateSemaphore(0);
		s_presentMx = SDL_CreateMutex();
		s_presentTh = SDL_CreateThread(sdlPresentThread, "ace-present", NULL);
		if(!s_presentTh) {
			fprintf(stderr, "[ACE_HOST] present thread failed: %s\n", SDL_GetError());
			s_presentRun = 0;
			SDL_DestroySemaphore(s_presentSem);
			s_presentSem = 0;
			SDL_DestroyMutex(s_presentMx);
			s_presentMx = 0;
		}
		else {
			fprintf(stderr, "[ACE_HOST] present thread started\n");
			presentQueueChrome(ACE_HOST_CHROME_APPLY_RENDER | ACE_HOST_CHROME_APPLY_TEXTURE);
		}
	}
#else
	fprintf(stderr, "[ACE_HOST] built without SDL2 — no window/audio\n");
#endif
}

void aceHostSdlShutdown(void) {
#ifdef ACE_HOST_HAS_SDL
	if(s_iniPath[0]) {
		aceHostSettingsSave(s_iniPath);
	}
	if(s_presentTh) {
		/* Tell the present thread to exit, wake it, and join it before we
		 * destroy the SDL renderer/texture/window it owns. */
		SDL_LockMutex(s_presentMx);
		s_presentRun = 0;
		SDL_UnlockMutex(s_presentMx);
		SDL_SemPost(s_presentSem);
		SDL_WaitThread(s_presentTh, 0);
		s_presentTh = 0;
	}
	if(s_presentSem) {
		SDL_DestroySemaphore(s_presentSem);
		s_presentSem = 0;
	}
	if(s_presentMx) {
		SDL_DestroyMutex(s_presentMx);
		s_presentMx = 0;
	}
	if(s_audio) {
		SDL_CloseAudioDevice(s_audio);
	}
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
	closePad();
#endif
	if(s_tex) {
		SDL_DestroyTexture(s_tex);
	}
	if(s_ren) {
		SDL_DestroyRenderer(s_ren);
	}
	if(s_win) {
		SDL_DestroyWindow(s_win);
	}
	SDL_Quit();
#endif
}

void aceHostSdlPump(void) {
#ifdef ACE_HOST_HAS_SDL
	SDL_Event e;
	while(SDL_PollEvent(&e)) {
		if(e.type == SDL_QUIT) {
			s_quit = 1;
		}
		else if(e.type == SDL_MOUSEMOTION) {
			s_mouseX = (UBYTE)(s_mouseX + e.motion.xrel);
			s_mouseY = (UBYTE)(s_mouseY + e.motion.yrel);
		}
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
		else if(e.type == SDL_CONTROLLERDEVICEADDED) {
			openPadAt(e.cdevice.which);
		}
		else if(e.type == SDL_CONTROLLERDEVICEREMOVED) {
			if(padInstanceId() == (SDL_JoystickID)e.cdevice.which) {
				fprintf(stderr, "[ACE_HOST] gamepad disconnected\n");
				closePad();
				openFirstPad();
			}
		}
#endif
		else if(e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
			int down = e.type == SDL_KEYDOWN;
			SDL_Scancode sc;
			int shift, alt;
			if(e.key.repeat) {
				continue;
			}
			sc = e.key.keysym.scancode;
			shift = (e.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0;
			alt = (e.key.keysym.mod & (KMOD_LALT | KMOD_RALT)) != 0;
			if(down && alt && sc == SDL_SCANCODE_RETURN) {
				aceHostChromeCycleFullscreen();
			}
			else if(down && shift && sc == SDL_SCANCODE_F10) {
				chipsetSetTimingLog(!chipsetTimingLogEnabled());
				fprintf(stderr, "[ACE_HOST] timing log %s\n",
					chipsetTimingLogEnabled() ? "on" : "off");
			}
			else if(down && shift && sc == SDL_SCANCODE_F11) {
#ifdef ACE_HOST_DEBUG
				s_hudOn ^= 1;
#endif
			}
			else if(down && shift && sc == SDL_SCANCODE_F12) {
#ifdef ACE_HOST_DEBUG
				s_hudFull ^= 1;
#endif
			}
			else if(down && !shift && sc == SDL_SCANCODE_F11) {
				aceHostMenuToggle(ACE_HOST_MENU_VIDEO);
				applyChrome(ACE_HOST_CHROME_APPLY_TEXTURE);
			}
			else if(down && !shift && sc == SDL_SCANCODE_F12) {
				aceHostMenuToggle(ACE_HOST_MENU_HOST);
				applyChrome(ACE_HOST_CHROME_APPLY_TEXTURE);
			}
			else if(down && aceHostMenuIsOpen()) {
				aceHostMenuOnKey((int)sc);
			}
		}
	}
	applyChrome(aceHostChromeConsumeApply());
	if(s_quit) {
		systemKill("window closed");
	}
#endif
}

void aceHostSdlApplyInput(void) {
#ifdef ACE_HOST_HAS_SDL
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
	const Uint8 *ks = SDL_GetKeyboardState(NULL);
#endif
	int m1;
	syncKeysFromSdl();
	m1 = SDL_GetMouseState(NULL, NULL);
	s_joy0 = (UWORD)(((UWORD)s_mouseY << 8) | s_mouseX);
	s_joy1 = 0;
	s_fireMask = 0;
	s_pot = 0xFFFF;
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
	if(aceHostSettings()->virtualJoy && !aceHostMenuIsOpen())
	{
		int up = ks[SDL_SCANCODE_KP_8];
		int down = ks[SDL_SCANCODE_KP_2];
		int left = ks[SDL_SCANCODE_KP_4];
		int right = ks[SDL_SCANCODE_KP_6];
		int fire = ks[SDL_SCANCODE_KP_5] || ks[SDL_SCANCODE_RCTRL];
		int fire2 = 0;
		if(s_pad) {
			up |= SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_UP) ||
				(SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTY) < -8000);
			down |= SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) ||
				(SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTY) > 8000);
			left |= SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) ||
				(SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTX) < -8000);
			right |= SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ||
				(SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTX) > 8000);
			fire |= SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_A);
			fire2 |= SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_B);
		}
		s_joy1 = joyDatFromDirs(up, down, left, right);
		if(fire) {
			s_fireMask |= CIAAPRA_FIR1;
		}
		if(fire2) {
			s_pot &= (UWORD)~(1u << 14);
		}
	}
#endif
	if(m1 & SDL_BUTTON_LMASK) {
		s_fireMask |= CIAAPRA_FIR0;
	}
	if(m1 & SDL_BUTTON_RMASK) {
		s_pot &= (UWORD)~(1u << 10);
	}
	if(m1 & SDL_BUTTON_MMASK) {
		s_pot &= (UWORD)~(1u << 8);
	}
	chipsetSetJoyDat(s_joy0, s_joy1);
	chipsetSetPotinp(s_pot);
	chipsetSetCiaPraFire(CIAAPRA_FIR0 | CIAAPRA_FIR1, s_fireMask);
#else
	(void)s_joy0;
	(void)s_joy1;
	(void)s_pot;
	(void)s_fireMask;
#endif
}

#ifdef ACE_HOST_HAS_SDL
/* Sleep until the next PAL (50 Hz) / NTSC (60 Hz) deadline. Vsync alone is
 * not enough: a 60/144 Hz display would run the playfield too fast. With the
 * chipset thread running it is the sole pace owner and the game thread must
 * NOT sleep a second 50 Hz period here — the beam is already advancing on a
 * single monotonic deadline. */
static void aceHostPaceVblank(void) {
	unsigned hz;
	Uint64 period, now;
	hz = s_isPal ? 50u : 60u;
	if(chipsetThreadRunning()) {
		return;
	}
	period = (s_paceFreq + (hz / 2u)) / hz;
	if(!period) {
		period = 1;
	}
	now = SDL_GetPerformanceCounter();
	if(!s_paceReady) {
		s_nextPace = now + period;
		s_paceReady = 1;
		return;
	}
	if(now < s_nextPace) {
		hostOsSleepUntilTicks(s_nextPace);
	}
	/* Skip catch-up frames after a hitch so the next wait is a full period. */
	if(now >= s_nextPace + period) {
		s_nextPace = now + period;
	}
	else {
		s_nextPace += period;
	}
}
#endif

void aceHostSdlPresent(const UWORD *pFb, int width, int height) {
#ifdef ACE_HOST_HAS_SDL
	int next;
	if(s_headless || !s_presentRun || !s_presentSem || !s_presentMx) {
		return;
	}
	SDL_LockMutex(s_presentMx);
	next = (s_presentHead + 1) % ACE_HOST_PRESENT_SLOTS;
	/* Ring full → the present thread is behind (vsync hiccup / heavy filter).
	 * Instead of stalling the emulation, drop the oldest pending frame. */
	if(next == s_presentTail) {
		s_presentTail = (s_presentTail + 1) % ACE_HOST_PRESENT_SLOTS;
		s_presentDrops++;
		next = (s_presentHead + 1) % ACE_HOST_PRESENT_SLOTS;
	}
	SDL_UnlockMutex(s_presentMx);
	memcpy(s_presentRing[next], pFb, (size_t)width * (size_t)height * sizeof(UWORD));
	/* Memory barrier via the mutex so the present thread sees the copy. */
	SDL_LockMutex(s_presentMx);
	s_presentHead = next;
	SDL_UnlockMutex(s_presentMx);
	SDL_SemPost(s_presentSem);
#else
	(void)pFb;
	(void)width;
	(void)height;
#endif
}

/* Runs on the present thread. Owns s_ren/s_tex and the ring read side, and
 * performs the software scale filters plus the (blocking, vsync'd) render. */
static int sdlPresentThread(void *ud) {
	(void)ud;
#ifdef ACE_HOST_HAS_SDL
	UWORD *tmp = s_presentTmp;
	int firstPresent = 1;
	Uint64 lastDropLog = 0;
	while(s_presentRun) {
		int chrome, menu, n, slot;
		tAceHostSettings *st = aceHostSettings();
		const UWORD *upload;
		int upW, upH;
		SDL_Rect dst;
		int rw, rh, scale, unitW, unitH;
		int width = ACE_HOST_FB_WIDTH, height = fbHeight();
		Uint64 now;
		Uint64 freq = SDL_GetPerformanceFrequency();

		/* Park until a frame is queued. A 16 ms timeout also lets keyed chrome
		 * changes and shutdown be serviced when no frame arrives. */
		if(SDL_SemWaitTimeout(s_presentSem, 16)) {
			continue;
		}
		SDL_LockMutex(s_presentMx);
		chrome = s_presentChrome;
		s_presentChrome = 0;
		SDL_UnlockMutex(s_presentMx);
		if(chrome) {
			if(chrome & ACE_HOST_CHROME_APPLY_RENDER) {
				recreateHostRenderer();
			}
			else if(chrome & ACE_HOST_CHROME_APPLY_TEXTURE) {
				createHostTexture();
			}
			if(chrome & ACE_HOST_CHROME_APPLY_WINDOW) {
				applyFullscreen();
			}
			paulaSetHostVolume(st->volume);
		}
		if(s_presentTail == s_presentHead || !s_ren || !s_tex) {
			continue;
		}
		slot = s_presentTail;
		SDL_LockMutex(s_presentMx);
		s_presentTail = (s_presentTail + 1) % ACE_HOST_PRESENT_SLOTS;
		SDL_UnlockMutex(s_presentMx);
		/* Copy out of the ring early so the game thread can reuse the slot. */
		memcpy(tmp, s_presentRing[slot], (size_t)width * (size_t)height * sizeof(UWORD));

		menu = aceHostMenuIsOpen();
#ifdef ACE_HOST_DEBUG
		if(s_hudOn) {
			aceHostHudDraw(tmp, width, height);
		}
#endif
		if(menu) {
			aceHostMenuDraw(tmp, width, height);
		}
		n = menu ? 1 : filterInteger();
		upload = tmp;
		upW = width;
		upH = height;
		if(n > 1) {
			if(st->filter == ACE_HOST_FILTER_SCALE2X) {
				aceHostScale2x(tmp, width, height, s_presentScale);
			}
			else if(st->filter == ACE_HOST_FILTER_HQ2X) {
				aceHostHq2x(tmp, width, height, s_presentScale);
			}
			else if(st->filter == ACE_HOST_FILTER_HQ3X) {
				aceHostHq3x(tmp, width, height, s_presentScale);
			}
			else {
				int x, y;
				int dw = width * 2;
				for(y = 0; y < height; ++y) {
					for(x = 0; x < width; ++x) {
						UWORD p = tmp[y * width + x];
						s_presentScale[(y * 2) * dw + x * 2] = p;
						s_presentScale[(y * 2) * dw + x * 2 + 1] = p;
						s_presentScale[(y * 2 + 1) * dw + x * 2] = p;
						s_presentScale[(y * 2 + 1) * dw + x * 2 + 1] = p;
					}
				}
			}
			if(st->scanlines && st->filter != ACE_HOST_FILTER_LINEAR &&
				st->filter != ACE_HOST_FILTER_SCALE2X) {
				int x, y;
				int totalW = width * n;
				int totalH = height * n;
				for(y = 1; y < totalH; y += 2) {
					for(x = 0; x < totalW; ++x) {
						s_presentScale[y * totalW + x] =
							(UWORD)((s_presentScale[y * totalW + x] >> 1) & 0x7BEF);
					}
				}
			}
			upload = s_presentScale;
			upW = width * n;
			upH = height * n;
		}
		if(s_texW != upW || s_texH != upH) {
			createHostTexture();
		}
		if(!s_tex) {
			continue;
		}
		SDL_UpdateTexture(s_tex, NULL, upload, upW * (int)sizeof(UWORD));
		SDL_GetRendererOutputSize(s_ren, &rw, &rh);
		if(!menu && st->aspect == ACE_HOST_ASPECT_STRETCH) {
			dst.x = 0;
			dst.y = 0;
			dst.w = rw;
			dst.h = rh;
			scale = 0;
		}
		else {
			aspectUnit(&unitW, &unitH);
			if(!menu && st->filter == ACE_HOST_FILTER_LINEAR) {
				if(rw * unitH >= rh * unitW) {
					dst.h = rh;
					dst.w = rh * unitW / unitH;
					dst.x = (rw - dst.w) / 2;
					dst.y = 0;
				}
				else {
					dst.w = rw;
					dst.h = rw * unitH / unitW;
					dst.x = 0;
					dst.y = (rh - dst.h) / 2;
				}
				scale = 0;
			}
			else {
				scale = rw / unitW;
				if(rh / unitH < scale) {
					scale = rh / unitH;
				}
				if(scale < 1) {
					if(rw * unitH >= rh * unitW) {
						dst.h = rh;
						dst.w = rh * unitW / unitH;
						dst.x = (rw - dst.w) / 2;
						dst.y = 0;
					}
					else {
						dst.w = rw;
						dst.h = rw * unitH / unitW;
						dst.x = 0;
						dst.y = (rh - dst.h) / 2;
					}
				}
				else {
					dst.w = unitW * scale;
					dst.h = unitH * scale;
					dst.x = (rw - dst.w) / 2;
					dst.y = (rh - dst.h) / 2;
				}
			}
		}
		if(firstPresent) {
			int ww = 0, wh = 0;
			if(s_win) {
				SDL_GetWindowSize(s_win, &ww, &wh);
			}
			fprintf(stderr,
				"[ACE_HOST] present window=%dx%d renderer=%dx%d dest=%dx%d integer=%d\n",
				ww, wh, rw, rh, dst.w, dst.h, scale);
			firstPresent = 0;
		}
		/* Surface ring overflows at most ~once per second so a healthy beat
		 * (emu 50Hz vs display 60Hz) isn't a log flood. */
		if(s_presentDrops) {
			now = SDL_GetPerformanceCounter();
			if(!lastDropLog || (now - lastDropLog) >= freq) {
				SDL_LockMutex(s_presentMx);
				lastDropLog = now;
				fprintf(stderr, "[ACE_HOST] present: %d dropped frame(s)\n", s_presentDrops);
				s_presentDrops = 0;
				SDL_UnlockMutex(s_presentMx);
			}
		}
		SDL_SetRenderDrawColor(s_ren, 0, 0, 0, 255);
		SDL_RenderClear(s_ren);
		SDL_RenderCopy(s_ren, s_tex, NULL, &dst);
		SDL_RenderPresent(s_ren);
	}
#else
	(void)ud;
#endif
	return 0;
}

void aceHostOnVblank(void) {
	int w, h;
	UWORD *fb = chipsetFramebuffer(&w, &h);
	{
		const char *szDump = getenv("ACE_HOST_DUMP_FB");
		if(szDump && szDump[0] && s_vbl + 1 == atoi(szDump)) {
			int y, x, y0 = -1;
			FILE *p;
			for(y = 0; y < h && y0 < 0; ++y) {
				for(x = 0; x < w; ++x) {
					if(fb[y * w + x]) {
						y0 = y;
						break;
					}
				}
			}
			fprintf(stderr, "[ACE_HOST] fb first lit row %d (%dx%d)\n", y0, w, h);
			p = fopen("host_fb.ppm", "wb");
			if(p) {
				fprintf(p, "P6\n%d %d\n255\n", w, h);
				for(y = 0; y < h; ++y) {
					for(x = 0; x < w; ++x) {
						UWORD p565 = fb[y * w + x];
						unsigned char rgb[3] = {
							(unsigned char)(((p565 >> 11) & 0x1F) * 255 / 31),
							(unsigned char)(((p565 >> 5) & 0x3F) * 255 / 63),
							(unsigned char)((p565 & 0x1F) * 255 / 31)
						};
						fwrite(rgb, 1, 3, p);
					}
				}
				fclose(p);
			}
		}
	}
	if(!s_headless) {
		aceHostSdlPump();
		aceHostSdlApplyInput();
	}
	aceHostSdlPresent(fb, w, h);
#ifdef ACE_HOST_HAS_SDL
	if(!s_headless && !noPaceNow() && !chipsetThreadRunning()) {
		aceHostPaceVblank();
	}
#endif
	s_vbl++;
	if(s_autoKey && s_vbl == s_autoKeyFrame) {
		fprintf(stderr, "[ACE_HOST] inject key 0x%02X down vbl=%d\n", s_autoKey, s_vbl);
		chipsetInjectKey(s_autoKey);
	}
	if(s_autoKey && s_vbl == s_autoKeyUpFrame) {
		chipsetInjectKey((UBYTE)(s_autoKey | 0x80));
	}
	if(s_quitAfter > 0 && s_vbl >= s_quitAfter) {
		fprintf(stderr, "[ACE_HOST] quit after %d vblanks\n", s_vbl);
		gameExit();
		s_quit = 1;
		/* Games may use a custom main-loop condition that ignores gameIsRunning(). */
		exit(0);
	}
}

int aceHostPollQuit(void) {
	return s_quit;
}

int aceHostHudEnabled(void) {
	return s_hudOn;
}

int aceHostHudFull(void) {
	return s_hudFull;
}
