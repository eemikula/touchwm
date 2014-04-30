#include "wmwindow.h"
#include "windowsystem.h"

#include <iostream>
#include <cmath>

static const int MM_WIDTH = 125;
static const int MM_HEIGHT = 125;

WMWindow::WMWindow(Window &parent, Screen &screen, Style s)
:
// derive dimensions from physical screen size
width(MM_WIDTH * ((xcb_screen_t*)screen)->width_in_pixels / ((xcb_screen_t*)screen)->width_in_millimeters),
height(MM_HEIGHT * ((xcb_screen_t*)screen)->height_in_pixels / ((xcb_screen_t*)screen)->height_in_millimeters),
window(CreateWindow(parent, screen, s)
){
	this->id = 0;
	this->style = s;
	this->visible = false;
	this->surface = cairo_xcb_surface_create(xcb(),
				window,
				screen.GetVisualType(),
				width,
				height);

	if (this->surface){
		this->cairo = cairo_create(this->surface);
	}
}

WMWindow::~WMWindow(){
	if (surface)
		cairo_surface_destroy(surface);
	if (cairo)
		cairo_destroy(cairo);
}

Window WMWindow::CreateWindow(Window &parent, Screen &screen, Style s){

	xcb_visualtype_t *visual = screen.GetVisualType();
	xcb_window_t w = xcb_generate_id(xcb());
	uint32_t values[4] = { 0, 0, XCB_EVENT_MASK_EXPOSURE, screen.GetRGBAColorMap() };
	xcb_create_window(xcb(), 32, w, parent,
			0, 0, width, height, 0,
			XCB_WINDOW_CLASS_INPUT_OUTPUT, visual->visual_id,
			XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP, values);

	return Window(w, parent);
}

void WMWindow::Show(int id, int x, int y){
	this->id = id;
	uint32_t values[3];
	values[0] = x - (width/2);
	values[1] = y - (height/2);
	values[2] = XCB_STACK_MODE_ABOVE;
	xcb_configure_window(xcb(), window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_STACK_MODE, values);
	xcb_map_window(xcb(), window);
	visible = true;
}

void WMWindow::Hide(){
	if (visible){
		xcb_unmap_window(xcb(), window);
		visible = false;
	}
}

void WMWindow::Draw(){

	/*if (this->surface)
		this->cairo = cairo_create(this->surface);
	if (!cairo){
		std::cerr << "Unable to generate cairo\n";
		return;
	}*/

	cairo_set_source_rgba(cairo, 1.0, 1.0, 1.0, 0.0);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cairo);

	cairo_set_source_rgba(cairo, 1, 0.2, 0.2, 0.6);
	cairo_arc(cairo, width/2.0, height/2.0, width/2.0, 0.0, M_PI*2);
	cairo_fill(cairo);
	cairo_stroke(cairo);

	/*cairo_destroy(cairo);
	cairo = NULL;*/
}

