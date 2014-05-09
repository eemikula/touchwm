#include "wmwindow.h"
#include "windowsystem.h"

#include <iostream>
#include <cmath>

static const int MM_WIDTH = 125;
static const int MM_HEIGHT = 125;

WMWindow::WMButton::WMButton(char r, char g, char b, Action action, const char *svgFile){
	this->r = r;
	this->g = g;
	this->b = b;
	this->action = action;
	GError *error = NULL;
	rsvg = rsvg_handle_new_from_file(svgFile, &error);
	if (rsvg == NULL || error){
		std::cerr << "Error occurred reading " << svgFile << "\n";
	}
}

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
	WMButton rightClickButton(0, 0, 1, RIGHT_CLICK, DATA_PATH "/default-theme/rclick.svg");
	WMButton closeButton(1, 0, 0, CLOSE, DATA_PATH "/default-theme/close.svg");
	WMButton vertMaxButton(0, 1, 0, VERT_MAXIMIZE, DATA_PATH "/default-theme/vmax.svg");
	WMButton horzMaxButton(1, 1, 0, HORZ_MAXIMIZE, DATA_PATH "/default-theme/hmax.svg");
	WMButton minimizeButton(0, 1, 1, MINIMIZE, DATA_PATH "/default-theme/minimize.svg");
	WMButton topButton(1, 0, 1, TOPMOST, DATA_PATH "/default-theme/topmost.svg");
	WMButton maxButton(1, 1, 1, MAXIMIZE, DATA_PATH "/default-theme/max.svg");

	buttons.push_back(rightClickButton);
	buttons.push_back(closeButton);
	buttons.push_back(vertMaxButton);
	buttons.push_back(horzMaxButton);
	//buttons.push_back(minimizeButton);
	buttons.push_back(topButton);
	buttons.push_back(maxButton);
}

WMWindow::~WMWindow(){
	if (surface)
		cairo_surface_destroy(surface);
	if (cairo)
		cairo_destroy(cairo);
	for (int i = 0; i < buttons.size(); i++){
		if (buttons[i].rsvg)
			g_object_unref(buttons[i].rsvg);
	}
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

	float angle = 0;
	float angleInc = M_PI*2.0 / buttons.size();
	cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
	for (int i = 0; i < buttons.size(); i++, angle += angleInc){
		cairo_save(cairo);
		cairo_set_source_rgba(cairo, buttons[i].r, buttons[i].g, buttons[i].b, 0.6);

		// translate, then rotate
		cairo_translate(cairo, width/2.0, height/2.0);
		cairo_rotate(cairo, angle);

		// draw the section
		cairo_arc(cairo, 0, 0, width/2.0, 0, angleInc);
		cairo_line_to(cairo, 0, 0);
		cairo_close_path(cairo);
		cairo_fill(cairo);
		cairo_stroke(cairo);

		if (buttons[i].rsvg){
			// render svg to cimage, and rotate backwards so that it's
			// displayed upright in final render
			RsvgDimensionData dimension;
			rsvg_handle_get_dimensions(buttons[i].rsvg, &dimension);
			cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dimension.width*2, dimension.height*2);
			cairo_t *cimage = cairo_create(image);
			cairo_translate(cimage, dimension.width/2.0, dimension.height/2.0);
			cairo_rotate(cimage, -(angle+angleInc/2.0));
			cairo_translate(cimage, -dimension.width/2.0, -dimension.height/2.0);
			rsvg_handle_render_cairo(buttons[i].rsvg, cimage);

			// render cimage to window
			cairo_rotate(cairo, angleInc/2.0);
			cairo_translate(cairo, 200-dimension.width/2.0, -dimension.height/2.0);
			cairo_set_source_surface(cairo, image, 0, 0);
			cairo_paint(cairo);

			// cleanup
			cairo_surface_destroy(image);
			cairo_destroy(cimage);
		}

		cairo_restore(cairo);
	}

	/*cairo_destroy(cairo);
	cairo = NULL;*/
	xcb_flush(xcb());
}

WMWindow::Action WMWindow::Click(int x, int y){

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

