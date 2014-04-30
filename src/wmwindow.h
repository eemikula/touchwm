#ifndef _WMWINDOW_H
#define _WMWINDOW_H

#include "window.h"
#include "screen.h"

#include <cairo/cairo-xcb.h>

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
	operator Window& (){return window;}

	int GetID(){return id;}
	bool Visible(){return visible;}
	void Show(int id, int x, int y);
	void Hide();

	void Draw();

private:

	Window CreateWindow(Window &parent, Screen &screen, Style s);

	int width, height;
	int id;
	Window window;
	Style style;
	bool visible;
	cairo_surface_t *surface;
	cairo_t *cairo;
};

#endif

