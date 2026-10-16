/* Cairo drawing of the Win7 Aero look.  Colours were sampled from win32calc's own bitmaps.
 * Port of win7calc/render.py. */
#include "render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "layout.h"

typedef struct { double t; const char *c; } Stop;

/* Vertical gradients: (stop, colour).  Sharp steps at ~0.48 give the glassy split. */
#define STOPS(name, ...) static const Stop name[] = {__VA_ARGS__, {-1, NULL}}
STOPS(G_DIGIT, {0, "f7fbff"}, {.42, "f1f5f9"}, {.475, "edf3f8"}, {.48, "e7edf5"}, {.75, "ecf4fa"}, {1, "f2fbff"});
STOPS(G_OP, {0, "f6fafe"}, {.05, "f0f6fb"}, {.475, "e2eaf3"}, {.48, "d5e0ed"}, {1, "d7e2ef"});
STOPS(G_MEM, {0, "d4e0ed"}, {1, "d7e2ef"});
STOPS(G_HOT, {0, "fffbf6"}, {.475, "fee2c2"}, {.48, "ffcf6a"}, {.7, "ffda74"}, {.82, "ffeb81"}, {.92, "fffab4"},
      {1, "fffff1"});
STOPS(G_HOT_MEM, {0, "ffda78"}, {.1, "ffce68"}, {.48, "ffcf6a"}, {.78, "ffea7f"}, {.9, "fff9b3"}, {1, "fffff2"});
STOPS(G_PRESS, {0, "e3c185"}, {.08, "eecb8e"}, {.475, "f5c779"}, {.48, "f5bb57"}, {.73, "f4cd61"}, {.85, "f3e37c"},
      {.93, "f2f0b0"}, {1, "f2f2bb"});
STOPS(G_PRESS_MEM, {0, "d1a75c"}, {.08, "e2b15d"}, {.2, "ecb85e"}, {.48, "ecb85f"}, {.75, "eacd66"},
      {.85, "eadd7d"}, {.93, "e9e9b0"}, {1, "e9ebc0"});
STOPS(G_DISABLED, {0, "d9e4f1"}, {1, "d9e4f1"});
STOPS(G_DISPLAY, {0, "e5eefb"}, {.5, "f0f5fc"}, {1, "ffffff"});
STOPS(G_WINDOW, {0, "edf4fc"}, {1, "d9e4f1"});

static const char *BORDER[] = {"8797aa", "ffdb00", "c29b29", "b4c2d1"};  /* by BtnState */

const RGB TEXT_COLOR = {0, 0, 0};
const RGB TEXT_DISABLED = {0x8d / 255.0, 0x9d / 255.0, 0xb0 / 255.0};
const RGB PANEL_FILL = {0xd9 / 255.0, 0xe4 / 255.0, 0xf1 / 255.0};
const RGB PANEL_BORDER = {0x8e / 255.0, 0x9c / 255.0, 0xad / 255.0};

RGB hexrgb(const char *h)
{
    unsigned long v = strtoul(h, NULL, 16);
    RGB c = {((v >> 16) & 255) / 255.0, ((v >> 8) & 255) / 255.0, (v & 255) / 255.0};
    return c;
}

static void set_rgb(cairo_t *cr, RGB c) { cairo_set_source_rgb(cr, c.r, c.g, c.b); }

static cairo_pattern_t *gradient(double y0, double y1, const Stop *stops)
{
    cairo_pattern_t *g = cairo_pattern_create_linear(0, y0, 0, y1);
    for (const Stop *s = stops; s->c; s++) {
        RGB c = hexrgb(s->c);
        cairo_pattern_add_color_stop_rgb(g, s->t, c.r, c.g, c.b);
    }
    return g;
}

static void set_gradient(cairo_t *cr, double y0, double y1, const Stop *stops)
{
    cairo_pattern_t *g = gradient(y0, y1, stops);
    cairo_set_source(cr, g);
    cairo_pattern_destroy(g);
}

void render_rrect(cairo_t *cr, double x, double y, double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

void render_window_bg(cairo_t *cr, double w, double h)
{
    set_gradient(cr, 0, 75, G_WINDOW);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_fill(cr);
}

void render_button(cairo_t *cr, double x, double y, double w, double h, int style, BtnState state)
{
    const Stop *stops;
    if (state == BS_DISABLED)
        stops = G_DISABLED;
    else if (state == BS_HOT)
        stops = style == STYLE_MEM ? G_HOT_MEM : G_HOT;
    else if (state == BS_PRESSED)
        stops = style == STYLE_MEM ? G_PRESS_MEM : G_PRESS;
    else
        stops = style == STYLE_DIGIT ? G_DIGIT : style == STYLE_MEM ? G_MEM : G_OP;
    cairo_save(cr);
    render_rrect(cr, x + .5, y + .5, w - 1, h - 1, 2.5);
    set_gradient(cr, y, y + h, stops);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 1);
    set_rgb(cr, hexrgb(BORDER[state]));
    cairo_stroke(cr);
    if (state == BS_NORMAL || state == BS_HOT) {
        /* 1px inner highlight. */
        render_rrect(cr, x + 1.5, y + 1.5, w - 3, h - 3, 1.5);
        cairo_set_source_rgba(cr, 1, 1, 1, state == BS_NORMAL ? .55 : .45);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}

void render_panel(cairo_t *cr, double x, double y, double w, double h, const RGB *fill,
                  cairo_pattern_t *fill_pattern, RGB border, double radius)
{
    cairo_save(cr);
    render_rrect(cr, x + .5, y + .5, w - 1, h - 1, radius);
    if (fill_pattern)
        cairo_set_source(cr, fill_pattern);
    else
        set_rgb(cr, fill ? *fill : PANEL_FILL);
    cairo_fill_preserve(cr);
    set_rgb(cr, border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void render_display_panel(cairo_t *cr, double x, double y, double w, double h)
{
    cairo_pattern_t *g = gradient(y, y + h, G_DISPLAY);
    render_panel(cr, x, y, w, h, NULL, g, PANEL_BORDER, 2);
    cairo_pattern_destroy(g);
    cairo_save(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, .9);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, x + 1.5, y + h - 2);
    cairo_line_to(cr, x + 1.5, y + 1.5);
    cairo_line_to(cr, x + w - 2, y + 1.5);
    cairo_stroke(cr);
    cairo_restore(cr);
}

/* ---------------------------------------------------------------- text */

PangoLayout *render_layout(cairo_t *cr, const char *text, const char *family, double px, bool markup)
{
    PangoLayout *lo = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_from_string(family);
    pango_font_description_set_absolute_size(fd, px * PANGO_SCALE);
    pango_layout_set_font_description(lo, fd);
    pango_font_description_free(fd);
    if (markup)
        pango_layout_set_markup(lo, text, -1);
    else
        pango_layout_set_text(lo, text, -1);
    return lo;
}

void render_text_size(PangoLayout *lo, int *w, int *h)
{
    PangoRectangle logical;
    pango_layout_get_pixel_extents(lo, NULL, &logical);
    if (w)
        *w = logical.width;
    if (h)
        *h = logical.height;
}

int render_text_width(cairo_t *cr, const char *text, const char *family, double px)
{
    PangoLayout *lo = render_layout(cr, text, family, px, false);
    int w;
    render_text_size(lo, &w, NULL);
    g_object_unref(lo);
    return w;
}

int render_draw_text(cairo_t *cr, const char *text, double x, double y, double w, double h,
                     const char *family, double px, HAlign align, RGB color, bool markup, VAlign valign)
{
    PangoLayout *lo = render_layout(cr, text, family, px, markup);
    int tw, th;
    render_text_size(lo, &tw, &th);
    double tx = align == ALIGN_CENTER ? x + (w - tw) / 2 : align == ALIGN_RIGHT ? x + w - tw : x;
    double ty = valign == VALIGN_CENTER ? y + (h - th) / 2 : valign == VALIGN_BOTTOM ? y + h - th : y;
    cairo_save(cr);
    set_rgb(cr, color);
    cairo_move_to(cr, py_round(tx), py_round(ty));
    pango_cairo_show_layout(cr, lo);
    cairo_restore(cr);
    g_object_unref(lo);
    return tw;
}

double render_fit_text_px(cairo_t *cr, const char *text, const char *family, const double *sizes, int n,
                          double max_w)
{
    /* Largest font size from `sizes` at which `text` fits in max_w (Win7 shrinks long numbers). */
    for (int i = 0; i < n; i++)
        if (render_text_width(cr, text, family, sizes[i]) <= max_w)
            return sizes[i];
    return sizes[n - 1];
}

void render_radio(cairo_t *cr, double x, double y, double h, const char *text, bool checked, bool hot,
                  double px)
{
    const double r = 6.5;
    double cx = x + r + .5, cy = y + h / 2;
    cairo_save(cr);
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    Stop g1[] = {{0, hot ? "e8f6fd" : "f4f4f4"}, {1, hot ? "dff2fc" : "ffffff"}, {-1, NULL}};
    set_gradient(cr, cy - r, cy + r, g1);
    cairo_fill_preserve(cr);
    set_rgb(cr, hexrgb(hot ? "3c7fb1" : "8e8f8f"));
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    cairo_arc(cr, cx, cy, r - 2, 0, 2 * M_PI);
    set_rgb(cr, hexrgb(hot ? "c6e9fb" : "dfe6ee"));
    cairo_fill(cr);
    if (checked) {
        cairo_arc(cr, cx, cy, 3.2, 0, 2 * M_PI);
        static const Stop g2[] = {{0, "8ad0f2"}, {.5, "1f6ba8"}, {1, "0d3a6b"}, {-1, NULL}};
        set_gradient(cr, cy - 3, cy + 3, g2);
        cairo_fill(cr);
    }
    cairo_restore(cr);
    render_draw_text(cr, text, x + 2 * r + 5, y, 200, h, UI_FONT, px, ALIGN_LEFT, TEXT_COLOR, false,
                     VALIGN_CENTER);
}

void render_arrow_button(cairo_t *cr, double x, double y, double w, double h, bool up, BtnState state)
{
    render_button(cr, x, y, w, h, STYLE_OP, state);
    cairo_save(cr);
    double cx = x + w / 2, cy = y + h / 2;
    set_rgb(cr, hexrgb(state == BS_DISABLED ? "aab8c8" : "1e395b"));
    if (up) {
        cairo_move_to(cr, cx - 4, cy + 2);
        cairo_line_to(cr, cx + 4, cy + 2);
        cairo_line_to(cr, cx, cy - 2);
    } else {
        cairo_move_to(cr, cx - 4, cy - 2);
        cairo_line_to(cr, cx + 4, cy - 2);
        cairo_line_to(cr, cx, cy + 2);
    }
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_restore(cr);
}
