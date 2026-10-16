/* Window geometry, taken from win32calc's dialog templates (port of win7calc/layout.py).
 *
 * Dialog coordinates are converted to 96-dpi pixels: x/2, y*0.6275 — this
 * reproduces Win7's 34x27 buttons on a 39x32 pitch exactly.
 */
#ifndef WIN7CALC_LAYOUT_H
#define WIN7CALC_LAYOUT_H

#include <stdbool.h>

#include "engine.h"

#define LAYOUT_MARGIN 11          /* window edge to display / keypad */
#define LAYOUT_GAP 8              /* display to keypad */
#define LAYOUT_DISPLAY_H 50
#define LAYOUT_HISTORY_DISPLAY_H 151
#define LAYOUT_STAT_DISPLAY_H 150
#define LAYOUT_PANEL_W 363
#define LAYOUT_PANEL_H 251

typedef enum {
    IT_NONE = -1,
    IT_BUTTON, IT_RADIO, IT_BOX, IT_PARENS, IT_BIT, IT_BITLABEL, IT_ARROW, IT_ROW
} ItemKind;

/* Button styles (bitlabels reuse the field: 0 = left aligned, 1 = right aligned). */
enum { STYLE_DIGIT, STYLE_OP, STYLE_MEM };

typedef struct {
    ItemKind kind;
    int id;
    double x, y, w, h;
    int style;
    const char *text;             /* static string (Pango markup for buttons) */
} Item;

#define LAYOUT_MAX_ITEMS 160

typedef struct {
    int width, height;
    double display[4];            /* x, y, w, h */
    Item items[LAYOUT_MAX_ITEMS];
    int n_items;
    bool has_panel;
    double panel[4];
    bool has_bitpanel;
    double bitpanel[4];
} Layout;

void layout_build(Layout *lay, CalcMode mode, bool history, bool panel);
bool item_contains(const Item *it, double px, double py);

const char *layout_label(int cid);        /* "" when unknown */
const char *layout_inv_label(int cid);    /* NULL when the label does not change with Inv */
const char *layout_tooltip(int cid);      /* NULL when there is none */
bool layout_is_toggle(int cid);           /* Inv / F-E */

/* Engine command id for a Python constant name ("ADD", "STAT_CAD"...), -1 if unknown. */
int layout_cmd_by_name(const char *name);
/* "std" / "sci" / "prog" / "stat" <-> CalcMode. */
bool layout_mode_by_name(const char *name, CalcMode *out);
const char *layout_mode_name(CalcMode m);

/* Python's round(): half to even. */
double py_round(double v);

#endif
