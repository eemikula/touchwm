#ifndef _SCREEN_H
#define _SCREEN_H

#include <xcb/xcb.h>

#include "window.h"

class Screen {
public:
	Screen(xcb_screen_t *screen){this->screen = screen;}

	Window GetRoot(){return Window(screen->root,screen->root);}

	operator xcb_screen_t* (){return screen;}

private:
	xcb_screen_t *screen;
};

#endif

