#ifndef _WM_H
#define _WM_H

#include <list>

#include <xcb/xcb.h>

#include "screen.h"
#include "window.h"

typedef std::list<Screen> ScreenList;

class WindowManager {
public:
	WindowManager();

	ScreenList GetScreens();
	void Redirect(Screen &screen);
	void AddWindow(Window &window);
	xcb_generic_event_t *WaitForEvent();

	void HandleEvent(xcb_generic_event_t *e);

private:
	xcb_window_t root;
	xcb_connection_t *connection;
	xcb_window_t clickWindow;
	int xoff, yoff;

	void HandleEvent(xcb_map_request_event_t &e);
	void HandleButtonPressEvent(xcb_button_press_event_t &e);
	void HandleButtonReleaseEvent(xcb_button_release_event_t &e);
	void HandleEvent(xcb_motion_notify_event_t &e);

	void SendEvent(xcb_window_t window, xcb_generic_event_t &e, xcb_event_mask_t mask);
};

#endif
