#include "window.h"
#include "windowsystem.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

Window::Window(xcb_window_t win, xcb_window_t root){
	this->connection = WindowSystem::Get();
	this->window = win;
	this->root = root;

	xcb_generic_error_t *error = NULL;
	xcb_get_geometry_cookie_t c = xcb_get_geometry(connection, window);
	xcb_get_geometry_reply_t *r = xcb_get_geometry_reply(connection, c, &error);
	if (error || !r){
		return;
	}

	this->x = r->x;
	this->y = r->y;
	this->width = r->width;
	this->height = r->height;

	free(r);
}

WindowList Window::GetChildren(){
	WindowList l;
	xcb_generic_error_t *error;
	xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, window);
	xcb_query_tree_reply_t *tree = xcb_query_tree_reply(connection, cookie, &error);
	if (tree != 0 && error == 0){
		int len = xcb_query_tree_children_length(tree);
		xcb_window_t *children = xcb_query_tree_children(tree);
		for (int i = 0; i < len; i++)
			l.push_back(Window(children[i],window));
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
	this->x = values[0] = x;
	this->y = values[1] = y;
	xcb_configure_window(connection, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
}

void Window::Expand(int width, int height, bool xshift, bool yshift){
	uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
	uint32_t values[4];
	this->x = values[0] = xshift ? this->x-width : this->x;
	this->y = values[1] = yshift ? this->y-height : this->y;

	// prevent sending resize unless the window is actually being resized
	if (this->width != width || this->height != height){
		mask |= XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
		this->width = values[2] = this->width+width;
		this->height = values[3] = this->height+height;
	}
	xcb_configure_window(connection, window, mask, values);
	xcb_flush(connection);
}

void Window::Configure(uint16_t mask, const uint32_t *values){
	int i = 0;
	if (mask & XCB_CONFIG_WINDOW_X)
		this->x = values[i++];
	if (mask & XCB_CONFIG_WINDOW_Y)
		this->y = values[i++];
	if (mask & XCB_CONFIG_WINDOW_WIDTH)
		this->width = values[i++];
	if (mask & XCB_CONFIG_WINDOW_HEIGHT)
		this->height = values[i++];
	xcb_configure_window(connection, window, mask, values);
	xcb_flush(connection);
}

void Window::SetOpacity(double op){
	const char wm_opacity[] = "_NET_WM_WINDOW_OPACITY";
	xcb_intern_atom_cookie_t cookie;

	cookie = xcb_intern_atom(connection, false, strlen(wm_opacity), wm_opacity);
	xcb_intern_atom_reply_t *r;
	r = xcb_intern_atom_reply(connection, cookie, NULL);
	xcb_atom_t atom_opacity = r->atom;

	uint32_t opacity = op*0xffffffff;
	xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, atom_opacity, XCB_ATOM_CARDINAL, 32, sizeof(opacity), &opacity);
	xcb_flush(connection);

}

void Window::Maximize(){
	std::cout << "Size!\n";
}

