#include "window.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

Window::Window(xcb_connection_t *connection, xcb_window_t win){
	this->connection = connection;
	this->window = win;

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
	this->x = values[0] = x;
	this->y = values[1] = y;
	xcb_configure_window(connection, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
}

void Window::Expand(int width, int height, bool xshift, bool yshift){

	/*xcb_generic_error_t *error = NULL;
	xcb_get_geometry_cookie_t c = xcb_get_geometry(connection, window);
	xcb_get_geometry_reply_t *r = xcb_get_geometry_reply(connection, c, &error);
	if (error){
		return;
	}

	if (this->x != r->x || this->y != r->y || this->width != r->width || this->height != r->height){
		std::cout << "this: (" << this->x << "," << this->y << "), (" << this->width << "," << this->height << "\n";
		std::cout << "repl: (" << r->x << "," << r->y << "), (" << r->width << "," << r->height << "\n";
	}*/

	uint32_t values[4];
	this->x = values[0] = xshift ? this->x-width : this->x;
	this->y = values[1] = yshift ? this->y-height : this->y;
	this->width = values[2] = this->width+width;
	this->height = values[3] = this->height+height;
	xcb_configure_window(connection, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
	xcb_flush(connection);

	//if (r)
	//	free(r);
}

void Window::Maximize(xcb_window_t root){

	Expand(1024, 768, false, false);

	const char wm_state[] = "_NET_WM_STATE";
	const char max_horz[] = "_NET_WM_STATE_MAXIMIZED_HORZ";
	const char max_vert[] = "_NET_WM_STATE_MAXIMIZED_VERT";
	xcb_intern_atom_cookie_t cookie[3];

	cookie[0] = xcb_intern_atom(connection, false, strlen(wm_state), wm_state);
	cookie[1] = xcb_intern_atom(connection, false, strlen(max_horz), max_horz);
	cookie[2] = xcb_intern_atom(connection, false, strlen(max_vert), max_vert);

	xcb_intern_atom_reply_t *r;
	r = xcb_intern_atom_reply(connection, cookie[0], NULL);
	xcb_atom_t atom_wm_state = r->atom;
	r = xcb_intern_atom_reply(connection, cookie[1], NULL);
	xcb_atom_t atom_max_horz = r->atom;
	r = xcb_intern_atom_reply(connection, cookie[2], NULL);
	xcb_atom_t atom_max_vert = r->atom;

	xcb_client_message_event_t event;
	event.response_type = XCB_CLIENT_MESSAGE;
	event.format = 32;
	event.sequence = 0;
	event.window = window;
	event.type = atom_wm_state;
	event.data.data32[0] = 1L;
	event.data.data32[1] = atom_max_horz;
	event.data.data32[2] = atom_max_vert;
	xcb_send_event(connection, false, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, (const char*)&event);

	Move(0,0);

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
