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
	bool Redirect(Screen &screen);
	void AddWindow(Window &window);
	xcb_generic_event_t *WaitForEvent();

	void HandleEvent(xcb_generic_event_t *e);

	static void *EventThread(void *arg);

private:
	xcb_connection_t *connection;
	xcb_window_t clickWindow;
	int xoff, yoff;

	void HandleMapRequest(xcb_map_request_event_t &e);
	void HandleButtonPress(xcb_button_press_event_t &e);
	void HandleButtonRelease(xcb_button_release_event_t &e);
	void HandleMotion(xcb_motion_notify_event_t &e);

	void SendEvent(xcb_window_t window, xcb_generic_event_t &e, xcb_event_mask_t mask);
};

struct EventThreadArg {
	EventThreadArg(WindowManager &wm) : wm(wm) {}
	WindowManager &wm;
};

#endif
