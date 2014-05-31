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

#define INTERSECTS(x1, y1, w1, h1, x2, y2, w2, h2) \
	(((x1 >= x2 && x1 < (x2 + w2)) || (x2 >= x1 && x2 < (x1 + w1))) && \
	 ((y1 >= y2 && y1 < (y2 + h2)) || (y2 >= y1 && y2 < (y1 + h1))))

int
clientwin_cmp_func(dlist *l, void *data)
{
	return ((ClientWin*)l->data)->client.window == (Window)data;
}

int
clientwin_validate_func(dlist *l, void *data)
{
	ClientWin *cw = (ClientWin *)l->data;
	CARD32 desktop = (*(CARD32*)data),
		w_desktop = wm_get_window_desktop(cw->mainwin->dpy, cw->client.window);
	
#ifdef XINERAMA
	if(cw->mainwin->xin_active && ! INTERSECTS(cw->client.x, cw->client.y, cw->client.width, cw->client.height,
	                                           cw->mainwin->xin_active->x_org, cw->mainwin->xin_active->y_org,
	                                           cw->mainwin->xin_active->width, cw->mainwin->xin_active->height))
		return 0;
#endif
	
	return (w_desktop == (CARD32)-1 || desktop == w_desktop) &&
	       wm_validate_window(cw->mainwin->dpy, cw->client.window);
}

int
clientwin_check_group_leader_func(dlist *l, void *data)
{
	ClientWin *cw = (ClientWin *)l->data;
	return wm_get_group_leader(cw->mainwin->dpy, cw->client.window) == *((Window*)data);
}

int
clientwin_sort_func(dlist* a, dlist* b, void* data)
{
	unsigned int pa = ((ClientWin*)a->data)->client.x * ((ClientWin*)a->data)->client.y,
	             pb = ((ClientWin*)b->data)->client.x * ((ClientWin*)b->data)->client.y;
	return (pa < pb) ? -1 : (pa == pb) ? 0 : 1;
}

ClientWin *
clientwin_create(MainWin *mw, Window client)
{
	ClientWin *cw = (ClientWin *)malloc(sizeof(ClientWin));
	XSetWindowAttributes sattr;
	
	cw->mainwin = mw;
	cw->bg_pixmap = None;
	cw->bg_image = 0;
	cw->focused = 0;
	
	sattr.border_pixel = BORDER_COLOR(mw);
	sattr.background_pixel = BlackPixel(mw->dpy, mw->screen);
	sattr.event_mask = ButtonPressMask |
	                   ButtonReleaseMask |
	                   KeyReleaseMask |
	                   EnterWindowMask |
	                   LeaveWindowMask |
	                   PointerMotionMask |
	                   FocusChangeMask;
	
	cw->client.window = client;
	cw->mini.window = XCreateWindow(mw->dpy, mw->window, 0, 0, 1, 1, 5,
	                                CopyFromParent, InputOutput, CopyFromParent,
	                                CWBackPixel | CWBorderPixel | CWEventMask, &sattr);

	if(cw->mini.window == None)
	{
		free(cw);
		return 0;
	}
	
	XSelectInput(cw->mainwin->dpy, cw->client.window, SubstructureNotifyMask | StructureNotifyMask);
	
	return cw;
}

void
clientwin_update(ClientWin *cw)
{
	Window tmpwin;
	XWindowAttributes wattr;
	
	XGetWindowAttributes(cw->mainwin->dpy, cw->client.window, &wattr);
	
	XTranslateCoordinates(cw->mainwin->dpy, cw->client.window, wattr.root,
		                      -wattr.border_width,
		                      -wattr.border_width,
		                      &cw->client.x, &cw->client.y, &tmpwin);
	cw->client.width = wattr.width;
	cw->client.height = wattr.height;
	
	cw->mini.x = cw->mini.y = 0;
	cw->mini.width = cw->mini.height = 1;
}

void
clientwin_destroy(ClientWin *cw)
{
	if(cw->bg_pixmap != None)
		XFreePixmap(cw->mainwin->dpy, cw->bg_pixmap);
	
	if(cw->bg_image)
	{
		imlib_context_set_image(cw->bg_image);
		imlib_free_image();
	}
	
	XDestroyWindow(cw->mainwin->dpy, cw->mini.window);
	
	free(cw);
}

void
clientwin_snap(ClientWin *cw)
{
	struct timespec req, rem;
	
	XRaiseWindow(cw->mainwin->dpy, cw->client.window);
	XFlush(cw->mainwin->dpy);
	
	req.tv_sec = 0;
	req.tv_nsec = 500000000;
	while(nanosleep(&req, &rem))
		req = rem;
	
	imlib_context_set_drawable(cw->client.window);
	cw->bg_image = imlib_create_image_from_drawable(0, 0, 0, cw->client.width, cw->client.height, 1);
	
	cw->mainwin->stack_touched = True;
}

void
clientwin_render(ClientWin *cw)
{
	if(cw->bg_pixmap != None) {
		XFreePixmap(cw->mainwin->dpy, cw->bg_pixmap);
		cw->bg_pixmap = None;
	}
	
	if(cw->bg_image) {
		Imlib_Image tmp;

		cw->bg_pixmap = XCreatePixmap(cw->mainwin->dpy, cw->mini.window, cw->mini.width, cw->mini.height, cw->mainwin->depth);

		imlib_context_set_image(cw->mainwin->background);
		tmp = imlib_create_cropped_image(cw->mainwin->x + cw->mini.x, cw->mainwin->y + cw->mini.y, cw->mini.width, cw->mini.height);
		imlib_context_set_image(tmp);
		
		imlib_context_set_color_modifier(cw->focused ? cw->mainwin->cm_highlight : cw->mainwin->cm_normal);
		imlib_blend_image_onto_image(cw->bg_image, 1, 0, 0, cw->client.width, cw->client.height, 0, 0, cw->mini.width, cw->mini.height);
		imlib_context_set_color_modifier(0);
		
		imlib_context_set_drawable(cw->bg_pixmap);
		imlib_render_image_on_drawable(0, 0);
		
		imlib_free_image();
		
		XSetWindowBackgroundPixmap(cw->mainwin->dpy, cw->mini.window, cw->bg_pixmap);
	} else
		XSetWindowBackground(cw->mainwin->dpy, cw->mini.window, BlackPixel(cw->mainwin->dpy, cw->mainwin->depth));
	
	XClearArea(cw->mainwin->dpy, cw->mini.window, 0, 0, 0, 0, True);
}

void
clientwin_move(ClientWin *cw, float f, int x, int y)
{
	int border = MAX(1, (double)DISTANCE(cw->mainwin) * f * 0.25);
	XSetWindowBorderWidth(cw->mainwin->dpy, cw->mini.window, border);
	
	cw->mini.x = x + (int)cw->x * f;
	cw->mini.y = y + (int)cw->y * f;
	cw->mini.width = MAX(1, (int)cw->client.width * f);
	cw->mini.height = MAX(1, (int)cw->client.height * f);
	XMoveResizeWindow(cw->mainwin->dpy, cw->mini.window, cw->mini.x - border, cw->mini.y - border, cw->mini.width, cw->mini.height);
}

void
clientwin_map(ClientWin *cw, int force)
{
	if(cw->bg_image)
	{
		imlib_context_set_image(cw->bg_image);
		if(force || imlib_image_get_width() != cw->client.width || imlib_image_get_height() != cw->client.height)
		{
			imlib_free_image();
			cw->bg_image = 0;
			clientwin_snap(cw);
		}
	} else
		clientwin_snap(cw); 
	
	clientwin_render(cw);
	
	XMapWindow(cw->mainwin->dpy, cw->mini.window);
}

void
clientwin_unmap(ClientWin *cw)
{
	XUnmapWindow(cw->mainwin->dpy, cw->mini.window);
	XSetWindowBackgroundPixmap(cw->mainwin->dpy, cw->mini.window, None);
	XSetWindowBorder(cw->mainwin->dpy, cw->mini.window, BORDER_COLOR(cw->mainwin));
	cw->focused = 0;
	
	if(cw->bg_pixmap)
	{
		XFreePixmap(cw->mainwin->dpy, cw->bg_pixmap);
		cw->bg_pixmap = None;
	}
}

static void
childwin_focus(ClientWin *cw)
{
	XWarpPointer(cw->mainwin->dpy, None, cw->client.window, 0, 0, 0, 0, cw->client.width / 2, cw->client.height / 2);
	XRaiseWindow(cw->mainwin->dpy, cw->client.window);
	XSetInputFocus(cw->mainwin->dpy, cw->client.window, RevertToParent, CurrentTime);
}

int
clientwin_handle(ClientWin *cw, XEvent *ev)
{
	if((ev->type == ButtonRelease && ev->xbutton.button == 1 && cw->mainwin->pressed == cw)) {
		if((ev->xbutton.x >= 0 && ev->xbutton.y >= 0 && ev->xbutton.x < cw->mini.width && ev->xbutton.y < cw->mini.height))
			childwin_focus(cw);
		cw->mainwin->pressed = 0;
		return 1;
	} else if(ev->type == KeyRelease) {
		if(ev->xkey.keycode == cw->mainwin->key_up)
			focus_up(cw);
		else if(ev->xkey.keycode == cw->mainwin->key_down)
			focus_down(cw);
		else if(ev->xkey.keycode == cw->mainwin->key_left)
			focus_left(cw);
		else if(ev->xkey.keycode == cw->mainwin->key_right)
			focus_right(cw);
		else if(ev->xkey.keycode == cw->mainwin->key_enter || ev->xkey.keycode == cw->mainwin->key_space) {
			childwin_focus(cw);
			return 1;
		}
	} else if(ev->type == ButtonPress && ev->xbutton.button == 1) {
		cw->mainwin->pressed = cw;
	} else if(ev->type == FocusIn) {
		cw->focused = 1;
		XRaiseWindow(cw->mainwin->dpy, cw->mini.window);
		XSetWindowBorder(cw->mainwin->dpy, cw->mini.window, HIGHLIGHT_COLOR(cw->mainwin));
		clientwin_render(cw);
		XFlush(cw->mainwin->dpy);
	} else if(ev->type == FocusOut) {
		cw->focused = 0;
		XSetWindowBorder(cw->mainwin->dpy, cw->mini.window, BORDER_COLOR(cw->mainwin));
		clientwin_render(cw);
		XFlush(cw->mainwin->dpy);
	} else if(ev->type == EnterNotify) {
		XSetInputFocus(cw->mainwin->dpy, cw->mini.window, RevertToNone, CurrentTime);
		if(cw->mainwin->tooltip)
		{
			int win_title_len = 0;
			FcChar8 *win_title = wm_get_window_title(cw->mainwin->dpy, cw->client.window, &win_title_len);
			if(win_title)
			{
				tooltip_map(cw->mainwin->tooltip,
				            ev->xcrossing.x_root + 20, ev->xcrossing.y_root + 20,
				            win_title, win_title_len);
				free(win_title);
			}
		}
	} else if(ev->type == MotionNotify) {
		if(cw->mainwin->tooltip)
			tooltip_move(cw->mainwin->tooltip, ev->xmotion.x_root + 20, ev->xcrossing.y_root + 20);
	} else if(ev->type == LeaveNotify) {
		if(cw->mainwin->tooltip)
			tooltip_unmap(cw->mainwin->tooltip);
	}
	return 0;
}
