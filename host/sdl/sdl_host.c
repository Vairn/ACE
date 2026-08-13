#include "chipset_priv.h"
#include <ace/managers/key.h>
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
static UWORD s_joy0, s_joy1;
static UWORD s_pot = 0xFFFF;
static UBYTE s_fireMask;
static UBYTE s_mouseX, s_mouseY;

#ifdef ACE_HOST_HAS_SDL
static SDL_Window *s_win;
static SDL_Renderer *s_ren;
static SDL_Texture *s_tex;
static SDL_AudioDeviceID s_audio;
static Uint32 s_lastPresent;

static void audioCb(void *ud, Uint8 *stream, int len) {
	(void)ud;
	paulaMix((short *)stream, len / 4);
}

static UWORD joyEncode(int l, int r, int u, int d) {
	UWORD dat = 0;
	if(l) {
		dat |= 0x200;
	}
	if(r) {
		dat |= 0x002;
	}
	if(u) {
		dat |= 0x100;
	}
	if(d) {
		dat |= 0x001;
	}
	if(l && !u) {
		dat |= 0x100;
	}
	if(r && !d) {
		dat |= 0x001;
	}
	return dat;
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
#endif

void aceHostSdlInit(int isPal) {
	s_isPal = isPal;
#ifdef ACE_HOST_HAS_SDL
	{
		SDL_AudioSpec want, have;
		int h = isPal ? ACE_HOST_FB_HEIGHT_PAL : ACE_HOST_FB_HEIGHT_NTSC;
		if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
			fprintf(stderr, "[ACE_HOST] SDL_Init: %s\n", SDL_GetError());
			return;
		}
		s_win = SDL_CreateWindow(
			"ACE host",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			ACE_HOST_FB_WIDTH * s_scale, h * s_scale, SDL_WINDOW_RESIZABLE
		);
		s_ren = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_ACCELERATED);
		if(!s_ren) {
			s_ren = SDL_CreateRenderer(s_win, -1, 0);
		}
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
		s_tex = SDL_CreateTexture(
			s_ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
			ACE_HOST_FB_WIDTH, h
		);
		memset(&want, 0, sizeof(want));
		want.freq = 44100;
		want.format = AUDIO_S16SYS;
		want.channels = 2;
		want.samples = 1024;
		want.callback = audioCb;
		s_audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
		if(s_audio) {
			SDL_PauseAudioDevice(s_audio, 0);
		}
		SDL_SetRelativeMouseMode(SDL_TRUE);
		s_lastPresent = SDL_GetTicks();
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
		else if(e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
			int down = e.type == SDL_KEYDOWN;
			if(e.key.repeat) {
				continue;
			}
			if(down && e.key.keysym.scancode == SDL_SCANCODE_F11) {
				s_hudOn ^= 1;
			}
			else if(down && e.key.keysym.scancode == SDL_SCANCODE_F12) {
				s_hudFull ^= 1;
			}
			else if(down && e.key.keysym.scancode == SDL_SCANCODE_F10) {
				chipsetSetTimingLog(!chipsetTimingLogEnabled());
				fprintf(stderr, "[ACE_HOST] timing log %s\n",
					chipsetTimingLogEnabled() ? "on" : "off");
			}
			else {
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
	const Uint8 *ks = SDL_GetKeyboardState(NULL);
	int u = ks[SDL_SCANCODE_UP];
	int d = ks[SDL_SCANCODE_DOWN];
	int l = ks[SDL_SCANCODE_LEFT];
	int r = ks[SDL_SCANCODE_RIGHT];
	int fire = ks[SDL_SCANCODE_LCTRL] || ks[SDL_SCANCODE_RCTRL] || ks[SDL_SCANCODE_Z];
	int fire2 = ks[SDL_SCANCODE_LALT] || ks[SDL_SCANCODE_X];
	int m1 = SDL_GetMouseState(NULL, NULL);
	s_joy1 = joyEncode(l, r, u, d);
	s_joy0 = (UWORD)(((UWORD)s_mouseY << 8) | s_mouseX);
	s_fireMask = 0;
	/* Port 2 (joy1dat / JOY1) fire is CIA FIR1; port 1 mouse LMB is FIR0. */
	if(fire) {
		s_fireMask |= CIAAPRA_FIR1;
	}
	if(m1 & SDL_BUTTON_LMASK) {
		s_fireMask |= CIAAPRA_FIR0;
	}
	s_pot = 0xFFFF;
	if(fire2) {
		s_pot &= (UWORD)~(1u << 14);
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
#endif

void aceHostSdlPresent(const UWORD *pFb, int width, int height) {
#ifdef ACE_HOST_HAS_SDL
	Uint32 now, target;
	if(!s_ren || !s_tex) {
		return;
	}
	memcpy(s_presentTmp, pFb, (size_t)width * (size_t)height * sizeof(UWORD));
	if(s_hudOn) {
		aceHostHudDraw(s_presentTmp, width, height);
		if(s_hudFull) {
			/* extra panel drawn inside hud */
		}
	}
	SDL_UpdateTexture(s_tex, NULL, s_presentTmp, width * (int)sizeof(UWORD));
	SDL_RenderClear(s_ren);
	SDL_RenderCopy(s_ren, s_tex, NULL, NULL);
	SDL_RenderPresent(s_ren);
	now = SDL_GetTicks();
	target = 1000u / (s_isPal ? 50u : 60u);
	if(now - s_lastPresent < target) {
		SDL_Delay(target - (now - s_lastPresent));
	}
	s_lastPresent = SDL_GetTicks();
#else
	(void)pFb;
	(void)width;
	(void)height;
#endif
}

void aceHostOnVblank(void) {
	int w, h;
	UWORD *fb = chipsetFramebuffer(&w, &h);
	aceHostSdlPump();
	aceHostSdlApplyInput();
	aceHostSdlPresent(fb, w, h);
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
