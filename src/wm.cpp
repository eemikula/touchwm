/*
Window manager?
*/

#include <iostream>
#include <list>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <pthread.h>

#include <xcb/xcb.h>
#include <xcb/xinput.h>

#include "wm.h"

int main(int argc, char* argv[]){

	WindowManager wm;
	/*ScreenList screens = wm.GetScreens();
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

	pthread_t thread;
	EventThreadArg arg(wm);
	pthread_create(&thread, NULL, WindowManager::EventThread, (void*)&arg);

	void *result;
	pthread_join(thread, &result);*/
}

void *WindowManager::EventThread(void *arg){
	EventThreadArg *args = (EventThreadArg*)arg;
	WindowManager &wm = args->wm;
	while (xcb_generic_event_t *e = wm.WaitForEvent()){
		wm.HandleEvent(e);
		free(e);
	}
	return NULL;
}

WindowManager::WindowManager(){

	connection = xcb_connect (NULL, NULL);
	clickWindow = 0;

	if (connection == NULL){
		std::cerr << "Unable to get connection\n";
		//return 1;
	}

	xcb_generic_error_t *error;
	xcb_input_list_input_devices_cookie_t cookie = xcb_input_list_input_devices(connection);
	xcb_input_list_input_devices_reply_t *reply = xcb_input_list_input_devices_reply (connection, cookie, &error);
	if (error){
		std::cerr << "error!\n";
		free(error);
	}
	
	xcb_input_device_info_iterator_t itr = xcb_input_list_input_devices_devices_iterator(reply);
	for (; itr.rem; xcb_input_device_info_next(&itr)){
		std::cout << "device: " << (int)itr.data->device_id;// << "\n";
		//std::cout << "\tdevice_type:" << "\n";
		std::cout << "\tnum_class_info: ";
		switch ((int)itr.data->num_class_info){
		case XCB_INPUT_DEVICE_CLASS_TYPE_KEY:
			std::cout << "key\n";
			break;
		case XCB_INPUT_DEVICE_CLASS_TYPE_BUTTON:
			std::cout << "button\n";
			break;
		case XCB_INPUT_DEVICE_CLASS_TYPE_VALUATOR:
			std::cout << "valuator\n";
			break;
		case XCB_INPUT_DEVICE_CLASS_TYPE_SCROLL:
			std::cout << "scroll\n";
			break;
		case XCB_INPUT_DEVICE_CLASS_TYPE_TOUCH:
			std::cout << "touch\n";
			break;
		default:
			std::cout << "unknown\n";
		}
		//std::cout << "\tdevice_use:" << (int)itr.data->device_use << "\n";
	}

	if (reply)
		free(reply);

	/*XIDeviceInfo *info;
	int nevices;

	info = XIQueryDevice(display, XIAllDevices, &ndevices);

	for (i = 0; i < ndevices; i++)
	{
	    XIDeviceInfo *dev = &info[i];
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
	    }
	}*/

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

	const static uint32_t values[] = {	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT/* | 
						XCB_EVENT_MASK_BUTTON_PRESS |
						XCB_EVENT_MASK_BUTTON_RELEASE |
						//XCB_EVENT_MASK_POINTER_MOTION
						XCB_EVENT_MASK_BUTTON_1_MOTION*/
					 };

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
	std::cout << "Adding " << window.GetTitle() << "\n";

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
	xcb_flush(connection);
}

xcb_generic_event_t *WindowManager::WaitForEvent(){
	return xcb_wait_for_event(connection);
}

void WindowManager::HandleEvent(xcb_generic_event_t *e){
	switch (e->response_type){
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
		std::cout << "release\n";
		HandleButtonRelease(*(xcb_button_release_event_t*)e);
		break;
	}
	case XCB_MOTION_NOTIFY:{
		std::cout << "motion\n";
		HandleMotion(*(xcb_motion_notify_event_t*)e);
		break;
	}
	default:
		std::cout << "Unknown event " << (int)e->response_type << "\n";
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
		std::cout << "Move: " << x << "," << y << "\n";
		Window w(connection, clickWindow);
		w.Move(e.root_x-xoff, e.root_y-yoff);
	} else {
		//std::cout << "Allowing motion\n";
		//xcb_allow_events(connection, XCB_ALLOW_REPLAY_POINTER, XCB_CURRENT_TIME);
		//xcb_flush(connection);
	}
}

