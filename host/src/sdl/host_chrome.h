#ifndef ACE_HOST_CHROME_H
#define ACE_HOST_CHROME_H

#include <ace/types.h>

typedef enum tAceHostFullscreen {
	ACE_HOST_FS_OFF = 0,
	ACE_HOST_FS_BORDERLESS,
	ACE_HOST_FS_EXCLUSIVE
} tAceHostFullscreen;

typedef enum tAceHostAspect {
	ACE_HOST_ASPECT_43 = 0,
	ACE_HOST_ASPECT_SQUARE,
	ACE_HOST_ASPECT_STRETCH
} tAceHostAspect;

typedef enum tAceHostFilter {
	ACE_HOST_FILTER_NEAREST = 0,
	ACE_HOST_FILTER_LINEAR,
	ACE_HOST_FILTER_SCALE2X,
	ACE_HOST_FILTER_HQ2X,
	ACE_HOST_FILTER_HQ3X
} tAceHostFilter;

typedef enum tAceHostMenu {
	ACE_HOST_MENU_NONE = 0,
	ACE_HOST_MENU_VIDEO,
	ACE_HOST_MENU_HOST
} tAceHostMenu;

#define ACE_HOST_CHROME_APPLY_WINDOW  1
#define ACE_HOST_CHROME_APPLY_RENDER  2
#define ACE_HOST_CHROME_APPLY_TEXTURE 4
#define ACE_HOST_CHROME_APPLY_AUDIO   8
#define ACE_HOST_CHROME_APPLY_ALL     15

typedef struct tAceHostSettings {
	int fullscreen;
	int scale;
	int aspect;
	int filter;
	int scanlines;
	int vsync;
	int pace;
	int volume;
	int virtualJoy;
} tAceHostSettings;

void aceHostOverlayDrawStr(UWORD *fb, int w, int h, int x, int y, const char *s, UWORD c);
void aceHostOverlayFill(UWORD *fb, int w, int h, int x, int y, int bw, int bh, UWORD c);

void aceHostScale2x(const UWORD *src, int w, int h, UWORD *dst);
void aceHostHq2x(const UWORD *src, int w, int h, UWORD *dst);
void aceHostHq3x(const UWORD *src, int w, int h, UWORD *dst);

tAceHostSettings *aceHostSettings(void);
void aceHostSettingsDefaults(void);
void aceHostSettingsLoad(const char *szPath);
void aceHostSettingsSave(const char *szPath);
int aceHostMenuKind(void);
int aceHostMenuIsOpen(void);
void aceHostMenuToggle(int kind);
void aceHostMenuClose(void);
void aceHostMenuOnKey(int scancode);
void aceHostMenuDraw(UWORD *fb, int w, int h);
int aceHostChromeConsumeApply(void);
void aceHostChromeCycleFullscreen(void);

void paulaSetHostVolume(int vol0to10);

#endif
