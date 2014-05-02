#ifndef _WMWINDOW_H
#define _WMWINDOW_H

#include "window.h"
#include "screen.h"

#include <cairo/cairo-xcb.h>

#include <vector>

enum Action {
	UNKNOWN,
	RIGHT_CLICK,
	CLOSE,
	HORZ_MAXIMIZE,
	VERT_MAXIMIZE,
	MAXIMIZE,
	MINIMIZE,
	TOPMOST
};

class WMButton {
public:
	char r, g, b;
	Action action;

private:
};

class WMWindow{
public:

	enum Style {
		RADIAL,
		VERTICAL,
		HORIZONTAL
	};

	WMWindow(Window &parent, Screen &screen, Style s = RADIAL);
	~WMWindow();

	operator xcb_window_t (){return window;}
	Window &GetWindow(){return window;}
	void SetTarget(xcb_window_t t){target = t;}
	xcb_window_t GetTarget(){return target;}

	int GetID(){return id;}
	bool Visible(){return visible;}
	void Show(int id, int x, int y);
	void Hide();

	void Draw();
	Action Click(int x, int y);

private:

	Window CreateWindow(Window &parent, Screen &screen, Style s);

	int width, height;
	int id;
	Window window;
	xcb_window_t target;
	Style style;
	bool visible;
	cairo_surface_t *surface;
	cairo_t *cairo;

	typedef std::vector<WMButton> buttonList;
	buttonList buttons;
};

#endif

