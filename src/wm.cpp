/*
Touchscreen Window Manager prototype
*/

#include <iostream>
#include <list>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <err.h>

#include <xcb/xcb.h>
#include <xcb/xinput.h>

extern "C" {
#include <suinput.h>
}

#include "wm.h"

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

	connection = xcb_connect (NULL, NULL);
	clickWindow = 0;
	captureTouch = false;

	if (connection == NULL){
		std::cerr << "Unable to get connection\n";
		//return 1;
	}

	xcb_generic_error_t *error;
	xcb_input_xi_query_version_cookie_t c = xcb_input_xi_query_version_unchecked(connection, 2, 2);
	xcb_input_xi_query_version_reply_t *r = xcb_input_xi_query_version_reply(connection, c, &error);
	std::cout << "xcb input version: " << r->major_version << "." << r->minor_version << "\n";
	if (r)
		free(r);

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
		/*XIDeviceInfo *dev = &info[i];
		printf("Device name %d\n", dev->name);
		for (j = 0; j < dev->num_classes; j++)
		{
		XIAnyClassInfo *class = dev->classes[j];
		XITouchClassInfo *t = (XITouchClassInfo*)class;

		if (class->type != XITouchClass)
			continue;

		printf("%s touch device, supporting %d touches.\n",
			   (t->mode == XIDirectTouch) ?  "direct" : "dependent",
			   t->num_touches);
		}*/
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
		l.push_back(Screen(connection, iter.data));
	return l;
}

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

	WindowList topLevel = rootWindow.GetChildren();
	for (WindowList::iterator itr = topLevel.begin(); itr != topLevel.end(); itr++)
		AddWindow(*itr);

	return true;
}

void WindowManager::AddWindow(Window &window){
	//std::cout << "Adding " << window.GetTitle() << "\n";

	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;
	cookie = xcb_map_window_checked(connection, window);
	error = xcb_request_check(connection, cookie);

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

	xcb_flush(connection);
}

void WindowManager::OutputError(xcb_generic_error_t &e){
	std::cerr << "Error code " << (int)e.error_code << ": Major " << (int)e.major_code << " minor " << (int)e.minor_code << "\n";
}

WindowManager::Touch *WindowManager::GetTouch(unsigned int id){
	Touch *t = NULL;
	for (TouchList::iterator itr = touch.begin(); itr != touch.end(); itr++){
		if (itr->id == id)
			return &(*itr);
	}
	return t;
}

xcb_generic_event_t *WindowManager::WaitForEvent(){
	return xcb_wait_for_event(connection);
}

void WindowManager::HandleEvent(xcb_generic_event_t *e){
	uint8_t event_type = e->response_type&0x7f;
	switch (event_type){
	case 0:{
		xcb_generic_error_t *error = (xcb_generic_error_t*)e;
		std::cerr << "Received x11 error: " << (int)error->error_code << ", major: " << (int)error->major_code << ", minor: " << (int)error->minor_code << "\n";
		break;
	}
	case XCB_MAP_REQUEST:{
		HandleMapRequest(*(xcb_map_request_event_t*)e);
		break;
	}
	case XCB_BUTTON_PRESS:{
		std::cout << "press\n";
		HandleButtonPress(*(xcb_button_press_event_t*)e);
		break;
	}
	case XCB_BUTTON_RELEASE:{
		std::cout << "release " << e->sequence << "\n";
		HandleButtonRelease(*(xcb_button_release_event_t*)e);
		break;
	}
	case XCB_MOTION_NOTIFY:{
		std::cout << "motion\n";
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
	default:
		std::cout << "Unknown event " << (int)event_type << "\n";
		break;
	}
}

void WindowManager::HandleMapRequest(xcb_map_request_event_t &e){
	Window window(connection, e.window);
	AddWindow(window);
}

void WindowManager::SendEvent(xcb_window_t window, xcb_generic_event_t &event, xcb_event_mask_t mask){
	xcb_void_cookie_t cookie = xcb_send_event_checked(connection, false, window, mask, (char*)&event);
	xcb_request_check(connection, cookie);
	xcb_flush(connection);
}

void WindowManager::HandleButtonPress(xcb_button_press_event_t &e){

	std::cout << "Button press on button " << (int)e.detail << "\n";
	std::cout << "Root: " << (int)e.root << ", event: " << (int)e.event << ", child: " << (int)e.child << "\n";

	// always raise window on press
	xcb_void_cookie_t cookie;
	const static uint32_t values[] = { XCB_STACK_MODE_ABOVE };
	xcb_configure_window(connection, e.event, XCB_CONFIG_WINDOW_STACK_MODE, values);
	xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, e.event, XCB_CURRENT_TIME);

	// if moving
	if (e.state & XCB_MOD_MASK_1){
		xoff = e.event_x;
		yoff = e.event_y;
		clickWindow = e.event;
		xcb_allow_events(connection, XCB_ALLOW_SYNC_POINTER, XCB_CURRENT_TIME);
		xcb_flush(connection);
	} else {
		xcb_allow_events(connection, XCB_ALLOW_REPLAY_POINTER, XCB_CURRENT_TIME);
		xcb_flush(connection);
	}
}

void WindowManager::HandleButtonRelease(xcb_button_release_event_t &e){

	std::cout << "Button release on button " << (int)e.detail << "\n";

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
		Window w(connection, clickWindow);
		w.Move(e.root_x-xoff, e.root_y-yoff);
	}
}

void WindowManager::HandleTouchBegin(xcb_input_touch_begin_event_t &e){

	// always raise window on press
	xcb_void_cookie_t cookie;
	const static uint32_t values[] = { XCB_STACK_MODE_ABOVE };
	xcb_configure_window(connection, e.event, XCB_CONFIG_WINDOW_STACK_MODE, values);
	xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, e.event, XCB_CURRENT_TIME);
	xcb_flush(connection);

	touch.push_back(Touch(e.detail, e.event, e.event_x / 65536.0, e.event_y / 65536.0, e.root_x / 65536.0, e.root_y / 65536.0));

	if (touch.size() > 1)
		captureTouch = true;

	if (touch.size() == 3){
		std::cout << "maximizing\n";
		Window w(connection, e.event);
		w.Maximize(e.root);
	}

}

void WindowManager::HandleTouchUpdate(xcb_input_touch_update_event_t &e){

	double x = e.root_x / 65536.0;
	double y = e.root_y / 65536.0;
	Touch *t = GetTouch(e.detail);
	if (!t){
		std::cerr << "Unknown touch\n";
		return;
	}

	// Two touches required for WM uses
	if (touch.size() == 1){
		xcb_input_xi_allow_events(connection, XCB_CURRENT_TIME, e.deviceid, XCB_INPUT_EVENT_MODE_REJECT_TOUCH, e.detail, e.event);
		xcb_flush(connection);
		t->x = x;
		t->y = y;
		t->moved = true;
	} else if (touch.size() == 2){

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

		Window w(connection, t->window);
		w.Expand(dx, dy, xshift, yshift);

	} else {
		t->x = x;
		t->y = y;
		t->moved = true;
	}

}

void WindowManager::HandleTouchEnd(xcb_input_touch_end_event_t &e){

	if (!captureTouch){
		xcb_input_xi_allow_events(connection, XCB_CURRENT_TIME, e.deviceid, XCB_INPUT_EVENT_MODE_REJECT_TOUCH, e.detail, e.event);
		xcb_flush(connection);
	}

	bool moved = false;
	for (TouchList::iterator itr = touch.begin(); itr != touch.end(); itr++){
		if (itr->id == e.detail){
			moved = itr->moved;
			touch.erase(itr);
			break;
		}
	}

	if (touch.size() == 0)
		captureTouch = false;
	else if (touch.size() == 1 && moved == false){

		int r;
		r = suinput_emit(uinput_fd, EV_KEY, BTN_RIGHT, 1);
		if (r != -1)
			r = suinput_syn(uinput_fd);
		if (r != -1)
			r = suinput_emit(uinput_fd, EV_KEY, BTN_RIGHT, 0);
		if (r != -1)
			r = suinput_syn(uinput_fd);
		if (r == -1)
			std::cerr << "Error occurred simulating mouse click\n";

		/*
		const int button = 3;
		xcb_flush(connection);
		std::cout << "Sending events (size " << sizeof(xcb_button_release_event_t) << ")\n";
		xcb_button_release_event_t event;
		event.response_type = XCB_BUTTON_PRESS;
		event.detail = button;
		event.sequence = e.sequence+1;
		event.time = XCB_CURRENT_TIME;
		event.root = e.root;
		event.event = touch[0].window;
		event.child = 0;
		event.root_x = touch[0].x;
		event.root_y = touch[0].y;
		event.event_x = touch[0].xoff;
		event.event_y = touch[0].yoff;
		event.state = 0;
		event.same_screen = true;
		std::cout << "Root: " << (int)event.root << ", event: " << (int)event.event << ", child: " << (int)event.child << "\n";
		xcb_void_cookie_t c = xcb_send_event_checked(connection, true, touch[0].window, XCB_EVENT_MASK_BUTTON_PRESS, (const char*)&event);
		xcb_generic_error_t *error;
		error = xcb_request_check(connection, c);
		if (error){
			std::cerr << "Error sending event!\n";
		}
		xcb_flush(connection);

		event.response_type = XCB_BUTTON_RELEASE;
		event.detail = button;
		event.sequence = e.sequence+2;
		event.time = XCB_CURRENT_TIME;
		event.root = e.root;
		event.event = touch[0].window;
		event.child = 0;
		event.root_x = touch[0].x;
		event.root_y = touch[0].y;
		event.event_x = touch[0].xoff;
		event.event_y = touch[0].yoff;
		event.state = 0;
		event.same_screen = true;
		std::cout << "Root: " << (int)event.root << ", event: " << (int)event.event << ", child: " << (int)event.child << "\n";
		c = xcb_send_event_checked(connection, true, touch[0].window, XCB_EVENT_MASK_BUTTON_RELEASE, (const char*)&event);
		error = xcb_request_check(connection, c);
		if (error){
			std::cerr << "Error sending event!\n";
		}
		xcb_flush(connection);*/

	}

}

