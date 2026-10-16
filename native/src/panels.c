/* GTK3 side panels: Unit conversion, Date calculation, Worksheets (port of win7calc/panels.py). */
#include "panels.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "datecalc.h"
#include "units.h"
#include "worksheets.h"

/* ------------------------------------------------------------------ number parsing */

/* Append a run of digits (single '_' allowed between digits) to `out`; returns chars consumed. */
static size_t digit_run(const char *s, GString *out)
{
    size_t i = 0;
    while (g_ascii_isdigit(s[i])) {
        g_string_append_c(out, s[i++]);
        if (s[i] == '_' && g_ascii_isdigit(s[i + 1]))
            i++;
    }
    return i;
}

char *parse_number(const char *text)
{
    if (!text)
        return NULL;
    if (!g_utf8_validate(text, -1, NULL))
        return NULL;
    /* text.strip().replace(",", "").replace(" ", "") */
    char *stripped = g_strdup(text);
    const char *b = stripped;
    while (*b && g_unichar_isspace(g_utf8_get_char(b)))
        b = g_utf8_next_char(b);
    char *end = stripped + strlen(stripped);
    while (end > b) {
        char *prev = g_utf8_prev_char(end);
        if (!g_unichar_isspace(g_utf8_get_char(prev)))
            break;
        end = prev;
    }
    *end = 0;
    GString *t = g_string_new(NULL);
    for (const char *q = b; *q; q++)
        if (*q != ',' && *q != ' ')
            g_string_append_c(t, *q);
    g_free(stripped);
    const char *s = t->str;
    GString *ip = g_string_new(NULL), *fp = g_string_new(NULL), *ep = g_string_new(NULL);
    bool neg = false, ok = false, eneg = false;
    if (*s == '+' || *s == '-')
        neg = *s++ == '-';
    s += digit_run(s, ip);
    bool have_digits = ip->len > 0;
    if (*s == '.') {
        s++;
        s += digit_run(s, fp);
        have_digits = have_digits || fp->len > 0;
    }
    if (have_digits) {
        ok = true;
        if (*s == 'e' || *s == 'E') {
            s++;
            if (*s == '+' || *s == '-')
                eneg = *s++ == '-';
            s += digit_run(s, ep);
            if (ep->len == 0)
                ok = false;
        }
        if (*s)
            ok = false;
    }
    char *res = NULL;
    if (ok) {
        GString *r = g_string_new(neg ? "-" : "");
        g_string_append(r, ip->len ? ip->str : "0");
        if (fp->len)
            g_string_append_printf(r, ".%s", fp->str);
        if (ep->len)
            g_string_append_printf(r, "E%c%s", eneg ? '-' : '+', ep->str);
        res = g_string_free(r, FALSE);
    }
    g_string_free(ip, TRUE);
    g_string_free(fp, TRUE);
    g_string_free(ep, TRUE);
    g_string_free(t, TRUE);
    return res;
}

/* ------------------------------------------------------------------ base panel */

typedef struct Panel Panel;
struct Panel {
    GtkWidget *box, *fixed;
    double scale;
    void (*set_value)(Panel *p, const char *text);
    void (*free_fn)(Panel *p);
};

static int px(const Panel *p, double v) { return (int)rint(v * p->scale); }

static void panel_free(gpointer data)
{
    Panel *p = data;
    if (p->free_fn)
        p->free_fn(p);
    g_free(p);
}

static void panel_init(Panel *p, double scale)
{
    p->scale = scale;
    p->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(p->box), "w7-panel");
    p->fixed = gtk_fixed_new();
    gtk_widget_set_size_request(p->fixed, px(p, PANEL_W), px(p, PANEL_H));
    gtk_box_pack_start(GTK_BOX(p->box), p->fixed, TRUE, TRUE, 0);
    g_object_set_data_full(G_OBJECT(p->box), "w7-panel", p, panel_free);
}

/* Place `w`; coordinates in dialog units if `du`, else in unscaled px. */
static GtkWidget *put_in(Panel *p, GtkWidget *fixed, GtkWidget *w, double x, double y, double wd, double ht,
                         bool du)
{
    double kx = du ? 0.5 : 1.0, ky = du ? 0.6275 : 1.0;
    gtk_fixed_put(GTK_FIXED(fixed), w, px(p, x * kx), px(p, y * ky));
    gtk_widget_set_size_request(w, wd > 0 ? px(p, wd * kx) : -1, ht > 0 ? px(p, ht * ky) : -1);
    return w;
}

static GtkWidget *put(Panel *p, GtkWidget *w, double x, double y, double wd, double ht, bool du)
{
    return put_in(p, p->fixed, w, x, y, wd, ht, du);
}

static void move(Panel *p, GtkWidget *w, double x, double y)
{
    gtk_fixed_move(GTK_FIXED(p->fixed), w, px(p, x * 0.5), px(p, y * 0.6275));
}

static GtkWidget *label_new(const char *text)
{
    GtkWidget *lab = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(lab), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(lab), "w7-panel-label");
    return lab;
}

static GtkWidget *readonly_entry(void)
{
    GtkWidget *e = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(e), FALSE);
    gtk_widget_set_can_focus(e, TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(e), "w7-readonly");
    return e;
}

void panel_set_value(GtkWidget *panel, const char *text)
{
    Panel *p = g_object_get_data(G_OBJECT(panel), "w7-panel");
    if (p && p->set_value)
        p->set_value(p, text);
}

/* ------------------------------------------------------------------ units */

typedef struct {
    Panel base;
    GtkWidget *category, *from_entry, *from_unit, *to_entry, *to_unit;
    bool loading;
} UnitPanel;

static void unit_recompute(UnitPanel *u)
{
    if (u->loading)
        return;
    char *cat = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(u->category));
    char *fu = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(u->from_unit));
    char *tu = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(u->to_unit));
    char *val = parse_number(gtk_entry_get_text(GTK_ENTRY(u->from_entry)));
    char *out = NULL;
    if (cat && fu && tu && val) {
        char *conv = units_convert(val, cat, fu, tu);
        if (conv)
            out = units_format_number(conv, 16);
        free(conv);
    }
    gtk_entry_set_text(GTK_ENTRY(u->to_entry), out ? out : "");
    free(out);
    g_free(val);
    g_free(cat);
    g_free(fu);
    g_free(tu);
}

static void on_unit_changed(GtkWidget *w, gpointer data) { unit_recompute(data); }

static void on_unit_category(GtkWidget *w, gpointer data)
{
    UnitPanel *u = data;
    char *cat = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(u->category));
    if (!cat)
        return;
    u->loading = true;
    int n = units_count(cat);
    const char *frm = NULL, *to = NULL;
    units_default(cat, &frm, &to);
    GtkWidget *combos[2] = {u->from_unit, u->to_unit};
    const char *sel[2] = {frm, to};
    for (int c = 0; c < 2; c++) {
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combos[c]));
        int active = -1;
        for (int i = 0; i < n; i++) {
            const char *name = units_name(cat, i);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combos[c]), name);
            if (sel[c] && strcmp(name, sel[c]) == 0)
                active = i;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(combos[c]), active);
    }
    u->loading = false;
    g_free(cat);
    unit_recompute(u);
}

static void unit_set_value(Panel *p, const char *text)
{
    gtk_entry_set_text(GTK_ENTRY(((UnitPanel *)p)->from_entry), text);  /* 'changed' recomputes To */
}

GtkWidget *panel_unit_new(double scale)
{
    UnitPanel *u = g_new0(UnitPanel, 1);
    Panel *p = &u->base;
    panel_init(p, scale);
    p->set_value = unit_set_value;
    put(p, label_new("Select the type of unit you want to convert"), 18, 16, -1, -1, true);
    u->category = gtk_combo_box_text_new();
    for (int i = 0; i < units_category_count(); i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(u->category), units_category(i));
    put(p, u->category, 18, 68, 690, -1, true);

    put(p, label_new("From"), 18, 117, -1, -1, true);
    u->from_entry = put(p, gtk_entry_new(), 18, 148, 690, 36, true);
    u->from_unit = put(p, gtk_combo_box_text_new(), 18, 190, 690, -1, true);

    put(p, label_new("To"), 18, 247, -1, -1, true);
    u->to_entry = put(p, readonly_entry(), 18, 278, 690, 36, true);
    u->to_unit = put(p, gtk_combo_box_text_new(), 18, 320, 690, -1, true);

    g_signal_connect(u->category, "changed", G_CALLBACK(on_unit_category), u);
    g_signal_connect(u->from_entry, "changed", G_CALLBACK(on_unit_changed), u);
    g_signal_connect(u->from_unit, "changed", G_CALLBACK(on_unit_changed), u);
    g_signal_connect(u->to_unit, "changed", G_CALLBACK(on_unit_changed), u);
    gtk_combo_box_set_active(GTK_COMBO_BOX(u->category), 0);
    return p->box;
}

/* ------------------------------------------------------------------ dates */

static const char *const WEEKDAYS[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
                                       "Sunday"};
static const char *const MONTHS[] = {"January", "February", "March", "April", "May", "June", "July",
                                     "August", "September", "October", "November", "December"};

static bool valid_date(int y, int m, int d)
{
    return y >= 1 && y <= 9999 && m >= 1 && m <= 12 && d >= 1 && d <= date_days_in_month(y, m);
}

/* Win7 en-US short date (M/d/yyyy). */
static char *short_date(CDate d) { return g_strdup_printf("%d/%d/%d", d.month, d.day, d.year); }

/* Button showing a short date; click opens a popover calendar. */
typedef struct {
    GtkWidget *button, *label, *cal, *pop;
    CDate date;
} DatePicker;

static void picker_set_label(DatePicker *dp)
{
    char *s = short_date(dp->date);
    gtk_label_set_text(GTK_LABEL(dp->label), s);
    g_free(s);
}

static void picker_set_date(DatePicker *dp, CDate d)
{
    dp->date = d;
    picker_set_label(dp);
    guint y, m, dd;
    gtk_calendar_get_date(GTK_CALENDAR(dp->cal), &y, &m, &dd);
    if ((int)y != d.year || (int)m + 1 != d.month || (int)dd != d.day) {
        gtk_calendar_select_month(GTK_CALENDAR(dp->cal), d.month - 1, d.year);
        gtk_calendar_select_day(GTK_CALENDAR(dp->cal), d.day);
    }
}

static void on_picker_day(GtkCalendar *cal, gpointer data)
{
    DatePicker *dp = data;
    guint y, m, d;
    gtk_calendar_get_date(cal, &y, &m, &d);
    if (d == 0)  /* no day selected (transient during month change) */
        return;
    if (!valid_date((int)y, (int)m + 1, (int)d))
        return;
    dp->date = (CDate){(int)y, (int)m + 1, (int)d};
    picker_set_label(dp);
}

static void on_picker_double(GtkCalendar *cal, gpointer data)
{
    gtk_popover_popdown(GTK_POPOVER(((DatePicker *)data)->pop));
}

static GtkWidget *picker_init(DatePicker *dp)
{
    GDateTime *now = g_date_time_new_now_local();
    CDate today = {g_date_time_get_year(now), g_date_time_get_month(now), g_date_time_get_day_of_month(now)};
    g_date_time_unref(now);
    dp->button = gtk_menu_button_new();
    dp->label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(dp->label), 0.0);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(box), dp->label, TRUE, TRUE, 0);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_box_pack_end(GTK_BOX(box), gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT), FALSE, FALSE, 0);
    G_GNUC_END_IGNORE_DEPRECATIONS
    gtk_container_add(GTK_CONTAINER(dp->button), box);
    dp->cal = gtk_calendar_new();
    dp->pop = gtk_popover_new(NULL);
    gtk_container_add(GTK_CONTAINER(dp->pop), dp->cal);
    gtk_widget_show(dp->cal);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(dp->button), dp->pop);
    g_signal_connect(dp->cal, "day-selected", G_CALLBACK(on_picker_day), dp);
    g_signal_connect(dp->cal, "day-selected-double-click", G_CALLBACK(on_picker_double), dp);
    picker_set_date(dp, today);
    return dp->button;
}

typedef struct {
    Panel base;
    GtkWidget *mode, *stack;
    DatePicker diff_from, diff_to, add_from;
    GtkWidget *diff_ymwd, *diff_days, *rb_add, *rb_sub, *spins[3], *add_result;
} DatePanel;

static void on_date_mode(GtkWidget *w, gpointer data)
{
    DatePanel *dp = data;
    gtk_stack_set_visible_child_name(GTK_STACK(dp->stack),
                                     gtk_combo_box_get_active(GTK_COMBO_BOX(dp->mode)) == 0 ? "diff" : "add");
}

static void on_calc_difference(GtkWidget *w, gpointer data)
{
    DatePanel *dp = data;
    int y, m, wk, d;
    long total;
    date_difference(dp->diff_from.date, dp->diff_to.date, &y, &m, &wk, &d, &total);
    char *a = date_format_difference(y, m, wk, d), *b = date_format_total_days(total);
    gtk_entry_set_text(GTK_ENTRY(dp->diff_ymwd), a ? a : "");
    gtk_entry_set_text(GTK_ENTRY(dp->diff_days), b ? b : "");
    free(a);
    free(b);
}

static int weekday(CDate d)  /* 0 = Monday */
{
    GDate *g = g_date_new_dmy((GDateDay)d.day, (GDateMonth)d.month, (GDateYear)d.year);
    int wd = (int)g_date_get_weekday(g) - 1;
    g_date_free(g);
    return wd;
}

static void on_calc_add(GtkWidget *w, gpointer data)
{
    DatePanel *dp = data;
    int v[3];
    for (int i = 0; i < 3; i++)
        v[i] = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(dp->spins[i]));
    CDate r;
    if (date_add(dp->add_from.date, v[0], v[1], v[2],
                 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dp->rb_sub)), &r)) {
        char *s = g_strdup_printf("%s, %s %d, %d", WEEKDAYS[weekday(r)], MONTHS[r.month - 1], r.day, r.year);
        gtk_entry_set_text(GTK_ENTRY(dp->add_result), s);
        g_free(s);
    } else {
        gtk_entry_set_text(GTK_ENTRY(dp->add_result), DATE_OUT_OF_BOUND);
    }
}

static GtkWidget *sub_put(Panel *p, GtkWidget *fx, GtkWidget *w, double x, double y, double wd)
{
    return put_in(p, fx, w, x, y, wd, -1, false);
}

static GtkWidget *build_diff(DatePanel *dp)
{
    Panel *p = &dp->base;
    GtkWidget *fx = gtk_fixed_new();
    sub_put(p, fx, label_new("From"), 9, 2, -1);
    sub_put(p, fx, picker_init(&dp->diff_from), 9, 18, 168);
    sub_put(p, fx, label_new("To"), 186, 2, -1);
    sub_put(p, fx, picker_init(&dp->diff_to), 186, 18, 168);
    sub_put(p, fx, label_new("Difference (years, months, weeks, days)"), 9, 54, -1);
    dp->diff_ymwd = sub_put(p, fx, readonly_entry(), 9, 70, 345);
    sub_put(p, fx, label_new("Difference (days)"), 9, 102, -1);
    dp->diff_days = sub_put(p, fx, readonly_entry(), 9, 118, 240);
    GtkWidget *btn = sub_put(p, fx, gtk_button_new_with_label("Calculate"), 259, 152, 95);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_calc_difference), dp);
    return fx;
}

static GtkWidget *build_add(DatePanel *dp)
{
    Panel *p = &dp->base;
    GtkWidget *fx = gtk_fixed_new();
    sub_put(p, fx, label_new("From"), 9, 2, -1);
    sub_put(p, fx, picker_init(&dp->add_from), 9, 18, 168);
    dp->rb_add = sub_put(p, fx, gtk_radio_button_new_with_label(NULL, "Add"), 9, 52, -1);
    dp->rb_sub = sub_put(p, fx, gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(dp->rb_add),
                                                                          "Subtract"), 90, 52, -1);
    static const struct { const char *name; double hi; } sp[] = {
        {"Year(s)", 9999}, {"Month(s)", 9999}, {"Day(s)", 99999},
    };
    for (int k = 0; k < 3; k++) {
        double x = 9 + k * 117;
        sub_put(p, fx, label_new(sp[k].name), x, 80, -1);
        GtkWidget *s = gtk_spin_button_new_with_range(0, sp[k].hi, 1);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(s), TRUE);
        gtk_entry_set_width_chars(GTK_ENTRY(s), 3);
        dp->spins[k] = sub_put(p, fx, s, x, 96, 111);
    }
    sub_put(p, fx, label_new("Date"), 9, 134, -1);
    dp->add_result = sub_put(p, fx, readonly_entry(), 9, 152, 240);
    GtkWidget *btn = sub_put(p, fx, gtk_button_new_with_label("Calculate"), 259, 152, 95);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_calc_add), dp);
    return fx;
}

GtkWidget *panel_date_new(double scale)
{
    DatePanel *dp = g_new0(DatePanel, 1);
    Panel *p = &dp->base;
    panel_init(p, scale);
    /* coordinates here are unscaled px */
    put(p, label_new("Select the date calculation you want"), 9, 8, -1, -1, false);
    dp->mode = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dp->mode), "Calculate the difference between two dates");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dp->mode), "Add or subtract days to a specified date");
    put(p, dp->mode, 9, 26, 345, -1, false);

    dp->stack = gtk_stack_new();
    put(p, dp->stack, 0, 58, PANEL_W, PANEL_H - 58, false);
    gtk_stack_add_named(GTK_STACK(dp->stack), build_diff(dp), "diff");
    gtk_stack_add_named(GTK_STACK(dp->stack), build_add(dp), "add");
    g_signal_connect(dp->mode, "changed", G_CALLBACK(on_date_mode), dp);
    gtk_combo_box_set_active(GTK_COMBO_BOX(dp->mode), 0);
    return p->box;
}

/* ------------------------------------------------------------------ worksheets */

static const int ROW_Y[] = {96, 136, 176, 216, 256, 296};  /* dialog units */

typedef struct {
    Panel base;
    char *worksheet;
    GtkWidget *target, *calc_btn, *result;
    int n;                  /* number of fields */
    char **fields;
    GtkWidget **labels, **entries;
    int last_focus;         /* field index, -1 = none */
} WorksheetPanel;

typedef struct { WorksheetPanel *ws; int field; } FieldRef;

/* Input fields (all fields except the target, spec order); returns the count. */
static int ws_input_names(WorksheetPanel *w, int *out)
{
    char *tgt = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(w->target));
    int n = 0;
    for (int i = 0; i < w->n; i++)
        if (!tgt || strcmp(w->fields[i], tgt) != 0)
            out[n++] = i;
    g_free(tgt);
    return n;
}

static bool in_list(const int *a, int n, int v)
{
    for (int i = 0; i < n; i++)
        if (a[i] == v)
            return true;
    return false;
}

static void on_ws_target(GtkWidget *wd, gpointer data)
{
    WorksheetPanel *w = data;
    int *names = g_new(int, w->n + 1);
    int nn = ws_input_names(w, names);
    for (int f = 0; f < w->n; f++) {
        bool visible = in_list(names, nn, f);
        gtk_widget_set_visible(w->labels[f], visible);
        gtk_widget_set_visible(w->entries[f], visible);
        gtk_widget_set_no_show_all(w->labels[f], !visible);
        gtk_widget_set_no_show_all(w->entries[f], !visible);
    }
    for (int row = 0; row < nn && row < 6; row++) {
        int y = ROW_Y[row];
        move(&w->base, w->labels[names[row]], 18, y + 6);
        move(&w->base, w->entries[names[row]], 360, y);
    }
    if (!in_list(names, nn, w->last_focus))
        w->last_focus = -1;
    gtk_entry_set_text(GTK_ENTRY(w->result), "");
    g_free(names);
}

static gboolean on_ws_focus(GtkWidget *wd, GdkEvent *ev, gpointer data)
{
    FieldRef *r = data;
    r->ws->last_focus = r->field;
    return FALSE;
}

static void on_ws_grab_focus(GtkWidget *wd, gpointer data)
{
    FieldRef *r = data;
    r->ws->last_focus = r->field;
}

static void ws_calculate(GtkWidget *wd, gpointer data)
{
    WorksheetPanel *w = data;
    char *tgt = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(w->target));
    int *names = g_new(int, w->n + 1);
    int nn = ws_input_names(w, names);
    const char **keys = g_new(const char *, nn + 1), **vals = g_new(const char *, nn + 1);
    for (int i = 0; i < nn; i++) {
        keys[i] = w->fields[names[i]];
        vals[i] = gtk_entry_get_text(GTK_ENTRY(w->entries[names[i]]));
    }
    const char *err = NULL, *err_field = NULL;
    char *res = tgt ? ws_solve(w->worksheet, tgt, keys, vals, nn, &err, &err_field) : NULL;
    if (res) {
        char *s = units_format_number(res, 16);
        gtk_entry_set_text(GTK_ENTRY(w->result), s ? s : "");
        free(s);
        free(res);
    } else {
        gtk_entry_set_text(GTK_ENTRY(w->result), err ? err : WS_UNABLE);
        if (err_field)
            for (int f = 0; f < w->n; f++)
                if (strcmp(w->fields[f], err_field) == 0)
                    gtk_widget_grab_focus(w->entries[f]);
    }
    g_free(keys);
    g_free(vals);
    g_free(names);
    g_free(tgt);
}

static void ws_set_value(Panel *p, const char *text)
{
    WorksheetPanel *w = (WorksheetPanel *)p;
    int *names = g_new(int, w->n + 1);
    int nn = ws_input_names(w, names);
    if (nn > 0) {
        int field = in_list(names, nn, w->last_focus) ? w->last_focus : names[0];
        gtk_entry_set_text(GTK_ENTRY(w->entries[field]), text);
    }
    g_free(names);
}

static void ws_free(Panel *p)
{
    WorksheetPanel *w = (WorksheetPanel *)p;
    g_free(w->worksheet);
    g_free(w->fields);  /* strings are owned by the worksheets module */
    g_free(w->labels);
    g_free(w->entries);
}

GtkWidget *panel_worksheet_new(const char *worksheet, double scale)
{
    WorksheetPanel *w = g_new0(WorksheetPanel, 1);
    Panel *p = &w->base;
    panel_init(p, scale);
    p->set_value = ws_set_value;
    p->free_fn = ws_free;
    w->worksheet = g_strdup(worksheet);
    w->last_focus = -1;
    put(p, label_new("Select the value you want to calculate"), 18, 16, -1, -1, true);
    w->target = gtk_combo_box_text_new();
    for (int i = 0; i < ws_calculable_count(worksheet); i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->target), ws_calculable(worksheet, i));
    put(p, w->target, 18, 40, 690, -1, true);

    w->n = ws_field_count(worksheet);
    if (w->n < 0)
        w->n = 0;
    w->fields = g_new0(char *, w->n + 1);
    w->labels = g_new0(GtkWidget *, w->n + 1);
    w->entries = g_new0(GtkWidget *, w->n + 1);
    for (int f = 0; f < w->n; f++) {
        w->fields[f] = (char *)ws_field(worksheet, f);
        GtkWidget *lab = label_new(w->fields[f]), *ent = gtk_entry_new();
        gtk_label_set_ellipsize(GTK_LABEL(lab), PANGO_ELLIPSIZE_END);
        put(p, lab, 18, 96, 340, -1, true);
        put(p, ent, 360, 96, 346, 36, true);
        FieldRef *ref = g_new(FieldRef, 1);
        ref->ws = w;
        ref->field = f;
        g_object_set_data_full(G_OBJECT(ent), "w7-field", ref, g_free);
        g_signal_connect(ent, "focus-in-event", G_CALLBACK(on_ws_focus), ref);
        g_signal_connect(ent, "grab-focus", G_CALLBACK(on_ws_grab_focus), ref);
        g_signal_connect(ent, "activate", G_CALLBACK(ws_calculate), w);
        w->labels[f] = lab;
        w->entries[f] = ent;
    }
    w->calc_btn = put(p, gtk_button_new_with_label("Calculate"), 18, 336, 330, -1, true);
    w->result = put(p, readonly_entry(), 360, 338, 346, 36, true);
    g_signal_connect(w->calc_btn, "clicked", G_CALLBACK(ws_calculate), w);

    g_signal_connect(w->target, "changed", G_CALLBACK(on_ws_target), w);
    gtk_combo_box_set_active(GTK_COMBO_BOX(w->target), 0);
    return p->box;
}
