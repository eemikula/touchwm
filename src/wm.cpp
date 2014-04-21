/*
Touchscreen Window Manager prototype
*/

#include "config.h"

#include <iostream>
#include <list>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <xcb/xcb.h>
#include <xcb/xinput.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#ifdef HAVE_LIBXCB_UTIL
#include <xcb/xcb_util.h>
#endif

extern "C" {
#include <suinput.h>
}

#include "wm.h"
#include "windowsystem.h"

const double OPACITY_MAX = 1.0;
const double OPACITY_MIN = 0.1;
const double OPACITY_HALF = 0.5;
const double OPACITY_TRANSLUCENT = 0.75;

int main(int argc, char* argv[]){

	WindowManager wm;
	ScreenList screens = wm.GetScreens();
	if (screens.size() == 0){
		std::cerr << "No screens found\n";
		return 1;
	}

	for (ScreenList::iterator itr = screens.begin(); itr != screens.end(); itr++){
		Screen &s = *itr;
		if (!wm.Redirect(s)){
			std::cerr << "Unable to apply substructure redirect\n";
			return 1;
		}

	}

	while (xcb_generic_event_t *e = wm.WaitForEvent()){
		wm.HandleEvent(e);
		free(e);
	}

	return 0;
}

WindowManager::WindowManager(){

	// Because of the nature of X11, mouse events produced by a client are
	// not reliably handled by applications. In order to work around this,
	// libsuinput is used as a wrapper around systemd / udev to simulate
	// mouse input. Because this behavior is only needed for translating
	// touch input into other events like a right click, failure to
	// initialize this component is not considered to be a fatal error.
	memset(&user_dev, 0, sizeof(struct uinput_user_dev));
	strcpy(user_dev.name, "libsuinput-example-mouse");
	uinput_fd = suinput_open();
	if (suinput_enable_event(uinput_fd, EV_REL, REL_X) == -1)
		std::cerr << "enable x error\n";
	if (suinput_enable_event(uinput_fd, EV_REL, REL_Y) == -1)
		std::cerr << "enable y error\n";
	if (suinput_enable_event(uinput_fd, EV_KEY, BTN_LEFT) == -1)
		std::cerr << "enable button left error\n";
	if (suinput_enable_event(uinput_fd, EV_KEY, BTN_RIGHT) == -1)
		std::cerr << "enable button right error\n";
	if (suinput_create(uinput_fd, &user_dev) == -1)
		std::cerr << "Create error\n";

	// The window manager shares its connection to the X server with many
	// other components. To minimize the extent to which this information
	// must be passed around, it is managed with the singleton WindowSystem
	// class, which provides shared access to a single instance of the
	// connection to the X server.
	connection = WindowSystem::Get();
	clickWindow = 0;
	touchWindow = 0;
	captureTouch = false;

	if (connection == NULL){
		std::cerr << "Unable to get connection\n";
		//FIXME: Throw an error here
		return;
	}

	xcb_input_xi_query_version_cookie_t c = xcb_input_xi_query_version_unchecked(connection, 2, 2);
	xcb_input_xi_query_version_reply_t *r = xcb_input_xi_query_version_reply(connection, c, NULL);
	std::cout << "xcb input version: " << r->major_version << "." << r->minor_version << "\n";
	if (r)
		free(r);

	/*xcb_atom_t atoms[] = {
		ewmh()._NET_WM_STATE,
		ewmh()._NET_WM_STATE_MAXIMIZED_VERT,
		ewmh()._NET_WM_STATE_MAXIMIZED_HORZ
	};
	xcb_ewmh_set_supported(&ewmh(), 0, 3, atoms);*/

	std::cout << "_NET_WM_STATE: " << ewmh()._NET_WM_STATE << "\n";
	std::cout << "_NET_WM_STATE_MAXIMIZED_VERT: " << ewmh()._NET_WM_STATE_MAXIMIZED_VERT << "\n";
	std::cout << "_NET_WM_STATE_MAXIMIZED_HORZ: " << ewmh()._NET_WM_STATE_MAXIMIZED_HORZ << "\n";

}

void WindowManager::ListDevices(){

	xcb_generic_error_t *error;
	xcb_input_xi_query_device_cookie_t cookie = xcb_input_xi_query_device_unchecked(connection, XCB_INPUT_DEVICE_ALL);
	xcb_input_xi_query_device_reply_t *reply = xcb_input_xi_query_device_reply(connection, cookie, &error);
	if (error){
		std::cerr << "query device error\n";
		free(error);
	}
	if (reply)
		free(reply);

	xcb_input_xi_device_info_iterator_t itr = xcb_input_xi_query_device_infos_iterator(reply);
	for (; itr.rem; xcb_input_xi_device_info_next(&itr)){
		std::cout << "Device id: " << itr.data->deviceid << "\n";
		std::cout << "\tname: " << xcb_input_xi_device_info_name(itr.data) << "\n";
		xcb_input_device_class_iterator_t i = xcb_input_xi_device_info_classes_iterator(itr.data);
		for (; i.rem; xcb_input_device_class_next(&i)){
			/*if (i.data->type != XCB_INPUT_DEVICE_CLASS_TYPE_TOUCH)
				std::cout << "\tsupports touch\n";
			else
				continue;*/
		}
	}
}

WindowManager::~WindowManager(){
	suinput_destroy(uinput_fd);
}

ScreenList WindowManager::GetScreens(){
	ScreenList l;

	const xcb_setup_t *setup = xcb_get_setup(connection);
	if (setup == NULL)
		return l;

	int roots = xcb_setup_roots_length(setup);
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
	for (; iter.rem; xcb_screen_next(&iter))
		l.push_back(Screen(iter.data));
	return l;
}

/*
 * This method initiates a substructure redirect on the specified screen. This will
 * cause relevant events to be reported to the window manager. This will fail if
 * anything else has already obtained the substructure redirect.
 *
 * Parameters:
 * 	screen: The screen on which the redirect is applied
 */
bool WindowManager::Redirect(Screen &screen){

	const static uint32_t values[] = {	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT };

	Window rootWindow = screen.GetRoot();
	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(connection, rootWindow, XCB_CW_EVENT_MASK, values);
	xcb_generic_error_t *error;
	if (error = xcb_request_check(connection, cookie)) {
		free(error);
		return false;
	} else {
		std::cout << "Redirect successful\n";
	}

	// Manage any existing toplevel windows
	WindowList topLevel = rootWindow.GetChildren();
	for (WindowList::iterator itr = topLevel.begin(); itr != topLevel.end(); itr++)
		AddWindow(*itr);

	// Also manage the root window itself
	AddWindow(rootWindow);

	return true;
}

/*
 * This method adds a window to the list of windows being managed. In addition,
 * it applies the necessary grabs to track mouse and touch input to the window
 * and respond to it if necessary.
 *
 * Parameters:
 * 	window: The window to be managed
 */
void WindowManager::AddWindow(Window &window){
	windows.push_back(window);

	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;

	xcb_grab_button(connection,
						false,
						window,
						XCB_EVENT_MASK_BUTTON_PRESS |
						XCB_EVENT_MASK_BUTTON_RELEASE |
						//XCB_EVENT_MASK_POINTER_MOTION,// |
						XCB_EVENT_MASK_BUTTON_1_MOTION,
						XCB_GRAB_MODE_SYNC,
						XCB_GRAB_MODE_ASYNC,
						XCB_NONE, XCB_NONE,
						XCB_BUTTON_INDEX_ANY,
						//XCB_MOD_MASK_1 | XCB_MOD_MASK_2);
						XCB_MOD_MASK_ANY);
	//xcb_flush(connection);

	const uint32_t mask[] = {XCB_INPUT_XI_EVENT_MASK_TOUCH_BEGIN
							| XCB_INPUT_XI_EVENT_MASK_TOUCH_UPDATE
							| XCB_INPUT_XI_EVENT_MASK_TOUCH_END
	};
	const uint32_t modifiers[] = {XCB_INPUT_MODIFIER_MASK_ANY};
	xcb_input_xi_passive_grab_device_cookie_t c = xcb_input_xi_passive_grab_device(
			connection,
			XCB_CURRENT_TIME,
			window,
			XCB_CURSOR_NONE,
			0, // detail - as used by XIPassiveGrab
			XCB_INPUT_DEVICE_ALL_MASTER,
			1, // num_modifiers
			1, // mask_len
			XCB_INPUT_GRAB_TYPE_TOUCH_BEGIN,
			XCB_INPUT_GRAB_MODE_22_TOUCH,
			XCB_INPUT_GRAB_MODE_22_ASYNC,
			XCB_INPUT_GRAB_OWNER_NO_OWNER,
			mask,
			modifiers);
	xcb_input_xi_passive_grab_device_reply_t *r = xcb_input_xi_passive_grab_device_reply(connection, c, &error);
	if (error){
		OutputError(*error);
		free(error);
	}

	//TODO: Apply any pre-existing maximization here!
	if (window.GetWMState(MAXIMIZED_HORZ | MAXIMIZED_VERT))
		window.Maximize(SET);

	xcb_flush(connection);
}

void WindowManager::OutputError(xcb_generic_error_t &e){
#ifdef HAVE_LIBXCB_UTIL
		std::cerr << "Received error: " << xcb_event_get_error_label(e.error_code) << " major: " << (int)e.major_code << " minor: " << (int)e.minor_code << "\n";
#else
		std::cerr << "Received x11 error: " << (int)e.error_code << ", major: " << (int)e.major_code << ", minor: " << (int)e.minor_code << "\n";
#endif
}

/*
 * This method returns the Touch object that matches the specified touch
 * sequence identifier, if it exists. Returns NULL otherwise.
 *
 * Parameters:
 * 	id: The touch sequence identifier
 *
 * Return value:
 * 	The Touch object for the specified touch sequence identifier
 */
WindowManager::Touch *WindowManager::GetTouch(unsigned int id){
	Touch *t = NULL;
	for (TouchList::iterator itr = touch.begin(); itr != touch.end(); itr++){
		if (itr->id == id)
			return &(*itr);
	}
	return t;
}

/*
 * This method returns the Window object that matches the specified
 * window identifier, if it exists. Returns NULL otherwise.
 *
 * Parameters:
 * 	w: The XCB window identifier
 *
 * Return value:
 * 	The Window object for the specified xcb window
 */
Window *WindowManager::GetWindow(xcb_window_t w){
	Window *win = NULL;
	for (WindowList::iterator itr = windows.begin(); itr != windows.end(); itr++)
		if (*itr == w)
			return &(*itr);
	return win;
}

void WindowManager::SelectWindow(Window &w){
}

/*
 * This method releases a window touch. It releases the window manager from
 * tracking the window and the touch sequence, and also resets its opacity.
 */
void WindowManager::DeselectWindow(){
	captureTouch = false;
	if (touchWindow)
		touchWindow->SetOpacity(OPACITY_MAX);
	touchWindow = NULL;
}

xcb_generic_event_t *WindowManager::WaitForEvent(){
	return xcb_wait_for_event(connection);
}

void WindowManager::HandleEvent(xcb_generic_event_t *e){

	// &0x7f in order to filter out the most significant bit, which
	// indicates whether the event was produced by the X server or by
	// a client. This distinction is ignored here.
	uint8_t event_type = e->response_type&0x7f;
	switch (event_type){
	case 0:{
		OutputError(*(xcb_generic_error_t*)e);
		break;
	}
	case XCB_MAP_REQUEST:{
		HandleMapRequest(*(xcb_map_request_event_t*)e);
		break;
	}
	case XCB_CONFIGURE_REQUEST:{
		HandleConfigureRequest(*(xcb_configure_request_event_t*)e);
		break;
	}
	case XCB_CLIENT_MESSAGE:{
		HandleClientMessage(*(xcb_client_message_event_t*)e);
		break;
	}
	case XCB_BUTTON_PRESS:{
		HandleButtonPress(*(xcb_button_press_event_t*)e);
		break;
	}
	case XCB_BUTTON_RELEASE:{
		HandleButtonRelease(*(xcb_button_release_event_t*)e);
		break;
	}
	case XCB_MOTION_NOTIFY:{
		HandleMotion(*(xcb_motion_notify_event_t*)e);
		break;
	}
	case XCB_GE_GENERIC:{
		xcb_ge_generic_event_t *ge = (xcb_ge_generic_event_t*)e;
		switch (ge->event_type){
		case XCB_INPUT_TOUCH_BEGIN:
			HandleTouchBegin(*(xcb_input_touch_begin_event_t*)e);
			break;
		case XCB_INPUT_TOUCH_UPDATE:
			HandleTouchUpdate(*(xcb_input_touch_update_event_t*)e);
			break;
		case XCB_INPUT_TOUCH_END:
			HandleTouchEnd(*(xcb_input_touch_end_event_t*)e);
			break;
		default:
			std::cout << "Unknown generic event " << ge->event_type << "\n";
		}
		break;
	}

	// currently ignored
	case XCB_UNMAP_NOTIFY:
	case XCB_MAP_NOTIFY:
	case XCB_DESTROY_NOTIFY:
	case XCB_CREATE_NOTIFY:
	case XCB_CONFIGURE_NOTIFY:
		break;

	default:
		std::cout << "Unknown event " << (int)event_type
#ifdef HAVE_LIBXCB_UTIL
		<< " - " << xcb_event_get_label(event_type)
#endif
		<< "\n";
		break;
	}
}

void WindowManager::HandleMapRequest(xcb_map_request_event_t &e){
	Window window(e.window, e.parent);
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;
	cookie = xcb_map_window_checked(connection, e.window);
	error = xcb_request_check(connection, cookie);
	AddWindow(window);
	xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, e.window, XCB_CURRENT_TIME);
}

void WindowManager::HandleClientMessage(xcb_client_message_event_t &e){
	if (e.type == ewmh()._NET_WM_STATE){

		Window *w = GetWindow(e.window);
		if (w == NULL)
			return;

		if (e.format != 32){
			std::cerr << "Unexpected format: " << e.format << "\n";
			return;
		}

		bool maxVert = false;
		bool maxHorz = false;
		if (e.data.data32[1] == ewmh()._NET_WM_STATE_MAXIMIZED_VERT
			|| e.data.data32[2] == ewmh()._NET_WM_STATE_MAXIMIZED_VERT)
			maxVert = true;
		if (e.data.data32[1] == ewmh()._NET_WM_STATE_MAXIMIZED_HORZ
			|| e.data.data32[2] == ewmh()._NET_WM_STATE_MAXIMIZED_HORZ)
			maxHorz = true;

		switch (e.data.data32[0]){
		case 0: // remove
			if (maxVert && maxHorz)
				w->Maximize(CLEAR);
			break;
		case 1: // add
			if (maxVert && maxHorz)
				w->Maximize(SET);
			break;
		case 2: // toggle
			if (maxVert && maxHorz)
				w->Maximize(TOGGLE);
			break;
		default:
			std::cerr << "Unknown operation " << e.data.data32[0] << "\n";
		}
	} else {
		xcb_get_atom_name_cookie_t cc = xcb_get_atom_name(connection, e.type);
		xcb_get_atom_name_reply_t *cr = xcb_get_atom_name_reply(connection, cc, NULL);
		std::cout << "\tname: " << xcb_get_atom_name_name(cr) << "\n";
		std::cout << "\tevent.data.data32[0] = " << e.data.data32[0] << "\n";
		std::cout << "\tevent.data.data32[1] = " << e.data.data32[1] << "\n";
		std::cout << "\tevent.data.data32[2] = " << e.data.data32[2] << "\n";
	}
}

void WindowManager::HandleConfigureRequest(xcb_configure_request_event_t &e){
	Window *w = GetWindow(e.window);
	if (w){
		uint32_t values[7];
		int i = 0;
		if (e.value_mask & XCB_CONFIG_WINDOW_X)
			values[i++] = e.x;
		if (e.value_mask & XCB_CONFIG_WINDOW_Y)
			values[i++] = e.y;
		if (e.value_mask & XCB_CONFIG_WINDOW_WIDTH)
			values[i++] = e.width;
		if (e.value_mask & XCB_CONFIG_WINDOW_HEIGHT)
			values[i++] = e.height;
		if (e.value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
			values[i++] = e.border_width;
		if (e.value_mask & XCB_CONFIG_WINDOW_SIBLING)
			values[i++] = e.sibling;
		if (e.value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
			values[i++] = e.stack_mode;
		w->Configure(e.value_mask, values);
	}
}

void WindowManager::SendEvent(xcb_window_t window, xcb_generic_event_t &event, xcb_event_mask_t mask){
	xcb_void_cookie_t cookie = xcb_send_event_checked(connection, false, window, mask, (char*)&event);
	xcb_request_check(connection, cookie);
	xcb_flush(connection);
}

void WindowManager::HandleButtonPress(xcb_button_press_event_t &e){

	// always raise window on press
	xcb_void_cookie_t cookie;
	const static uint32_t values[] = { XCB_STACK_MODE_ABOVE };
	xcb_configure_window(connection, e.event, XCB_CONFIG_WINDOW_STACK_MODE, values);
	xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, e.event, XCB_CURRENT_TIME);

	// if moving
	if (e.state & XCB_MOD_MASK_1){
		xoff = e.event_x;
		yoff = e.event_y;
		clickWindow = GetWindow(e.event);
		xcb_allow_events(connection, XCB_ALLOW_SYNC_POINTER, XCB_CURRENT_TIME);
		xcb_flush(connection);
	} else {
		xcb_allow_events(connection, XCB_ALLOW_REPLAY_POINTER, XCB_CURRENT_TIME);
		xcb_flush(connection);
	}
}

void WindowManager::HandleButtonRelease(xcb_button_release_event_t &e){

	clickWindow = 0;
	xcb_flush(connection);
	xcb_allow_events(connection, XCB_ALLOW_SYNC_POINTER, XCB_CURRENT_TIME);
	xcb_flush(connection);

}

void WindowManager::HandleMotion(xcb_motion_notify_event_t &e){

	xcb_allow_events(connection, XCB_ALLOW_SYNC_POINTER, XCB_CURRENT_TIME);
	xcb_flush(connection);

	if (clickWindow){
		int x = e.root_x-xoff, y = e.root_y-yoff;
		clickWindow->Move(e.root_x-xoff, e.root_y-yoff);
	}
}

void WindowManager::HandleTouchBegin(xcb_input_touch_begin_event_t &e){

	xcb_window_t event = e.event;
	xcb_window_t root = e.root;
	xcb_window_t child = e.child;
	if (event == root){
		if (child == root)
			DeselectWindow();
		captureTouch = false;
		xcb_input_xi_allow_events(connection, XCB_CURRENT_TIME, e.deviceid, XCB_INPUT_EVENT_MODE_REJECT_TOUCH, e.detail, e.event);
		xcb_flush(connection);
		return;
	}

	// always raise window on press
	if (captureTouch == false){
		xcb_void_cookie_t cookie;
		const static uint32_t values[] = { XCB_STACK_MODE_ABOVE };
		xcb_configure_window(connection, event, XCB_CONFIG_WINDOW_STACK_MODE, values);
		xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, event, XCB_CURRENT_TIME);
	}

	Window *w = GetWindow(event);
	touch.push_back(Touch(e.detail, event, e.event_x / 65536.0, e.event_y / 65536.0, e.root_x / 65536.0, e.root_y / 65536.0));

	if (touch.size() == 1 && w != touchWindow){
		DeselectWindow();
	}
	else if (touch.size() == 1){
		captureTouch = false;
	}
	else if (touch.size() == 2 && captureTouch == false){
		captureTouch = true;
		if (w){
			w->SetOpacity(OPACITY_TRANSLUCENT);
			touchWindow = w;
		}
	}
	else if (touch.size() == 3 && w){
		MaximizeWindow(*w, root);
	}

}

void WindowManager::HandleTouchUpdate(xcb_input_touch_update_event_t &e){

	xcb_window_t event = e.event;
	xcb_window_t root = e.root;
	xcb_window_t child = e.child;
	if (event == root){
		if (child == root)
			DeselectWindow();
		captureTouch = false;
		xcb_input_xi_allow_events(connection, XCB_CURRENT_TIME, e.deviceid, XCB_INPUT_EVENT_MODE_REJECT_TOUCH, e.detail, e.event);
		xcb_flush(connection);
		return;
	}

	double x = e.root_x / 65536.0;
	double y = e.root_y / 65536.0;
	Touch *t = GetTouch(e.detail);
	if (!t){
		std::cerr << "Unknown touch\n";
		return;
	}

	if (touch.size() == 1 && touchWindow == NULL){
		xcb_input_xi_allow_events(connection, XCB_CURRENT_TIME, e.deviceid, XCB_INPUT_EVENT_MODE_REJECT_TOUCH, e.detail, e.event);
		xcb_flush(connection);
		t->x = x;
		t->y = y;
		t->moved = true;
	} else if (touch.size() == 1 && touchWindow != NULL){
		t->x = x;
		t->y = y;
		touchWindow->Maximize(CLEAR);
		touchWindow->Move(t->x-t->xoff, t->y-t->yoff);
		xcb_flush(connection);
		captureTouch = true;
	} else if (touch.size() == 2){
		captureTouch = true;
		Touch *ot = NULL;
		if (&touch[0] == t)
			ot = &touch[1];
		else
			ot = &touch[0];

		// determine the change in distance between the two touches
		int dx = abs(touch[0].x-touch[1].x);
		int dy = abs(touch[0].y-touch[1].y);

		bool xshift = false, yshift = false;
		if (x < ot->x)
			xshift = true;
		if (y < ot->y)
			yshift = true;

		// update the touch
		t->x = x;
		t->y = y;
		t->moved = true;

		dx = abs(touch[0].x-touch[1].x)-dx;
		dy = abs(touch[0].y-touch[1].y)-dy;

		Window *w = GetWindow(t->window);
		if (w){
			w->Maximize(CLEAR);
			w->Expand(dx, dy, xshift, yshift);
		}

	} else {
		t->x = x;
		t->y = y;
		t->moved = true;
	}

}

void WindowManager::HandleTouchEnd(xcb_input_touch_end_event_t &e){

	xcb_window_t event = e.event;
	xcb_window_t root = e.root;
	xcb_window_t child = e.child;
	if (event == root){
		if (child == root)
			DeselectWindow();
		captureTouch = false;
		xcb_input_xi_allow_events(connection, XCB_CURRENT_TIME, e.deviceid, XCB_INPUT_EVENT_MODE_REJECT_TOUCH, e.detail, e.event);
		xcb_flush(connection);
		return;
	}

	if (!captureTouch){
		xcb_input_xi_allow_events(connection, XCB_CURRENT_TIME, e.deviceid, XCB_INPUT_EVENT_MODE_REJECT_TOUCH, e.detail, e.event);
		xcb_flush(connection);
	} else {
		//xcb_input_xi_allow_events(connection, XCB_CURRENT_TIME, e.deviceid, XCB_INPUT_EVENT_MODE_ACCEPT_TOUCH, e.detail, e.event);
	}

	bool moved = false;
	for (TouchList::iterator itr = touch.begin(); itr != touch.end(); itr++){
		if (itr->id == e.detail){
			moved = itr->moved;
			touch.erase(itr);
			break;
		}
	}

	Window *w = GetWindow(event);

	if (touch.size() == 0 && moved == false && captureTouch == false){
		DeselectWindow();
	} else if (touch.size() == 1 && moved == false && w == touchWindow && captureTouch == false){

		/*int r;
		r = suinput_emit(uinput_fd, EV_KEY, BTN_RIGHT, 1);
		if (r != -1)
			r = suinput_emit(uinput_fd, EV_KEY, BTN_RIGHT, 0);
		if (r != -1)
			r = suinput_syn(uinput_fd);
		if (r == -1)
			std::cerr << "Error occurred simulating mouse click\n";
		DeselectWindow();*/

	}

	// always release captureTouch if there are no other touches
	if (touch.size() == 0)
		captureTouch = false;

}

/*
 * This method toggles the vertical and horizontal maximized states of 
 * the specified window, using extended window manager hints. It ultimately
 * produces a CLIENT_MESSAGE event for the window manager. The actual size
 * changes are managed by the Window object in response to this event - this
 * ensures that the correct behavior results even if the state is changed
 * by something other than the window manager.
 *
 * Parameters:
 * 	window: the window to maximize
 * 	root: the root window of "window"
 */
void WindowManager::MaximizeWindow(xcb_window_t window, xcb_window_t root){

	xcb_client_message_event_t event;
	event.response_type = XCB_CLIENT_MESSAGE;
	event.format = 32;
	event.sequence = 0;
	event.window = window;
	event.type = ewmh()._NET_WM_STATE;
	event.data.data32[0] = 2L; // 0 = remove, 1 = add, 2 = toggle
	event.data.data32[1] = ewmh()._NET_WM_STATE_MAXIMIZED_HORZ;
	event.data.data32[2] = ewmh()._NET_WM_STATE_MAXIMIZED_VERT;
	event.data.data32[3] = 0L;
	event.data.data32[4] = 0L;

	// send the event. Note the event mask is derived from source for xcb-ewmh
	xcb_send_event(connection, false, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char*)&event);

	// flush to make sure the event is processed before the object
	// goes out of scope
	xcb_flush(connection);

}

