#include "window.h"
#include "windowsystem.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

Window::Window(xcb_window_t win, xcb_window_t root){
	this->connection = WindowSystem::Get();
	this->window = win;
	this->root = root;
	this->wmState = 0;
	this->type = NORMAL;

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

	this->wmState = 0;
	/*xcb_get_property_cookie_t pc = xcb_ewmh_get_wm_state(&ewmh(),window);
	xcb_ewmh_get_atoms_reply_t atomsr;
	int i = xcb_ewmh_get_wm_state_reply(&ewmh(),
                    pc,
                    &atomsr,
                    &error);
	if (error || i != 1){
		std::cerr << "Error! " << i << "\n";
	} else {
		std::cout << "Atoms: " << atomsr.atoms_len << "\n";
		for (int i = 0; i < atomsr.atoms_len; i++)
			std::cout << (int)atomsr.atoms[i] << "\n";
	}*/
	xcb_get_property_cookie_t pc[2];
	pc[0] = xcb_get_property_unchecked(connection,
		     0,
		     window,
		     ewmh()._NET_WM_STATE,
		     XCB_ATOM_ATOM,
		     0,
		     1024);
	pc[1] = xcb_get_property_unchecked(connection,
		     0,
		     window,
		     ewmh()._NET_WM_WINDOW_TYPE,
		     XCB_ATOM_ATOM,
		     0,
		     1024);

	xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, pc[0], &error);
	if (error){
		std::cerr << "Error getting wm state\n";
		return;
	}
	if (reply && reply->format == 32 && reply->type == XCB_ATOM_ATOM){
		int length = xcb_get_property_value_length(reply)/sizeof(xcb_atom_t);
		xcb_atom_t *prop = (xcb_atom_t*)xcb_get_property_value(reply);
		if (prop){
			for (int i = 0; i < length; i++){
				if (prop[i] == ewmh()._NET_WM_STATE_MAXIMIZED_HORZ)
					wmState |= MAXIMIZED_HORZ;
				if (prop[i] == ewmh()._NET_WM_STATE_MAXIMIZED_VERT)
					wmState |= MAXIMIZED_VERT;
			}
			free(reply);
		}
	}

	reply = xcb_get_property_reply(connection, pc[1], &error);
	if (error){
		std::cerr << "Error getting wm window type\n";
		return;
	}
	if (reply && reply->format == 32 && reply->type == XCB_ATOM_ATOM){
		int length = xcb_get_property_value_length(reply)/sizeof(xcb_atom_t);
		xcb_atom_t *prop = (xcb_atom_t*)xcb_get_property_value(reply);
		if (prop){
			for (int i = 0; i < length; i++){
				if (prop[i] == ewmh()._NET_WM_WINDOW_TYPE_DESKTOP)
					type = DESKTOP;
			}
			free(reply);
		}
	}

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
		if (reply){
			char *prop = (char*)xcb_get_property_value(reply);
			title = prop;
			free(reply);
		}
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

void Window::Raise(){

	// never raise a desktop window!
	if (type != DESKTOP){
		xcb_void_cookie_t cookie;
		const static uint32_t values[] = { XCB_STACK_MODE_ABOVE };
		xcb_configure_window(connection, window, XCB_CONFIG_WINDOW_STACK_MODE, values);
		xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);
	}
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

/*
 * This method maximizes the window to a target window (e.g., root window)
 * 
 * Parameters:
 * 	target: A window object whose geometry will be used as the frame
 *		of reference for maximizing the window
 */
void Window::Maximize(const Window &target, WMStateChange change){
	uint16_t state = wmState;
	switch (change){
	case SET:
		state |= MAXIMIZED_VERT | MAXIMIZED_HORZ;
		break;
	case CLEAR:
		state &= ~(MAXIMIZED_VERT | MAXIMIZED_HORZ);
		break;
	case TOGGLE:
		state ^= MAXIMIZED_VERT | MAXIMIZED_HORZ;
		break;
	}
	SetWMState(state);

	if (GetWMState(MAXIMIZED_VERT | MAXIMIZED_HORZ)){
		uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
		uint32_t values[4];
		values[0] = 0;
		values[1] = 0;

		// prevent sending resize unless the window is actually being resized
		if (this->width != target.width || this->height != target.height){
			mask |= XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
			values[2] = target.width;
			values[3] = target.height;
		}
		xcb_configure_window(connection, window, mask, values);
	} else {
		uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
				| XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
		uint32_t values[4];
		values[0] = x;
		values[1] = y;
		values[2] = width;
		values[3] = height;
		xcb_configure_window(connection, window, mask, values);
	}
	xcb_flush(connection);
}

/*
 * This method sets the window's state mask, and applies the relevant atoms to
 * the window's _NET_WM_STATE property, thereby enacting those window states.
 * Note that this method does not flush its state, so it must be done manually.
 *
 * Parameters:
 * 	state: the new state mask for the window, composed of WMState values
 */
void Window::SetWMState(uint16_t state){
	if (wmState != state){
		wmState = state;
		xcb_atom_t atoms[16];
		int i = 0;
		if ((wmState & MODAL) == MODAL)
			atoms[i++] = ewmh()._NET_WM_STATE_MODAL;
		if ((wmState & STICKY) == STICKY)
			atoms[i++] = ewmh()._NET_WM_STATE_STICKY;
		if ((wmState & MAXIMIZED_VERT) == MAXIMIZED_VERT)
			atoms[i++] = ewmh()._NET_WM_STATE_MAXIMIZED_VERT;
		if ((wmState & MAXIMIZED_HORZ) == MAXIMIZED_HORZ)
			atoms[i++] = ewmh()._NET_WM_STATE_MAXIMIZED_HORZ;
		if ((wmState & SHADED) == SHADED)
			atoms[i++] = ewmh()._NET_WM_STATE_SHADED;
		if ((wmState & SKIP_TASKBAR) == SKIP_TASKBAR)
			atoms[i++] = ewmh()._NET_WM_STATE_SKIP_TASKBAR;
		if ((wmState & SKIP_PAGER) == SKIP_PAGER)
			atoms[i++] = ewmh()._NET_WM_STATE_SKIP_PAGER;
		if ((wmState & HIDDEN) == HIDDEN)
			atoms[i++] = ewmh()._NET_WM_STATE_HIDDEN;
		if ((wmState & FULLSCREEN) == FULLSCREEN)
			atoms[i++] = ewmh()._NET_WM_STATE_FULLSCREEN;
		if ((wmState & ABOVE) == ABOVE)
			atoms[i++] = ewmh()._NET_WM_STATE_ABOVE;
		if ((wmState & BELOW) == BELOW)
			atoms[i++] = ewmh()._NET_WM_STATE_BELOW;
		if ((wmState & DEMANDS_ATTENTION) == DEMANDS_ATTENTION)
			atoms[i++] = ewmh()._NET_WM_STATE_DEMANDS_ATTENTION;
		xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, ewmh()._NET_WM_STATE, XCB_ATOM_ATOM, 32, i, atoms);
	}
}

/*
 * This method maximizes the window, using its parent as a target
 */
void Window::Maximize(WMStateChange change){
	if (root)
		Maximize(Window(root,0), change);
}
