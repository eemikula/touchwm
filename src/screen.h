#ifndef _SCREEN_H
#define _SCREEN_H

#include <xcb/xcb.h>

#include "window.h"

class Screen {
public:
	Screen(xcb_screen_t *screen, int id);
	Screen(const Screen &s){*this = s;}
	~Screen();
	Screen &operator = (const Screen &s){
		this->screen = s.screen;
		this->wmWindow = s.wmWindow;
		this->atom = s.atom;
		this->visualType = s.visualType;
		this->rgbaColorMap = s.rgbaColorMap;
	}

	Window GetRoot(){return Window(screen->root,screen->root);}
	xcb_window_t GetWindow(){return wmWindow;}
	xcb_atom_t GetAtom(){return atom;}
	xcb_visualtype_t *GetVisualType(){return visualType;}
	xcb_colormap_t GetRGBAColorMap(){return rgbaColorMap;}

	operator xcb_screen_t* (){return screen;}

private:
	xcb_screen_t *screen;
	xcb_window_t wmWindow;
	xcb_atom_t atom;
	xcb_visualtype_t *visualType;
	xcb_colormap_t rgbaColorMap;
};

#endif

