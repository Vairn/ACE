#include "chipset_priv.h"
#include "host_os.h"
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
static int s_scale = 2;
static int s_quit;
static int s_isPal = 1;
static int s_noPace;
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

#ifdef ACE_HOST_HAS_SDL
static SDL_Window *s_win;
static SDL_Renderer *s_ren;
static SDL_Texture *s_tex;
static SDL_AudioDeviceID s_audio;
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
static SDL_GameController *s_pad;
#endif
static Uint64 s_paceFreq;
static Uint64 s_nextPace;
static int s_paceReady;

static void audioCb(void *ud, Uint8 *stream, int len) {
	(void)ud;
	paulaMix((short *)stream, len / 4);
}

static void initKeyMap(UBYTE *m) {
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
	return 0;
}

static void sendKey(UBYTE code, int down) {
	UBYTE raw = code;
	if(!code) {
		return;
	}
	if(!down) {
		raw |= 0x80;
	}
	chipsetInjectKey(raw);
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
#endif

void aceHostSdlInit(int isPal) {
	s_isPal = isPal;
	{
		const char *sz;
		sz = getenv("ACE_HOST_NOPACE");
		s_noPace = (sz && sz[0] && sz[0] != '0');
		sz = getenv("ACE_HOST_HEADLESS");
		if(sz && sz[0] && sz[0] != '0') {
			s_headless = 1;
			s_noPace = 1;
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
		int h = isPal ? ACE_HOST_FB_HEIGHT_PAL : ACE_HOST_FB_HEIGHT_NTSC;
		int winW = ACE_HOST_FB_WIDTH * s_scale;
		int winH = winW * 3 / 4; /* 4:3 CRT, not square-pixel 640x256 */
#if SDL_VERSION_ATLEAST(2, 0, 16)
		SDL_SetMemoryFunctions(hostOsHeapMalloc, hostOsHeapCalloc, hostOsHeapRealloc, hostOsHeapFree);
#endif
#if SDL_VERSION_ATLEAST(2, 24, 0)
		SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
#ifdef SDL_HINT_WINDOWS_DPI_SCALING
		SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "0");
#endif
		/* Nearest-neighbor must be set before the renderer/texture exist. Linear
		 * (or a non-integer stretch of 640x256 into a 4:3 window) turns lores
		 * text and edges into a vertical comb / screen-door. */
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
		if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
			| SDL_INIT_GAMECONTROLLER
#endif
			) != 0) {
			fprintf(stderr, "[ACE_HOST] SDL_Init: %s\n", SDL_GetError());
			return;
		}
		s_win = SDL_CreateWindow(
			"ACE host",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			winW, winH, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
		);
		s_ren = SDL_CreateRenderer(
			s_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
		);
		if(!s_ren) {
			s_ren = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_ACCELERATED);
		}
		if(!s_ren) {
			s_ren = SDL_CreateRenderer(s_win, -1, 0);
		}
		s_tex = SDL_CreateTexture(
			s_ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
			ACE_HOST_FB_WIDTH, h
		);
#if SDL_VERSION_ATLEAST(2, 0, 12)
		if(s_tex) {
			SDL_SetTextureScaleMode(s_tex, SDL_ScaleModeNearest);
		}
#endif
		memset(&want, 0, sizeof(want));
		want.freq = 44100;
		want.format = AUDIO_S16SYS;
		want.channels = 2;
		want.samples = 1024;
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
		fprintf(stderr, "[ACE_HOST] virtual joystick ON (numpad 8462, fire 5/RCtrl → JOY1DAT)\n");
#endif
		s_paceFreq = SDL_GetPerformanceFrequency();
		if(!s_paceFreq) {
			s_paceFreq = 1000;
		}
		s_paceReady = 0;
	}
#else
	fprintf(stderr, "[ACE_HOST] built without SDL2 — no window/audio\n");
#endif
}

void aceHostSdlShutdown(void) {
#ifdef ACE_HOST_HAS_SDL
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
			if(e.key.repeat) {
				continue;
			}
			if(down && e.key.keysym.scancode == SDL_SCANCODE_F11) {
#ifdef ACE_HOST_DEBUG
				s_hudOn ^= 1;
#endif
			}
			else if(down && e.key.keysym.scancode == SDL_SCANCODE_F12) {
#ifdef ACE_HOST_DEBUG
				s_hudFull ^= 1;
#endif
			}
			else if(down && e.key.keysym.scancode == SDL_SCANCODE_F10) {
				chipsetSetTimingLog(!chipsetTimingLogEnabled());
				fprintf(stderr, "[ACE_HOST] timing log %s\n",
					chipsetTimingLogEnabled() ? "on" : "off");
			}
			else {
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
				if(isVirtualJoyKey(e.key.keysym.scancode)) {
					continue;
				}
#endif
				sendKey(amiKey(e.key.keysym.scancode), down);
			}
		}
	}
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
	int m1 = SDL_GetMouseState(NULL, NULL);
	s_joy0 = (UWORD)(((UWORD)s_mouseY << 8) | s_mouseX);
	s_joy1 = 0;
	s_fireMask = 0;
	s_pot = 0xFFFF;
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
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
static UWORD s_presentTmp[ACE_HOST_FB_WIDTH * ACE_HOST_FB_HEIGHT_PAL];

/* Sleep until the next PAL (50 Hz) / NTSC (60 Hz) deadline. Vsync alone is
 * not enough: a 60/144 Hz display would run the playfield too fast. */
static void aceHostPaceVblank(void) {
	unsigned hz;
	Uint64 period, now;
	hz = s_isPal ? 50u : 60u;
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
		Uint64 remainMs = (s_nextPace - now) * 1000u / s_paceFreq;
		if(remainMs > 1u) {
			SDL_Delay((Uint32)(remainMs - 1u));
		}
		while(SDL_GetPerformanceCounter() < s_nextPace) {
		}
		now = SDL_GetPerformanceCounter();
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
	SDL_Rect dst;
	int rw, rh;
	/* 4:3 CRT unit: 640x480. Integer multiples keep lores columns 1:1 or 2:1
	 * instead of 1.6x/2.5x nearest-neighbor combing on DPI or resized windows. */
	const int unitW = ACE_HOST_FB_WIDTH;
	const int unitH = ACE_HOST_FB_WIDTH * 3 / 4;
	int scale;
	if(s_headless || !s_ren || !s_tex) {
		return;
	}
	memcpy(s_presentTmp, pFb, (size_t)width * (size_t)height * sizeof(UWORD));
#ifdef ACE_HOST_DEBUG
	if(s_hudOn) {
		aceHostHudDraw(s_presentTmp, width, height);
	}
#endif
	SDL_UpdateTexture(s_tex, NULL, s_presentTmp, width * (int)sizeof(UWORD));
	SDL_GetRendererOutputSize(s_ren, &rw, &rh);
	scale = rw / unitW;
	if(rh / unitH < scale) {
		scale = rh / unitH;
	}
	if(scale < 1) {
		if(rw * 3 >= rh * 4) {
			dst.h = rh;
			dst.w = rh * 4 / 3;
			dst.x = (rw - dst.w) / 2;
			dst.y = 0;
		}
		else {
			dst.w = rw;
			dst.h = rw * 3 / 4;
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
	if(s_vbl == 0) {
		int ww = 0, wh = 0;
		if(s_win) {
			SDL_GetWindowSize(s_win, &ww, &wh);
		}
		fprintf(stderr,
			"[ACE_HOST] present window=%dx%d renderer=%dx%d dest=%dx%d integer=%d\n",
			ww, wh, rw, rh, dst.w, dst.h, scale);
	}
	SDL_SetRenderDrawColor(s_ren, 0, 0, 0, 255);
	SDL_RenderClear(s_ren);
	SDL_RenderCopy(s_ren, s_tex, NULL, &dst);
	SDL_RenderPresent(s_ren);
#else
	(void)pFb;
	(void)width;
	(void)height;
#endif
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
	if(!s_headless && !s_noPace && !chipsetThreadRunning()) {
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
