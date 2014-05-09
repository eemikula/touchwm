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
#include "wmwindow.h"

#define MAX_TOUCHES 10

typedef std::list<Screen> ScreenList;

class WindowManager {
public:
	WindowManager();
	~WindowManager();

	ScreenList GetScreens();
	Screen *GetScreen(xcb_window_t root);
	bool Redirect(Screen &screen, bool replace);
	void AddWindow(Window &window, bool focus);
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
			this->unaccepted = true;
		}

		xcb_window_t window;
		int x, y;
		int xoff, yoff;
		unsigned int id;
		bool moved;
		bool unaccepted;
	};

	typedef std::vector<Touch> TouchList;
	typedef std::list<Window> WindowList;

	xcb_connection_t *connection;
	ScreenList screens;
	TouchList touch;
	bool captureTouch;

	/*
	 * EWMH requires that the window manager maintain both a list of the windows
	 * being managed, sorted by age, as well as a list of the same windows by stacking
	 * order. These lists are not directly stored in memory here - they are constructed.
	 * "topWindows" is a list of all windows marked as "above," "bottomWindows" is a
	 * list of all windows marked as "below," and "windows" is a list of all other
	 * windows. Each of these lists is sorted by stacking order, where the front is
	 * highest, and back is lowest.
	 */
	WindowList windows;
	WindowList topWindows;
	WindowList bottomWindows;

	Window *clickWindow;
	Window *touchWindow;
	int xoff, yoff;
	WMWindow *wmMenu;

	void OutputError(xcb_generic_error_t &e);

	Touch *GetTouch(unsigned int id);
	Window *GetWindow(xcb_window_t w);

	void SelectWindow(Window &w);
	void DeselectWindow();
	void MaximizeWindow(xcb_window_t window, xcb_window_t root, bool maxHorz = true, bool maxVert = true);
	void ChangeWMState(xcb_window_t window, xcb_window_t root, WMStateChange change, WMState state);
	void CloseWindow(xcb_window_t window, xcb_window_t root);
	void DeleteWindow(xcb_window_t window);
	void RaiseWindow(Window &w, bool focus = false);
	void GrabTouch(Window &w);

	void AcceptTouch(xcb_input_touch_begin_event_t t);
	void RejectTouch(xcb_input_touch_begin_event_t t);

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
	void HandleSelectionClear(xcb_selection_clear_event_t &e);
	void HandleDestroyNotify(xcb_destroy_notify_event_t &e);
	void HandleConfigureNotify(xcb_configure_notify_event_t &e);
	void HandleExpose(xcb_expose_event_t &e);

	void SendEvent(xcb_window_t window, xcb_generic_event_t &e, xcb_event_mask_t mask);

};

struct EventThreadArg {
	EventThreadArg(WindowManager &wm) : wm(wm) {}
	WindowManager &wm;
};

#endif
