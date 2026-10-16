/* Cairo drawing of the Win7 Aero look (port of win7calc/render.py). */
#ifndef WIN7CALC_RENDER_H
#define WIN7CALC_RENDER_H

#include <stdbool.h>

#include <cairo.h>
#include <pango/pangocairo.h>

#define UI_FONT "Segoe UI, Selawik, Noto Sans, Open Sans, DejaVu Sans"
#define NUM_FONT "Consolas, Inconsolata, Noto Sans Mono, DejaVu Sans Mono, monospace"

typedef struct { double r, g, b; } RGB;

typedef enum { BS_NORMAL, BS_HOT, BS_PRESSED, BS_DISABLED } BtnState;
typedef enum { ALIGN_CENTER, ALIGN_LEFT, ALIGN_RIGHT } HAlign;
typedef enum { VALIGN_CENTER, VALIGN_TOP, VALIGN_BOTTOM } VAlign;

RGB hexrgb(const char *hex);

extern const RGB TEXT_COLOR, TEXT_DISABLED, PANEL_FILL, PANEL_BORDER;

void render_rrect(cairo_t *cr, double x, double y, double w, double h, double r);
void render_window_bg(cairo_t *cr, double w, double h);
void render_button(cairo_t *cr, double x, double y, double w, double h, int style, BtnState state);
/* fill == NULL means PANEL_FILL; fill_pattern overrides fill when non-NULL. */
void render_panel(cairo_t *cr, double x, double y, double w, double h, const RGB *fill,
                  cairo_pattern_t *fill_pattern, RGB border, double radius);
void render_display_panel(cairo_t *cr, double x, double y, double w, double h);

/* Text.  The returned layout is owned by the caller (g_object_unref). */
PangoLayout *render_layout(cairo_t *cr, const char *text, const char *family, double px, bool markup);
void render_text_size(PangoLayout *lo, int *w, int *h);
int render_text_width(cairo_t *cr, const char *text, const char *family, double px);
int render_draw_text(cairo_t *cr, const char *text, double x, double y, double w, double h,
                     const char *family, double px, HAlign align, RGB color, bool markup, VAlign valign);
double render_fit_text_px(cairo_t *cr, const char *text, const char *family, const double *sizes, int n,
                          double max_w);
void render_radio(cairo_t *cr, double x, double y, double h, const char *text, bool checked, bool hot,
                  double px);
void render_arrow_button(cairo_t *cr, double x, double y, double w, double h, bool up, BtnState state);

#endif
