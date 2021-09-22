/* Copyright 2011-2013 Bert Muennich
 *
 * This file is part of sxiv.
 *
 * sxiv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * sxiv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with sxiv.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _POSIX_C_SOURCE 200112L
#define _WINDOW_CONFIG

#include <string.h>
#include <locale.h>
#include <X11/cursorfont.h>

#include "options.h"
#include "util.h"
#include "window.h"
#include "config.h"

#include <X11/Xft/Xft.h>
#include <stdint.h>

enum {
	H_TEXT_PAD = 5,
	V_TEXT_PAD = 1
};

static Cursor carrow;
static Cursor cnone;
static Cursor chand;
static Cursor cwatch;
static GC gc;

Atom wm_delete_win;

/* static struct { */
/* 	int ascent; */
/* 	int descent; */
/* 	XFontStruct *xfont; */
/* 	XFontSet set; */
/* } font; */

static XftFont *font;
static int fontheight;
static double fontsize;
static int barheight;

void win_init_font(Display *dpy, const char *fontstr)
{
	if ((font = XftFontOpenName(dpy, DefaultScreen(dpy), fontstr)) == NULL)
		error(EXIT_FAILURE, 0, "Error loading font '%s'", fontstr);
	fontheight = font->ascent + font->descent;
	FcPatternGetDouble(font->pattern, FC_SIZE, 0, &fontsize);
	barheight = fontheight + 2 * V_TEXT_PAD;
}

/* void win_init_font(, const char *fontstr) */
/* { */
/* 	int n; */
/* 	char *def, **missing; */

/* 	font.set = XCreateFontSet(dpy, fontstr, &missing, &n, &def); */
/* 	if (missing) */
/* 		XFreeStringList(missing); */
/* 	if (font.set) { */
/* 		XFontStruct **xfonts; */
/* 		char **font_names; */

/* 		font.ascent = font.descent = 0; */
/* 		XExtentsOfFontSet(font.set); */
/* 		n = XFontsOfFontSet(font.set, &xfonts, &font_names); */
/* 		while (n--) { */
/* 			font.ascent  = MAX(font.ascent, (*xfonts)->ascent); */
/* 			font.descent = MAX(font.descent,(*xfonts)->descent); */
/* 			xfonts++; */
/* 		} */
/* 	} else { */
/* 		if ((font.xfont = XLoadQueryFont(dpy, fontstr)) == NULL && */
/* 		    (font.xfont = XLoadQueryFont(dpy, "fixed")) == NULL) */
/* 		{ */
/* 			die("could not load font: %s", fontstr); */
/* 		} */
/* 		font.ascent  = font.xfont->ascent; */
/* 		font.descent = font.xfont->descent; */
/* 	} */
/* 	fontheight = font.ascent + font.descent; */
/* 	barheight = fontheight + 2 * V_TEXT_PAD; */
/* } */

unsigned long win_alloc_color(win_t *win, const char *name)
{
	XColor col;

	if (win == NULL)
		return 0UL;
	if (XAllocNamedColor(win->env.dpy,
	                     DefaultColormap(win->env.dpy, win->env.scr),
	                     name, &col, &col) == 0)
	{
		die("could not allocate color: %s", name);
	}
	return col.pixel;
}

void win_init(win_t *win)
{
	win_env_t *e;

	if (win == NULL)
		return;

	memset(win, 0, sizeof(win_t));

	e = &win->env;
	if ((e->dpy = XOpenDisplay(NULL)) == NULL)
		die("could not open display");

	e->scr = DefaultScreen(e->dpy);
	e->scrw = DisplayWidth(e->dpy, e->scr);
	e->scrh = DisplayHeight(e->dpy, e->scr);
	e->vis = DefaultVisual(e->dpy, e->scr);
	e->cmap = DefaultColormap(e->dpy, e->scr);
	e->depth = DefaultDepth(e->dpy, e->scr);

	if (setlocale(LC_CTYPE, "") == NULL || XSupportsLocale() == 0)
		warn("no locale support");

	win_init_font(e->dpy, BAR_FONT);

	win->white     = WhitePixel(e->dpy, e->scr);
	win->bgcol     = win_alloc_color(win, WIN_BG_COLOR);
	win->fscol     = win_alloc_color(win, WIN_FS_COLOR);
	win->selcol    = win_alloc_color(win, SEL_COLOR);
	win->bar.bgcol = win_alloc_color(win, BAR_BG_COLOR);
	win->bar.fgcol = win_alloc_color(win, BAR_FG_COLOR);
	win->bar.h     = options->hide_bar ? 0 : barheight;

	win->sizehints.flags = PWinGravity;
	win->sizehints.win_gravity = NorthWestGravity;
	if (options->fixed_win)
		/* actual min/max values set in win_update_sizehints() */
		win->sizehints.flags |= PMinSize | PMaxSize;

	wm_delete_win = XInternAtom(e->dpy, "WM_DELETE_WINDOW", False);
}

void win_update_sizehints(win_t *win)
{
	if (win == NULL || win->xwin == None)
		return;

	if ((win->sizehints.flags & USSize) != 0) {
		win->sizehints.width  = win->w;
		win->sizehints.height = win->h + win->bar.h;
	}
	if ((win->sizehints.flags & USPosition) != 0) {
		win->sizehints.x = win->x;
		win->sizehints.y = win->y;
	}
	if (options->fixed_win) {
		win->sizehints.min_width  = win->sizehints.max_width  = win->w;
		win->sizehints.min_height = win->sizehints.max_height = win->h + win->bar.h;
	}
	XSetWMNormalHints(win->env.dpy, win->xwin, &win->sizehints);
}

void win_open(win_t *win)
{
	win_env_t *e;
	XClassHint classhint;
	XSetWindowAttributes attr;
	unsigned long attr_mask;
	XColor col;
	char none_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	Pixmap none;
	int gmask;

	if (win == NULL)
		return;

	e = &win->env;

	/* determine window offsets, width & height */
	if (options->geometry == NULL)
		gmask = 0;
	else
		gmask = XParseGeometry(options->geometry, &win->x, &win->y,
		                       &win->w, &win->h);
	if ((gmask & WidthValue) != 0)
		win->sizehints.flags |= USSize;
	else
		win->w = WIN_WIDTH;
	if ((gmask & HeightValue) != 0)
		win->sizehints.flags |= USSize;
	else
		win->h = WIN_HEIGHT;
	if ((gmask & XValue) != 0) {
		if ((gmask & XNegative) != 0) {
			win->x += e->scrw - win->w;
			win->sizehints.win_gravity = NorthEastGravity;
		}
		win->sizehints.flags |= USPosition;
	} else {
		win->x = (e->scrw - win->w) / 2;
	}
	if ((gmask & YValue) != 0) {
		if ((gmask & YNegative) != 0) {
			win->y += e->scrh - win->h;
			if (win->sizehints.win_gravity == NorthEastGravity)
				win->sizehints.win_gravity = SouthEastGravity;
			else
				win->sizehints.win_gravity = SouthWestGravity;
		}
		win->sizehints.flags |= USPosition;
	} else {
		win->y = (e->scrh - win->h) / 2;
	}

	attr.background_pixel = win->bgcol;
	attr_mask = CWBackPixel;

	win->xwin = XCreateWindow(e->dpy, RootWindow(e->dpy, e->scr),
	                          win->x, win->y, win->w, win->h, 0,
	                          e->depth, InputOutput, e->vis, attr_mask, &attr);
	if (win->xwin == None)
		die("could not create window");

	XSelectInput(e->dpy, win->xwin,
	             ExposureMask | ButtonReleaseMask | ButtonPressMask |
	             KeyPressMask | PointerMotionMask | StructureNotifyMask);

	carrow = XCreateFontCursor(e->dpy, XC_left_ptr);
	chand = XCreateFontCursor(e->dpy, XC_fleur);
	cwatch = XCreateFontCursor(e->dpy, XC_watch);

	if (XAllocNamedColor(e->dpy, DefaultColormap(e->dpy, e->scr), "black",
	                     &col, &col) == 0)
	{
		die("could not allocate color: black");
	}
	none = XCreateBitmapFromData(e->dpy, win->xwin, none_data, 8, 8);
	cnone = XCreatePixmapCursor(e->dpy, none, none, &col, &col, 0, 0);

	gc = XCreateGC(e->dpy, win->xwin, 0, None);

	win_set_title(win, "sxiv");

	classhint.res_class = "Sxiv";
	classhint.res_name = options->res_name != NULL ? options->res_name : "sxiv";
	XSetClassHint(e->dpy, win->xwin, &classhint);

	XSetWMProtocols(e->dpy, win->xwin, &wm_delete_win, 1);

	win->h -= win->bar.h;
	win_update_sizehints(win);

	XMapWindow(e->dpy, win->xwin);
	XFlush(e->dpy);

	if (options->fullscreen)
		win_toggle_fullscreen(win);
}

void win_close(win_t *win)
{
	if (win == NULL || win->xwin == None)
		return;

	XFreeCursor(win->env.dpy, carrow);
	XFreeCursor(win->env.dpy, cnone);
	XFreeCursor(win->env.dpy, chand);
	XFreeCursor(win->env.dpy, cwatch);

	XFreeGC(win->env.dpy, gc);

	XDestroyWindow(win->env.dpy, win->xwin);
	XCloseDisplay(win->env.dpy);
}

bool win_configure(win_t *win, XConfigureEvent *c)
{
	bool changed;

	if (win == NULL || c == NULL)
		return false;

	if ((changed = win->w != c->width || win->h + win->bar.h != c->height)) {
		if (win->pm != None) {
			XFreePixmap(win->env.dpy, win->pm);
			win->pm = None;
		}
	}

	win->x = c->x;
	win->y = c->y;
	win->w = c->width;
	win->h = c->height - win->bar.h;
	win->bw = c->border_width;

	return changed;
}

void win_expose(win_t *win, XExposeEvent *e)
{
	if (win == NULL || win->xwin == None || win->pm == None || e == NULL)
		return;

	XCopyArea(win->env.dpy, win->pm, win->xwin, gc,
	          e->x, e->y, e->width, e->height, e->x, e->y);
}

bool win_moveresize(win_t *win, int x, int y, unsigned int w, unsigned int h)
{
	if (win == NULL || win->xwin == None)
		return false;

	/* caller knows nothing about the bar */
	h += win->bar.h;

	x = MAX(0, x);
	y = MAX(0, y);
	w = MIN(w, win->env.scrw - 2 * win->bw);
	h = MIN(h, win->env.scrh - 2 * win->bw);

	if (win->x == x && win->y == y && win->w == w && win->h + win->bar.h == h)
		return false;

	win->x = x;
	win->y = y;
	win->w = w;
	win->h = h - win->bar.h;

	win_update_sizehints(win);

	XMoveResizeWindow(win->env.dpy, win->xwin, x, y, w, h);

	if (win->pm != None) {
		XFreePixmap(win->env.dpy, win->pm);
		win->pm = None;
	}

	return true;
}

void win_toggle_fullscreen(win_t *win)
{
	XEvent ev;
	XClientMessageEvent *cm;

	if (win == NULL || win->xwin == None)
		return;

	win->fullscreen = !win->fullscreen;

	memset(&ev, 0, sizeof(ev));
	ev.type = ClientMessage;

	cm = &ev.xclient;
	cm->window = win->xwin;
	cm->message_type = XInternAtom(win->env.dpy, "_NET_WM_STATE", False);
	cm->format = 32;
	cm->data.l[0] = win->fullscreen;
	cm->data.l[1] = XInternAtom(win->env.dpy, "_NET_WM_STATE_FULLSCREEN", False);
	cm->data.l[2] = cm->data.l[3] = 0;

	XSendEvent(win->env.dpy, DefaultRootWindow(win->env.dpy), False,
	           SubstructureNotifyMask | SubstructureRedirectMask, &ev);
}

void win_toggle_bar(win_t *win)
{
	if (win == NULL || win->xwin == None)
		return;

	if (win->bar.h != 0) {
		win->h += win->bar.h;
		win->bar.h = 0;
	} else {
		win->bar.h = barheight;
		win->h -= win->bar.h;
	}
}

void win_clear(win_t *win)
{
	int h;
	win_env_t *e;

	if (win == NULL || win->xwin == None)
		return;

	h = win->h + win->bar.h;
	e = &win->env;

	if (win->pm == None)
		win->pm = XCreatePixmap(e->dpy, win->xwin, win->w, h, e->depth);

	XSetForeground(e->dpy, gc, win->fullscreen ? win->fscol : win->bgcol);
	XFillRectangle(e->dpy, win->pm, gc, 0, 0, win->w, h);
}

#define UTF_SIZ       4
#define UTF_INVALID   0xFFFD
#define uchar char

static const uchar utfbyte[UTF_SIZ + 1] = {0x80,    0, 0xC0, 0xE0, 0xF0};
static const uchar utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const uint32_t utfmin[UTF_SIZ + 1] = {       0,    0,  0x80,  0x800,  0x10000};
static const uint32_t utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

#define BETWEEN(x, a, b)	((a) <= (x) && (x) <= (b))
#define LEN(a)			(sizeof(a) / sizeof(a)[0])


size_t
utf8validate(uint32_t *u, size_t i)
{
	if (!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
		*u = UTF_INVALID;
	for (i = 1; *u > utfmax[i]; ++i)
		;

	return i;
}

uint32_t
utf8decodebyte(char c, size_t *i)
{
	for (*i = 0; *i < LEN(utfmask); ++(*i))
		if (((uchar)c & utfmask[*i]) == utfbyte[*i])
			return (uchar)c & ~utfmask[*i];

	return 0;
}

size_t
utf8decode(const char *c, uint32_t *u, size_t clen)
{
	size_t i, j, len, type;
	uint32_t udecoded;

	*u = UTF_INVALID;
	if (!clen)
		return 0;
	udecoded = utf8decodebyte(c[0], &len);
	if (!BETWEEN(len, 1, UTF_SIZ))
		return 1;
	for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
		udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
		if (type != 0)
			return j;
	}
	if (j < len)
		return 0;
	*u = udecoded;
	utf8validate(u, len);

	return len;
}

#define TEXTWIDTH(win, text, len) \
	win_draw_text(win, NULL, NULL, 0, 0, text, len, 0)

int win_draw_text(win_t *win, XftDraw *d, unsigned long *color, int x, int y,
                  char *text, int len, int w)
{
    unsigned long c = 0;
    if (color != NULL) {
        c = *color;
    }
#define TO16(c) \
    ((unsigned short) (((float) (c) / 0xff) * 0xffffU))

    XRenderColor xc;
    xc.alpha = 0xfffff;
    xc.blue = TO16(c & 0xff);
    c >>= 8;
    xc.green = TO16(c & 0xff);
    c >>= 8;
    xc.red = TO16(c & 0xff);

    XftColor real_color;
    XftColorAllocValue(win->env.dpy, win->env.vis, win->env.cmap, &xc, &real_color);

	int err, tw = 0;
	char *t, *next;
	uint32_t rune;
	XftFont *f;
	FcCharSet *fccharset;
	XGlyphInfo ext;

	for (t = text; t - text < len; t = next) {
		next = t + utf8decode(t, &rune, &err);
		if (XftCharExists(win->env.dpy, font, rune)) {
			f = font;
		} else { /* fallback font */
			fccharset = FcCharSetCreate();
			FcCharSetAddChar(fccharset, rune);
			f = XftFontOpen(win->env.dpy, win->env.scr, FC_CHARSET, FcTypeCharSet,
			                fccharset, FC_SCALABLE, FcTypeBool, FcTrue, NULL);
			FcCharSetDestroy(fccharset);
		}
		XftTextExtentsUtf8(win->env.dpy, f, (XftChar8*)t, next - t, &ext);
		tw += ext.xOff;
		if (tw <= w) {
			XftDrawStringUtf8(d, &real_color, f, x, y, (XftChar8*)t, next - t);
			x += ext.xOff;
		}
		if (f != font)
			XftFontClose(win->env.dpy, f);
	}
	return tw;
}

void win_draw_bar(win_t *win)
{
	int len, x, y, w, tw;
	win_env_t *e;
	char *l, *r;
	XftDraw *d;

	if ((l = win->bar.l) == NULL || (r = win->bar.r) == NULL)
		return;

	e = &win->env;
	y = win->h + font->ascent + V_TEXT_PAD;
	w = win->w - 2*H_TEXT_PAD;
	d = XftDrawCreate(e->dpy, win->pm, DefaultVisual(e->dpy, e->scr),
	                  DefaultColormap(e->dpy, e->scr));

	XSetForeground(e->dpy, gc, win->bar.bgcol);
	XFillRectangle(e->dpy, win->pm, gc, 0, win->h, win->w, win->bar.h);

	XSetForeground(e->dpy, gc, win->bar.fgcol);
	XSetBackground(e->dpy, gc, win->bar.bgcol);

	if ((len = strlen(r)) > 0) {
		if ((tw = TEXTWIDTH(win, r, len)) > w)
			return;
		x = win->w - tw - H_TEXT_PAD;
		w -= tw;
		win_draw_text(win, d, &win->bar.fgcol, x, y, r, len, tw);
	}
	if ((len = strlen(l)) > 0) {
		x = H_TEXT_PAD;
		w -= 2 * H_TEXT_PAD; /* gap between left and right parts */
		win_draw_text(win, d, &win->bar.fgcol, x, y, l, len, w);
	}
	XftDrawDestroy(d);
}

/* void win_draw_bar(win_t *win) */
/* { */
/* 	int len, olen, x, y, w, tw; */
/* 	char rest[3]; */
/* 	const char *dots = "..."; */
/* 	win_env_t *e; */

/* 	if (win == NULL || win->xwin == None || win->pm == None) */
/* 		return; */

/* 	e = &win->env; */
/* 	y = win->h + font.ascent + V_TEXT_PAD; */
/* 	w = win->w; */

/* 	XSetForeground(e->dpy, gc, win->bar.bgcol); */
/* 	XFillRectangle(e->dpy, win->pm, gc, 0, win->h, win->w, win->bar.h); */

/* 	XSetForeground(e->dpy, gc, win->bar.fgcol); */
/* 	XSetBackground(e->dpy, gc, win->bar.bgcol); */

/* 	if ((len = strlen(win->bar.r)) > 0) { */
/* 		if ((tw = win_textwidth(win->bar.r, len, true)) > w) */
/* 			return; */
/* 		x = win->w - tw + H_TEXT_PAD; */
/* 		w -= tw; */
/*         /1* Xutf8DrawString(e->dpy, win->pm, font.set, gc, x, y, win->bar.l, len); *1/ */
/* 		if (font.set) */
/* 			XmbDrawString(e->dpy, win->pm, font.set, gc, x, y, win->bar.r, len); */
/* 		else */
/* 			XDrawString(e->dpy, win->pm, gc, x, y, win->bar.r, len); */
/* 	} */
/* 	if ((len = strlen(win->bar.l)) > 0) { */
/* 		olen = len; */
/* 		while (len > 0 && (tw = win_textwidth(win->bar.l, len, true)) > w) */
/* 			len--; */
/* 		if (len > 0) { */
/* 			if (len != olen) { */
/* 				w = strlen(dots); */
/* 				if (len <= w) */
/* 					return; */
/* 				memcpy(rest, win->bar.l + len - w, w); */
/* 				memcpy(win->bar.l + len - w, dots, w); */
/* 			} */
/* 			x = H_TEXT_PAD; */
/* 			if (font.set) */
/* 				XmbDrawString(e->dpy, win->pm, font.set, gc, x, y, win->bar.l, len); */
/*             /1* Xutf8DrawString(e->dpy, win->pm, font.set, gc, x, y, win->bar.l, len); *1/ */
/* 			else */
/*                 XDrawString(e->dpy, win->pm, gc, x, y, win->bar.l, len); */
/* 				/1* Xutf8DrawString(e->dpy, win->pm, font.set, gc, x, y, win->bar.l, len); *1/ */
/* 			if (len != olen) */
/* 			  memcpy(win->bar.l + len - w, rest, w); */
/* 		} */
/* 	} */
/* } */

void win_draw(win_t *win)
{
	if (win == NULL || win->xwin == None || win->pm == None)
		return;

	if (win->bar.h > 0)
		win_draw_bar(win);

	XCopyArea(win->env.dpy, win->pm, win->xwin, gc,
	          0, 0, win->w, win->h + win->bar.h, 0, 0);
}

void win_draw_rect(win_t *win, Pixmap pm, int x, int y, int w, int h,
                   bool fill, int lw, unsigned long col)
{
	XGCValues gcval;

	if (win == NULL || pm == None)
		return;

	gcval.line_width = lw;
	gcval.foreground = col;
	XChangeGC(win->env.dpy, gc, GCForeground | GCLineWidth, &gcval);

	if (fill)
		XFillRectangle(win->env.dpy, pm, gc, x, y, w, h);
	else
		XDrawRectangle(win->env.dpy, pm, gc, x, y, w, h);
}

void win_update_bar(win_t *win)
{
	if (win == NULL || win->xwin == None || win->pm == None)
		return;

	if (win->bar.h > 0) {
		win_draw_bar(win);
		XCopyArea(win->env.dpy, win->pm, win->xwin, gc,
		          0, win->h, win->w, win->bar.h, 0, win->h);
	}
}

/* int win_textwidth(const char *text, unsigned int len, bool with_padding) */
/* { */
/* 	XRectangle r; */
/* 	int padding = with_padding ? 2 * H_TEXT_PAD : 0; */

/* 	if (font.set) { */
/* 		XmbTextExtents(font.set, text, len, NULL, &r); */
/* 		return r.width + padding; */
/* 	} else { */
/* 		return XTextWidth(font.xfont, text, len) + padding; */
/* 	} */
/* } */

void win_set_title(win_t *win, const char *title)
{
	if (win == NULL || win->xwin == None)
		return;

	if (title == NULL)
		title = "sxiv";

	XStoreName(win->env.dpy, win->xwin, title);
	XSetIconName(win->env.dpy, win->xwin, title);

	XChangeProperty(win->env.dpy, win->xwin,
	                XInternAtom(win->env.dpy, "_NET_WM_NAME", False),
	                XInternAtom(win->env.dpy, "UTF8_STRING", False), 8,
	                PropModeReplace, (unsigned char *) title, strlen(title));
	XChangeProperty(win->env.dpy, win->xwin,
	                XInternAtom(win->env.dpy, "_NET_WM_ICON_NAME", False),
	                XInternAtom(win->env.dpy, "UTF8_STRING", False), 8,
	                PropModeReplace, (unsigned char *) title, strlen(title));
}

void win_set_cursor(win_t *win, cursor_t cursor)
{
	if (win == NULL || win->xwin == None)
		return;

	switch (cursor) {
		case CURSOR_NONE:
			XDefineCursor(win->env.dpy, win->xwin, cnone);
			break;
		case CURSOR_HAND:
			XDefineCursor(win->env.dpy, win->xwin, chand);
			break;
		case CURSOR_WATCH:
			XDefineCursor(win->env.dpy, win->xwin, cwatch);
			break;
		case CURSOR_ARROW:
		default:
			XDefineCursor(win->env.dpy, win->xwin, carrow);
			break;
	}

	XFlush(win->env.dpy);
}
