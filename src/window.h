#ifndef _WINDOW_H
#define _WINDOW_H

#include <list>
#include <string>

#include <xcb/xcb.h>

class Window {
public:
	Window(xcb_connection_t *connection, xcb_window_t win){this->connection = connection; this->window = win;}

	std::list<Window> GetChildren();
	std::string GetTitle();
	void Move(int x, int y);
	void Expand(int width, int height, bool xshift, bool yshift);
	void Maximize(xcb_window_t root);

	operator xcb_window_t (){return window;}
	xcb_window_t window;
private:
	xcb_connection_t *connection;

	std::string title;
};

typedef std::list<Window> WindowList;

#endif

