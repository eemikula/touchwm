/*
 * This class provides a singleton for accessing a connection to an X11 server
 */

#ifndef WINDOWSYSTEM_H
#define WINDOWSYSTEM_H

// xcb defs
#include <xcb/xcb.h>
#include <xcb/xinput.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

// taken from X11/keysymdef.h
#define XK_Tab                           0xff09

#ifdef HAVE_LIBXCB_UTIL
#include <xcb/xcb_util.h>
#endif

#include <xcb/xcb.h>
typedef xcb_map_request_event_t MapRequestEvent;
typedef xcb_client_message_event_t ClientMessageEvent;
typedef xcb_configure_request_event_t ConfigureRequestEvent;
typedef xcb_button_press_event_t ButtonPressEvent;
typedef xcb_button_release_event_t ButtonReleaseEvent;
typedef xcb_key_press_event_t KeyPressEvent;
typedef xcb_key_release_event_t KeyReleaseEvent;
typedef xcb_motion_notify_event_t MotionNotifyEvent;
typedef xcb_input_touch_begin_event_t TouchBeginEvent;
typedef xcb_input_touch_update_event_t TouchUpdateEvent;
typedef xcb_input_touch_end_event_t TouchEndEvent;
typedef xcb_selection_clear_event_t SelectionClearEvent;
typedef xcb_destroy_notify_event_t DestroyNotifyEvent;
typedef xcb_configure_notify_event_t ConfigureNotifyEvent;
typedef xcb_expose_event_t ExposeEvent;

// libx11 defs
#ifdef HAVE_LIBX11
#include <X11/Xlib.h>
typedef XMapRequestEvent MapRequestEvent;
typedef XClientMessageEvent ClientMessageEvent;
typedef XConfigureRequestEvent ConfigureRequestEvent;
typedef XButtonPressEvent ButtonPressEvent;
typedef XButtonReleaseEvent ButtonReleaseEvent;
typedef XKeyPressEvent KeyPressEvent;
typedef XKeyReleaseEvent KeyReleaseEvent;
typedef XMotionNotifyEvent MotionNotifyEvent;
typedef XToughBeginEvent TouchBeginEvent;
typedef XTouchUpdateEvent TouchUpdateEvent;
typedef XTouchEndEvent TouchEndEvent;
typedef XSelectionClearEvent SelectionClearEvent;
typedef XDestroyNotifyEvent DestroyNotifyEvent;
typedef XConfigureNotifyEvent ConfigureNotifyEvent;
typedef XExposeEvent ExposeEvent;

typedef Display Connection;

#endif

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

struct WMAtoms {
	xcb_atom_t WM_PROTOCOLS;
	xcb_atom_t WM_DELETE_WINDOW;
	xcb_atom_t WM_STATE;
	xcb_atom_t WM_CHANGE_STATE;
	xcb_atom_t _NET_WM_WINDOW_OPACITY;
};

class WindowSystem {
public:
	static WindowSystem &Get();

	operator xcb_connection_t* (){return xcb;}
	xcb_ewmh_connection_t &GetEWMH(){return ewmh;}
	const WMAtoms &GetAtoms(){return atoms;}

private:
	WindowSystem();

	// intentionally not implemented
	WindowSystem(const WindowSystem &ws);
	WindowSystem &operator = (const WindowSystem &ws);

	static WindowSystem *system;
	xcb_connection_t *xcb;
	xcb_ewmh_connection_t ewmh;
	WMAtoms atoms;
};

static inline xcb_connection_t *xcb(){return WindowSystem::Get();}
static inline xcb_ewmh_connection_t &ewmh(){return WindowSystem::Get().GetEWMH();}
static inline const WMAtoms &wmAtoms(){return WindowSystem::Get().GetAtoms();}

#endif

