/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   framerender.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "frame.h"
#include "openbox.h"
#include "screen.h"
#include "client.h"
#include "framerender.h"
#include "obrender/theme.h"

#include <math.h>

/*! The vector glyphs that are layered over the themed button background with
  Cairo: circles (macOS traffic-light style), anti-aliased rounds strokes. */
typedef enum {
    FR_GLYPH_CLOSE,
    FR_GLYPH_MAX,
    FR_GLYPH_RESTORE,
    FR_GLYPH_ICONIFY,
    FR_GLYPH_DESK,
    FR_GLYPH_SHADE,
    FR_GLYPH_UNSHADE
} RrButtonGlyph;

static void framerender_label(ObFrame *self, RrAppearance *a);
static void framerender_icon(ObFrame *self, RrAppearance *a);
static void framerender_max(ObFrame *self, RrAppearance *a);
static void framerender_iconify(ObFrame *self, RrAppearance *a);
static void framerender_desk(ObFrame *self, RrAppearance *a);
static void framerender_shade(ObFrame *self, RrAppearance *a);
static void framerender_close(ObFrame *self, RrAppearance *a);
static void framerender_button(ObFrame *self, RrAppearance *a, Window win,
                               guint size, RrButtonGlyph glyph);
static void framerender_draw_glyph(ObFrame *self, RrAppearance *a,
                                   guint size, RrButtonGlyph glyph);

void framerender_frame(ObFrame *self)
{
    if (frame_iconify_animating(self))
        return; /* delay redrawing until the animation is done */
    if (!self->need_render)
        return;
    if (!self->visible)
        return;
    self->need_render = FALSE;

    {
        gulong px;

        px = (self->focused ?
              RrColorPixel(ob_rr_theme->cb_focused_color) :
              RrColorPixel(ob_rr_theme->cb_unfocused_color));

        XSetWindowBackground(obt_display, self->backback, px);
        XClearWindow(obt_display, self->backback);
        XSetWindowBackground(obt_display, self->innerleft, px);
        XClearWindow(obt_display, self->innerleft);
        XSetWindowBackground(obt_display, self->innertop, px);
        XClearWindow(obt_display, self->innertop);
        XSetWindowBackground(obt_display, self->innerright, px);
        XClearWindow(obt_display, self->innerright);
        XSetWindowBackground(obt_display, self->innerbottom, px);
        XClearWindow(obt_display, self->innerbottom);
        XSetWindowBackground(obt_display, self->innerbll, px);
        XClearWindow(obt_display, self->innerbll);
        XSetWindowBackground(obt_display, self->innerbrr, px);
        XClearWindow(obt_display, self->innerbrr);
        XSetWindowBackground(obt_display, self->innerblb, px);
        XClearWindow(obt_display, self->innerblb);
        XSetWindowBackground(obt_display, self->innerbrb, px);
        XClearWindow(obt_display, self->innerbrb);

        px = RrColorPixel(self->focused ?
            (self->client->undecorated ?
             ob_rr_theme->frame_undecorated_focused_border_color :
             ob_rr_theme->frame_focused_border_color) :
            (self->client->undecorated ?
             ob_rr_theme->frame_undecorated_unfocused_border_color :
             ob_rr_theme->frame_unfocused_border_color));

        XSetWindowBackground(obt_display, self->left, px);
        XClearWindow(obt_display, self->left);
        XSetWindowBackground(obt_display, self->right, px);
        XClearWindow(obt_display, self->right);

        XSetWindowBackground(obt_display, self->titleleft, px);
        XClearWindow(obt_display, self->titleleft);
        XSetWindowBackground(obt_display, self->titletop, px);
        XClearWindow(obt_display, self->titletop);
        XSetWindowBackground(obt_display, self->titletopleft, px);
        XClearWindow(obt_display, self->titletopleft);
        XSetWindowBackground(obt_display, self->titletopright, px);
        XClearWindow(obt_display, self->titletopright);
        XSetWindowBackground(obt_display, self->titleright, px);
        XClearWindow(obt_display, self->titleright);

        XSetWindowBackground(obt_display, self->handleleft, px);
        XClearWindow(obt_display, self->handleleft);
        XSetWindowBackground(obt_display, self->handletop, px);
        XClearWindow(obt_display, self->handletop);
        XSetWindowBackground(obt_display, self->handleright, px);
        XClearWindow(obt_display, self->handleright);
        XSetWindowBackground(obt_display, self->handlebottom, px);
        XClearWindow(obt_display, self->handlebottom);

        XSetWindowBackground(obt_display, self->lgripleft, px);
        XClearWindow(obt_display, self->lgripleft);
        XSetWindowBackground(obt_display, self->lgriptop, px);
        XClearWindow(obt_display, self->lgriptop);
        XSetWindowBackground(obt_display, self->lgripbottom, px);
        XClearWindow(obt_display, self->lgripbottom);

        XSetWindowBackground(obt_display, self->rgripright, px);
        XClearWindow(obt_display, self->rgripright);
        XSetWindowBackground(obt_display, self->rgriptop, px);
        XClearWindow(obt_display, self->rgriptop);
        XSetWindowBackground(obt_display, self->rgripbottom, px);
        XClearWindow(obt_display, self->rgripbottom);

        /* don't use the separator color for shaded windows */
        if (!self->client->shaded)
            px = (self->focused ?
                  RrColorPixel(ob_rr_theme->title_separator_focused_color) :
                  RrColorPixel(ob_rr_theme->title_separator_unfocused_color));

        XSetWindowBackground(obt_display, self->titlebottom, px);
        XClearWindow(obt_display, self->titlebottom);
    }

    if (self->decorations & OB_FRAME_DECOR_TITLEBAR) {
        RrAppearance *t, *l, *m, *n, *i, *d, *s, *c, *clear;
        if (self->focused) {
            t = ob_rr_theme->a_focused_title;
            l = ob_rr_theme->a_focused_label;
            m = (!(self->decorations & OB_FRAME_DECOR_MAXIMIZE) ?
                 ob_rr_theme->btn_max->a_focused_disabled :
                 (self->client->max_vert || self->client->max_horz ?
                  (self->max_press ?
                   ob_rr_theme->btn_max->a_focused_pressed_toggled :
                   (self->max_hover ?
                    ob_rr_theme->btn_max->a_focused_hover_toggled :
                    ob_rr_theme->btn_max->a_focused_unpressed_toggled)) :
                  (self->max_press ?
                   ob_rr_theme->btn_max->a_focused_pressed :
                   (self->max_hover ?
                    ob_rr_theme->btn_max->a_focused_hover :
                    ob_rr_theme->btn_max->a_focused_unpressed))));
            n = ob_rr_theme->a_icon;
            i = (!(self->decorations & OB_FRAME_DECOR_ICONIFY) ?
                 ob_rr_theme->btn_iconify->a_focused_disabled :
                 (self->iconify_press ?
                  ob_rr_theme->btn_iconify->a_focused_pressed :
                  (self->iconify_hover ?
                   ob_rr_theme->btn_iconify->a_focused_hover :
                   ob_rr_theme->btn_iconify->a_focused_unpressed)));
            d = (!(self->decorations & OB_FRAME_DECOR_ALLDESKTOPS) ?
                 ob_rr_theme->btn_desk->a_focused_disabled :
                 (self->client->desktop == DESKTOP_ALL ?
                  (self->desk_press ?
                   ob_rr_theme->btn_desk->a_focused_pressed_toggled :
                   (self->desk_hover ?
                    ob_rr_theme->btn_desk->a_focused_hover_toggled :
                    ob_rr_theme->btn_desk->a_focused_unpressed_toggled)) :
                  (self->desk_press ?
                   ob_rr_theme->btn_desk->a_focused_pressed :
                   (self->desk_hover ?
                    ob_rr_theme->btn_desk->a_focused_hover :
                    ob_rr_theme->btn_desk->a_focused_unpressed))));
            s = (!(self->decorations & OB_FRAME_DECOR_SHADE) ?
                 ob_rr_theme->btn_shade->a_focused_disabled :
                 (self->client->shaded ?
                  (self->shade_press ?
                   ob_rr_theme->btn_shade->a_focused_pressed_toggled :
                   (self->shade_hover ?
                    ob_rr_theme->btn_shade->a_focused_hover_toggled :
                    ob_rr_theme->btn_shade->a_focused_unpressed_toggled)) :
                  (self->shade_press ?
                   ob_rr_theme->btn_shade->a_focused_pressed :
                   (self->shade_hover ?
                    ob_rr_theme->btn_shade->a_focused_hover :
                    ob_rr_theme->btn_shade->a_focused_unpressed))));
            c = (!(self->decorations & OB_FRAME_DECOR_CLOSE) ?
                 ob_rr_theme->btn_close->a_focused_disabled :
                 (self->close_press ?
                  ob_rr_theme->btn_close->a_focused_pressed :
                  (self->close_hover ?
                   ob_rr_theme->btn_close->a_focused_hover :
                   ob_rr_theme->btn_close->a_focused_unpressed)));
        } else {
            t = ob_rr_theme->a_unfocused_title;
            l = ob_rr_theme->a_unfocused_label;
            m = (!(self->decorations & OB_FRAME_DECOR_MAXIMIZE) ?
                 ob_rr_theme->btn_max->a_unfocused_disabled :
                 (self->client->max_vert || self->client->max_horz ?
                  (self->max_press ?
                   ob_rr_theme->btn_max->a_unfocused_pressed_toggled :
                   (self->max_hover ?
                    ob_rr_theme->btn_max->a_unfocused_hover_toggled :
                    ob_rr_theme->btn_max->a_unfocused_unpressed_toggled)) :
                  (self->max_press ?
                   ob_rr_theme->btn_max->a_unfocused_pressed :
                   (self->max_hover ?
                    ob_rr_theme->btn_max->a_unfocused_hover :
                    ob_rr_theme->btn_max->a_unfocused_unpressed))));
            n = ob_rr_theme->a_icon;
            i = (!(self->decorations & OB_FRAME_DECOR_ICONIFY) ?
                 ob_rr_theme->btn_iconify->a_unfocused_disabled :
                 (self->iconify_press ?
                  ob_rr_theme->btn_iconify->a_unfocused_pressed :
                  (self->iconify_hover ?
                   ob_rr_theme->btn_iconify->a_unfocused_hover :
                   ob_rr_theme->btn_iconify->a_unfocused_unpressed)));
            d = (!(self->decorations & OB_FRAME_DECOR_ALLDESKTOPS) ?
                 ob_rr_theme->btn_desk->a_unfocused_disabled :
                 (self->client->desktop == DESKTOP_ALL ?
                  (self->desk_press ?
                   ob_rr_theme->btn_desk->a_unfocused_pressed_toggled :
                   (self->desk_hover ?
                    ob_rr_theme->btn_desk->a_unfocused_hover_toggled :
                    ob_rr_theme->btn_desk->a_unfocused_unpressed_toggled)) :
                  (self->desk_press ?
                   ob_rr_theme->btn_desk->a_unfocused_pressed :
                   (self->desk_hover ?
                    ob_rr_theme->btn_desk->a_unfocused_hover :
                    ob_rr_theme->btn_desk->a_unfocused_unpressed))));
            s = (!(self->decorations & OB_FRAME_DECOR_SHADE) ?
                 ob_rr_theme->btn_shade->a_unfocused_disabled :
                 (self->client->shaded ?
                  (self->shade_press ?
                   ob_rr_theme->btn_shade->a_unfocused_pressed_toggled :
                   (self->shade_hover ?
                    ob_rr_theme->btn_shade->a_unfocused_hover_toggled :
                    ob_rr_theme->btn_shade->a_unfocused_unpressed_toggled)) :
                  (self->shade_press ?
                   ob_rr_theme->btn_shade->a_unfocused_pressed :
                   (self->shade_hover ?
                    ob_rr_theme->btn_shade->a_unfocused_hover :
                    ob_rr_theme->btn_shade->a_unfocused_unpressed))));
            c = (!(self->decorations & OB_FRAME_DECOR_CLOSE) ?
                 ob_rr_theme->btn_close->a_unfocused_disabled :
                 (self->close_press ?
                  ob_rr_theme->btn_close->a_unfocused_pressed :
                  (self->close_hover ?
                   ob_rr_theme->btn_close->a_unfocused_hover :
                   ob_rr_theme->btn_close->a_unfocused_unpressed)));
        }
        clear = ob_rr_theme->a_clear;

        RrPaint(t, self->title, self->width, ob_rr_theme->title_height);

        clear->surface.parent = t;
        clear->surface.parenty = 0;

        clear->surface.parentx = ob_rr_theme->grip_width;

        RrPaint(clear, self->topresize,
                self->width - ob_rr_theme->grip_width * 2,
                ob_rr_theme->paddingy + 1);

        clear->surface.parentx = 0;

        if (ob_rr_theme->grip_width > 0)
            RrPaint(clear, self->tltresize,
                    ob_rr_theme->grip_width, ob_rr_theme->paddingy + 1);
        if (ob_rr_theme->title_height > 0)
            RrPaint(clear, self->tllresize,
                    ob_rr_theme->paddingx + 1, ob_rr_theme->title_height);

        clear->surface.parentx = self->width - ob_rr_theme->grip_width;

        if (ob_rr_theme->grip_width > 0)
            RrPaint(clear, self->trtresize,
                    ob_rr_theme->grip_width, ob_rr_theme->paddingy + 1);

        clear->surface.parentx = self->width - (ob_rr_theme->paddingx + 1);

        if (ob_rr_theme->title_height > 0)
            RrPaint(clear, self->trrresize,
                    ob_rr_theme->paddingx + 1, ob_rr_theme->title_height);

        /* set parents for any parent relative guys */
        l->surface.parent = t;
        l->surface.parentx = self->label_x;
        l->surface.parenty = ob_rr_theme->paddingy;

        m->surface.parent = t;
        m->surface.parentx = self->max_x;
        m->surface.parenty = ob_rr_theme->paddingy + 1;

        n->surface.parent = t;
        n->surface.parentx = self->icon_x;
        n->surface.parenty = ob_rr_theme->paddingy;

        i->surface.parent = t;
        i->surface.parentx = self->iconify_x;
        i->surface.parenty = ob_rr_theme->paddingy + 1;

        d->surface.parent = t;
        d->surface.parentx = self->desk_x;
        d->surface.parenty = ob_rr_theme->paddingy + 1;

        s->surface.parent = t;
        s->surface.parentx = self->shade_x;
        s->surface.parenty = ob_rr_theme->paddingy + 1;

        c->surface.parent = t;
        c->surface.parentx = self->close_x;
        c->surface.parenty = ob_rr_theme->paddingy + 1;

        framerender_label(self, l);
        framerender_max(self, m);
        framerender_icon(self, n);
        framerender_iconify(self, i);
        framerender_desk(self, d);
        framerender_shade(self, s);
        framerender_close(self, c);
    }

    if (self->decorations & OB_FRAME_DECOR_HANDLE &&
        ob_rr_theme->handle_height > 0)
    {
        RrAppearance *h, *g;

        h = (self->focused ?
             ob_rr_theme->a_focused_handle : ob_rr_theme->a_unfocused_handle);

        RrPaint(h, self->handle, self->width, ob_rr_theme->handle_height);

        if (self->decorations & OB_FRAME_DECOR_GRIPS) {
            g = (self->focused ?
                 ob_rr_theme->a_focused_grip : ob_rr_theme->a_unfocused_grip);

            if (g->surface.grad == RR_SURFACE_PARENTREL)
                g->surface.parent = h;

            g->surface.parentx = 0;
            g->surface.parenty = 0;

            RrPaint(g, self->lgrip,
                    ob_rr_theme->grip_width, ob_rr_theme->handle_height);

            g->surface.parentx = self->width - ob_rr_theme->grip_width;
            g->surface.parenty = 0;

            RrPaint(g, self->rgrip,
                    ob_rr_theme->grip_width, ob_rr_theme->handle_height);
        }
    }

    XFlush(obt_display);
}

static void framerender_label(ObFrame *self, RrAppearance *a)
{
    if (!self->label_on) return;
    /* set the texture's text! */
    a->texture[0].data.text.string = self->client->title;
    RrPaint(a, self->label, self->label_width, ob_rr_theme->label_height);
}

static void framerender_icon(ObFrame *self, RrAppearance *a)
{
    RrImage *icon;

    if (!self->icon_on) return;

    icon = client_icon(self->client);

    if (icon) {
        RrAppearanceClearTextures(a);
        a->texture[0].type = RR_TEXTURE_IMAGE;
        a->texture[0].data.image.alpha = 0xff;
        a->texture[0].data.image.image = icon;
    } else {
        RrAppearanceClearTextures(a);
        a->texture[0].type = RR_TEXTURE_NONE;
    }

    RrPaint(a, self->icon,
            ob_rr_theme->button_size + 2, ob_rr_theme->button_size + 2);
}

static void framerender_max(ObFrame *self, RrAppearance *a)
{
    if (!self->max_on) return;
    framerender_button(self, a, self->max, ob_rr_theme->button_size,
                       (self->client->max_vert && self->client->max_horz) ?
                       FR_GLYPH_RESTORE : FR_GLYPH_MAX);
}

static void framerender_iconify(ObFrame *self, RrAppearance *a)
{
    if (!self->iconify_on) return;
    framerender_button(self, a, self->iconify, ob_rr_theme->button_size,
                       FR_GLYPH_ICONIFY);
}

static void framerender_desk(ObFrame *self, RrAppearance *a)
{
    if (!self->desk_on) return;
    framerender_button(self, a, self->desk, ob_rr_theme->button_size,
                       FR_GLYPH_DESK);
}

static void framerender_shade(ObFrame *self, RrAppearance *a)
{
    if (!self->shade_on) return;
    framerender_button(self, a, self->shade, ob_rr_theme->button_size,
                       self->client->shaded ?
                       FR_GLYPH_UNSHADE : FR_GLYPH_SHADE);
}

static void framerender_close(ObFrame *self, RrAppearance *a)
{
    if (!self->close_on) return;
    framerender_button(self, a, self->close, ob_rr_theme->button_size,
                       FR_GLYPH_CLOSE);
}

/* Paint the themed background only (the classic XBM glyph mask is skipped) and
   then layer the Cairo vector glyph on top.  Both end up in
   RrAppearance.pixmap, the very pixmap the X server repaints on exposure, so
   the glyph survives window damage like the original bitmap did. */
static void framerender_button(ObFrame *self, RrAppearance *a, Window win,
                               guint size, RrButtonGlyph glyph)
{
    gboolean xbm;

    /* The fork normally paints just the themed background and layers a Cairo
       vector glyph on top, but when the theme ships its own XBM icons (the
       classic Openbox masks) respect them instead so the buttons follow the
       look the theme author designed.  Only fall back to the vector glyphs
       when no real theme mask is available. */
    xbm = (a->textures >= 1 &&
           a->texture[0].type == RR_TEXTURE_MASK &&
           a->texture[0].data.mask.mask != NULL &&
           a->texture[0].data.mask.mask->from_theme);

    if (xbm) {
        /* the themed mask renders inside RrPaint, nothing else to do */
        RrPaint(a, win, size, size);
        return;
    }

    {
        gint textures;

        textures = a->textures;
        a->textures = 0;
        RrPaint(a, win, size, size);
        a->textures = textures;

        framerender_draw_glyph(self, a, size, glyph);

        /* the cairo glyph was drawn into a->pixmap after RrPaint's clear, so
           re-associate the pixmap and redraw the button window or the glyph
           never makes it to the screen */
        if (a->pixmap != None) {
            XSetWindowBackgroundPixmap(obt_display, win, a->pixmap);
            XClearWindow(obt_display, win);
        }
    }
}

static void framerender_draw_glyph(ObFrame *self, RrAppearance *a,
                                   guint size, RrButtonGlyph glyph)
{
    cairo_surface_t *s = NULL;
    cairo_t *cr;
    gboolean focused = self->focused;
    gdouble cx = size / 2.0;
    gdouble cy = size / 2.0;
    gdouble r, base_r, base_g, base_b;

    if (!a->pixmap)
        return;

    cr = RrThemeCairoContext(ob_rr_theme, a->pixmap, size, size, &s);
    if (!cr || !s)
        return;

    /* macOS traffic-light colours: close = red, max = yellow, iconify = green.
       desk/shade borrow neighbouring hues so the whole row reads as "lights". */
    if (focused) {
        switch (glyph) {
        case FR_GLYPH_CLOSE:
            base_r = 0xff/255.0; base_g = 0x5f/255.0; base_b = 0x57/255.0;
            break;
        case FR_GLYPH_MAX:
        case FR_GLYPH_RESTORE:
            base_r = 0xfb/255.0; base_g = 0xbc/255.0; base_b = 0x2e/255.0;
            break;
        case FR_GLYPH_ICONIFY:
            base_r = 0x28/255.0; base_g = 0xc8/255.0; base_b = 0x40/255.0;
            break;
        case FR_GLYPH_DESK:
            base_r = 0x5a/255.0; base_g = 0x9d/255.0; base_b = 0xf0/255.0;
            break;
        case FR_GLYPH_SHADE:
        case FR_GLYPH_UNSHADE:
            base_r = 0xf0/255.0; base_g = 0x83/255.0; base_b = 0x2e/255.0;
            break;
        default:
            g_assert_not_reached();
        }
    }
    else {
        /* every badge turns grey when the window does not have focus */
        base_r = base_g = base_b = 0xd5/255.0;
    }

    /* the badge circle */
    r = size / 2.0 - size * 0.125;
    cairo_arc(cr, cx, cy, r, 0.0, 2.0 * M_PI);
    cairo_set_source_rgba(cr, base_r, base_g, base_b, 1.0);
    cairo_fill(cr);

    /* anti-aliased white glyph on the badge (dark grey when unfocused) */
    if (focused)
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    else
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.30);
    cairo_set_line_width(cr, MAX(1.25, size * 0.075));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    switch (glyph) {
    case FR_GLYPH_CLOSE: {
        gdouble q = r * 0.38;
        cairo_move_to(cr, cx - q, cy - q);
        cairo_line_to(cr, cx + q, cy + q);
        cairo_move_to(cr, cx + q, cy - q);
        cairo_line_to(cr, cx - q, cy + q);
        cairo_stroke(cr);
        break;
    }
    case FR_GLYPH_ICONIFY: {
        gdouble q = r * 0.42;
        cairo_move_to(cr, cx - q, cy);
        cairo_line_to(cr, cx + q, cy);
        cairo_stroke(cr);
        break;
    }
    case FR_GLYPH_MAX: {
        gdouble q = r * 0.38;
        cairo_rectangle(cr, cx - q, cy - q + size * 0.05,
                        2.0 * q, 2.0 * q - size * 0.05);
        cairo_stroke(cr);
        break;
    }
    case FR_GLYPH_RESTORE: {
        /* two overlapping rounded squares, as in macOS's restore button */
        gdouble q = r * 0.45;
        gdouble off = r * 0.18;
        cairo_rectangle(cr, cx - q * 0.9 + off, cy - q * 0.9 - off,
                        q * 0.9, q * 0.9);
        cairo_stroke(cr);
        cairo_rectangle(cr, cx - q * 0.9, cy - q * 0.9, q * 0.9, q * 0.9);
        cairo_stroke(cr);
        break;
    }
    case FR_GLYPH_DESK: {
        /* four dots, or a single filled dot when sticky (all desktops) */
        if (self->client->desktop == DESKTOP_ALL) {
            cairo_arc(cr, cx, cy, r * 0.24, 0.0, 2.0 * M_PI);
            cairo_fill(cr);
        }
        else {
            gdouble q = r * 0.42;
            gdouble d = r * 0.24;
            cairo_arc(cr, cx - q, cy - q, d, 0.0, 2.0 * M_PI);
            cairo_fill(cr);
            cairo_arc(cr, cx + q, cy - q, d, 0.0, 2.0 * M_PI);
            cairo_fill(cr);
            cairo_arc(cr, cx - q, cy + q, d, 0.0, 2.0 * M_PI);
            cairo_fill(cr);
            cairo_arc(cr, cx + q, cy + q, d, 0.0, 2.0 * M_PI);
            cairo_fill(cr);
        }
        break;
    }
    case FR_GLYPH_SHADE: {
        /* chevron pointing up: / \ */
        gdouble q = r * 0.40;
        cairo_move_to(cr, cx - q, cy + q);
        cairo_line_to(cr, cx, cy - q);
        cairo_line_to(cr, cx + q, cy + q);
        cairo_stroke(cr);
        break;
    }
    case FR_GLYPH_UNSHADE: {
        /* chevron pointing down: \ / */
        gdouble q = r * 0.40;
        cairo_move_to(cr, cx - q, cy - q);
        cairo_line_to(cr, cx, cy + q);
        cairo_line_to(cr, cx + q, cy - q);
        cairo_stroke(cr);
        break;
    }
    }

    RrThemeCairoFinish(ob_rr_theme, cr, s);
}
