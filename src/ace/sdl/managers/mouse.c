/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <SDL.h>

#include <ace/managers/mouse.h>
#include <ace/managers/sdl_private.h>
#include <ace/generic/screen.h>
#include <string.h>

tMouseManager g_sMouseManager;

static void mouseProcessPortFromSdl(
	UBYTE ubPort, int mouseX, int mouseY, Uint32 mouseButtons
) {
	mouseSetPosition(ubPort, (UWORD)mouseX, (UWORD)mouseY);

	if(mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) {
		if(!mouseCheck(ubPort, MOUSE_LMB)) {
			mouseSetButton(ubPort, MOUSE_LMB, MOUSE_ACTIVE);
		}
	}
	else {
		mouseSetButton(ubPort, MOUSE_LMB, MOUSE_NACTIVE);
	}

	if(mouseButtons & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
		if(!mouseCheck(ubPort, MOUSE_RMB)) {
			mouseSetButton(ubPort, MOUSE_RMB, MOUSE_ACTIVE);
		}
	}
	else {
		mouseSetButton(ubPort, MOUSE_RMB, MOUSE_NACTIVE);
	}

	if(mouseButtons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
		if(!mouseCheck(ubPort, MOUSE_MMB)) {
			mouseSetButton(ubPort, MOUSE_MMB, MOUSE_ACTIVE);
		}
	}
	else {
		mouseSetButton(ubPort, MOUSE_MMB, MOUSE_NACTIVE);
	}
}

void mouseCreate(UBYTE ubPortFlags) {
	memset(&g_sMouseManager, 0, sizeof(tMouseManager));
	g_sMouseManager.ubPortFlags = ubPortFlags;

	if(ubPortFlags & MOUSE_PORT_1) {
		mouseSetBounds(MOUSE_PORT_1, 0, 0, SCREEN_PAL_WIDTH - 1, SCREEN_PAL_HEIGHT - 1);
		mouseResetPos(MOUSE_PORT_1);
	}
	if(ubPortFlags & MOUSE_PORT_2) {
		mouseSetBounds(MOUSE_PORT_2, 0, 0, SCREEN_PAL_WIDTH - 1, SCREEN_PAL_HEIGHT - 1);
		mouseResetPos(MOUSE_PORT_2);
	}
}

void mouseDestroy(void) {
}

void mouseProcess(void) {
	struct SDL_Window *pWin = sdlGetWindow();
	int winW = 640;
	int winH = 512;
	if(pWin) {
		SDL_GetWindowSize(pWin, &winW, &winH);
	}
	if(winW < 1) {
		winW = 640;
	}
	if(winH < 1) {
		winH = 512;
	}

	int mx, my;
	Uint32 btns = SDL_GetMouseState(&mx, &my);

	long lx = (long)mx * (long)SCREEN_PAL_WIDTH / (long)winW;
	long ly = (long)my * (long)SCREEN_PAL_HEIGHT / (long)winH;
	if(lx < 0) {
		lx = 0;
	}
	if(ly < 0) {
		ly = 0;
	}
	if(lx > SCREEN_PAL_WIDTH - 1) {
		lx = SCREEN_PAL_WIDTH - 1;
	}
	if(ly > SCREEN_PAL_HEIGHT - 1) {
		ly = SCREEN_PAL_HEIGHT - 1;
	}

	if(g_sMouseManager.ubPortFlags & MOUSE_PORT_1) {
		mouseProcessPortFromSdl(MOUSE_PORT_1, (int)lx, (int)ly, btns);
	}
	if(g_sMouseManager.ubPortFlags & MOUSE_PORT_2) {
		mouseProcessPortFromSdl(MOUSE_PORT_2, (int)lx, (int)ly, btns);
	}
}
