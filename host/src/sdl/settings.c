#include "host_chrome.h"
#include "chipset_priv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ACE_HOST_HAS_SDL
#include <SDL.h>
#endif

static tAceHostSettings s_set;
static int s_menu;
static int s_row;
static int s_apply;
static int s_loaded;

static void mark(int bits) {
	s_apply |= bits;
}

void aceHostSettingsDefaults(void) {
	s_set.fullscreen = ACE_HOST_FS_OFF;
	s_set.scale = 2;
	s_set.aspect = ACE_HOST_ASPECT_43;
	s_set.filter = ACE_HOST_FILTER_NEAREST;
	s_set.scanlines = 0;
	s_set.vsync = 1;
	s_set.pace = 1;
	s_set.volume = 10;
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
	s_set.virtualJoy = 1;
#else
	s_set.virtualJoy = 0;
#endif
}

tAceHostSettings *aceHostSettings(void) {
	if(!s_loaded) {
		aceHostSettingsDefaults();
		s_loaded = 1;
	}
	return &s_set;
}

static int parseBool(const char *v) {
	return v && v[0] && v[0] != '0' && strcmp(v, "off") != 0 && strcmp(v, "false") != 0;
}

void aceHostSettingsLoad(const char *szPath) {
	FILE *p;
	char line[256];
	aceHostSettingsDefaults();
	s_loaded = 1;
	if(!szPath || !szPath[0]) {
		return;
	}
	p = fopen(szPath, "r");
	if(!p) {
		return;
	}
	while(fgets(line, sizeof(line), p)) {
		char *eq, *k, *v;
		char *nl = strchr(line, '\n');
		if(nl) {
			*nl = 0;
		}
		if(line[0] == '#' || line[0] == 0) {
			continue;
		}
		eq = strchr(line, '=');
		if(!eq) {
			continue;
		}
		*eq = 0;
		k = line;
		v = eq + 1;
		if(strcmp(k, "fullscreen") == 0) {
			if(strcmp(v, "borderless") == 0) {
				s_set.fullscreen = ACE_HOST_FS_BORDERLESS;
			}
			else if(strcmp(v, "exclusive") == 0) {
				s_set.fullscreen = ACE_HOST_FS_EXCLUSIVE;
			}
			else {
				s_set.fullscreen = ACE_HOST_FS_OFF;
			}
		}
		else if(strcmp(k, "scale") == 0) {
			int n = atoi(v);
			if(n < 1) {
				n = 1;
			}
			if(n > 4) {
				n = 4;
			}
			s_set.scale = n;
		}
		else if(strcmp(k, "aspect") == 0) {
			if(strcmp(v, "square") == 0) {
				s_set.aspect = ACE_HOST_ASPECT_SQUARE;
			}
			else if(strcmp(v, "stretch") == 0) {
				s_set.aspect = ACE_HOST_ASPECT_STRETCH;
			}
			else {
				s_set.aspect = ACE_HOST_ASPECT_43;
			}
		}
		else if(strcmp(k, "filter") == 0) {
			if(strcmp(v, "linear") == 0) {
				s_set.filter = ACE_HOST_FILTER_LINEAR;
			}
			else if(strcmp(v, "scale2x") == 0) {
				s_set.filter = ACE_HOST_FILTER_SCALE2X;
			}
			else if(strcmp(v, "hq2x") == 0) {
				s_set.filter = ACE_HOST_FILTER_HQ2X;
			}
			else if(strcmp(v, "hq3x") == 0) {
				s_set.filter = ACE_HOST_FILTER_HQ3X;
			}
			else {
				s_set.filter = ACE_HOST_FILTER_NEAREST;
			}
		}
		else if(strcmp(k, "scanlines") == 0) {
			s_set.scanlines = parseBool(v);
		}
		else if(strcmp(k, "vsync") == 0) {
			s_set.vsync = parseBool(v);
		}
		else if(strcmp(k, "pace") == 0) {
			s_set.pace = parseBool(v);
		}
		else if(strcmp(k, "volume") == 0) {
			int n = atoi(v);
			if(n < 0) {
				n = 0;
			}
			if(n > 10) {
				n = 10;
			}
			s_set.volume = n;
		}
		else if(strcmp(k, "virtual_joy") == 0) {
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
			s_set.virtualJoy = parseBool(v);
#else
			(void)v;
			s_set.virtualJoy = 0;
#endif
		}
	}
	fclose(p);
}

void aceHostSettingsSave(const char *szPath) {
	FILE *p;
	static const char *fsName[] = {"off", "borderless", "exclusive"};
	static const char *asName[] = {"4:3", "square", "stretch"};
	static const char *fiName[] = {"nearest", "linear", "scale2x", "hq2x", "hq3x"};
	if(!szPath || !szPath[0]) {
		return;
	}
	p = fopen(szPath, "w");
	if(!p) {
		return;
	}
	fprintf(p, "fullscreen=%s\n", fsName[s_set.fullscreen]);
	fprintf(p, "scale=%d\n", s_set.scale);
	fprintf(p, "aspect=%s\n", asName[s_set.aspect]);
	fprintf(p, "filter=%s\n", fiName[s_set.filter]);
	fprintf(p, "scanlines=%d\n", s_set.scanlines ? 1 : 0);
	fprintf(p, "vsync=%d\n", s_set.vsync ? 1 : 0);
	fprintf(p, "pace=%d\n", s_set.pace ? 1 : 0);
	fprintf(p, "volume=%d\n", s_set.volume);
	fprintf(p, "virtual_joy=%d\n", s_set.virtualJoy ? 1 : 0);
	fclose(p);
}

int aceHostMenuKind(void) {
	return s_menu;
}

int aceHostMenuIsOpen(void) {
	return s_menu != ACE_HOST_MENU_NONE;
}

void aceHostMenuClose(void) {
	s_menu = ACE_HOST_MENU_NONE;
}

void aceHostMenuToggle(int kind) {
	if(s_menu == kind) {
		s_menu = ACE_HOST_MENU_NONE;
	}
	else {
		s_menu = kind;
		s_row = 0;
	}
}

int aceHostChromeConsumeApply(void) {
	int bits = s_apply;
	s_apply = 0;
	return bits;
}

void aceHostChromeCycleFullscreen(void) {
	s_set.fullscreen = (s_set.fullscreen + 1) % 3;
	mark(ACE_HOST_CHROME_APPLY_WINDOW);
}

static int videoRows(void) {
	return 5;
}

static int hostRows(void) {
	return 4;
}

static void nudgeVideo(int dir) {
	switch(s_row) {
		case 0:
			s_set.fullscreen = (s_set.fullscreen + dir + 3) % 3;
			mark(ACE_HOST_CHROME_APPLY_WINDOW);
			break;
		case 1:
			s_set.scale += dir;
			if(s_set.scale < 1) {
				s_set.scale = 4;
			}
			if(s_set.scale > 4) {
				s_set.scale = 1;
			}
			mark(ACE_HOST_CHROME_APPLY_WINDOW);
			break;
		case 2:
			s_set.aspect = (s_set.aspect + dir + 3) % 3;
			mark(ACE_HOST_CHROME_APPLY_WINDOW | ACE_HOST_CHROME_APPLY_TEXTURE);
			break;
		case 3:
			s_set.filter = (s_set.filter + dir + 5) % 5;
			mark(ACE_HOST_CHROME_APPLY_TEXTURE);
			break;
		case 4:
			s_set.scanlines = !s_set.scanlines;
			mark(ACE_HOST_CHROME_APPLY_TEXTURE);
			break;
		default:
			break;
	}
}

static void nudgeHost(int dir) {
	switch(s_row) {
		case 0:
			s_set.vsync = !s_set.vsync;
			mark(ACE_HOST_CHROME_APPLY_RENDER);
			break;
		case 1:
			s_set.pace = !s_set.pace;
			mark(ACE_HOST_CHROME_APPLY_AUDIO);
			break;
		case 2:
			s_set.volume += dir;
			if(s_set.volume < 0) {
				s_set.volume = 0;
			}
			if(s_set.volume > 10) {
				s_set.volume = 10;
			}
			mark(ACE_HOST_CHROME_APPLY_AUDIO);
			break;
		case 3:
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
			s_set.virtualJoy = !s_set.virtualJoy;
			mark(ACE_HOST_CHROME_APPLY_AUDIO);
#else
			(void)dir;
#endif
			break;
		default:
			break;
	}
}

void aceHostMenuOnKey(int scancode) {
	int n;
	int dir = 0;
#ifdef ACE_HOST_HAS_SDL
	if(scancode == SDL_SCANCODE_ESCAPE) {
		aceHostMenuClose();
		return;
	}
	if(scancode == SDL_SCANCODE_UP) {
		s_row--;
	}
	else if(scancode == SDL_SCANCODE_DOWN) {
		s_row++;
	}
	else if(scancode == SDL_SCANCODE_LEFT) {
		dir = -1;
	}
	else if(scancode == SDL_SCANCODE_RIGHT || scancode == SDL_SCANCODE_RETURN) {
		dir = 1;
	}
#else
	(void)scancode;
#endif
	n = (s_menu == ACE_HOST_MENU_VIDEO) ? videoRows() : hostRows();
	if(s_row < 0) {
		s_row = n - 1;
	}
	if(s_row >= n) {
		s_row = 0;
	}
	if(dir) {
		if(s_menu == ACE_HOST_MENU_VIDEO) {
			nudgeVideo(dir);
		}
		else {
			nudgeHost(dir);
		}
	}
}

static const char *fsLabel(void) {
	static const char *n[] = {"OFF", "BORDERLESS", "EXCLUSIVE"};
	return n[s_set.fullscreen];
}

static const char *asLabel(void) {
	static const char *n[] = {"4:3 CRT", "SQUARE", "STRETCH"};
	return n[s_set.aspect];
}

static const char *fiLabel(void) {
	static const char *n[] = {"NEAREST", "LINEAR", "SCALE2X", "HQ2X", "HQ3X"};
	return n[s_set.filter];
}

void aceHostMenuDraw(UWORD *fb, int w, int h) {
	char line[80];
	UWORD white = 0xFFFF, yellow = 0xFFE0, dim = 0xC618, bg = 0x1082;
	int x0 = 40, y0 = 40, bw = 240, bh;
	int i, n, y;
	if(s_menu == ACE_HOST_MENU_NONE) {
		return;
	}
	n = (s_menu == ACE_HOST_MENU_VIDEO) ? videoRows() : hostRows();
	bh = 16 + (n + 3) * 10;
	aceHostOverlayFill(fb, w, h, x0 - 4, y0 - 4, bw + 8, bh + 8, bg);
	aceHostOverlayDrawStr(fb, w, h, x0, y0,
		s_menu == ACE_HOST_MENU_VIDEO ? "VIDEO" : "HOST", yellow);
	y = y0 + 12;
	for(i = 0; i < n; ++i) {
		UWORD col = (i == s_row) ? yellow : white;
		const char *sel = (i == s_row) ? ">" : " ";
		if(s_menu == ACE_HOST_MENU_VIDEO) {
			switch(i) {
				case 0:
					snprintf(line, sizeof(line), "%s FULLSCREEN  %s", sel, fsLabel());
					break;
				case 1:
					snprintf(line, sizeof(line), "%s SCALE       %dX", sel, s_set.scale);
					break;
				case 2:
					snprintf(line, sizeof(line), "%s ASPECT      %s", sel, asLabel());
					break;
				case 3:
					snprintf(line, sizeof(line), "%s FILTER      %s", sel, fiLabel());
					break;
				default:
					snprintf(line, sizeof(line), "%s SCANLINES   %s", sel,
						s_set.scanlines ? "ON" : "OFF");
					break;
			}
		}
		else {
			switch(i) {
				case 0:
					snprintf(line, sizeof(line), "%s VSYNC       %s", sel,
						s_set.vsync ? "ON" : "OFF");
					break;
				case 1:
					snprintf(line, sizeof(line), "%s PACE        %s", sel,
						s_set.pace ? "ON" : "OFF");
					break;
				case 2:
					snprintf(line, sizeof(line), "%s VOLUME      %d", sel, s_set.volume);
					break;
				default:
#ifdef ACE_HOST_USE_VIRTUAL_JOYSTICK
					snprintf(line, sizeof(line), "%s VJOY        %s", sel,
						s_set.virtualJoy ? "ON" : "OFF");
#else
					snprintf(line, sizeof(line), "%s VJOY        N/A", sel);
					col = dim;
#endif
					break;
			}
		}
		aceHostOverlayDrawStr(fb, w, h, x0, y, line, col);
		y += 10;
	}
	y += 4;
	aceHostOverlayDrawStr(fb, w, h, x0, y,
		s_menu == ACE_HOST_MENU_VIDEO ? "F11/ESC CLOSE  F12 HOST" :
			"F12/ESC CLOSE  F11 VIDEO", dim);
}
