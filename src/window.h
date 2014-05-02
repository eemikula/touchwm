#ifndef _WINDOW_H
#define _WINDOW_H

#include <list>
#include <string>

#include <xcb/xcb.h>

enum WMState {
	MODAL = 1,
	STICKY = 1<<1,
	MAXIMIZED_VERT = 1<<2,
	MAXIMIZED_HORZ = 1<<3,
	SHADED = 1<<4,
	SKIP_TASKBAR = 1<<5,
	SKIP_PAGER = 1<<6,
	HIDDEN = 1<<7,
	FULLSCREEN = 1<<8,
	ABOVE = 1<<9,
	BELOW = 1<<10,
	DEMANDS_ATTENTION = 1<<11
};

enum WindowType {
	DESKTOP, DOCK, TOOLBAR, MENU, UTILITY, SPLASH, DIALOG, NORMAL
};

enum WMStateChange {
	SET, CLEAR, TOGGLE
};

class Window {
public:
	Window(xcb_window_t win, xcb_window_t root);
	Window(const Window &w){
		*this = w;
	}

	Window &operator = (const Window &w){
		this->connection = w.connection;
		this->window = w.window;
		this->title = w.title;
		this->x = w.x;
		this->y = w.y;
		this->width = w.width;
		this->height = w.height;
		this->root = w.root;
		this->type = w.type;
	}

	std::list<Window> GetChildren();
	std::string GetTitle();
	void Move(int x, int y);
	void Expand(int width, int height, bool xshift, bool yshift);
	void Raise();
	void Configure(uint16_t mask, const uint32_t *values);
	void SetOpacity(double opacity);
	void Maximize(WMStateChange change, bool horz = true, bool vert = true);
	void Maximize(xcb_window_t target, WMStateChange change, bool horz = true, bool vert = true);
	uint16_t GetWMState(){return wmState;}
	bool GetWMState(uint16_t mask){return (wmState & mask) == mask;}

	operator xcb_window_t (){return window;}
	xcb_window_t GetRootWindow(){return root;}

private:
	xcb_connection_t *connection;

	std::string title;
	int x, y, width, height;
	xcb_window_t window;
	xcb_window_t root;
	uint16_t wmState;
	WindowType type;

	void SetWMState(uint16_t state);
};

typedef std::list<Window> WindowList;

#endif

