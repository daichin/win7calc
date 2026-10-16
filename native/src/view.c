/* Paints the whole calculator client area (port of win7calc/view.py). */
#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

static const double NUM_SIZES[] = {24, 20, 16, 13, 11};

void ui_state_init(UIState *ui)
{
    ui->hot = ui->pressed = ui->flash = KEY_NONE;
    ui->history = false;
    ui->list_top = 0;
}

ItemKey item_key(const Item *it) { return (ItemKey){it->kind, it->id}; }

bool key_eq(ItemKey a, ItemKey b)
{
    if (a.kind == IT_NONE || b.kind == IT_NONE)
        return a.kind == b.kind;
    return a.kind == b.kind && a.id == b.id;
}

int view_visible_rows(void) { return 4; }

bool view_has_list(const Engine *eng, const UIState *ui)
{
    CalcMode m = engine_mode(eng);
    return m == MODE_STAT || (ui->history && (m == MODE_STD || m == MODE_SCI));
}

int view_display_items(const Layout *lay, Engine *eng, const UIState *ui, Item out[8])
{
    double x = lay->display[0], y = lay->display[1], w = lay->display[2];
    if (!view_has_list(eng, ui))
        return 0;
    int n = 0;
    out[n++] = (Item){IT_ARROW, VIEW_UP, x + w - 52, y + 5, VIEW_ARROW_W, VIEW_ARROW_H, STYLE_OP, ""};
    out[n++] = (Item){IT_ARROW, VIEW_DOWN, x + w - 30, y + 5, VIEW_ARROW_W, VIEW_ARROW_H, STYLE_OP, ""};
    for (int i = 0; i < view_visible_rows(); i++)
        out[n++] = (Item){IT_ROW, i, x + 6, y + 27 + i * VIEW_ROW_H, w - 12, VIEW_ROW_H, STYLE_OP, ""};
    return n;
}

int view_list_count(const Engine *eng)
{
    return engine_mode(eng) == MODE_STAT ? engine_dataset_count(eng) : engine_history_count(eng);
}

/* Row text of the history / dataset list (g_free). */
static char *list_entry(Engine *eng, int i)
{
    if (engine_mode(eng) == MODE_STAT) {
        char *s = engine_format_value(eng, engine_dataset_value(eng, i), -1);
        char *r = g_strdup(s ? s : "");
        free(s);
        return r;
    }
    return g_strdup(engine_history_text(eng, i));
}

int view_max_top(const Engine *eng)
{
    int m = view_list_count(eng) - view_visible_rows();
    return m > 0 ? m : 0;
}

BtnState view_item_state(const Item *it, Engine *eng, const UIState *ui)
{
    ItemKey k = item_key(it);
    if (it->kind == IT_BUTTON) {
        if (!engine_is_enabled(eng, it->id))
            return BS_DISABLED;
        if ((it->id == CMD_INV && engine_inv(eng)) || (it->id == CMD_FE && engine_fe(eng)))
            return BS_PRESSED;
    }
    if (key_eq(ui->flash, k) || (key_eq(ui->pressed, k) && key_eq(ui->hot, k)))
        return BS_PRESSED;
    if (key_eq(ui->hot, k) && (ui->pressed.kind == IT_NONE || key_eq(ui->pressed, k)))
        return BS_HOT;
    return BS_NORMAL;
}

static bool radio_checked(Engine *eng, int id)
{
    switch (id) {
    case CMD_DEG: return engine_angle(eng) == ANGLE_DEG;
    case CMD_RAD: return engine_angle(eng) == ANGLE_RAD;
    case CMD_GRAD: return engine_angle(eng) == ANGLE_GRAD;
    case CMD_HEX: return engine_radix(eng) == 16;
    case CMD_DEC: return engine_radix(eng) == 10;
    case CMD_OCT: return engine_radix(eng) == 8;
    case CMD_BIN: return engine_radix(eng) == 2;
    case CMD_QWORD: return engine_bits(eng) == 64;
    case CMD_DWORD: return engine_bits(eng) == 32;
    case CMD_WORD: return engine_bits(eng) == 16;
    case CMD_BYTE: return engine_bits(eng) == 8;
    default: return false;
    }
}

static void paint_item(cairo_t *cr, const Item *it, Engine *eng, const UIState *ui, uint64_t uval)
{
    switch (it->kind) {
    case IT_BUTTON: {
        BtnState st = view_item_state(it, eng, ui);
        render_button(cr, it->x, it->y, it->w, it->h, it->style, st);
        const char *text = it->text;
        if (engine_inv(eng)) {
            const char *inv = layout_inv_label(it->id);
            if (inv)
                text = inv;
        }
        render_draw_text(cr, text, it->x, it->y, it->w, it->h, UI_FONT, 12, ALIGN_CENTER,
                         st == BS_DISABLED ? TEXT_DISABLED : TEXT_COLOR, true, VALIGN_CENTER);
        break;
    }
    case IT_BOX:
        render_panel(cr, it->x, it->y, it->w, it->h, NULL, NULL, hexrgb("8696aa"), 2);
        break;
    case IT_PARENS:
        render_panel(cr, it->x, it->y, it->w, it->h, NULL, NULL, PANEL_BORDER, 2);
        if (engine_paren_count(eng)) {
            char buf[32];
            snprintf(buf, sizeof buf, "(=%d", engine_paren_count(eng));
            render_draw_text(cr, buf, it->x, it->y, it->w, it->h, UI_FONT, 12, ALIGN_CENTER, TEXT_COLOR, false,
                             VALIGN_CENTER);
        }
        break;
    case IT_RADIO:
        render_radio(cr, it->x, it->y, it->h, it->text, radio_checked(eng, it->id),
                     key_eq(ui->hot, item_key(it)), 11);
        break;
    case IT_BIT: {
        bool enabled = it->id < engine_bits(eng);
        const char *bit = (uval >> it->id) & 1 ? "1" : "0";
        RGB color = enabled ? TEXT_COLOR : hexrgb("9aa8b8");
        if (enabled && key_eq(ui->hot, item_key(it)))
            color = hexrgb("1a5fb4");
        render_draw_text(cr, bit, it->x - 2, it->y, it->w + 4, it->h, NUM_FONT, 13, ALIGN_CENTER, color, false,
                         VALIGN_CENTER);
        break;
    }
    case IT_BITLABEL: {
        bool left = it->style == 0;
        render_draw_text(cr, it->text, left ? it->x : it->x - it->w + 7.5, it->y, it->w, it->h, NUM_FONT, 10,
                         left ? ALIGN_LEFT : ALIGN_RIGHT, hexrgb("4e5d6e"), false, VALIGN_CENTER);
        break;
    }
    default:
        break;
    }
}

char *view_elide_left(cairo_t *cr, const char *text, double max_w, double px, const char *family)
{
    if (!text || !*text || render_text_width(cr, text, family, px) <= max_w)
        return g_strdup(text ? text : "");
    glong n = g_utf8_strlen(text, -1);
    glong lo = 0, hi = n;
    while (lo < hi) {
        glong mid = (lo + hi) / 2;
        char *s = g_strconcat("«", g_utf8_offset_to_pointer(text, mid), NULL);
        bool fits = render_text_width(cr, s, family, px) <= max_w;
        g_free(s);
        if (fits)
            hi = mid;
        else
            lo = mid + 1;
    }
    return g_strconcat("«", g_utf8_offset_to_pointer(text, lo), NULL);
}

static void paint_display(cairo_t *cr, const Layout *lay, Engine *eng, const UIState *ui)
{
    double x = lay->display[0], y = lay->display[1], w = lay->display[2], h = lay->display[3];
    render_display_panel(cr, x, y, w, h);
    CalcMode mode = engine_mode(eng);
    if (view_has_list(eng, ui)) {
        cairo_save(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_line_width(cr, 1);
        const double lys[] = {y + 26.5, y + 100.5};
        for (int i = 0; i < 2; i++) {
            cairo_move_to(cr, x + 1, lys[i]);
            cairo_line_to(cr, x + w - 1, lys[i]);
        }
        cairo_stroke(cr);
        cairo_restore(cr);
        int count = view_list_count(eng);
        Item di[8];
        int nd = view_display_items(lay, eng, ui, di);
        for (int k = 0; k < nd; k++) {
            const Item *it = &di[k];
            if (it->kind == IT_ARROW) {
                bool can = it->id == VIEW_UP ? ui->list_top > 0 : ui->list_top < view_max_top(eng);
                render_arrow_button(cr, it->x, it->y, it->w, it->h, it->id == VIEW_UP,
                                    can ? view_item_state(it, eng, ui) : BS_DISABLED);
            } else {
                int idx = ui->list_top + it->id;
                if (idx < count) {
                    if (key_eq(ui->hot, item_key(it))) {
                        RGB fill = hexrgb("e5f1fb");
                        render_panel(cr, it->x, it->y + 1, it->w, it->h - 1, &fill, NULL, hexrgb("a8cbef"), 2);
                    }
                    char *entry = list_entry(eng, idx);
                    char *t = view_elide_left(cr, entry, it->w - 8, 12, UI_FONT);
                    render_draw_text(cr, t, it->x, it->y, it->w - 4, it->h, UI_FONT, 12, ALIGN_RIGHT, TEXT_COLOR,
                                     false, VALIGN_CENTER);
                    g_free(t);
                    g_free(entry);
                }
            }
        }
        if (mode == MODE_STAT) {
            char buf[48];
            snprintf(buf, sizeof buf, "Count = %d", engine_dataset_count(eng));
            render_draw_text(cr, buf, x + 5, y + 101, 160, 18, UI_FONT, 12, ALIGN_LEFT, TEXT_COLOR, false,
                             VALIGN_CENTER);
        }
        y += 100;
        h = 50;
    }
    /* Expression line (control 404) and memory indicator (401). */
    if (mode != MODE_STAT) {
        char *expr = view_elide_left(cr, engine_expression(eng), w - 12, 12, UI_FONT);
        render_draw_text(cr, expr, x + 6, y + 2, w - 12, 16, UI_FONT, 12, ALIGN_RIGHT, TEXT_COLOR, false,
                         VALIGN_CENTER);
        g_free(expr);
    }
    if (engine_has_memory(eng))
        render_draw_text(cr, "M", x + 5, y + h - 25, 14, 18, UI_FONT, 12, ALIGN_LEFT, TEXT_COLOR, false,
                         VALIGN_CENTER);
    /* Main number (control 150), shrinking the font when it is long. */
    const char *display = engine_display(eng);
    double avail = w - 24;
    double px = render_fit_text_px(cr, display, NUM_FONT, NUM_SIZES, 5, avail);
    char *text = render_text_width(cr, display, NUM_FONT, px) > avail
                     ? view_elide_left(cr, display, avail, px, NUM_FONT)
                     : g_strdup(display);
    render_draw_text(cr, text, x + 18, y + 17, avail, h - 19, NUM_FONT, px, ALIGN_RIGHT, TEXT_COLOR, false,
                     VALIGN_CENTER);
    g_free(text);
}

void view_paint(cairo_t *cr, const Layout *lay, Engine *eng, const UIState *ui)
{
    render_window_bg(cr, lay->width, lay->height);
    paint_display(cr, lay, eng, ui);
    uint64_t uval = engine_mode(eng) == MODE_PROG ? engine_unsigned_value(eng) : 0;
    for (int i = 0; i < lay->n_items; i++)
        paint_item(cr, &lay->items[i], eng, ui, uval);
}
