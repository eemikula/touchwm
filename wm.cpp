/*
Window manager?
*/

#include <iostream>
#include <list>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <xcb/xcb.h>
#include <pthread.h>

#include "wm.h"
//#include "window.h"
//using namespace wm;

int main(int argc, char* argv[]){

	WindowManager wm;
	ScreenList screens = wm.GetScreens();
	for (ScreenList::iterator itr = screens.begin(); itr != screens.end(); itr++){
		Screen &s = *itr;
		wm.Redirect(s);
	}

	pthread_t threads[5];

	while (xcb_generic_event_t *e = wm.WaitForEvent()){
		wm.HandleEvent(e);
		free(e);
	}
	//return xcb_test();
}

WindowManager::WindowManager(){

	connection = xcb_connect (NULL, NULL);
	clickWindow = 0;

	if (connection == NULL){
		std::cout << "Unable to get connection\n";
		//return 1;
	}

}

ScreenList WindowManager::GetScreens(){
	ScreenList l;

	const xcb_setup_t *setup = xcb_get_setup(connection);
	int roots = xcb_setup_roots_length(setup);
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
	for (; iter.rem; xcb_screen_next(&iter))
		l.push_back(Screen(connection, iter.data));
	return l;
}

void WindowManager::Redirect(Screen &screen){

	const static uint32_t values[] = {	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT/* | 
						XCB_EVENT_MASK_BUTTON_PRESS |
						XCB_EVENT_MASK_BUTTON_RELEASE |
						//XCB_EVENT_MASK_POINTER_MOTION
						XCB_EVENT_MASK_BUTTON_1_MOTION*/
					 };

	Window rootWindow = screen.GetRoot();
	root = rootWindow.window;
	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(connection, rootWindow, XCB_CW_EVENT_MASK, values);
	xcb_generic_error_t *error;
	if (error = xcb_request_check(connection, cookie)) {
		std::cerr << "Could not apply substructure redirect\n";
		free(error);
	} else {
		std::cout << "Redirect successful\n";
	}

	WindowList topLevel = rootWindow.GetChildren();
	for (WindowList::iterator itr = topLevel.begin(); itr != topLevel.end(); itr++)
		AddWindow(*itr);
}

void WindowManager::AddWindow(Window &window){
	if (window.window == root){
		std::cout << "Skipping root\n";
	}
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
						XCB_BUTTON_INDEX_1,
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
		HandleEvent(*(xcb_map_request_event_t*)e);
		break;
	}
	case XCB_BUTTON_PRESS:{
		std::cout << "press\n";
		HandleButtonPressEvent(*(xcb_button_press_event_t*)e);
		break;
	}
	case XCB_BUTTON_RELEASE:{
		std::cout << "release\n";
		HandleButtonReleaseEvent(*(xcb_button_release_event_t*)e);
		break;
	}
	case XCB_MOTION_NOTIFY:{
		std::cout << "motion\n";
		HandleEvent(*(xcb_motion_notify_event_t*)e);
		break;
	}
	default:
		std::cout << "Unknown event " << (int)e->response_type << "\n";
		break;
	}
}

void WindowManager::HandleEvent(xcb_map_request_event_t &e){
	Window window(connection, e.window);
	AddWindow(window);
}

void WindowManager::SendEvent(xcb_window_t window, xcb_generic_event_t &event, xcb_event_mask_t mask){
	xcb_void_cookie_t cookie = xcb_send_event_checked(connection, false, window, mask, (char*)&event);
	xcb_request_check(connection, cookie);
	xcb_flush(connection);
}

void WindowManager::HandleButtonPressEvent(xcb_button_press_event_t &e){

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

void WindowManager::HandleButtonReleaseEvent(xcb_button_release_event_t &e){

	clickWindow = 0;
	xcb_flush(connection);
	xcb_allow_events(connection, XCB_ALLOW_SYNC_POINTER, XCB_CURRENT_TIME);
	xcb_flush(connection);

}

void WindowManager::HandleEvent(xcb_motion_notify_event_t &e){

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

