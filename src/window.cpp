#include "window.h"
#include "windowsystem.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

static const int UNMAXIMIZE_THRESHOLD = 50;

Window::Window(xcb_window_t win){
	this->window = win;
	this->wmState = 0;
	this->type = NORMAL;

	xcb_generic_error_t *error = NULL;
	xcb_get_geometry_cookie_t c = xcb_get_geometry(xcb(), window);
	xcb_get_geometry_reply_t *r = xcb_get_geometry_reply(xcb(), c, &error);
	if (error || !r){
		return;
	}

	this->root = r->root;
	this->x = r->x;
	this->y = r->y;
	this->width = r->width;
	this->height = r->height;
	free(r);

	xcb_get_window_attributes_cookie_t ac = xcb_get_window_attributes(xcb(), window);
	xcb_get_window_attributes_reply_t *ar = xcb_get_window_attributes_reply(xcb(), ac, NULL);
	if (ar){
		this->visible = (ar->map_state != 0);
		this->overrideRedirect = ar->override_redirect;
		free(ar);
	}

	this->wmState = 0;

	xcb_get_property_cookie_t pc[3];
	pc[0] = xcb_get_property_unchecked(xcb(),
		     0,
		     window,
		     ewmh()._NET_WM_STATE,
		     XCB_ATOM_ATOM,
		     0,
		     1024);
	pc[1] = xcb_get_property_unchecked(xcb(),
		     0,
		     window,
		     ewmh()._NET_WM_WINDOW_TYPE,
		     XCB_ATOM_ATOM,
		     0,
		     1024);
	pc[2] = xcb_get_property_unchecked(xcb(),
		     0,
		     window,
		     wmAtoms().WM_PROTOCOLS,
		     XCB_ATOM_ATOM,
		     0,
		     1024);

	xcb_get_property_reply_t *reply = xcb_get_property_reply(xcb(), pc[0], &error);
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
				else if (prop[i] == ewmh()._NET_WM_STATE_MAXIMIZED_VERT)
					wmState |= MAXIMIZED_VERT;
				else if (prop[i] == ewmh()._NET_WM_STATE_ABOVE)
					wmState |= ABOVE;
				else if (prop[i] == ewmh()._NET_WM_STATE_BELOW)
					wmState |= BELOW;
			}
		}
	}
	if (reply)
		free(reply);

	reply = xcb_get_property_reply(xcb(), pc[1], &error);
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
				if (prop[i] == ewmh()._NET_WM_WINDOW_TYPE_DOCK)
					type = DOCK;
				if (prop[i] == ewmh()._NET_WM_WINDOW_TYPE_TOOLBAR)
					type = TOOLBAR;
				if (prop[i] == ewmh()._NET_WM_WINDOW_TYPE_MENU)
					type = MENU;
				if (prop[i] == ewmh()._NET_WM_WINDOW_TYPE_UTILITY)
					type = UTILITY;
				if (prop[i] == ewmh()._NET_WM_WINDOW_TYPE_SPLASH)
					type = SPLASH;
				if (prop[i] == ewmh()._NET_WM_WINDOW_TYPE_DIALOG)
					type = DIALOG;
				else if (prop[i] == ewmh()._NET_WM_WINDOW_TYPE_NORMAL)
					type = NORMAL;
			}
		}
	}
	if (reply)
		free(reply);

	reply = xcb_get_property_reply(xcb(), pc[2], &error);
	if (error){
		std::cerr << "Error getting wm protocols\n";
		return;
	}
	if (reply && reply->format == 32 && reply->type == XCB_ATOM_ATOM){
		int length = xcb_get_property_value_length(reply)/sizeof(xcb_atom_t);
		xcb_atom_t *prop = (xcb_atom_t*)xcb_get_property_value(reply);
		if (prop){
			for (int i = 0; i < length; i++){
				if (prop[i] == wmAtoms().WM_DELETE_WINDOW)
					supportsDelete = true;
			}
		}
	}
	if (reply)
		free(reply);

}

WindowList Window::GetChildren(){
	WindowList l;
	xcb_generic_error_t *error;
	xcb_query_tree_cookie_t cookie = xcb_query_tree(xcb(), window);
	xcb_query_tree_reply_t *tree = xcb_query_tree_reply(xcb(), cookie, &error);
	if (tree != 0 && error == 0){
		int len = xcb_query_tree_children_length(tree);
		xcb_window_t *children = xcb_query_tree_children(tree);
		for (int i = 0; i < len; i++)
			l.push_back(Window(children[i]));
		free(tree);
	}
	return l;
}

std::string Window::GetTitle(){
	if (title.length() == 0){
		xcb_get_property_cookie_t c = xcb_get_property(xcb(),
		     0,
		     window,
		     XCB_ATOM_WM_NAME,
		     XCB_ATOM_STRING,
		     0,
		     100);

		xcb_get_property_reply_t *reply = xcb_get_property_reply(xcb(), c, NULL);
		if (reply){
			char *prop = (char*)xcb_get_property_value(reply);
			title = prop;
			free(reply);
		}
	}
	return title;
}

void Window::Move(int x, int y){

	uint32_t values[4];
	uint16_t mask = 0;

	// if only maximized on one axis, allow moving without
	// de-maximizing, within a threshold
	if (GetWMState(MAXIMIZED_HORZ) && !GetWMState(MAXIMIZED_VERT) && abs(x-this->x) < UNMAXIMIZE_THRESHOLD){
		this->y = values[0] = y;
		mask = XCB_CONFIG_WINDOW_Y;
	} else if (GetWMState(MAXIMIZED_VERT) && !GetWMState(MAXIMIZED_HORZ) && abs(y-this->y) < UNMAXIMIZE_THRESHOLD){
		this->x = values[0] = x;
		mask = XCB_CONFIG_WINDOW_X;
	} else {
		this->x = values[0] = x;
		this->y = values[1] = y;
		values[2] = width;
		values[3] = height;
		mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
			| XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
		SetWMState(GetWMState() & ~(MAXIMIZED_HORZ | MAXIMIZED_VERT));
	}
	xcb_configure_window(xcb(), window, mask, values);
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
	xcb_configure_window(xcb(), window, mask, values);
	xcb_flush(xcb());
}

void Window::Raise(){

	// never raise a desktop window!
	if (type != DESKTOP){
		xcb_void_cookie_t cookie;
		const static uint32_t values[] = { XCB_STACK_MODE_ABOVE };
		xcb_configure_window(xcb(), window, XCB_CONFIG_WINDOW_STACK_MODE, values);
	}

	// apply focus
	//xcb_set_input_focus(xcb(), XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);
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
	xcb_configure_window(xcb(), window, mask, values);
	xcb_flush(xcb());
}

void Window::SetOpacity(double op){
	
	uint32_t opacity = op*0xffffffff;
	xcb_change_property(xcb(), XCB_PROP_MODE_REPLACE, window, wmAtoms()._NET_WM_WINDOW_OPACITY, XCB_ATOM_CARDINAL, 32, sizeof(opacity), &opacity);
	xcb_flush(xcb());

}

/*
 * This method maximizes the window to a target window (e.g., root window)
 * 
 * Parameters:
 * 	target: A window object whose geometry will be used as the frame
 *		of reference for maximizing the window
 */
void Window::Maximize(xcb_window_t target, WMStateChange change, bool horz, bool vert){

	uint16_t state = wmState;
	uint16_t setMask = 0;
	if (horz)
		setMask |= MAXIMIZED_HORZ;
	if (vert)
		setMask |= MAXIMIZED_VERT;

	switch (change){
	case SET:
		state |= setMask;
		break;
	case CLEAR:
		state &= ~(setMask);
		break;
	case TOGGLE:
		state ^= setMask;
		break;
	}

	// just return if there were no changes
	if (state == wmState)
		return;

	SetWMState(state);

	uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
	uint32_t values[4];
	if (GetWMState(MAXIMIZED_VERT | MAXIMIZED_HORZ)){
		values[0] = 0;
		values[1] = 0;

		// only generate a Window object if it's actually going to be used,
		// since retrieving its geometry is expensive
		Window w(target);

		// prevent sending resize unless the window is actually being resized
		if (this->width != w.width || this->height != w.height){
			mask |= XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
			values[2] = w.width;
			values[3] = w.height;
		}
	} else if (GetWMState(MAXIMIZED_VERT)){
		values[0] = x;
		values[1] = 0;

		// only generate a Window object if it's actually going to be used,
		// since retrieving its geometry is expensive
		Window w(target);

		// prevent sending resize unless the window is actually being resized
		if (this->height != w.height){
			mask |= XCB_CONFIG_WINDOW_HEIGHT;
			values[2] = w.height;
		}
	} else if (GetWMState(MAXIMIZED_HORZ)){
		values[0] = 0;
		values[1] = y;

		// only generate a Window object if it's actually going to be used,
		// since retrieving its geometry is expensive
		Window w(target);

		// prevent sending resize unless the window is actually being resized
		if (this->width != w.width || this->height != w.height){
			mask |= XCB_CONFIG_WINDOW_WIDTH;
			values[2] = w.width;
		}
	} else {
		mask |= XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
		values[0] = x;
		values[1] = y;
		values[2] = width;
		values[3] = height;
	}
	xcb_configure_window(xcb(), window, mask, values);
	xcb_flush(xcb());
}

/*
 * This method modifies maximization on the specified axes
 *
 * Parameters:
 * 	change: The change to apply to both maximizations - CLEAR, SET, TOGGLE
 * 	horz: True iff change applies to horizontal maximization
 * 	vert: True iff change applies to vertical maximization
 */
void Window::Maximize(WMStateChange change, bool horz, bool vert){
	if (root)
		Maximize(root, change, horz, vert);
}

/*
 * This method modifies topmost state of the window
 */

void Window::Topmost(WMStateChange change){
	uint16_t state = wmState;
	switch (change){
	case SET:
		state |= ABOVE;
		break;
	case CLEAR:
		state &= ~(ABOVE);
		break;
	case TOGGLE:
		state ^= ABOVE;
		break;
	}

	// just return if there were no changes
	if (state == wmState)
		return;

	SetWMState(state);
	xcb_flush(xcb());
}

void Window::Minimize(WMStateChange change){
	uint16_t state = wmState;
	switch (change){
	case SET:
		state |= HIDDEN;
		break;
	case CLEAR:
		state &= ~(HIDDEN);
		break;
	case TOGGLE:
		state ^= HIDDEN;
		break;
	}

	// just return if there were no changes
	if (state == wmState)
		return;

	SetWMState(state);
	if (GetWMState(HIDDEN))
		xcb_unmap_window(xcb(), window);
	else
		xcb_map_window(xcb(), window);

	xcb_flush(xcb());
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
		xcb_change_property(xcb(), XCB_PROP_MODE_REPLACE, window, ewmh()._NET_WM_STATE, XCB_ATOM_ATOM, 32, i, atoms);
	}
}

/*
 * This method returns the atom that corresponds with a given WMState value.
 * Note that this does not support multiple values in a bit-mask.
 *
 * Parameters:
 * 	state: the WMState value
 *
 * Return value:
 * 	the atom the indicates the WMState value
 */
xcb_atom_t GetWMStateAtom(WMState state){
	if (state == MODAL)
		return ewmh()._NET_WM_STATE_MODAL;
	else if (state == STICKY)
		return ewmh()._NET_WM_STATE_STICKY;
	else if (state == MAXIMIZED_VERT)
		return ewmh()._NET_WM_STATE_MAXIMIZED_VERT;
	else if (state == MAXIMIZED_HORZ)
		return ewmh()._NET_WM_STATE_MAXIMIZED_HORZ;
	else if (state == SHADED)
		return ewmh()._NET_WM_STATE_SHADED;
	else if (state == SKIP_TASKBAR)
		return ewmh()._NET_WM_STATE_SKIP_TASKBAR;
	else if (state == SKIP_PAGER)
		return ewmh()._NET_WM_STATE_SKIP_PAGER;
	else if (state == HIDDEN)
		return ewmh()._NET_WM_STATE_HIDDEN;
	else if (state == FULLSCREEN)
		return ewmh()._NET_WM_STATE_FULLSCREEN;
	else if (state == ABOVE)
		return ewmh()._NET_WM_STATE_ABOVE;
	else if (state == BELOW)
		return ewmh()._NET_WM_STATE_BELOW;
	else if (state == DEMANDS_ATTENTION)
		return ewmh()._NET_WM_STATE_DEMANDS_ATTENTION;
	return 0;
}

/*
 * This method returns the WMState that corresponds with a given WM_STATE atom.
 * Note that only WM_STATE related atoms are recognized.
 *
 * Parameters:
 * 	atom: the atom to find the WMState for
 *
 * Return value:
 * 	The WMState that corresponds with the given atom
 */
WMState GetWMState(xcb_atom_t atom){
	if (atom == ewmh()._NET_WM_STATE_MODAL)
		return MODAL;
	else if (atom == ewmh()._NET_WM_STATE_STICKY)
		return STICKY;
	else if (atom == ewmh()._NET_WM_STATE_MAXIMIZED_VERT)
		return MAXIMIZED_VERT;
	else if (atom == ewmh()._NET_WM_STATE_MAXIMIZED_HORZ)
		return MAXIMIZED_HORZ;
	else if (atom == ewmh()._NET_WM_STATE_SHADED)
		return SHADED;
	else if (atom == ewmh()._NET_WM_STATE_SKIP_TASKBAR)
		return SKIP_TASKBAR;
	else if (atom == ewmh()._NET_WM_STATE_SKIP_PAGER)
		return SKIP_PAGER;
	else if (atom == ewmh()._NET_WM_STATE_HIDDEN)
		return HIDDEN;
	else if (atom == ewmh()._NET_WM_STATE_FULLSCREEN)
		return FULLSCREEN;
	else if (atom == ewmh()._NET_WM_STATE_ABOVE)
		return ABOVE;
	else if (atom == ewmh()._NET_WM_STATE_BELOW)
		return BELOW;
	else if (atom == ewmh()._NET_WM_STATE_DEMANDS_ATTENTION)
		return DEMANDS_ATTENTION;
	return UNKNOWN;
}

