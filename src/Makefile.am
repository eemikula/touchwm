AM_CPPFLAGS = $(xcb_CFLAGS) $(xcbxinput_CFLAGS) $(xcbutil_CFLAGS) $(xcbewmh_CFLAGS) $(libsuinput_CFLAGS) $(cairo_CFLAGS) $(librsvg2_CFLAGS) -DDATA_PATH=\"$(pkgdatadir)\"
AM_LDFLAGS = $(xcb_LIBS) $(xcbxinput_LIBS) $(xcbutil_LIBS) $(xcbewmh_LIBS) $(libsuinput_LIBS) $(cairo_LIBS) $(librsvg2_LIBS)

bin_PROGRAMS = touchwm
touchwm_SOURCES = wm.cpp wm.h screen.cpp screen.h window.cpp window.h windowsystem.cpp windowsystem.h wmwindow.cpp wmwindow.h

