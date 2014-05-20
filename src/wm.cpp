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
#include <xcb/xcb_keysyms.h>

// taken from X11/keysymdef.h
#define XK_Tab                           0xff09

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

	bool replaceWm = false;
	for (int a = 1; a < argc; a++){
		if (argv[a][0] == '-'){
			if (strcmp(argv[a],"--replace") == 0){
				replaceWm = true;
			} else {
				std::cerr << "Unknown option " << argv[a] << "\n";
				return 1;
			}
		} else {
			
		}
	}

	WindowManager wm;
	ScreenList screens = wm.GetScreens();
	if (screens.size() == 0){
		std::cerr << "No screens found\n";
		return 1;
	}

	int i = 0;
	for (ScreenList::iterator itr = screens.begin(); itr != screens.end(); itr++, i++){
		Screen &s = *itr;
		xcb_get_selection_owner_cookie_t oc = xcb_get_selection_owner(xcb(),ewmh()._NET_WM_CM_Sn[i]);
		xcb_get_selection_owner_reply_t *r = xcb_get_selection_owner_reply(xcb(), oc, NULL);
		if (!r || !r->owner)
			std::cout << "No compositor is running. This will significantly impact user experience! Consider starting xcompmgr.\n";
		
		if (!wm.Redirect(s, replaceWm)){
			std::cerr << "Unable to apply substructure redirect\n";
			return 1;
		}
	}

	while (xcb_generic_event_t *e = wm.WaitForEvent()){
		if (!e)
			break;
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
	if (uinput_fd == 0 ||
		suinput_enable_event(uinput_fd, EV_REL, REL_X) == -1 ||
		suinput_enable_event(uinput_fd, EV_REL, REL_Y) == -1 ||
		suinput_enable_event(uinput_fd, EV_KEY, BTN_LEFT) == -1 ||
		suinput_enable_event(uinput_fd, EV_KEY, BTN_RIGHT) == -1 ||
		suinput_create(uinput_fd, &user_dev) == -1)
			std::cerr << "Error inititializing suinput. Cannot simulate mouse input\n";

	// The window manager shares its connection to the X server with many
	// other components. To minimize the extent to which this information
	// must be passed around, it is managed with the singleton WindowSystem
	// class, which provides shared access to a single instance of the
	// connection to the X server.
	clickWindow = 0;
	touchWindow = 0;
	captureTouch = false;
	wmMenu = NULL;
	touchGrab = true;

	if (xcb() == NULL){
		std::cerr << "Unable to get connection\n";
		//FIXME: Throw an error here
		return;
	}

	xcb_input_xi_query_version_cookie_t c = xcb_input_xi_query_version_unchecked(xcb(), 2, 2);
	xcb_input_xi_query_version_reply_t *r = xcb_input_xi_query_version_reply(xcb(), c, NULL);
	std::cout << "xcb input version: " << r->major_version << "." << r->minor_version << "\n";
	if (r)
		free(r);

	xcb_atom_t atoms[] = {
		ewmh()._NET_WM_STATE,
		ewmh()._NET_WM_STATE_MAXIMIZED_VERT,
		ewmh()._NET_WM_STATE_MAXIMIZED_HORZ,
		ewmh()._NET_WM_STATE_ABOVE
	};
	xcb_ewmh_set_supported(&ewmh(), 0, 3, atoms);

	// Store a list of keycodes for keys that are grabbed
	xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(xcb());
	xcb_keycode_t *keycodes = xcb_key_symbols_get_keycode(keysyms, XK_Tab);
	for (int i = 0; keycodes[i] != XCB_NO_SYMBOL; i++){
		//TODO: deduplicate
		tabKeys.push_back(keycodes[i]);
	}
	free(keycodes);
	free(keysyms);

}

void WindowManager::ListDevices(){

	xcb_generic_error_t *error;
	xcb_input_xi_query_device_cookie_t cookie = xcb_input_xi_query_device_unchecked(xcb(), XCB_INPUT_DEVICE_ALL);
	xcb_input_xi_query_device_reply_t *reply = xcb_input_xi_query_device_reply(xcb(), cookie, &error);
	if (error){
		std::cerr << "query device error\n";
		free(error);
	}
	if (!reply)
		return;

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
	free(reply);
}

WindowManager::~WindowManager(){
	suinput_destroy(uinput_fd);
}

ScreenList WindowManager::GetScreens(){

	if (screens.size())
		return screens;

	const xcb_setup_t *setup = xcb_get_setup(xcb());
	if (setup == NULL)
		return screens;

	int roots = xcb_setup_roots_length(setup);
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
	for (int i = 0; iter.rem; xcb_screen_next(&iter), i++)
		screens.push_back(Screen(iter.data, i));
	return screens;
}

/*
 * This method gets the Screen object that corresponds to the given root window.
 *
 * Parameters:
 * 	root: The root window used to determine the appropriate screen
 *
 * Return value:
 *
 */
Screen *WindowManager::GetScreen(xcb_window_t root){

	for (ScreenList::iterator itr = screens.begin(); itr != screens.end(); itr++){
		if (root == itr->GetRoot())
			return &(*itr);
	}
	return NULL;

}

/*
 * This method initiates a substructure redirect on the specified screen. This will
 * cause relevant events to be reported to the window manager. This will fail if
 * anything else has already obtained the substructure redirect.
 *
 * Parameters:
 * 	screen: The screen on which the redirect is applied
 */
bool WindowManager::Redirect(Screen &screen, bool replace){

	xcb_get_selection_owner_cookie_t oc = xcb_get_selection_owner(xcb(), screen.GetAtom());
	xcb_get_selection_owner_reply_t *r = xcb_get_selection_owner_reply(xcb(), oc, NULL);
	xcb_window_t owner = 0;
	if (r)
		owner = r->owner;
	free(r);

	if (replace){
		if (owner){
			const static uint32_t values[] = { XCB_EVENT_MASK_STRUCTURE_NOTIFY };
			xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(xcb(), owner, XCB_CW_EVENT_MASK, values);
			xcb_generic_error_t *error;
			if (error = xcb_request_check(xcb(), cookie)) {
				std::cerr << "Unable to replace running window manager\n";
				free(error);
				return false;
			}
		} else {
			std::cerr << "No running manager to replace\n";
		}
	} else if (owner){
		std::cerr << "Another window manager is currently active. Consider using --replace\n";
		return false;
	}

	// set this new window as owning the screen
	xcb_void_cookie_t soc = xcb_set_selection_owner(xcb(), screen.GetWindow(), screen.GetAtom(), XCB_CURRENT_TIME);
	xcb_flush(xcb());

	if (replace && owner){
		std::cout << "Waiting for previous window manager to shut down...\n";
		bool shutdown = false;
		while (shutdown == false){
			xcb_generic_event_t *e = WaitForEvent();

			// shutdown if no event, or event destroys previous manager
			if (!e ||
			   (((e->response_type)&0x7f) == XCB_DESTROY_NOTIFY &&
			   ((xcb_destroy_notify_event_t*)e)->window == owner))
				shutdown = true;
			free(e);
		}
	}

	const static uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT 
					 | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
					 | XCB_EVENT_MASK_STRUCTURE_NOTIFY};
	Window rootWindow = screen.GetRoot();
	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(xcb(), rootWindow.GetWindow(), XCB_CW_EVENT_MASK, values);
	xcb_generic_error_t *error;
	if (error = xcb_request_check(xcb(), cookie)) {
		free(error);
		return false;
	}

	// Manage any existing toplevel windows
	WindowList topLevel = rootWindow.GetChildren();
	for (WindowList::iterator itr = topLevel.begin(); itr != topLevel.end(); itr++){
		if ((*itr).OverrideRedirect()){
			//std::cout << "Skipping override redirect window " << (*itr).GetTitle() << "\n";
		} else if ((*itr).IsVisible()){
			//std::cout << "Adding window " << (*itr).GetTitle() << "\n";
			AddWindow(*itr, false);
		} else {
			//std::cout << "Skipping unmapped window " << (*itr).GetTitle() << "\n";
		}
	}

	// Also manage the root window itself
	AddWindow(rootWindow, false);

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
void WindowManager::AddWindow(Window &window, bool focus){

	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;

	xcb_grab_button(xcb(),
			false,
			window.GetWindow(),
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

	GrabTouch(window);

	for (KeyList::iterator itr = tabKeys.begin(); itr != tabKeys.end(); itr++){

		// grab alt+tab
		xcb_grab_key (xcb(), 
		      XCB_EVENT_MASK_KEY_PRESS,
		      window.GetWindow(),
		      XCB_MOD_MASK_1,
		      *itr,
		      XCB_GRAB_MODE_SYNC,
		      XCB_GRAB_MODE_ASYNC);

		// grab shift+alt+tab
		xcb_grab_key (xcb(), 
		      XCB_EVENT_MASK_KEY_PRESS,
		      window.GetWindow(),
		      XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_1,
		      *itr,
		      XCB_GRAB_MODE_SYNC,
		      XCB_GRAB_MODE_ASYNC);
	}

	// if window is root, skip all further processing
	if (!window.GetRootWindow() || window.GetRootWindow() == window.GetWindow())
		return;

	allWindows.push_front(window);

	// apply WM_STATE
	uint32_t state[2];
	state[0] = 1; // 0 = withdrawn, 1 = normal, 3 = iconic
	state[1] = 0;
	xcb_change_property(xcb(), XCB_PROP_MODE_REPLACE, window.GetWindow(), wmAtoms().WM_STATE, wmAtoms().WM_STATE, 32, 2, &state);

	// Apply any pre-existing maximization here
	if (window.GetWMState(MAXIMIZED_HORZ | MAXIMIZED_VERT))
		window.Maximize(SET, window.GetWMState(MAXIMIZED_HORZ),
				     window.GetWMState(MAXIMIZED_VERT));

	// really not sure what to do here. Maybe purge both states?
	if (window.GetWMState(ABOVE | BELOW))
		std::cerr << "Window added as both above and below.\n";

	// add the window to the appropriate stack of windows
	if (window.GetWMState(ABOVE)
	    || window.GetType() == DOCK
	   ){
		topWindows.push_front(window);
	} else if (window.GetWMState(BELOW)){
		bottomWindows.push_front(window);
	} else {
		windows.push_front(window);
	}

	// call RaiseWindow here to ensure proper stacking against windows
	// with above or below stacking order. The scope of this call could
	// be narrowed.
	RaiseWindow(window, focus);

	UpdateClientLists(window.GetRootWindow());

	xcb_atom_t actions[] = {
		ewmh()._NET_WM_ACTION_CLOSE,
		ewmh()._NET_WM_ACTION_MAXIMIZE_HORZ,
		ewmh()._NET_WM_ACTION_MAXIMIZE_VERT,
		ewmh()._NET_WM_ACTION_MINIMIZE,
		ewmh()._NET_WM_ACTION_ABOVE,
		ewmh()._NET_WM_ACTION_BELOW
	};
	xcb_change_property(xcb(), XCB_PROP_MODE_REPLACE, window.GetWindow(), ewmh()._NET_WM_ALLOWED_ACTIONS, XCB_ATOM_ATOM, 32, sizeof(actions)/sizeof(xcb_atom_t), actions);

	xcb_flush(xcb());
}

/*
 * For purposes of integration with other applications like pagers and taskbars
 * EWMH specifies two lists for the window manager to maintain. This method
 * updates both with the currently managed windows
 */
void WindowManager::UpdateClientLists(xcb_window_t root){

	// Add window to list of all managed windows
	int size, i;
	size = allWindows.size();
	xcb_window_t *xwindows = new xcb_window_t[size];
	i = 0;
	for (WindowList::iterator itr = allWindows.begin(); itr != allWindows.end(); itr++)
		xwindows[i++] = itr->GetWindow();
	xcb_change_property(xcb(), XCB_PROP_MODE_REPLACE, root, ewmh()._NET_CLIENT_LIST, XCB_ATOM_WINDOW, 32, size, xwindows);

	// also add to list of windows by stacking order
	size = windows.size() + topWindows.size() + bottomWindows.size();
	xcb_window_t *stackWindows = new xcb_window_t[size];
	i = 0;
	for (WindowList::iterator itr = windows.begin(); itr != windows.end(); itr++)
		stackWindows[i++] = itr->GetWindow();
	for (WindowList::iterator itr = topWindows.begin(); itr != topWindows.end(); itr++)
		stackWindows[i++] = itr->GetWindow();
	for (WindowList::iterator itr = bottomWindows.begin(); itr != bottomWindows.end(); itr++)
		stackWindows[i++] = itr->GetWindow();
	xcb_change_property(xcb(), XCB_PROP_MODE_REPLACE, root, ewmh()._NET_CLIENT_LIST_STACKING, XCB_ATOM_WINDOW, 32, size, stackWindows);

	delete[] xwindows;
	delete[] stackWindows;
}

void WindowManager::GrabTouch(Window &window){

	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;
	
	const uint32_t mask[] = {XCB_INPUT_XI_EVENT_MASK_TOUCH_BEGIN
				| XCB_INPUT_XI_EVENT_MASK_TOUCH_UPDATE
				| XCB_INPUT_XI_EVENT_MASK_TOUCH_END
				//| XCB_INPUT_XI_EVENT_MASK_TOUCH_OWNERSHIP
	};
	const uint32_t modifiers[] = {XCB_INPUT_MODIFIER_MASK_ANY};
	xcb_input_xi_passive_grab_device_cookie_t c = xcb_input_xi_passive_grab_device(
			xcb(),
			XCB_CURRENT_TIME,
			window.GetWindow(),
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
	xcb_input_xi_passive_grab_device_reply_t *r = xcb_input_xi_passive_grab_device_reply(xcb(), c, &error);
	if (error){
		OutputError(*error);
		free(error);
	}

}

void WindowManager::ActiveGrabTouch(Window &window){

	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;
	
	const uint32_t mask[] = {XCB_INPUT_XI_EVENT_MASK_TOUCH_BEGIN
							| XCB_INPUT_XI_EVENT_MASK_TOUCH_UPDATE
							//| XCB_INPUT_XI_EVENT_MASK_TOUCH_END
	};
	const uint32_t modifiers[] = {XCB_INPUT_MODIFIER_MASK_ANY};
	xcb_input_xi_grab_device_cookie_t c = xcb_input_xi_grab_device(
			xcb(),
			window.GetWindow(),
			XCB_CURRENT_TIME,
			XCB_CURSOR_NONE,
			XCB_INPUT_DEVICE_ALL_MASTER,
			XCB_INPUT_GRAB_MODE_22_TOUCH,
			XCB_INPUT_GRAB_MODE_22_ASYNC,
			XCB_INPUT_GRAB_OWNER_NO_OWNER,
			1, // mask_len
			mask);
	xcb_input_xi_grab_device_reply_t *r = xcb_input_xi_grab_device_reply(xcb(), c, &error);
	if (error){
		OutputError(*error);
		free(error);
	}

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
		if (itr->GetWindow() == w)
			return &(*itr);
	for (WindowList::iterator itr = topWindows.begin(); itr != topWindows.end(); itr++)
		if (itr->GetWindow() == w)
			return &(*itr);
	for (WindowList::iterator itr = bottomWindows.begin(); itr != bottomWindows.end(); itr++)
		if (itr->GetWindow() == w)
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

void WindowManager::AcceptAllTouch(){
	for (TouchList::iterator itr = touch.begin(); itr != touch.end(); itr++){
		if (itr->unaccepted == true){
			xcb_input_xi_allow_events(xcb(), XCB_CURRENT_TIME, itr->device, XCB_INPUT_EVENT_MODE_ACCEPT_TOUCH, itr->id, itr->window);
			xcb_flush(xcb());
			itr->unaccepted = false;
		}
	}
}

void WindowManager::AcceptTouch(xcb_input_touch_begin_event_t e){
	Touch *t = GetTouch(e.detail);
	if (!t || t->unaccepted == true){
		xcb_input_xi_allow_events(xcb(), XCB_CURRENT_TIME, e.deviceid, XCB_INPUT_EVENT_MODE_ACCEPT_TOUCH, e.detail, e.event);	
		xcb_flush(xcb());
	}
	if (t)
		t->unaccepted = false;
}

void WindowManager::RejectAllTouch(){
	for (TouchList::iterator itr = touch.begin(); itr != touch.end(); itr++){
		if (itr->unaccepted == true){
			xcb_input_xi_allow_events(xcb(), XCB_CURRENT_TIME, itr->device, XCB_INPUT_EVENT_MODE_REJECT_TOUCH, itr->id, itr->window);
			xcb_flush(xcb());
			itr->unaccepted = false;
		}
	}
}

void WindowManager::RejectTouch(xcb_input_touch_begin_event_t e){
	Touch *t = GetTouch(e.detail);
	if (!t || t->unaccepted == true){
		xcb_input_xi_allow_events_checked(xcb(), XCB_CURRENT_TIME, e.deviceid, XCB_INPUT_EVENT_MODE_REJECT_TOUCH, e.detail, e.event);
		xcb_flush(xcb());
	}
	if (t)
		t->unaccepted = false;
}

/*
 * This method synchronously returns the next event. Note that if the window
 * manager is no longer controlling any screens, it will instead return NULL.
 */
xcb_generic_event_t *WindowManager::WaitForEvent(){
	if (screens.size() == 0)
		return NULL;
	return xcb_wait_for_event(xcb());
}

/*
 * This method returns the next event, if available, without blocking.
 * Note that if the window manager is no longer controlling any screens,
 * it will instead return NULL.
 */
xcb_generic_event_t *WindowManager::PollForEvent(){
	if (screens.size() == 0)
		return NULL;
	return xcb_poll_for_event(xcb());
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
	case XCB_EXPOSE:{
		HandleExpose(*(xcb_expose_event_t*)e);
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
		case XCB_INPUT_TOUCH_OWNERSHIP:
			break;
		default:
			std::cout << "Unknown generic event " << ge->event_type << "\n";
		}
		break;
	}
	case XCB_SELECTION_CLEAR:{
		HandleSelectionClear(*(xcb_selection_clear_event_t*)e);
		break;
	}
	case XCB_DESTROY_NOTIFY:{
		HandleDestroyNotify(*(xcb_destroy_notify_event_t*)e);
		break;
	}
	case XCB_CONFIGURE_NOTIFY:{
		HandleConfigureNotify(*(xcb_configure_notify_event_t*)e);
		break;
	}
	case XCB_KEY_PRESS:{
		HandleKeyPress(*(xcb_key_press_event_t*)e);
		break;
	}
	case XCB_KEY_RELEASE:{
		HandleKeyRelease(*(xcb_key_release_event_t*)e);
		break;
	}

	// currently ignored
	case XCB_UNMAP_NOTIFY:
	case XCB_MAP_NOTIFY:
	case XCB_CREATE_NOTIFY:
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

	// map the window regardless of whether we know it or not
	xcb_map_window(xcb(), e.window);

	if (GetWindow(e.window) == NULL){
		Window window(e.window);
		AddWindow(window, true);
	}
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

		WMStateChange change = (WMStateChange)e.data.data32[0];

		if (maxVert || maxHorz){
			w->Maximize(change, maxHorz, maxVert);
		} else if (e.data.data32[1] == ewmh()._NET_WM_STATE_ABOVE){
			w->Topmost(change);
			if (w->GetWMState(ABOVE)){
				//TODO: Efficiency improvements
				windows.remove(*w);
				topWindows.remove(*w);
				bottomWindows.remove(*w);
				topWindows.push_front(*w);
			} else if (w->GetWMState(BELOW)){
				//TODO: Efficiency improvements
				windows.remove(*w);
				topWindows.remove(*w);
				bottomWindows.remove(*w);
				bottomWindows.push_front(*w);
			} else {
				//TODO: Efficiency improvements
				windows.remove(*w);
				topWindows.remove(*w);
				bottomWindows.remove(*w);
				windows.push_front(*w);
			}
		} else {
			std::cout << "Attempting to change unsupported WM state " << e.data.data32[1] << "\n";
		}

	} else if (e.type == ewmh()._NET_CLOSE_WINDOW) {

		Window *w = GetWindow(e.window);
		if (w == NULL)
			return;

		if (e.format != 32){
			std::cerr << "Unexpected format: " << e.format << "\n";
			return;
		}

		if (w->SupportsDelete()){
			DeleteWindow(e.window);
		} else
			xcb_kill_client(xcb(), e.window);

	} else if (e.type == ewmh()._NET_ACTIVE_WINDOW) {

		Window *w = GetWindow(e.window);
		if (w == NULL)
			return;

		if (e.format != 32){
			std::cerr << "Unexpected format: " << e.format << "\n";
			return;
		}

		uint32_t source = e.data.data32[0];
		uint32_t timestamp = e.data.data32[1];
		xcb_window_t requestor = e.data.data32[2];
		RaiseWindow(*w, true);

	} else if (e.type == wmAtoms().WM_CHANGE_STATE) {

		Window *w = GetWindow(e.window);
		if (w == NULL)
			return;

		if (e.format != 32){
			std::cerr << "Unexpected format: " << e.format << "\n";
			return;
		}

		uint32_t iconicState = e.data.data32[0]; // 0 = withdrawn, 1 = normal, 3 = iconic
		if (iconicState == 0){
			std::cout << "Withdraw\n";
			w->Minimize(SET);
		} else if (iconicState == 1){
			std::cout << "Normal\n";
			w->Minimize(CLEAR);
		} else if (iconicState == 3){
			std::cout << "Iconify\n";
			w->Minimize(SET);
		} else {
			std::cerr << "Attempting to change iconic state to unknown value " << iconicState << "\n";
		}

	} else {
		xcb_get_atom_name_cookie_t cc = xcb_get_atom_name(xcb(), e.type);
		xcb_get_atom_name_reply_t *cr = xcb_get_atom_name_reply(xcb(), cc, NULL);
		std::cout << "\tname: " << xcb_get_atom_name_name(cr) << "\n";
		std::cout << "\tevent.data.data32[0] = " << e.data.data32[0] << "\n";
		std::cout << "\tevent.data.data32[1] = " << e.data.data32[1] << "\n";
		std::cout << "\tevent.data.data32[2] = " << e.data.data32[2] << "\n";
		free(cr);
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
	xcb_void_cookie_t cookie = xcb_send_event_checked(xcb(), false, window, mask, (char*)&event);
	xcb_request_check(xcb(), cookie);
	xcb_flush(xcb());
}

void WindowManager::HandleButtonPress(xcb_button_press_event_t &e){

	DeselectWindow();

	// always raise window on press
	Window *w = GetWindow(e.event);
	if (w)
		RaiseWindow(*w, true);

	// if moving
	if (e.state & XCB_MOD_MASK_1){
		xoff = e.event_x;
		yoff = e.event_y;
		clickWindow = GetWindow(e.event);
		xcb_allow_events(xcb(), XCB_ALLOW_SYNC_POINTER, XCB_CURRENT_TIME);
	} else {
		xcb_allow_events(xcb(), XCB_ALLOW_REPLAY_POINTER, XCB_CURRENT_TIME);
	}

	xcb_flush(xcb());
}

void WindowManager::HandleButtonRelease(xcb_button_release_event_t &e){

	clickWindow = 0;
	xcb_flush(xcb());
	xcb_allow_events(xcb(), XCB_ALLOW_SYNC_POINTER, XCB_CURRENT_TIME);
	xcb_flush(xcb());

}

void WindowManager::HandleKeyPress(xcb_key_press_event_t &e){
	bool reverse = (e.state & XCB_MOD_MASK_SHIFT) == XCB_MOD_MASK_SHIFT;
	std::cout << "Press " << (int)e.detail << ", reverse " << reverse << "\n";
}

void WindowManager::HandleKeyRelease(xcb_key_release_event_t &e){
	std::cout << "Release " << (int)e.detail << "\n";
}

void WindowManager::HandleMotion(xcb_motion_notify_event_t &e){

	xcb_allow_events(xcb(), XCB_ALLOW_SYNC_POINTER, XCB_CURRENT_TIME);
	xcb_flush(xcb());

	if (clickWindow){
		int x = e.root_x-xoff, y = e.root_y-yoff;
		MoveWindow(*clickWindow, e.root_x-xoff, e.root_y-yoff);
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
		RejectTouch(e);
		xcb_flush(xcb());
		return;
	}

	touch.push_back(Touch(e.deviceid, e.detail, event, e.event_x / 65536.0, e.event_y / 65536.0, e.root_x / 65536.0, e.root_y / 65536.0));

	// three touches means toggle touch grab
	if (touch.size() == 3){
		AcceptAllTouch();
		touchGrab = !touchGrab;
	}

	if (!touchGrab){
		xcb_flush(xcb());
		return;
	}

	Window *w = GetWindow(event);
	if (w && captureTouch == false)
		RaiseWindow(*w, true);

	if (wmMenu && wmMenu->GetNativeWindow() != event)
		wmMenu->Hide();

	// no accept or reject, additional touches may mean accept,
	// but so can't reject yet
	if (touch.size() == 1 && w != touchWindow){
		DeselectWindow();
	}

	// if touching touchWindow, accept, since this means moving
	else if (touch.size() == 1){
		captureTouch = false;
		AcceptAllTouch();
	}

	// if two touches, accept
	else if (touch.size() == 2 && captureTouch == false){
		captureTouch = true;
		if (w){
			w->SetOpacity(OPACITY_TRANSLUCENT);
			touchWindow = w;
			if (!wmMenu){
				Screen *s = GetScreen(root);
				if (s){
					wmMenu = new WMWindow(*w, *s, WMWindow::RADIAL);
					GrabTouch(wmMenu->GetWindow());
				} else {
					std::cerr << "Unable to find screen\n";
				}
			}

			// show the menu based on the location of the initial
			// touch, not the newest
			wmMenu->Show(touch[0].id, touch[0].x, touch[0].y);
			wmMenu->SetTarget(w->GetWindow());
			//AcceptAllTouch();
		}// else RejectTouch(e);
		AcceptAllTouch();
		xcb_flush(xcb());
	}

	/* // three touches means toggle maximize
	else if (touch.size() == 3 && w){
		MaximizeWindow(w->GetWindow(), root);
		AcceptAllTouch();
	}*/
	else {
		xcb_flush(xcb());
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
		RejectTouch(e);
		xcb_flush(xcb());
		return;
	}

	double x = e.root_x / 65536.0;
	double y = e.root_y / 65536.0;
	Touch *t = GetTouch(e.detail);

	if (!touchGrab){
		RejectTouch(e);
		xcb_flush(xcb());
		return;
	}

	// if moving the menu touch, move the menu
	if (wmMenu && e.detail == wmMenu->GetID()){

		wmMenu->Show(touch[0].id, x, y);
		xcb_flush(xcb());

	}

	if (touch.size() == 1 && touchWindow == NULL){
		RejectTouch(e);
		xcb_flush(xcb());
		t->x = x;
		t->y = y;
		t->moved = true;
	} else if (touch.size() == 1 && touchWindow != NULL){
		t->x = x;
		t->y = y;
		MoveWindow(*touchWindow, t->x-t->xoff, t->y-t->yoff);
		xcb_flush(xcb());
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

		// update the touch
		t->x = x;
		t->y = y;
		t->moved = true;

		bool xshift = false, yshift = false;
		Touch *xanchor = touch[0].x < touch[1].x ? &touch[0] : &touch[1];
		Touch *yanchor = touch[0].y < touch[1].y ? &touch[0] : &touch[1];
		if (t == xanchor)
			xshift = true;
		if (t == yanchor)
			yshift = true;

		dx = abs(touch[0].x-touch[1].x)-dx;
		dy = abs(touch[0].y-touch[1].y)-dy;

		Window *w = GetWindow(touch[1].window);
		if (w)
			ExpandWindow(*w, dx, dy, xshift, yshift);

	} else {
		t->x = x;
		t->y = y;
		t->moved = true;
		RejectTouch(e);
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
		// no need to reject touch here because it has already been rejected
		return;
	}

	if (!touchWindow){
		RejectTouch(e);
		//xcb_flush(xcb());
		//return;
	} else {
		AcceptAllTouch();
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

	if (touch.size() == 0){

		if (wmMenu){
			wmMenu->Hide();
			xcb_flush(xcb());
		}

		if (moved == false && captureTouch == false)
			DeselectWindow();

	} else if (touch.size() == 1 && wmMenu && wmMenu->GetNativeWindow() == event){

		double x = e.event_x / 65536.0;
		double y = e.event_y / 65536.0;
		WMWindow::Action action = wmMenu->Click(x,y);
		wmMenu->Hide();
		xcb_flush(xcb());
		switch (action){
		case WMWindow::RIGHT_CLICK: {
			std::cout << "Simulated right click\n";
			int r;
			r = suinput_emit(uinput_fd, EV_KEY, BTN_RIGHT, 1);
			if (r != -1)
				r = suinput_emit(uinput_fd, EV_KEY, BTN_RIGHT, 0);
			if (r != -1)
				r = suinput_syn(uinput_fd);
			if (r == -1)
				std::cerr << "Error occurred simulating mouse click\n";
			DeselectWindow();
			break;
		}
		case WMWindow::CLOSE:
			std::cout << "Close window\n";
			CloseWindow(wmMenu->GetTarget(), root);

			// menu no longer has a valid target, so hide and deselect
			wmMenu->Hide();
			DeselectWindow();
			break;
		case WMWindow::HORZ_MAXIMIZE:
			std::cout << "Horizontal maximize\n";
			MaximizeWindow(wmMenu->GetTarget(), root, true, false);
			//DeselectWindow();
			break;
		case WMWindow::VERT_MAXIMIZE:
			std::cout << "Vertical maximize\n";
			MaximizeWindow(wmMenu->GetTarget(), root, false, true);
			//DeselectWindow();
			break;
		case WMWindow::MAXIMIZE:
			std::cout << "Maximize\n";
			MaximizeWindow(wmMenu->GetTarget(), root);
			DeselectWindow();
			break;
		case WMWindow::MINIMIZE:
			std::cout << "Minimize\n";
			MinimizeWindow(wmMenu->GetTarget(), root);
			DeselectWindow();
			break;
		case WMWindow::BELOW:
			std::cout << "Below\n";
			break;
		case WMWindow::ABOVE:
			std::cout << "Above\n";
			ChangeWMState(wmMenu->GetTarget(), root, TOGGLE, ABOVE);
			break;
		default:
			std::cout << "Unknown action: " << action << "\n";
			break;
		}
		xcb_flush(xcb());

	}

	// always release captureTouch if there are no other touches
	if (touch.size() == 0)
		captureTouch = false;

}

/*
 * This method handles selection clear events. Currently this is only used
 * to recognize when another window manager has claimed control of a screen,
 * and to relinquish it accordingly.
 */
void WindowManager::HandleSelectionClear(xcb_selection_clear_event_t &e){
	for (ScreenList::iterator itr = screens.begin(); itr != screens.end(); itr++){
		Screen &s = *itr;
		if (e.selection == s.GetAtom()){
			if (s.GetWindow() == e.owner){
				std::cout << "Lost ownership of screen\n";
				screens.erase(itr);
				break;
			}
		}
	}
}

/*
 * This method handles destroy notify events. This results in removing the
 * window from all internal lists.
 */
void WindowManager::HandleDestroyNotify(xcb_destroy_notify_event_t &e){

	xcb_window_t root = 0;

	// first, find the window in the list of all windows. If found, store its
	// root for later use in updating the client lists.
	for (WindowList::iterator itr = allWindows.begin(); itr != allWindows.end(); itr++){
		if (itr->GetWindow() == e.window){
			root = itr->GetRootWindow();
			allWindows.erase(itr);
			break;
		}
	}

	// if no root was found, then this window isn't being managed, so abort
	if (root == 0)
		return;

	for (WindowList::iterator itr = windows.begin(); itr != windows.end(); itr++){
		if (itr->GetWindow() == e.window){
			windows.erase(itr);
			break;
		}
	}
	for (WindowList::iterator itr = bottomWindows.begin(); itr != bottomWindows.end(); itr++){
		if (itr->GetWindow() == e.window){
			windows.erase(itr);
			break;
		}
	}
	for (WindowList::iterator itr = topWindows.begin(); itr != topWindows.end(); itr++){
		if (itr->GetWindow() == e.window){
			windows.erase(itr);
			break;
		}
	}

	UpdateClientLists(root);
	xcb_flush(xcb());
}

void WindowManager::HandleConfigureNotify(xcb_configure_notify_event_t &e){
	if (wmMenu && e.window == wmMenu->GetNativeWindow()){
		//wmMenu->Draw();
	}
}

void WindowManager::HandleExpose(xcb_expose_event_t &e){
	if (wmMenu && e.window == wmMenu->GetNativeWindow()){
		wmMenu->Draw();
	}
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
void WindowManager::MaximizeWindow(xcb_window_t window, xcb_window_t root, bool maxHorz, bool maxVert){

	xcb_client_message_event_t event;
	event.response_type = XCB_CLIENT_MESSAGE;
	event.format = 32;
	event.sequence = 0;
	event.window = window;
	event.type = ewmh()._NET_WM_STATE;
	event.data.data32[0] = 2L; // 0 = remove, 1 = add, 2 = toggle
	event.data.data32[1] = maxHorz ? ewmh()._NET_WM_STATE_MAXIMIZED_HORZ : 0L;
	event.data.data32[2] = maxVert ? ewmh()._NET_WM_STATE_MAXIMIZED_VERT : 0L;
	event.data.data32[3] = 0L;
	event.data.data32[4] = 0L;

	// send the event. Note the event mask is derived from source for xcb-ewmh
	xcb_send_event(xcb(), false, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char*)&event);

	// flush to make sure the event is processed before the object
	// goes out of scope
	xcb_flush(xcb());

}

/*
 * This method minimizes a window
 *
 * Parameters:
 * 	window: The window to minimize
 * 	root: The root of the window to minimize
 */
void WindowManager::MinimizeWindow(xcb_window_t window, xcb_window_t root){

	xcb_client_message_event_t event;
	event.response_type = XCB_CLIENT_MESSAGE;
	event.format = 32;
	event.sequence = 0;
	event.window = window;
	event.type = wmAtoms().WM_CHANGE_STATE;
	event.data.data32[0] = 3L; // Iconify
	event.data.data32[1] = 0L;
	event.data.data32[2] = 0L;
	event.data.data32[3] = 0L;
	event.data.data32[4] = 0L;

	// send the event. Note the event mask is derived from source for xcb-ewmh
	xcb_send_event(xcb(), false, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char*)&event);

	// flush to make sure the event is processed before the object
	// goes out of scope
	xcb_flush(xcb());

}

/*
 * This method changes the specified WMState value for the specified window,
 * using extended window manager hints. Note that only a single WMState can be
 * toggled at a time - bit masks are not supported. It ultimately produces a
 * CLIENT_MESSAGE event for the window manager. Any associated changes to the
 * window are managed by the Window object in response to this event.
 *
 * Parameters:
 * 	window: the window whose WMstate is to be changed
 *	root: the root of the affected window
	change: the change to apply to the specified state (SET, TOGGLE, CLEAR)
 *	state: the WMState value to toggle.
 */
void WindowManager::ChangeWMState(xcb_window_t window, xcb_window_t root, WMStateChange change, WMState state){

	xcb_atom_t atom = GetWMStateAtom(state);
	if (!atom){
		std::cerr << "Attempting to toggle unknown state " << state << "\n";
		return;
	}

	xcb_client_message_event_t event;
	event.response_type = XCB_CLIENT_MESSAGE;
	event.format = 32;
	event.sequence = 0;
	event.window = window;
	event.type = ewmh()._NET_WM_STATE;
	event.data.data32[0] = (long)change; // 0 = remove, 1 = add, 2 = toggle
	event.data.data32[1] = atom;
	event.data.data32[2] = 0L;
	event.data.data32[3] = 0L;
	event.data.data32[4] = 0L;

	// send the event. Note the event mask is derived from source for xcb-ewmh
	xcb_send_event(xcb(), false, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char*)&event);

	// flush to make sure the event is processed before the object
	// goes out of scope
	xcb_flush(xcb());

}

/*
 * This method issues a client message to close a window
 *
 * Parameters:
 * 	window: The window to close
 */
void WindowManager::CloseWindow(xcb_window_t window, xcb_window_t root){

	xcb_client_message_event_t event;
	event.response_type = XCB_CLIENT_MESSAGE;
	event.format = 32;
	event.sequence = 0;
	event.window = window;
	event.type = ewmh()._NET_CLOSE_WINDOW;
	event.data.data32[0] = XCB_CURRENT_TIME;
	event.data.data32[1] = 2L; // source indicator
	event.data.data32[2] = 0L;
	event.data.data32[3] = 0L;
	event.data.data32[4] = 0L;

	// send the event. Note the event mask is derived from source for xcb-ewmh
	xcb_send_event(xcb(), false, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char*)&event);

	// flush to make sure the event is processed before the object
	// goes out of scope
	xcb_flush(xcb());
}

/*
 * This method issues a request for a window to destroy itself
 *
 * Parameters:
 * 	window: The window to destroy
 */
void WindowManager::DeleteWindow(xcb_window_t window){
	xcb_client_message_event_t event;
	event.response_type = XCB_CLIENT_MESSAGE;
	event.format = 32;
	event.sequence = 0;
	event.window = window;
	event.type = wmAtoms().WM_PROTOCOLS;
	event.data.data32[0] = wmAtoms().WM_DELETE_WINDOW;
	event.data.data32[1] = 0L;
	event.data.data32[2] = 0L;
	event.data.data32[3] = 0L;
	event.data.data32[4] = 0L;

	// send the event. Note the event mask is derived from source for xcb-ewmh
	xcb_send_event(xcb(), false, window, XCB_EVENT_MASK_NO_EVENT, (const char*)&event);

	// flush to make sure the event is processed before the object
	// goes out of scope
	xcb_flush(xcb());
}

/*
 * This method raises a window to the top of its stack of windows (respecting
 * above, below, etc)
 *
 * Parameters:
 * 	window: The window to raise
 */
void WindowManager::RaiseWindow(Window &w, bool focus){

	if (w.GetType() == DESKTOP)
		return;
	
	// if this window is "below," only raise it above other below windows
	if (w.GetWMState(BELOW)){

		Window *sibling = NULL;
		if (windows.size() > 0)
			sibling = &windows.back();
		else if (topWindows.size() > 0)
			sibling = &topWindows.back();

		if (sibling){
			uint32_t mask = XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
			uint32_t values[2];
			values[0] = sibling->GetWindow();
			values[1] = XCB_STACK_MODE_BELOW;
			w.Configure(mask, values);

			// ensure the window is front of the normal windows list
			//TODO: Efficiency improvements
			bottomWindows.remove(w);
			bottomWindows.push_front(w);
		} else
			w.Raise();

	// if this window is "above," raise it above other above windows
	} else if (w.GetWMState(ABOVE) || w.GetType() == DOCK){

		w.Raise();

		//TODO: Efficiency improvements
		topWindows.remove(w);
		topWindows.push_front(w);

	// else, only raise it above the lowest "above" window
	} else {

		// if no windows are marked above, just make this top
		if (topWindows.size() == 0){
			w.Raise();
		} else {
			// mark this window below the lowest "above" window
			Window &above = topWindows.back();
			uint32_t mask = XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
			uint32_t values[2];
			values[0] = above.GetWindow();
			values[1] = XCB_STACK_MODE_BELOW;
			w.Configure(mask, values);

			// ensure the window is front of the normal windows list
			//TODO: Efficiency improvements
			windows.remove(w);
			windows.push_front(w);
		}

	}

	// regardless of stacking order, set focus
	if (focus){
		xcb_set_input_focus(xcb(), XCB_INPUT_FOCUS_POINTER_ROOT, w.GetWindow(), XCB_CURRENT_TIME);
		xcb_window_t xwin = w.GetWindow();
		xcb_change_property(xcb(), XCB_PROP_MODE_REPLACE, w.GetRootWindow(), ewmh()._NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW, 32, 1, &xwin);

		// ensure window is not minimized
		w.Minimize(CLEAR);
	}

	xcb_flush(xcb());
}

/*
 * This method determines if a window should be moved, and if so, moves it
 *
 * Parameters:
 * 	w: The window to move
 * 	x: the amount to move the window on the x axis, relative to current position
 * 	y: the amount to move the window on the y axis, relative to current position
 */
void WindowManager::MoveWindow(Window &w, int x, int y){
	if (w.GetType() != DESKTOP && w.GetType() != DOCK){
		w.Move(x, y);
	}
}

/*
 * This method determines if a window should be expanded, and if so, resizes it
 * and, as indicated by boolean parameters, moves it to account for the change
 * in size without "moving" the window.
 *
 * Parameters:
 * 	w: The window to expand
 * 	width: The desired width of the window
 * 	height: The desired height of the window
 * 	xshift: boolean determining whether the window should move on the
 * 		x axis to account for the corresponding change in width
 * 	yshift: boolean determining whether the window should move on the
 * 		y axis to account for the corresponding change in height
 */
void WindowManager::ExpandWindow(Window &w, int width, int height, bool xshift, bool yshift){
	if (w.GetType() != DESKTOP && w.GetType() != DOCK){
		w.Maximize(CLEAR);
		w.Expand(width, height, xshift, yshift);
	}
}

