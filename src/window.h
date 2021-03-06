#ifndef _WINDOW_H
#define _WINDOW_H

#include <list>
#include <string>

#include <xcb/xcb.h>

enum WMState {
	UNKNOWN = 0,
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

xcb_atom_t GetWMStateAtom(WMState state);
WMState GetWMState(xcb_atom_t atom);

enum WindowType {
	DESKTOP, DOCK, TOOLBAR, MENU, UTILITY, SPLASH, DIALOG, NORMAL
};

enum WMStateChange {
	CLEAR = 0, SET = 1, TOGGLE = 2
};

struct Box {
	int x, y, width, height;
};

class Window {
public:
	Window(xcb_window_t win);
	Window(const Window &w){
		*this = w;
	}

	Window &operator = (const Window &w){
		this->window = w.window;
		this->title = w.title;
		this->x = w.x;
		this->y = w.y;
		this->width = w.width;
		this->height = w.height;
		this->root = w.root;
		this->type = w.type;
		this->wmState = w.wmState;
		this->supportsDelete = w.supportsDelete;
		this->visible = w.visible;
		this->overrideRedirect = w.overrideRedirect;
		this->box = w.box;
	}

	bool operator == (const Window &w){
		return this->window == w.window;
	}

	std::list<Window> GetChildren();
	std::string GetTitle();
	WindowType GetType(){return type;}
	void Move(int x, int y);
	void Expand(int width, int height, bool xshift, bool yshift);
	void Raise();
	void Configure(uint16_t mask, const uint32_t *values);
	void SetOpacity(double opacity);
	void Maximize(WMStateChange change, bool horz = true, bool vert = true);
	void Maximize(xcb_window_t target, WMStateChange change, bool horz = true, bool vert = true);
	void Maximize(const Box &target, WMStateChange change, bool horz = true, bool vert = true);
	void Topmost(WMStateChange change);
	void Minimize(WMStateChange change);
	uint16_t GetWMState(){return wmState;}

	bool GetWMState(uint16_t mask){return (wmState & mask) == mask;}
	bool SupportsDelete(){return supportsDelete;}
	bool IsVisible(){return visible || GetWMState(HIDDEN) == HIDDEN;}
	bool OverrideRedirect(){return overrideRedirect;}

	xcb_window_t GetWindow(){return window;}
	xcb_window_t GetRootWindow(){return root;}
	int GetX(){return x;};
	int GetY(){return y;};
	int GetWidth(){return width;}
	int GetHeight(){return height;}
	int x, y, width, height;

private:
	std::string title;
	xcb_window_t window;
	xcb_window_t root;
	uint16_t wmState;
	WindowType type;
	bool supportsDelete;
	bool visible;
	bool overrideRedirect;
	bool topLevel;
	Box box;

	void SetWMState(uint16_t state);
};

typedef std::list<Window> WindowList;

#endif

