/*
 * This class provides a singleton for accessing a connection to an X11 server
 */

#ifndef WINDOWSYSTEM_H
#define WINDOWSYSTEM_H

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

struct WMAtoms {
	xcb_atom_t WM_PROTOCOLS;
	xcb_atom_t WM_DELETE_WINDOW;
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

