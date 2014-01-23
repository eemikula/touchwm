#ifndef _SCREEN_H
#define _SCREEN_H

#include <xcb/xcb.h>

#include "window.h"

class Screen {
public:
	Screen(xcb_connection_t *connection, xcb_screen_t *screen){this->connection = connection; this->screen = screen;}

	Window GetRoot(){return Window(connection,screen->root);}

	operator xcb_screen_t* (){return screen;}

private:
	xcb_connection_t *connection;
	xcb_screen_t *screen;
};

#endif

