#include "window.h"

#include <cstdlib>

WindowList Window::GetChildren(){
	WindowList l;
	xcb_generic_error_t *error;
	xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, window);
	xcb_query_tree_reply_t *tree = xcb_query_tree_reply(connection, cookie, &error);
	if (tree != 0 && error == 0){
		int len = xcb_query_tree_children_length(tree);
		xcb_window_t *children = xcb_query_tree_children(tree);
		for (int i = 0; i < len; i++)
			l.push_back(Window(connection,children[i]));
		free(tree);
	}
	return l;
}

std::string Window::GetTitle(){
	if (title.length() == 0){
		xcb_get_property_cookie_t c = xcb_get_property(connection,
		     0,
		     window,
		     XCB_ATOM_WM_NAME,
		     XCB_ATOM_STRING,
		     0,
		     100);

		xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, c, NULL);
		char *prop = (char*)xcb_get_property_value(reply);
		title = prop;
		free(reply);
	}
	return title;
}

void Window::Move(int x, int y){
	uint32_t values[2];
	values[0] = x;
	values[1] = y;
	xcb_void_cookie_t cookie = xcb_configure_window_checked(connection, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
	xcb_request_check(connection, cookie);
}

