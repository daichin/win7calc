/* Paints the whole calculator client area, independent of GTK widgets so it can
 * render headless (port of win7calc/view.py). */
#ifndef WIN7CALC_VIEW_H
#define WIN7CALC_VIEW_H

#include <stdbool.h>

#include <cairo.h>

#include "engine.h"
#include "layout.h"
#include "render.h"

#define VIEW_ROW_H 18
#define VIEW_ARROW_W 17
#define VIEW_ARROW_H 16
#define VIEW_UP 1051
#define VIEW_DOWN 1052

typedef struct { ItemKind kind; int id; } ItemKey;   /* kind == IT_NONE: no item */

#define KEY_NONE ((ItemKey){IT_NONE, 0})

typedef struct {
    ItemKey hot;        /* item under the mouse */
    ItemKey pressed;    /* item held by the mouse */
    ItemKey flash;      /* briefly shown pressed after a key press */
    bool history;
    int list_top;       /* first visible row of the history / dataset list */
} UIState;

void ui_state_init(UIState *ui);
ItemKey item_key(const Item *it);
bool key_eq(ItemKey a, ItemKey b);

/* Interactive items inside the display: list scroll arrows and rows; returns the count (<= 8). */
int view_display_items(const Layout *lay, Engine *eng, const UIState *ui, Item out[8]);
int view_visible_rows(void);
int view_list_count(const Engine *eng);
int view_max_top(const Engine *eng);
bool view_has_list(const Engine *eng, const UIState *ui);
BtnState view_item_state(const Item *it, Engine *eng, const UIState *ui);
void view_paint(cairo_t *cr, const Layout *lay, Engine *eng, const UIState *ui);

/* Keep the tail of `text`, prefixed with '«' like win32calc, when it is too wide (g_free the result). */
char *view_elide_left(cairo_t *cr, const char *text, double max_w, double px, const char *family);

#endif
