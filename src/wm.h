#ifndef _WM_H
#define _WM_H

#include <list>
#include <vector>

#include <xcb/xcb.h>

#include "screen.h"
#include "window.h"

#define MAX_TOUCHES 10

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

	struct Touch {
		Touch(unsigned int id, xcb_window_t w, int x, int y, int root_x, int root_y){
			this->id = id;
			this->window = w;
			this->x = root_x;
			this->y = root_y;
			this->xoff = x;
			this->yoff = y;
		}

		/*bool operator == (const Touch &t) const {
			return window == t.window
					&& x == t.x && y == t.y
					&& xoff == t.xoff && yoff == t.yoff;
		}*/

		xcb_window_t window;
		int x, y;
		int xoff, yoff;
		unsigned int id;
	};

	typedef std::vector<Touch> TouchList;

	xcb_connection_t *connection;
	TouchList touch;
	xcb_window_t clickWindow;
	int xoff, yoff;

	void OutputError(xcb_generic_error_t &e);

	Touch *GetTouch(unsigned int id);
	Touch *GetNearestTouch(int x, int y);
	//TouchList::iterator GetNearestTouch(int x, int y);

	void HandleMapRequest(xcb_map_request_event_t &e);
	void HandleButtonPress(xcb_button_press_event_t &e);
	void HandleButtonRelease(xcb_button_release_event_t &e);
	void HandleMotion(xcb_motion_notify_event_t &e);
	void HandleTouchBegin(xcb_input_touch_begin_event_t &e);
	void HandleTouchUpdate(xcb_input_touch_update_event_t &e);
	void HandleTouchEnd(xcb_input_touch_end_event_t &e);
	void HandleTouchOwnership(xcb_input_touch_ownership_event_t &e);

	void SendEvent(xcb_window_t window, xcb_generic_event_t &e, xcb_event_mask_t mask);
};

struct EventThreadArg {
	EventThreadArg(WindowManager &wm) : wm(wm) {}
	WindowManager &wm;
};

#endif
