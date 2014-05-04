#include "windowsystem.h"

#include <cstring>

WindowSystem *WindowSystem::system = NULL;

WindowSystem &WindowSystem::Get(){
	if (system == NULL)
		system = new WindowSystem();
	return *system;
}

WindowSystem::WindowSystem(){
	xcb = xcb_connect (NULL, NULL);
	xcb_intern_atom_cookie_t *ac = xcb_ewmh_init_atoms(xcb, &ewmh);
	xcb_generic_error_t **errors;
	xcb_ewmh_init_atoms_replies(&ewmh, ac, errors);
	if (errors){
		//std::cerr << "Error initializing ewmh\n";
		if (ac)
			free(ac);
		free(errors);
	}

	// get some useful atoms that aren't a part of EWMH
	const char wm_opacity[] = "_NET_WM_WINDOW_OPACITY";
	const char wm_protocols[] = "WM_PROTOCOLS";
	const char wm_delete_window[] = "WM_DELETE_WINDOW";
	xcb_intern_atom_cookie_t c[3];
	c[0] = xcb_intern_atom(xcb, false, strlen(wm_protocols), wm_protocols);
	c[1] = xcb_intern_atom(xcb, false, strlen(wm_delete_window), wm_delete_window);
	c[2] = xcb_intern_atom(xcb, false, strlen(wm_opacity), wm_opacity);

	xcb_intern_atom_reply_t *ar;
	ar = xcb_intern_atom_reply(xcb, c[0], NULL);
	atoms.WM_PROTOCOLS = ar->atom;
	free(ar);
	ar = xcb_intern_atom_reply(xcb, c[1], NULL);
	atoms.WM_DELETE_WINDOW = ar->atom;
	free(ar);
	ar = xcb_intern_atom_reply(xcb, c[2], NULL);
	atoms._NET_WM_WINDOW_OPACITY = ar->atom;
	free(ar);

}

