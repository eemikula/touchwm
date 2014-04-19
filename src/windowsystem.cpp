#include "windowsystem.h"

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
}

