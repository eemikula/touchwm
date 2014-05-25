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
	const char *atomNames[] = {
		"_NET_WM_WINDOW_OPACITY",
		"WM_PROTOCOLS",
		"WM_DELETE_WINDOW",
		"WM_STATE",
		"WM_CHANGE_STATE"
	};
	static const int atomCount = sizeof(atomNames)/sizeof(char*);
	xcb_intern_atom_cookie_t c[atomCount];
	int i;
	for (i = 0; i < atomCount; i++)
		c[i] = xcb_intern_atom(xcb, false, strlen(atomNames[i]), atomNames[i]);
	xcb_intern_atom_reply_t *ar;
	i = 0;
	ar = xcb_intern_atom_reply(xcb, c[i++], NULL);
	atoms._NET_WM_WINDOW_OPACITY = ar->atom;
	free(ar);
	ar = xcb_intern_atom_reply(xcb, c[i++], NULL);
	atoms.WM_PROTOCOLS = ar->atom;
	free(ar);
	ar = xcb_intern_atom_reply(xcb, c[i++], NULL);
	atoms.WM_DELETE_WINDOW = ar->atom;
	free(ar);
	ar = xcb_intern_atom_reply(xcb, c[i++], NULL);
	atoms.WM_STATE = ar->atom;
	free(ar);
	ar = xcb_intern_atom_reply(xcb, c[i++], NULL);
	atoms.WM_CHANGE_STATE = ar->atom;
	free(ar);

}

