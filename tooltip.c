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

#define TT_BORDER(tt) tt->pixels[0]
#define TT_BACKGROUND(tt) tt->pixels[1]

void
tooltip_destroy(Tooltip *tt)
{
	if(tt->text)
		free(tt->text);
	if(tt->font)
		XftFontClose(tt->mainwin->dpy, tt->font);
	if(tt->draw)
		XftDrawDestroy(tt->draw);
	if(tt->color_allocated)
		XftColorFree(tt->mainwin->dpy,
		             tt->mainwin->visual,
		             tt->mainwin->colormap,
		             &tt->color);
	if(tt->window != None)
		XDestroyWindow(tt->mainwin->dpy, tt->window);
	
	if(! tt->no_free_color)
		XFreeColors(tt->mainwin->dpy, tt->mainwin->colormap, tt->pixels, 2, 0);
	else {
		if(! (tt->no_free_color & 1))
			XFreeColors(tt->mainwin->dpy, tt->mainwin->colormap, &TT_BORDER(tt), 1, 0);
		if(! (tt->no_free_color & 2))
			XFreeColors(tt->mainwin->dpy, tt->mainwin->colormap, &TT_BACKGROUND(tt), 1, 0);
	}
	
	free(tt);
}

Tooltip *
tooltip_create(MainWin *mw, dlist *config)
{
	Tooltip *tt;
	XSetWindowAttributes attr;
	const char *tmp, *text_color;
	XColor screen_color, exact_color;
	
	tt = (Tooltip *)malloc(sizeof(Tooltip));
	if(! tt)
		return 0;
	
	tt->mainwin = mw;
	tt->window = None;
	tt->font = 0;
	tt->draw = 0;
	tt->color_allocated = False;
	tt->text = 0;
	tt->no_free_color = 0;
	
	tmp = config_get(config, "tooltip", "border", "black");
	if(! XAllocNamedColor(mw->dpy, mw->colormap, tmp, &screen_color, &exact_color))
	{
		fprintf(stderr, "WARNING: Invalid color '%s', reverting to black.\n", tmp);
		TT_BORDER(tt) = BlackPixel(mw->dpy, mw->screen);
		tt->no_free_color |= 1;
	} else
		TT_BORDER(tt) = screen_color.pixel;
	
	tmp = config_get(config, "tooltip", "background", "#e0e0ff");
	if(! XAllocNamedColor(mw->dpy, mw->colormap, tmp, &screen_color, &exact_color))
	{
		fprintf(stderr, "WARNING: Invalid color '%s', reverting to white.\n", tmp);
		TT_BACKGROUND(tt) = WhitePixel(mw->dpy, mw->screen);
		tt->no_free_color |= 2;
	} else
		TT_BACKGROUND(tt) = screen_color.pixel;
	
	text_color = config_get(config, "tooltip", "text", "black");
	
	attr.override_redirect = True;
	attr.border_pixel = TT_BORDER(tt);
	attr.background_pixel = TT_BACKGROUND(tt);
	attr.event_mask = ExposureMask;
	
	tt->window = XCreateWindow(mw->dpy, mw->root,
	                           0, 0, 1, 1,
	                           1, mw->depth,
	                           InputOutput, mw->visual,
	                           CWBorderPixel|CWBackPixel|CWOverrideRedirect|CWEventMask,
	                           &attr);
	if(tt->window == None)
	{
		fprintf(stderr, "WARNING: Couldn't create tooltip window.\n");
		tooltip_destroy(tt);
		return 0;
	}
	
	if(! XftColorAllocName(mw->dpy, mw->visual, mw->colormap, text_color, &tt->color))
	{
		fprintf(stderr, "WARNING: Couldn't allocate color '%s'.\n", text_color);
		tooltip_destroy(tt);
		return 0;
	}
	tt->color_allocated = True;
	
	tt->draw = XftDrawCreate(mw->dpy, tt->window, mw->visual, mw->colormap);
	if(! tt->draw)
	{
		fprintf(stderr, "WARNING: Couldn't create Xft draw surface.\n");
		tooltip_destroy(tt);
		return 0;
	}
	
	tt->font = XftFontOpenName(mw->dpy, mw->screen, config_get(config, "tooltip", "font", "fixed-11:weight=bold"));
	if(! tt->font)
	{
		fprintf(stderr, "WARNING: Couldn't open Xft font.\n");
		tooltip_destroy(tt);
		return 0;
	}
	
	tt->font_height = tt->font->ascent + tt->font->descent;
	
	return tt;
}

void
tooltip_map(Tooltip *tt, int x, int y, const FcChar8 *text, int len)
{
	XUnmapWindow(tt->mainwin->dpy, tt->window);
	
	XftTextExtents8(tt->mainwin->dpy, tt->font, text, len, &tt->extents);
	
	XResizeWindow(tt->mainwin->dpy, tt->window, tt->extents.width + 6, tt->font_height + 4);
	tooltip_move(tt, x, y);
	
	if(tt->text)
		free(tt->text);
	
	tt->text = (FcChar8 *)malloc(len);
	memcpy(tt->text, text, len);
	
	tt->text_len = len;
	
	XMapWindow(tt->mainwin->dpy, tt->window);
	XRaiseWindow(tt->mainwin->dpy, tt->window);
}

void
tooltip_move(Tooltip *tt, int x, int y)
{
	if(x + tt->extents.width + 7 > tt->mainwin->x + tt->mainwin->width)
		x = tt->mainwin->x + tt->mainwin->width - tt->extents.width - 7;
	x = MAX(0, x);
	
	if(y + tt->extents.height + 8 > tt->mainwin->y + tt->mainwin->height)
		y = tt->mainwin->height + tt->mainwin->y - tt->extents.height - 8;
	y = MAX(0, y);
	
	XMoveWindow(tt->mainwin->dpy, tt->window, x, y);
}

void
tooltip_unmap(Tooltip *tt)
{
	XUnmapWindow(tt->mainwin->dpy, tt->window);
	if(tt->text)
		free(tt->text);
	tt->text = 0;
	tt->text_len = 0;
}

void
tooltip_handle(Tooltip *tt, XEvent *ev)
{
	if(! tt->text)
		return;
	
	if(ev->type == Expose && ev->xexpose.count == 0)
	{
		XClearWindow(tt->mainwin->dpy, tt->window);
		XftDrawString8(tt->draw, &tt->color, tt->font, 3, tt->extents.y + (tt->font_height - tt->extents.y + 4) / 2, tt->text, tt->text_len);
	}
}
