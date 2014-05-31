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

static Imlib_Color_Modifier
create_modifier(MainWin *mw, dlist *config, const char *item, const char *d_bright, const char *d_tint, const char *d_opacity)
{
	const char *tmp;
	XColor exact_color, screen_color;
	int i, alpha_v;
	DATA8 red[256], green[256], blue[256], alpha[256];
	Imlib_Color_Modifier modifier = imlib_create_color_modifier();
	
	imlib_context_set_color_modifier(modifier);
	
	tmp = config_get(config, item, "brightness", d_bright);
	imlib_modify_color_modifier_brightness(strtod(tmp, NULL));
	
	imlib_get_color_modifier_tables(red, green, blue, alpha);
	
	tmp = config_get(config, item, "tint", d_tint);
	if(XLookupColor(mw->dpy, mw->colormap, tmp, &exact_color, &screen_color) != 0)
	{
		
		double red_f = (double)exact_color.red / 65536.0,
		       green_f = (double)exact_color.green / 65536.0,
		       blue_f = (double)exact_color.blue / 65536.0;
		
		for(i = 0; i < 256; i++)
		{
			red[i] = (double)red[i] * red_f;
			green[i] = (double)green[i] * green_f;
			blue[i] = (double)blue[i] * blue_f;
		}
	} else
		fprintf(stderr, "WARNING: Couldn't look up tint color'%s'.\n", tmp);
	
	tmp = config_get(config, item, "opacity", d_opacity);
	alpha_v = strtol(tmp, 0, 10);
	if(alpha_v != 255)
	{
		for(i = 0; i < 256; i++)
			alpha[i] = alpha_v;
	}
	
	imlib_set_color_modifier_tables(red, green, blue, alpha);
	
	imlib_context_set_color_modifier(0);
	
	return modifier;
}

MainWin *
mainwin_create(Display *dpy, dlist *config)
{
	const char *tmp;
	XColor screen_color, exact_color;
	XSetWindowAttributes wattr;
	XWindowAttributes rootattr;
	unsigned long valuemask = CWEventMask;
#ifdef XINERAMA
	int event_base, error_base;
#endif /* XINERAMA */
	
	MainWin *mw = (MainWin *)malloc(sizeof(MainWin));
	
	mw->screen = DefaultScreen(dpy);
	mw->visual = imlib_get_best_visual(dpy, mw->screen, &mw->depth);
	mw->colormap = DefaultColormap(dpy, mw->screen);
	mw->root = RootWindow(dpy, mw->screen);
	mw->background = 0;
	mw->bg_pixmap = None;
#ifdef XINERAMA
	mw->xin_info = mw->xin_active = 0;
	mw->xin_screens = 0;
#endif /* XINERAMA */
	mw->x = mw->y = 0;
	
	mw->no_free_color = 0;
	mw->pressed = mw->focus = 0;
	mw->tooltip = 0;
	mw->cod = 0;
	mw->cm_normal = mw->cm_highlight = 0;
	mw->key_up = XKeysymToKeycode(dpy, XK_Up);
	mw->key_down = XKeysymToKeycode(dpy, XK_Down);
	mw->key_left = XKeysymToKeycode(dpy, XK_Left);
	mw->key_right = XKeysymToKeycode(dpy, XK_Right);
	mw->key_enter = XKeysymToKeycode(dpy, XK_Return);
	mw->key_space = XKeysymToKeycode(dpy, XK_space);
	mw->key_escape = XKeysymToKeycode(dpy, XK_Escape);
	mw->key_q = XKeysymToKeycode(dpy, XK_q);
	
	XGetWindowAttributes(dpy, mw->root, &rootattr);
	mw->width = rootattr.width;
	mw->height = rootattr.height;
	
	mw->dpy = dpy;
	
	valuemask |= CWBackPixel;
	wattr.background_pixel = BlackPixel(dpy, mw->screen);
	
	wattr.event_mask = VisibilityChangeMask |
	                   ButtonReleaseMask;
	
	mw->window = XCreateWindow(dpy, mw->root, 0, 0, mw->width, mw->height, 0,
	                           CopyFromParent, InputOutput, CopyFromParent,
	                           valuemask, &wattr);
	if(mw->window == None) {
		free(mw);
		return 0;
	}

#ifdef XINERAMA
# ifdef DEBUG
	fprintf(stderr, "--> checking for Xinerama extension... ");
# endif /* DEBUG */
	if(XineramaQueryExtension(dpy, &event_base, &error_base))
	{
# ifdef DEBUG
	    fprintf(stderr, "yes\n--> checking if Xinerama is enabled... ");
# endif /* DEBUG */
	    if(XineramaIsActive(dpy))
	    {
# ifdef DEBUG
	        fprintf(stderr, "yes\n--> fetching Xinerama info... ");
# endif /* DEBUG */
	        mw->xin_info = XineramaQueryScreens(mw->dpy, &mw->xin_screens);
# ifdef DEBUG	        
		fprintf(stderr, "done (%i screens)\n", mw->xin_screens);
# endif /* DEBUG */
	    }
# ifdef DEBUG
	    else
	        fprintf(stderr, "no\n");
# endif /* DEBUG */
	}
# ifdef DEBUG
	else
	    fprintf(stderr, "no\n");
# endif /* DEBUG */
#endif /* XINERAMA */
	
	tmp = config_get(config, "normal", "border", "black");
	if(! XAllocNamedColor(mw->dpy, mw->colormap, tmp, &screen_color, &exact_color))
	{
		fprintf(stderr, "WARNING: Invalid color '%s', reverting to black.\n", tmp);
		BORDER_COLOR(mw) = BlackPixel(mw->dpy, mw->screen);
		mw->no_free_color = 1;
	} else
		BORDER_COLOR(mw) = screen_color.pixel;
	
	tmp = config_get(config, "highlight", "border", "#d0d0ff");
	if(! XAllocNamedColor(mw->dpy, mw->colormap, tmp, &screen_color, &exact_color))
	{
		fprintf(stderr, "WARNING: Invalid color '%s', reverting to white.\n", tmp);
		HIGHLIGHT_COLOR(mw) = WhitePixel(mw->dpy, mw->screen);
		mw->no_free_color |= 2;
	} else
		HIGHLIGHT_COLOR(mw) = screen_color.pixel;
	
	tmp = config_get(config, "general", "distance", "50");
	DISTANCE(mw) = MAX(1, strtol(tmp, 0, 10));
	
	if(! strcasecmp(config_get(config, "tooltip", "show", "true"), "true"))
		mw->tooltip = tooltip_create(mw, config);
	
	mw->cm_normal = create_modifier(mw, config, "normal", "0.0", "white", "200");
	mw->cm_highlight = create_modifier(mw, config, "highlight", "0.05", "#d0d0ff", "255");
	
	return mw;
}

static
void mainwin_update_bg_pixmap(MainWin *mw)
{
	Imlib_Image tmp;
	XSetWindowAttributes wattr;
	Pixmap dummy;
	
	if(mw->bg_pixmap != None)
		imlib_free_pixmap_and_mask(mw->bg_pixmap);
	
	imlib_context_set_image(mw->background);
	tmp = imlib_create_cropped_image(mw->x, mw->y, mw->width, mw->height);
	imlib_context_set_image(tmp);
	
	imlib_context_set_drawable(mw->window);
	imlib_render_pixmaps_for_whole_image(&mw->bg_pixmap, &dummy);
	
	wattr.background_pixmap = mw->bg_pixmap;
	XChangeWindowAttributes(mw->dpy, mw->window, CWBackPixmap, &wattr);
	XClearArea(mw->dpy, mw->window, 0, 0, 0, 0, False);
	
	imlib_free_image();
}

void
mainwin_update_background(MainWin *mw)
{
	Pixmap dummy = wm_get_root_pmap(mw->dpy);
	Window dummy_root;
	int x, y;
	unsigned int root_w, root_h, border_width, depth;
	
	XGetGeometry(mw->dpy, mw->root, &dummy_root, &x, &y, &root_w, &root_h, &border_width, &depth);
	
	if(mw->background != 0)
	{
		imlib_context_set_image(mw->background);
		imlib_free_image();
	}
	
	if(dummy != None)
	{
		unsigned int width, height;
		
		mw->background = imlib_create_image(root_w, root_h);
		imlib_context_set_image(mw->background);
		
		XGetGeometry(mw->dpy, dummy, &dummy_root, &x, &y, &width, &height, &border_width, &depth);
		imlib_context_set_drawable(dummy);
		imlib_copy_drawable_to_image(0, 0, 0, width, height, 0, 0, 1);
		
		for(x = 1; x < (int)ceil((double)root_w / width); ++x)
			imlib_image_copy_rect(0, 0, width, height, x * width, 0);
		for(y = 1; y < (int)ceil((double)root_h / height); ++y)
			imlib_image_copy_rect(0, 0, root_w, height, 0, y * height);
	}
	else
	{
		mw->background = imlib_create_image(root_w, root_h);
		imlib_context_set_image(mw->background);
		imlib_image_clear();
	}
	
	mainwin_update_bg_pixmap(mw);
	REDUCE(clientwin_render((ClientWin*)iter->data), mw->cod);
}

void
mainwin_update(MainWin *mw)
{
#ifdef XINERAMA
	XineramaScreenInfo *iter;
	int i;
	Window dummy_w;
	int root_x, root_y, dummy_i;
	unsigned int dummy_u;
	
	if(! mw->xin_info || ! mw->xin_screens)
		return;
	
# ifdef DEBUG
	fprintf(stderr, "--> querying pointer... ");
# endif /* DEBUG */
	XQueryPointer(mw->dpy, mw->root, &dummy_w, &dummy_w, &root_x, &root_y, &dummy_i, &dummy_i, &dummy_u);
# ifdef DEBUG	
	fprintf(stderr, "+%i+%i\n", root_x, root_y);
	
	fprintf(stderr, "--> figuring out which screen we're on... ");
# endif /* DEBUG */
	iter = mw->xin_info;
	for(i = 0; i < mw->xin_screens; ++i)
	{
		if(root_x >= iter->x_org && root_x < iter->x_org + iter->width &&
		   root_y >= iter->y_org && root_y < iter->y_org + iter->height)
		{
# ifdef DEBUG
			fprintf(stderr, "screen %i %ix%i+%i+%i\n", iter->screen_number, iter->width, iter->height, iter->x_org, iter->y_org);
# endif /* DEBUG */
			break;
		}
		iter++;
	}
	if(i == mw->xin_screens)
	{
# ifdef DEBUG 
		fprintf(stderr, "unknown\n");
# endif /* DEBUG */
		return;
	}
	mw->x = iter->x_org;
	mw->y = iter->y_org;
	mw->width = iter->width;
	mw->height = iter->height;
	XMoveResizeWindow(mw->dpy, mw->window, iter->x_org, iter->y_org, mw->width, mw->height);
	mw->xin_active = iter;
#endif /* XINERAMA */
	
	mainwin_update_bg_pixmap(mw);
}

void
mainwin_map(MainWin *mw)
{
	wm_set_fullscreen(mw->dpy, mw->window, mw->x, mw->y, mw->width, mw->height);
	mw->pressed = 0;
	XMapWindow(mw->dpy, mw->window);
	XRaiseWindow(mw->dpy, mw->window);
}

void
mainwin_unmap(MainWin *mw)
{
	if(mw->tooltip)
		tooltip_unmap(mw->tooltip);
	XUnmapWindow(mw->dpy, mw->window);
}

void
mainwin_destroy(MainWin *mw)
{
	if(mw->tooltip)
		tooltip_destroy(mw->tooltip);
	
	if(! mw->no_free_color)
		XFreeColors(mw->dpy, mw->colormap, mw->pixels, 2, 0);
	else {
		if(! (mw->no_free_color & 1))
			XFreeColors(mw->dpy, mw->colormap, &BORDER_COLOR(mw), 1, 0);
		if(! (mw->no_free_color & 2))
			XFreeColors(mw->dpy, mw->colormap, &HIGHLIGHT_COLOR(mw), 1, 0);
	}
	
	if(mw->cm_highlight)
	{
		imlib_context_set_color_modifier(mw->cm_highlight);
		imlib_free_color_modifier();
	}
	
	if(mw->cm_normal)
	{
		imlib_context_set_color_modifier(mw->cm_normal);
		imlib_free_color_modifier();
	}
	
	if(mw->background)
	{
		imlib_context_set_image(mw->background);
		imlib_free_image();
	}
	
	if(mw->bg_pixmap != None)
		imlib_free_pixmap_and_mask(mw->bg_pixmap);
	
	XDestroyWindow(mw->dpy, mw->window);
	
#ifdef XINERAMA
	if(mw->xin_info)
		XFree(mw->xin_info);
#endif /* XINERAMA */
	
	free(mw);
}

int
mainwin_handle(MainWin *mw, XEvent *ev)
{
	switch(ev->type)
	{
	case KeyPress:
		if(ev->xkey.keycode == XKeysymToKeycode(mw->dpy, XK_q))
			return 2;
		break;
	case ButtonRelease:
		return 1;
		break;
	case VisibilityNotify:
		if(ev->xvisibility.state && mw->focus)
		{
			XSetInputFocus(mw->dpy, mw->focus->mini.window, RevertToParent, CurrentTime);
			mw->focus = 0;
		}
		break;
	default:
		;
	}
	return 0;
}
