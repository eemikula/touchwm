#include "screen.h"
#include "windowsystem.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>

Screen::Screen(xcb_screen_t *screen, int id){

	this->screen = screen;

	// build a string to name the atom controlling this screen
	std::stringstream ss;
	ss << "WM_S" << id;
	std::string s = ss.str();

	xcb_intern_atom_cookie_t ac = xcb_intern_atom(xcb(), false, s.length(), s.c_str());
	xcb_intern_atom_reply_t *ar = xcb_intern_atom_reply(xcb(), ac, NULL);
	if (ar == NULL){
		std::cerr << "Unable to find atom " << ss.str() << "\n";
		return;
	}
	atom = ar->atom;
	free(ar);

	// Create the window that will represent the window manager for this screen.
	// This window is never mapped (displayed), it is simply used as a means of
	// communication through X11 to the window manager
	wmWindow = xcb_generate_id(xcb());
	xcb_create_window(xcb(), 0, wmWindow, ((xcb_screen_t*)screen)->root,
				0, 0, 250, 150, 10, XCB_WINDOW_CLASS_INPUT_OUTPUT,
				((xcb_screen_t*)screen)->root_visual, 0, NULL);

	// set the window title to the name of the window manager
	char title[] = "touchWM";
        xcb_change_property (xcb(),
                             XCB_PROP_MODE_REPLACE,
                             wmWindow,
                             XCB_ATOM_WM_NAME,
                             XCB_ATOM_STRING,
                             8,
                             strlen (title),
                             title );
	xcb_change_property (xcb(),
                             XCB_PROP_MODE_REPLACE,
                             wmWindow,
                             ewmh()._NET_WM_NAME,
                             XCB_ATOM_STRING,
                             8,
                             strlen (title),
                             title );

	// set properties for the new window to mark details of the window manager
	pid_t pid = getpid();
	xcb_change_property (xcb(),
                             XCB_PROP_MODE_REPLACE,
                             wmWindow,
                             ewmh()._NET_WM_PID,
                             XCB_ATOM_CARDINAL,
                             32,
                             1,
                             &pid );

	// set _NET_SUPPORTING_WM_CHECK for both WM window and root
	// TODO: Enable this once it's not actually misleading
	/*xcb_change_property (xcb(),
                             XCB_PROP_MODE_REPLACE,
                             wmWindow,
                             ewmh()._NET_SUPPORTING_WM_CHECK,
                             XCB_ATOM_WINDOW,
                             32,
                             1,
                             &wmWindow );
	xcb_change_property (xcb(),
                             XCB_PROP_MODE_REPLACE,
                             screen->root,
                             ewmh()._NET_SUPPORTING_WM_CHECK,
                             XCB_ATOM_WINDOW,
                             32,
                             1,
                             &wmWindow );*/

	// select structure notify on the window as a means of handling shutdown
	const static uint32_t values[] = { XCB_EVENT_MASK_STRUCTURE_NOTIFY };
	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(xcb(), wmWindow, XCB_CW_EVENT_MASK, values);
	xcb_generic_error_t *error;
	if (error = xcb_request_check(xcb(), cookie)) {
		std::cerr << "Error substructure notify\n";
		free(error);
	}
}
