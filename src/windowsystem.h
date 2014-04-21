/*
 * This class provides a singleton for accessing a connection to an X11 server
 */

#ifndef WINDOWSYSTEM_H
#define WINDOWSYSTEM_H

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

class WindowSystem {
public:
	static WindowSystem &Get();

	operator xcb_connection_t* (){return xcb;}
	xcb_ewmh_connection_t &GetEWMH(){return ewmh;}

private:
	WindowSystem();

	// intentionally not implemented
	WindowSystem(const WindowSystem &ws);
	WindowSystem &operator = (const WindowSystem &ws);

	static WindowSystem *system;
	xcb_connection_t *xcb;
	xcb_ewmh_connection_t ewmh;
};

static inline xcb_connection_t *xcb(){return WindowSystem::Get();}
static inline xcb_ewmh_connection_t &ewmh(){return WindowSystem::Get().GetEWMH();}

#endif

