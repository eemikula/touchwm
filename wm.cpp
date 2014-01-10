/*
Window manager?
*/

#include <iostream>
#include <list>

#include <cstdlib>
#include <cstdio>
#include <xcb/xcb.h>

//#include "window.h"
//using namespace wm;

typedef std::list<xcb_window_t> xcb_window_list;

std::string get_window_title(xcb_connection_t *connection, xcb_window_t window){
	xcb_get_property_cookie_t c = xcb_get_property(connection,
             0,
             window,
             XCB_ATOM_WM_NAME,
             XCB_ATOM_STRING,
             0,
             100);

	xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, c, NULL);
	char *prop = (char*)xcb_get_property_value(reply);
	std::string s = prop;
	free(reply);
	return s;
}

xcb_window_list get_children(xcb_connection_t *connection, xcb_window_t window){
	xcb_window_list l;
	xcb_generic_error_t *error;
	xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, window);
	xcb_query_tree_reply_t *tree = xcb_query_tree_reply(connection, cookie, &error);
	if (tree != 0 && error == 0){
		int len = xcb_query_tree_children_length(tree);
		xcb_window_t *children = xcb_query_tree_children(tree);
		for (int i = 0; i < len; i++)
			l.push_back(children[i]);
		free(tree);
	}
	return l;
}

xcb_screen_t *screen_of_display(xcb_connection_t *c, int screen){
	xcb_screen_iterator_t iter;
	iter = xcb_setup_roots_iterator(xcb_get_setup(c));
	for (; iter.rem; --screen, xcb_screen_next(&iter))
		if (screen == 0)
			return iter.data;
	return NULL;
}

void add_window(xcb_connection_t *connection, xcb_window_t window){

	std::cout << "Adding " << get_window_title(connection, window) << "\n";

	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;
	cookie = xcb_map_window_checked(connection, window);
	error = xcb_request_check(connection, cookie);
	const static uint32_t values[] = {	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | 
						XCB_EVENT_MASK_BUTTON_PRESS |
						XCB_EVENT_MASK_BUTTON_RELEASE |
						XCB_EVENT_MASK_POINTER_MOTION };
	cookie = xcb_change_window_attributes_checked(connection, window, XCB_CW_EVENT_MASK, values);
	if (error = xcb_request_check(connection, cookie)) {
		std::cerr << "Unable to set event mask\n";
		free(error);
	}

	cookie = xcb_grab_button_checked(connection,
						1,
						window,
						XCB_EVENT_MASK_BUTTON_PRESS |
						XCB_EVENT_MASK_BUTTON_RELEASE |
						XCB_EVENT_MASK_POINTER_MOTION,
						XCB_GRAB_MODE_ASYNC,
						XCB_GRAB_MODE_ASYNC,
						XCB_NONE, XCB_NONE,
						XCB_BUTTON_INDEX_1,
						//XCB_MOD_MASK_CONTROL);
						XCB_MOD_MASK_ANY);
	if (error = xcb_request_check(connection, cookie)) {
		std::cerr << "Unable to grab button\n";
		free(error);
	}
}

int xcb_test(){
	xcb_connection_t *connection = xcb_connect (NULL, NULL);
	if (connection == NULL){
		std::cout << "Unable to get connection\n";
		return 1;
	}

	xcb_screen_t *screen = screen_of_display(connection, 0);
	xcb_window_t root = screen->root;

	const static uint32_t values[] = {	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | 
						XCB_EVENT_MASK_BUTTON_PRESS |
						XCB_EVENT_MASK_BUTTON_RELEASE |
						//XCB_EVENT_MASK_POINTER_MOTION
						XCB_EVENT_MASK_BUTTON_1_MOTION
					 };
	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(connection, root, XCB_CW_EVENT_MASK, values);
	xcb_generic_error_t *error;
	if (error = xcb_request_check(connection, cookie)) {
		std::cerr << "Could not apply substructure redirect\n";
		free(error);
		return 1;
	}
	std::cout << "Redirect successful\n";

	xcb_window_list children = get_children(connection, root);
	for (xcb_window_list::iterator itr = children.begin(); itr != children.end(); itr++){
		add_window(connection, *itr);
	}

	xcb_generic_event_t *e;
	bool button1 = false;
	bool drag = false;
	xcb_window_t dragee = 0;
	int xoff = 0;
	int yoff = 0;

	while ((e = xcb_wait_for_event(connection))){
		//std::cout << "event " << (int) e->response_type << "!!\n";
		switch (e->response_type){
		case XCB_MAP_REQUEST:{
			xcb_map_request_event_t *ev = (xcb_map_request_event_t*)e;
			//std::cout << "map window " << ev->window << "\n";
			add_window(connection, ev->window);
			break;
		}
		case XCB_BUTTON_PRESS:{
			xcb_button_press_event_t *ev = (xcb_button_press_event_t*)e;

			std::cout << "button press: " << (int)ev->detail << " title: " << get_window_title(connection, ev->event) << "\n";

			if ((int)ev->detail == 1){
				button1 = true;
				dragee = ev->event;
				xoff = ev->event_x;
				yoff = ev->event_y;
			}

			const static uint32_t values[] = { XCB_STACK_MODE_ABOVE };
			cookie = xcb_configure_window_checked(connection, ev->event, XCB_CONFIG_WINDOW_STACK_MODE, values);
			xcb_request_check(connection, cookie);

			cookie = xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, ev->event, XCB_CURRENT_TIME);
			xcb_request_check(connection, cookie);

			break;
		}
		case XCB_BUTTON_RELEASE:{
			xcb_button_release_event_t *ev = (xcb_button_release_event_t*)e;
			std::cout << "button release: " << (int)ev->detail << " title: " << get_window_title(connection, ev->event) << "\n";

			if (drag && dragee){
				uint32_t values[2];
				values[0] = ev->root_x;
				values[1] = ev->root_y;
			}

			if ((int)ev->detail == 1)
				button1 = false;
			drag = false;
			dragee = 0;

			//cookie = xcb_ungrab_button_checked(connection, XCB_BUTTON_INDEX_1, dragee, XCB_MOD_MASK_ANY);
			//xcb_request_check(connection, cookie);

			break;
		}
		case XCB_MOTION_NOTIFY:{
			xcb_motion_notify_event_t *ev = (xcb_motion_notify_event_t*)e;

			if (button1){
				drag = true;
				uint32_t values[2];
				values[0] = ev->root_x-xoff;
				values[1] = ev->root_y-yoff;
				cookie = xcb_configure_window_checked(connection, dragee, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
				xcb_request_check(connection, cookie);
			}

			break;
		}
		default:
			break;
		}
		free(e);
	}
	return 0;
}

int main(int argc, char* argv[]){
	return xcb_test();
}
