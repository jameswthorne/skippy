/* Skippy - Seduces Kids Into Perversion
 *
 * Copyright (C) 2004 Hyriand <hyriand@thegraveyard.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "skippy.h"

static int DIE_NOW = 0;

static dlist *
update_clients(MainWin *mw, dlist *clients)
{
	dlist *stack, *iter;
	
	stack = dlist_first(wm_get_stack(mw->dpy));
	iter = clients = dlist_first(clients);

	while(iter)
	{
		ClientWin *cw = (ClientWin *)iter->data;
		if(! dlist_find_data(stack, (void *)cw->client.window))
		{
			dlist *tmp = iter->next;
			clientwin_destroy((ClientWin *)iter->data);
			clients = dlist_remove(iter);
			iter = tmp;
			continue;
		}
		clientwin_update(cw);
		iter = iter->next;
	}
	
	for(iter = dlist_first(stack); iter; iter = iter->next)
	{
		ClientWin *cw = (ClientWin*)dlist_find(clients, clientwin_cmp_func, iter->data);
		if(! cw)
		{
			cw = clientwin_create(mw, (Window)iter->data);
			if(! cw)
				continue;
			clients = dlist_add(clients, cw);
			clientwin_update(cw);
		}
	}
	
	dlist_free(stack);
	
	return clients;
}

static dlist *
skippy_run(MainWin *mw, dlist *clients, int force, Window focus, Window leader, Bool all_xin)
{
	unsigned int width, height, tree_count, u;
	float factor;
	int xoff, yoff;
	XEvent ev;
	int die = 0;
	dlist *iter, *tmp;
	Window dummy_w, *tree_windows;
	CARD32 desktop = wm_get_current_desktop(mw->dpy);
	Bool refocus = False;
	
	/* Update the main window's geometry (and Xinerama info if applicable) */
	mainwin_update(mw);
#ifdef XINERAMA
	if(all_xin)
		mw->xin_active = 0;
#else /* ! XINERAMA */
	if(all_xin);
#endif /* XINERAMA */
	
	/* Update the client table, pick the ones we want and sort them */
	clients = update_clients(mw, clients);
	
	tmp = dlist_first(dlist_find_all(clients, (dlist_match_func)clientwin_validate_func, &desktop));
	if(leader != None)
	{
		mw->cod = dlist_first(dlist_find_all(tmp, clientwin_check_group_leader_func, (void*)&leader));
		dlist_free(tmp);
	} else
		mw->cod = tmp;
	
	if(! mw->cod)
		return clients;
	
	dlist_sort(mw->cod, clientwin_sort_func, 0);
	
	/* Move the mini windows around */
	layout_run(mw, mw->cod, &width, &height);
	factor = (float)(mw->width - 100) / width;
	if(factor * height > mw->height - 100)
		factor = (float)(mw->height - 100) / height;
	
	xoff = (mw->width - (float)width * factor) / 2;
	yoff = (mw->height - (float)height * factor) / 2;
	for(iter = mw->cod; iter; iter = iter->next)
		clientwin_move((ClientWin*)iter->data, factor, xoff, yoff);
	
	/* Get the currently focused window and select which mini-window to focus */
	iter = dlist_find(mw->cod, clientwin_cmp_func, (void *)focus);
	if(! iter)
		iter = mw->cod;
	mw->focus = (ClientWin*)iter->data;
	mw->focus->focused = 1;
	
	/* Save the stacking tree */
	XQueryTree(mw->dpy, mw->root, &dummy_w, &dummy_w, &tree_windows, &tree_count);
	mw->stack_touched = False;
	
	/* Map the client windows */
	for(iter = mw->cod; iter; iter = iter->next)
		clientwin_map((ClientWin*)iter->data, force);
	XWarpPointer(mw->dpy, None, mw->focus->mini.window, 0, 0, 0, 0, mw->focus->mini.width / 2, mw->focus->mini.height / 2);
	
	/* Restore the stacking order (using XRestackWindows seems like a bad idea) */
	if(mw->stack_touched)
		for(u = 0; u < tree_count; ++u)
			XRaiseWindow(mw->dpy, tree_windows[u]);
	XFree(tree_windows);
	
	/* Map the main window and run our event loop */
	mainwin_map(mw);
	
	while(! die) {
 		XNextEvent(mw->dpy, &ev);
		if(ev.type == KeyRelease && ev.xkey.keycode == mw->key_q) {
			DIE_NOW = 1;
			break;
		}
		else if(ev.type == DestroyNotify || ev.type == UnmapNotify) {
			dlist *iter = dlist_find(clients, clientwin_cmp_func, (void *)ev.xany.window);
			if(iter)
			{
				ClientWin *cw = (ClientWin *)iter->data;
				clients = dlist_first(dlist_remove(iter));
				iter = dlist_find(mw->cod, clientwin_cmp_func, (void *)ev.xany.window);
				if(iter)
					mw->cod = dlist_first(dlist_remove(iter));
				clientwin_destroy(cw);
				if(! mw->cod)
				{
					die = 1;
					break;
				}
			}
		}
		else if(ev.xany.window == mw->window)
			die = mainwin_handle(mw, &ev);
		else if(ev.type == PropertyNotify && (ev.xproperty.atom == ESETROOT_PMAP_ID || ev.xproperty.atom == _XROOTPMAP_ID))
			mainwin_update_background(mw);
		else if(mw->tooltip && ev.xany.window == mw->tooltip->window)
			tooltip_handle(mw->tooltip, &ev);
		else if(ev.type == KeyRelease && ev.xkey.keycode == mw->key_escape)
		{
			refocus = True;
			break;
		}
		else
		{
			dlist *iter;
			for(iter = mw->cod; iter; iter = iter->next)
			{
				ClientWin *cw = (ClientWin *)iter->data;
				if(cw->mini.window == ev.xany.window)
				{
					die = clientwin_handle(cw, &ev);
					break;
				}
			}
		}
	}
	
	/* Unmap the main window and clean up */
	mainwin_unmap(mw);
	XFlush(mw->dpy);
	
	REDUCE(clientwin_unmap((ClientWin*)iter->data), mw->cod);
	dlist_free(mw->cod);
	mw->cod = 0;
	
	if(die == 2)
		DIE_NOW = 1;
	
	if(refocus)
		XSetInputFocus(mw->dpy, focus, RevertToPointerRoot, CurrentTime);
	
	return clients;
}

int
main(void)
{
	dlist *clients = 0, *config = 0;
	Display *dpy = XOpenDisplay(NULL);
	MainWin *mw;
	Imlib_Context context;
	KeyCode keycode;
	KeySym keysym;
	const char *tmp, *homedir;
	char cfgpath[8192];
	Bool invertShift = False;
	
	if(! dpy) {
		fprintf(stderr, "FATAL: Couldn't connect to display.\n");
		return -1;
	}
	
	wm_get_atoms(dpy);
	
	if(! wm_check(dpy)) {
		fprintf(stderr, "FATAL: WM not NETWM or GNOME WM Spec compliant.\n");
		return -1;
	}
	
	homedir = getenv("HOME");
	if(homedir) {
		snprintf(cfgpath, 8191, "%s/%s", homedir, ".skippyrc");
		config = config_load(cfgpath);
	}
	else
		fprintf(stderr, "WARNING: $HOME not set, not loading config.\n");
	
	wm_use_netwm_fullscreen(strcasecmp("true", config_get(config, "general", "useNETWMFullscreen", "true")) == 0);
	wm_ignore_skip_taskbar(strcasecmp("true", config_get(config, "general", "ignoreSkipTaskbar", "false")) == 0);
	
	mw = mainwin_create(dpy, config);
	if(! mw)
	{
		fprintf(stderr, "FATAL: Couldn't create main window.\n");
		config_free(config);
		XCloseDisplay(mw->dpy);
		return -1;
	}
	
	invertShift = strcasecmp("true", config_get(config, "xinerama", "showAll", "false")) == 0;
	
	context = imlib_context_new();
	imlib_context_push(context);
	imlib_context_set_display(dpy);
	imlib_context_set_visual(mw->visual);
	imlib_context_set_colormap(mw->colormap);
	
	tmp = config_get(config, "general", "keysym", "F11");
	keysym = XStringToKeysym(tmp);
	if(keysym == NoSymbol)
	{
		fprintf(stderr, "FATAL: Couldn't look up keysym for '%s', bailing out.\n", tmp);
		config_free(config);
		XCloseDisplay(mw->dpy);
		return -1;
	}
	
	mainwin_update_background(mw);
	XSelectInput(mw->dpy, mw->root, PropertyChangeMask);
	
	keycode = XKeysymToKeycode(mw->dpy, keysym);
	XGrabKey(mw->dpy, keycode, AnyModifier, mw->root, False, GrabModeAsync, GrabModeAsync);
	while(! DIE_NOW)
	{
		XEvent ev;
		XNextEvent(mw->dpy, &ev);
		if(ev.type == PropertyNotify && (ev.xproperty.atom == ESETROOT_PMAP_ID || ev.xproperty.atom == _XROOTPMAP_ID))
			mainwin_update_background(mw);
		else if(ev.type == KeyRelease && ev.xkey.keycode == keycode)
		{
			Window leader = None, focused = wm_get_focused(mw->dpy);
			Bool shifted = (ev.xkey.state & ShiftMask) ? ! invertShift : invertShift;
			
			if(ev.xkey.state & Mod1Mask)
			{
				if(focused != None)
					leader = wm_get_group_leader(mw->dpy, focused);
				if(! leader)
					continue;
			}
			clients = skippy_run(mw, clients, (ev.xkey.state & ControlMask), focused, leader, shifted);
		}
	}
	
	dlist_free_with_func(clients, (dlist_free_func)clientwin_destroy);
	
	XFlush(mw->dpy);
	
	mainwin_destroy(mw);
	
	imlib_context_pop();
	imlib_context_free(context);
	
	XSync(dpy, True);
	XCloseDisplay(dpy);
	config_free(config);
	
	return 0;
}
