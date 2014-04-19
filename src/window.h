#ifndef _WINDOW_H
#define _WINDOW_H

#include <list>
#include <string>

#include <xcb/xcb.h>

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
	}

	std::list<Window> GetChildren();
	std::string GetTitle();
	void Move(int x, int y);
	void Expand(int width, int height, bool xshift, bool yshift);
	void Configure(uint16_t mask, const uint32_t *values);
	void SetOpacity(double opacity);
	void Maximize();

	operator xcb_window_t (){return window;}
	xcb_window_t GetRootWindow(){return root;}

private:
	xcb_connection_t *connection;

	std::string title;
	int x, y, width, height;
	xcb_window_t window;
	xcb_window_t root;
};

typedef std::list<Window> WindowList;

#endif

