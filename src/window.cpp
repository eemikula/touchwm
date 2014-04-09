#include "window.h"

#include <cstdlib>
#include <cstring>

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

void Window::Expand(int width, int height, bool xshift, bool yshift){

	xcb_generic_error_t *error = NULL;
	xcb_get_geometry_cookie_t c = xcb_get_geometry(connection, window);
	xcb_get_geometry_reply_t *r = xcb_get_geometry_reply(connection, c, &error);
	if (error){
		return;
	}

	uint32_t values[4];
	values[0] = xshift ? r->x-width : r->x;
	values[1] = yshift ? r->y-height : r->y;
	values[2] = r->width+width;
	values[3] = r->height+height;
	xcb_void_cookie_t cookie = xcb_configure_window_checked(connection, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
	xcb_request_check(connection, cookie);

	if (r)
		free(r);
}

void Window::Maximize(xcb_window_t root){

	Expand(1024, 768, false, false);

	/*const char wm_state[] = "_NET_WM_STATE";
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
	//event.type = _NET_WM_STATE;
	event.data.data32[0] = 1L;
	event.data.data32[1] = atom_max_horz;
	event.data.data32[2] = atom_max_vert;
	xcb_send_event(connection, false, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, (const char*)&event);
	xcb_flush(connection);*/

}
