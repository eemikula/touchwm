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

	// create windows for child buttons
	WMButton rightClickButton = {0, 0, 1, RIGHT_CLICK};
	WMButton closeButton = {1, 0, 0, CLOSE};
	WMButton vertMaxButton = {0, 1, 0, VERT_MAXIMIZE};
	WMButton horzMaxButton = {1, 1, 0, HORZ_MAXIMIZE};
	WMButton minimizeButton = {0, 1, 1, MINIMIZE};
	WMButton topButton = {1, 0, 1, TOPMOST};

	buttons.push_back(rightClickButton);
	buttons.push_back(closeButton);
	buttons.push_back(vertMaxButton);
	buttons.push_back(horzMaxButton);
	buttons.push_back(minimizeButton);
	buttons.push_back(topButton);
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
	xcb_create_window(xcb(), 32, w, parent.GetRootWindow(),
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

	/*cairo_set_source_rgba(cairo, 1, 0.2, 0.2, 0.6);
	cairo_arc(cairo, width/2.0, height/2.0, width/2.0, 0.0, M_PI*2);
	//cairo_fill(cairo);
	cairo_stroke(cairo);*/

	float angle = 0;
	float angleInc = M_PI*2.0 / buttons.size();
	bool red = false;
	cairo_set_line_width(cairo, 6.0);
	for (int i = 0; i < buttons.size(); i++){
		cairo_set_source_rgba(cairo, buttons[i].r, buttons[i].g, buttons[i].b, 0.6);
		red = !red;
		cairo_arc(cairo, width/2.0, height/2.0, width/2.0, angle, angle+angleInc);
		cairo_line_to(cairo, width/2.0, height/2.0);
		cairo_close_path(cairo);
		cairo_fill(cairo);
		cairo_stroke(cairo);
		angle += angleInc;
	}

	/*cairo_destroy(cairo);
	cairo = NULL;*/
}

Action WMWindow::Click(int x, int y){

	int opposite = y - height/2;
	int adjacent = x - width/2;
	float angle;

	// prevent NaN 
	if (adjacent == 0)
		angle = 0;
	else
		angle = atan((float)opposite/adjacent);

	// adjust to 0-2*M_PI range
	if (opposite > 0){
		if (adjacent > 0){
		} else
			angle = M_PI + angle;
	} else {
		if (adjacent > 0)
			angle = 2*M_PI + angle;
		else
			angle = M_PI + angle;
	}

	float angleInc = M_PI*2.0 / buttons.size();
	int i = floor(angle/angleInc);
	if (i >= 0 && i < buttons.size())
		return buttons[i].action;
	else
		return UNKNOWN;
}

