#ifndef _WM_H
#define _WM_H

#include <list>
#include <vector>

#include <xcb/xcb.h>
#include <linux/uinput.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include "screen.h"
#include "window.h"

#define MAX_TOUCHES 10

typedef std::list<Screen> ScreenList;

class WindowManager {
public:
	WindowManager();
	~WindowManager();

	ScreenList GetScreens();
	bool Redirect(Screen &screen);
	void AddWindow(Window &window);
	xcb_generic_event_t *WaitForEvent();
	void ListDevices();

	void HandleEvent(xcb_generic_event_t *e);

private:

	int uinput_fd;
	uinput_user_dev user_dev;

	struct Touch {
		Touch(unsigned int id, xcb_window_t w, int x, int y, int root_x, int root_y){
			this->id = id;
			this->window = w;
			this->x = root_x;
			this->y = root_y;
			this->xoff = x;
			this->yoff = y;
			this->moved = false;
		}

		xcb_window_t window;
		int x, y;
		int xoff, yoff;
		unsigned int id;
		bool moved;
	};

	typedef std::vector<Touch> TouchList;
	typedef std::list<Window> WindowList;

	xcb_connection_t *connection;
	TouchList touch;
	bool captureTouch;
	WindowList windows;
	Window *clickWindow;
	Window *touchWindow;
	int xoff, yoff;

	void OutputError(xcb_generic_error_t &e);

	Touch *GetTouch(unsigned int id);
	Window *GetWindow(xcb_window_t w);

	void SelectWindow(Window &w);
	void DeselectWindow();
	void MaximizeWindow(xcb_window_t window, xcb_window_t root);

	// Event handlers
	void HandleMapRequest(xcb_map_request_event_t &e);
	void HandleClientMessage(xcb_client_message_event_t &e);
	void HandleConfigureRequest(xcb_configure_request_event_t &e);
	void HandleButtonPress(xcb_button_press_event_t &e);
	void HandleButtonRelease(xcb_button_release_event_t &e);
	void HandleMotion(xcb_motion_notify_event_t &e);
	void HandleTouchBegin(xcb_input_touch_begin_event_t &e);
	void HandleTouchUpdate(xcb_input_touch_update_event_t &e);
	void HandleTouchEnd(xcb_input_touch_end_event_t &e);

	void SendEvent(xcb_window_t window, xcb_generic_event_t &e, xcb_event_mask_t mask);

	// stored atoms
	xcb_atom_t 	atom_NET_WM_STATE,
			atom_NET_WM_STATE_MAXIMIZED_HORZ,
			atom_NET_WM_STATE_MAXIMIZED_VERT,
			atom_NET_WM_WINDOW_OPACITY;

};

struct EventThreadArg {
	EventThreadArg(WindowManager &wm) : wm(wm) {}
	WindowManager &wm;
};

#endif
