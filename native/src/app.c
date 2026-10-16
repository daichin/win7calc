/* GTK3 front end: window, menus, mouse and keyboard handling (port of win7calc/app.py). */
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "engine.h"
#include "layout.h"
#include "panels.h"
#include "view.h"

static const char CSS[] =
    ".w7-client { background-image: linear-gradient(to bottom, #edf4fc, #d9e4f1 75px, #d9e4f1); }\n"
    "menubar {\n"
    "  background-image: linear-gradient(to bottom, #ffffff, #f2f5fa 50%, #e3e9f3);\n"
    "  border-bottom: 1px solid #b9c5d6; padding: 0; box-shadow: none;\n"
    "}\n"
    "menubar, menu, menuitem, .w7-panel, .w7-panel * { font-family: \"Segoe UI\", Selawik, \"Noto Sans\", "
    "sans-serif; font-size: @PT@pt; }\n"
    "menubar > menuitem { padding: 2px 7px; color: #000; border: 1px solid transparent; border-radius: 3px; }\n"
    "menubar > menuitem:hover {\n"
    "  background-image: linear-gradient(to bottom, #f8fbfe, #e1ecfa); border-color: #a8c3e5; color: #000; "
    "box-shadow: none;\n"
    "}\n"
    "menu, .menu, .context-menu { background-color: #f0f0f0; border: 1px solid #979797; padding: 2px; }\n"
    "menu menuitem { padding: 3px 18px 3px 6px; color: #000; border: 1px solid transparent; border-radius: 3px; "
    "min-height: 16px; }\n"
    "menu menuitem:hover { background-image: linear-gradient(to bottom, #fbfdff, #e3edfb); border-color: #a8c8ef; "
    "color: #000; }\n"
    "menu menuitem:disabled, menu menuitem:disabled label { color: #6d6d6d; }\n"
    "menu menuitem accelerator { color: #000; margin-left: 24px; }\n"
    "menu separator { background: #e0e0e0; margin: 3px 2px 3px 26px; min-height: 1px; }\n"
    ".w7-panel { background-color: #ffffff; border: 1px solid #8e9cad; border-radius: 2px; }\n"
    ".w7-panel label { color: #000; }\n"
    ".w7-panel entry { min-height: 18px; padding: 1px 4px; border-radius: 0; border: 1px solid #abadb3;\n"
    "                  background: #fff; color: #000; box-shadow: none; }\n"
    ".w7-panel entry.w7-readonly { background: #f4f7fb; }\n"
    ".w7-panel combobox button, .w7-panel button { min-height: 18px; padding: 1px 6px; border-radius: 3px;\n"
    "  border: 1px solid #707070; color: #000; box-shadow: none;\n"
    "  background-image: linear-gradient(to bottom, #f2f2f2, #ebebeb 50%, #dddddd 51%, #cfcfcf); }\n"
    ".w7-panel button:hover { border-color: #3c7fb1;\n"
    "  background-image: linear-gradient(to bottom, #eaf6fd, #d9f0fc 50%, #bee6fd 51%, #a7d9f5); }\n"
    ".w7-panel spinbutton { min-height: 18px; }\n";

static const struct { CalcMode mode; const char *label, *accel; } MODES[] = {
    {MODE_STD, "S_tandard", "<Alt>1"}, {MODE_SCI, "_Scientific", "<Alt>2"},
    {MODE_PROG, "_Programmer", "<Alt>3"}, {MODE_STAT, "St_atistics", "<Alt>4"},
};
#define N_WS 4
static const char *const WORKSHEETS[N_WS] = {"Mortgage", "Vehicle lease", "Fuel economy (mpg)",
                                             "Fuel economy (L/100 km)"};
static const char *const WORKSHEET_LABELS[N_WS] = {"_Mortgage", "_Vehicle lease", "_Fuel economy (mpg)",
                                                   "F_uel economy (L/100 km)"};
/* Extension keys; index 3.. are "worksheet:<name>". */
#define N_EXT (3 + N_WS)
static const char *const EXT_KEYS[N_EXT] = {"basic", "unit", "date", "worksheet:Mortgage",
                                            "worksheet:Vehicle lease", "worksheet:Fuel economy (mpg)",
                                            "worksheet:Fuel economy (L/100 km)"};

typedef struct { gunichar ch; int cid; } CharMap;
#define END_MAP {0, 0}

/* Character keys shared by all modes (accelerator table 906 and the common part of 900-903). */
static const CharMap COMMON_CHARS[] = {
    {'.', CMD_POINT}, {',', CMD_POINT}, {'+', CMD_ADD}, {'-', CMD_SUB}, {'*', CMD_MUL}, {'/', CMD_DIV},
    {'=', CMD_EQU}, END_MAP,
};
static const CharMap STD_CHARS[] = {{'r', CMD_RECIP}, {'@', CMD_SQRT}, {'%', CMD_PERCENT}, END_MAP};
static const CharMap SCI_CHARS[] = {
    {'!', CMD_FACT}, {'s', CMD_SIN}, {'o', CMD_COS}, {'t', CMD_TAN}, {'r', CMD_RECIP}, {'y', CMD_PWR},
    {'#', CMD_CUBE}, {'@', CMD_SQRT}, {'q', CMD_SQR}, {'i', CMD_INV}, {'m', CMD_DMS}, {'n', CMD_LN},
    {'l', CMD_LOG}, {'v', CMD_FE}, {'x', CMD_EXP}, {'e', CMD_EXP}, {'p', CMD_PI}, {'(', CMD_OPENP},
    {')', CMD_CLOSEP}, {';', CMD_INT}, {'d', CMD_MOD}, END_MAP,
};
static const CharMap PROG_CHARS[] = {
    {'a', 140}, {'b', 141}, {'c', 142}, {'d', 143}, {'e', 144}, {'f', 145}, {'j', CMD_ROL}, {'k', CMD_ROR},
    {'%', CMD_MOD}, {'&', CMD_AND}, {'|', CMD_OR}, {'~', CMD_NOT}, {'^', CMD_XOR}, {'(', CMD_OPENP},
    {')', CMD_CLOSEP}, {'<', CMD_LSH}, {'>', CMD_RSH}, END_MAP,
};
static const CharMap STAT_CHARS[] = {
    {'d', CMD_STAT_CAD}, {'v', CMD_FE}, {'a', CMD_STAT_AVE}, {'s', CMD_STAT_SUM}, {'t', CMD_STAT_SDEV},
    {'x', CMD_EXP}, {'e', CMD_EXP}, END_MAP,
};
static const CharMap SCI_CTRL[] = {
    {'s', CMD_SINH}, {'o', CMD_COSH}, {'t', CMD_TANH}, {'y', CMD_ROOT}, {'b', CMD_CUBEROOT}, {'g', CMD_POW10},
    END_MAP,
};
static const CharMap STAT_CTRL[] = {{'a', CMD_STAT_AVE2}, {'s', CMD_STAT_SUM2}, {'t', CMD_STAT_SDEV1}, END_MAP};
static const CharMap COMMON_CTRL[] = {
    {'l', CMD_MC}, {'r', CMD_MR}, {'m', CMD_MS}, {'p', CMD_MPLUS}, {'q', CMD_MMINUS}, END_MAP,
};

typedef struct { const char *name; int cid; } NameMap;
static const NameMap SCI_FKEYS[] = {{"F3", CMD_DEG}, {"F4", CMD_RAD}, {"F5", CMD_GRAD}, {NULL, 0}};
static const NameMap PROG_FKEYS[] = {
    {"F5", CMD_HEX}, {"F6", CMD_DEC}, {"F7", CMD_OCT}, {"F8", CMD_BIN}, {"F12", CMD_QWORD}, {"F2", CMD_DWORD},
    {"F3", CMD_WORD}, {"F4", CMD_BYTE}, {NULL, 0},
};
static const NameMap NAMED_KEYS[] = {
    {"Return", CMD_EQU}, {"KP_Enter", CMD_EQU}, {"Escape", CMD_CLEAR}, {"Delete", CMD_CE},
    {"KP_Delete", CMD_CE}, {"BackSpace", CMD_BACK}, {"F9", CMD_SIGN}, {"KP_Add", CMD_ADD},
    {"KP_Subtract", CMD_SUB}, {"KP_Multiply", CMD_MUL}, {"KP_Divide", CMD_DIV}, {"KP_Decimal", CMD_POINT},
    {"KP_Separator", CMD_POINT}, {NULL, 0},
};

static const char HELP_TEXT[] =
    "Keyboard shortcuts (same as Windows 7 Calculator)\n"
    "\n"
    "All modes\n"
    "  0-9 . +  -  *  /  =  Enter        number entry and operators\n"
    "  Esc  C      Del  CE      Backspace  ←      F9  ±\n"
    "  Ctrl+L MC   Ctrl+R MR   Ctrl+M MS   Ctrl+P M+   Ctrl+Q M-\n"
    "  Ctrl+C copy   Ctrl+V paste   Ctrl+H history\n"
    "  Alt+1..4 Standard / Scientific / Programmer / Statistics\n"
    "  Ctrl+F4 Basic   Ctrl+U Unit conversion   Ctrl+E Date calculation\n"
    "\n"
    "Standard      R 1/x   @ √   % percent\n"
    "Scientific    S sin  O cos  T tan  Ctrl+S/O/T sinh/cosh/tanh  I Inv\n"
    "              N ln  L log  Q x²  # x³  Y xʸ  Ctrl+Y ʸ√x  Ctrl+B ∛x\n"
    "              Ctrl+G 10ˣ  ! n!  M dms  ; Int  D Mod  P π  V F-E  X/E Exp\n"
    "              ( )   F3 Degrees  F4 Radians  F5 Grads\n"
    "Programmer    A-F digits  F5-F8 Hex/Dec/Oct/Bin  F12/F2/F3/F4 Qword/Dword/Word/Byte\n"
    "              & And  | Or  ^ Xor  ~ Not  < Lsh  > Rsh  % Mod  J RoL  K RoR\n"
    "Statistics    Enter Add  D CAD  A x̄  Ctrl+A x̄²  S ∑x  Ctrl+S ∑x²  T σn  Ctrl+T σn-1";

typedef struct {
    GtkWidget *win, *client, *canvas, *panel;
    GtkAccelGroup *accel;
    double scale;
    Engine *eng;
    UIState ui;
    Layout lay;
    char *ext;                 /* basic | unit | date | worksheet:<name> */
    bool syncing;
    guint flash_id;
    GtkWidget *mode_items[4], *history_item, *grouping_item, *ext_items[N_EXT];
    GtkWidget *edit_history, *edit_dataset;
} App;

static void set_mode(App *a, CalcMode mode);
static void set_ext(App *a, const char *ext);
static void command(App *a, int cid);
static void changed(App *a);

/* ------------------------------------------------------------------ settings (JSON) */

typedef enum { JV_NONE, JV_STR, JV_BOOL, JV_NUM, JV_NULL, JV_COMPOUND } JKind;
typedef struct { JKind kind; char *str; bool truthy; } JVal;

typedef struct { const char *p; } JP;

static void j_ws(JP *j)
{
    while (*j->p == ' ' || *j->p == '\t' || *j->p == '\n' || *j->p == '\r')
        j->p++;
}

static int hex4(const char *s)
{
    int v = 0;
    for (int i = 0; i < 4; i++) {
        int c = s[i], d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
        else
            return -1;
        v = v * 16 + d;
    }
    return v;
}

static char *j_string(JP *j)
{
    if (*j->p != '"')
        return NULL;
    j->p++;
    GString *s = g_string_new(NULL);
    for (;;) {
        unsigned char c = (unsigned char)*j->p;
        if (c == 0 || c < 0x20)
            goto fail;
        j->p++;
        if (c == '"')
            break;
        if (c != '\\') {
            g_string_append_c(s, (char)c);
            continue;
        }
        char e = *j->p++;
        switch (e) {
        case '"': g_string_append_c(s, '"'); break;
        case '\\': g_string_append_c(s, '\\'); break;
        case '/': g_string_append_c(s, '/'); break;
        case 'b': g_string_append_c(s, '\b'); break;
        case 'f': g_string_append_c(s, '\f'); break;
        case 'n': g_string_append_c(s, '\n'); break;
        case 'r': g_string_append_c(s, '\r'); break;
        case 't': g_string_append_c(s, '\t'); break;
        case 'u': {
            int u = hex4(j->p);
            if (u < 0)
                goto fail;
            j->p += 4;
            if (u >= 0xd800 && u < 0xdc00 && j->p[0] == '\\' && j->p[1] == 'u') {
                int lo = hex4(j->p + 2);
                if (lo >= 0xdc00 && lo < 0xe000) {
                    u = 0x10000 + ((u - 0xd800) << 10) + (lo - 0xdc00);
                    j->p += 6;
                }
            }
            g_string_append_unichar(s, (gunichar)u);
            break;
        }
        default:
            goto fail;
        }
    }
    return g_string_free(s, FALSE);
fail:
    g_string_free(s, TRUE);
    return NULL;
}

static bool j_value(JP *j, JVal *out, int depth);

static bool j_compound(JP *j, JVal *out, int depth)
{
    char close = *j->p == '{' ? '}' : ']';
    bool obj = close == '}';
    int n = 0;
    j->p++;
    j_ws(j);
    if (*j->p == close) {
        j->p++;
    } else {
        for (;;) {
            JVal v = {JV_NONE, NULL, false};
            if (obj) {
                j_ws(j);
                char *k = j_string(j);
                if (!k)
                    return false;
                g_free(k);
                j_ws(j);
                if (*j->p++ != ':')
                    return false;
            }
            if (!j_value(j, &v, depth + 1))
                return false;
            g_free(v.str);
            n++;
            j_ws(j);
            if (*j->p == ',') {
                j->p++;
                continue;
            }
            if (*j->p++ != close)
                return false;
            break;
        }
    }
    out->kind = JV_COMPOUND;
    out->truthy = n > 0;
    return true;
}

static bool j_value(JP *j, JVal *out, int depth)
{
    if (depth > 64)
        return false;
    j_ws(j);
    const char *p = j->p;
    out->str = NULL;
    if (*p == '"') {
        out->str = j_string(j);
        out->kind = JV_STR;
        out->truthy = out->str && *out->str;
        return out->str != NULL;
    }
    if (*p == '{' || *p == '[')
        return j_compound(j, out, depth);
    if (strncmp(p, "true", 4) == 0 || strncmp(p, "false", 5) == 0) {
        out->kind = JV_BOOL;
        out->truthy = *p == 't';
        j->p += out->truthy ? 4 : 5;
        return true;
    }
    if (strncmp(p, "null", 4) == 0) {
        out->kind = JV_NULL;
        out->truthy = false;
        j->p += 4;
        return true;
    }
    if (strncmp(p, "NaN", 3) == 0) {
        out->kind = JV_NUM;
        out->truthy = true;
        j->p += 3;
        return true;
    }
    char *end;
    double d = g_ascii_strtod(p, &end);
    if (end == p)
        return false;
    out->kind = JV_NUM;
    out->truthy = d != 0;
    j->p = end;
    return true;
}

typedef struct {
    bool have_mode, have_ext, grouping, history;
    char *mode, *ext;
} Settings;

static char *config_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "win7calc", "settings.json", NULL);
}

static void load_settings(Settings *s)
{
    memset(s, 0, sizeof *s);
    char *path = config_path(), *data = NULL;
    gboolean ok = g_file_get_contents(path, &data, NULL, NULL);
    g_free(path);
    if (!ok)
        return;
    Settings tmp = {0};
    JP j = {data};
    j_ws(&j);
    bool good = *j.p == '{';
    if (good) {
        j.p++;
        j_ws(&j);
        if (*j.p == '}') {
            j.p++;
        } else {
            for (;;) {
                j_ws(&j);
                char *key = j_string(&j);
                JVal v = {JV_NONE, NULL, false};
                j_ws(&j);
                if (!key || *j.p++ != ':' || !j_value(&j, &v, 1)) {
                    g_free(key);
                    g_free(v.str);
                    good = false;
                    break;
                }
                /* Later duplicates win, as with Python's json.load. */
                if (strcmp(key, "grouping") == 0) {
                    tmp.grouping = v.truthy;
                } else if (strcmp(key, "history") == 0) {
                    tmp.history = v.truthy;
                } else if (strcmp(key, "mode") == 0) {
                    g_free(tmp.mode);
                    tmp.mode = v.kind == JV_STR ? g_strdup(v.str) : NULL;
                    tmp.have_mode = true;
                } else if (strcmp(key, "ext") == 0) {
                    g_free(tmp.ext);
                    tmp.ext = v.kind == JV_STR ? g_strdup(v.str) : NULL;
                    tmp.have_ext = true;
                }
                g_free(key);
                g_free(v.str);
                j_ws(&j);
                if (*j.p == ',') {
                    j.p++;
                    continue;
                }
                if (*j.p++ != '}')
                    good = false;
                break;
            }
        }
        j_ws(&j);
        if (good && *j.p)
            good = false;
    }
    g_free(data);
    if (good) {
        *s = tmp;
    } else {
        g_free(tmp.mode);
        g_free(tmp.ext);
    }
}

/* json.dump string with ensure_ascii. */
static void json_str(GString *out, const char *s)
{
    g_string_append_c(out, '"');
    for (const char *p = s; *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        switch (c) {
        case '"': g_string_append(out, "\\\""); break;
        case '\\': g_string_append(out, "\\\\"); break;
        case '\n': g_string_append(out, "\\n"); break;
        case '\r': g_string_append(out, "\\r"); break;
        case '\t': g_string_append(out, "\\t"); break;
        case '\b': g_string_append(out, "\\b"); break;
        case '\f': g_string_append(out, "\\f"); break;
        default:
            if (c < 0x20 || (c >= 0x7f && c < 0x10000)) {
                g_string_append_printf(out, "\\u%04x", c);
            } else if (c >= 0x10000) {
                gunichar v = c - 0x10000;
                g_string_append_printf(out, "\\u%04x\\u%04x", 0xd800 + (v >> 10), 0xdc00 + (v & 0x3ff));
            } else {
                g_string_append_c(out, (char)c);
            }
        }
    }
    g_string_append_c(out, '"');
}

static void save_settings(App *a)
{
    char *path = config_path();
    char *dir = g_path_get_dirname(path);
    if (g_mkdir_with_parents(dir, 0777) == 0) {
        GString *s = g_string_new("{\"mode\": ");
        json_str(s, layout_mode_name(engine_mode(a->eng)));
        g_string_append_printf(s, ", \"grouping\": %s, \"history\": %s, \"ext\": ",
                               engine_grouping(a->eng) ? "true" : "false", a->ui.history ? "true" : "false");
        json_str(s, a->ext);
        g_string_append_c(s, '}');
        FILE *f = fopen(path, "w");
        if (f) {
            fputs(s->str, f);
            fclose(f);
        }
        g_string_free(s, TRUE);
    }
    g_free(dir);
    g_free(path);
}

/* ------------------------------------------------------------------ menus */

typedef enum { MI_PLAIN, MI_RADIO, MI_CHECK } MenuKind;

static GtkWidget *menu_item(App *a, GtkWidget *menu, const char *label, GCallback cb, gpointer data,
                            const char *accel, MenuKind kind, GtkWidget *group)
{
    GtkWidget *it;
    if (kind == MI_RADIO)
        it = gtk_radio_menu_item_new_with_mnemonic_from_widget(group ? GTK_RADIO_MENU_ITEM(group) : NULL, label);
    else if (kind == MI_CHECK)
        it = gtk_check_menu_item_new_with_mnemonic(label);
    else
        it = gtk_menu_item_new_with_mnemonic(label);
    if (cb)
        g_signal_connect(it, kind != MI_PLAIN ? "toggled" : "activate", cb, data);
    if (accel) {
        guint k;
        GdkModifierType mods;
        gtk_accelerator_parse(accel, &k, &mods);
        gtk_widget_add_accelerator(it, "activate", a->accel, k, mods, GTK_ACCEL_VISIBLE);
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    return it;
}

static GtkWidget *submenu(GtkWidget *bar, const char *label, GtkWidget **top_out)
{
    GtkWidget *top = gtk_menu_item_new_with_mnemonic(label);
    GtkWidget *menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(top), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), top);
    if (top_out)
        *top_out = top;
    return menu;
}

static void sync_menu(App *a)
{
    a->syncing = true;
    CalcMode mode = engine_mode(a->eng);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(a->mode_items[mode]), TRUE);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(a->history_item), a->ui.history);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(a->grouping_item), engine_grouping(a->eng));
    int ext = 0;
    for (int i = 0; i < N_EXT; i++)
        if (strcmp(a->ext, EXT_KEYS[i]) == 0)
            ext = i;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(a->ext_items[ext]), TRUE);
    bool has_hist = mode == MODE_STD || mode == MODE_SCI;
    gtk_widget_set_sensitive(a->history_item, has_hist);
    gtk_widget_set_sensitive(a->edit_history, has_hist && a->ui.history);
    gtk_widget_set_sensitive(a->edit_dataset, mode == MODE_STAT);
    a->syncing = false;
}

static App *app_of(GtkWidget *w) { return g_object_get_data(G_OBJECT(w), "w7-app"); }

static void relayout(App *a);

static void on_menu_mode(GtkCheckMenuItem *item, gpointer data)
{
    App *a = app_of(GTK_WIDGET(item));
    if (!a->syncing && gtk_check_menu_item_get_active(item))
        set_mode(a, (CalcMode)GPOINTER_TO_INT(data));
}

static void on_menu_history(GtkCheckMenuItem *item, gpointer data)
{
    App *a = data;
    if (!a->syncing) {
        a->ui.history = gtk_check_menu_item_get_active(item);
        a->ui.list_top = view_max_top(a->eng);
        relayout(a);
    }
}

static void on_menu_grouping(GtkCheckMenuItem *item, gpointer data)
{
    App *a = data;
    if (!a->syncing) {
        engine_set_grouping(a->eng, gtk_check_menu_item_get_active(item));
        changed(a);
    }
}

static void on_menu_ext(GtkCheckMenuItem *item, gpointer data)
{
    App *a = app_of(GTK_WIDGET(item));
    if (!a->syncing && gtk_check_menu_item_get_active(item))
        set_ext(a, EXT_KEYS[GPOINTER_TO_INT(data)]);
}

static GtkClipboard *clipboard(void) { return gtk_clipboard_get(GDK_SELECTION_CLIPBOARD); }

static void copy_display(App *a)
{
    const char *d = engine_display(a->eng);
    GString *s = g_string_new(NULL);
    if (engine_error(a->eng)) {
        g_string_assign(s, d);
    } else {
        char drop = !engine_int_mode(a->eng) || engine_radix(a->eng) == 10 ? ',' : ' ';
        for (const char *p = d; *p; p++)
            if (*p != drop)
                g_string_append_c(s, *p);
    }
    gtk_clipboard_set_text(clipboard(), s->str, -1);
    g_string_free(s, TRUE);
}

static void copy_history(App *a)
{
    GString *s = g_string_new(NULL);
    for (int i = 0; i < engine_history_count(a->eng); i++) {
        char *v = engine_format_value(a->eng, engine_history_value(a->eng, i), -1);
        g_string_append_printf(s, "%s%s = %s", i ? "\n" : "", engine_history_text(a->eng, i), v ? v : "");
        free(v);
    }
    gtk_clipboard_set_text(clipboard(), s->str, -1);
    g_string_free(s, TRUE);
}

static void copy_dataset(App *a)
{
    GString *s = g_string_new(NULL);
    for (int i = 0; i < engine_dataset_count(a->eng); i++) {
        char *v = engine_format_value(a->eng, engine_dataset_value(a->eng, i), 0);
        g_string_append_printf(s, "%s%s", i ? "\n" : "", v ? v : "");
        free(v);
    }
    gtk_clipboard_set_text(clipboard(), s->str, -1);
    g_string_free(s, TRUE);
}

static void clear_history(App *a)
{
    engine_clear_history(a->eng);
    a->ui.list_top = 0;
    gtk_widget_queue_draw(a->canvas);
}

static int char_command(App *a, gunichar ch);

/* Win7 treats pasted text as typed keys; a plain number is loaded directly. */
static void paste_text(App *a, const char *text)
{
    char *num = engine_int_mode(a->eng) ? NULL : parse_number(text);
    if (num) {
        engine_set_value(a->eng, num);
        g_free(num);
        changed(a);
        return;
    }
    if (!g_utf8_validate(text, -1, NULL))
        return;
    for (const char *p = text; *p; p = g_utf8_next_char(p)) {
        gunichar ch = g_utf8_get_char(p);
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == ',')
            continue;
        int cid = char_command(a, ch);
        if (cid < 0 && (ch == 'e' || ch == 'E') && !engine_int_mode(a->eng))
            cid = CMD_EXP;
        if (cid >= 0)
            engine_command(a->eng, cid);
    }
    a->ui.list_top = view_max_top(a->eng);
    changed(a);
}

static void paste(App *a)
{
    char *text = gtk_clipboard_wait_for_text(clipboard());
    if (text && *text)
        paste_text(a, text);
    g_free(text);
}

static void show_help(App *a)
{
    GtkWidget *dlg = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dlg), "Calculator Help");
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(a->win));
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_dialog_add_button(GTK_DIALOG(dlg), "_Close", GTK_RESPONSE_CLOSE);
    GtkWidget *lbl = gtk_label_new(NULL);
    char *esc = g_markup_escape_text(HELP_TEXT, -1);
    char *markup = g_strconcat("<tt>", esc, "</tt>", NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), markup);
    g_free(markup);
    g_free(esc);
    gtk_widget_set_margin_start(lbl, 14);
    gtk_widget_set_margin_end(lbl, 14);
    gtk_widget_set_margin_top(lbl, 10);
    gtk_widget_set_margin_bottom(lbl, 10);
    gtk_label_set_selectable(GTK_LABEL(lbl), TRUE);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), lbl);
    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static void show_about(App *a)
{
    GtkWidget *dlg = gtk_about_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(a->win));
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dlg), "Calculator");
    gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(dlg), "accessories-calculator");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dlg),
                                  "A Linux (GTK3) replica of the classic Windows 7 Calculator.\n"
                                  "Layout, key bindings and behaviour follow win32calc.exe.");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dlg), "1.0");
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static void on_copy(GtkMenuItem *w, gpointer d) { copy_display(d); }
static void on_paste(GtkMenuItem *w, gpointer d) { paste(d); }
static void on_copy_history(GtkMenuItem *w, gpointer d) { copy_history(d); }
static void on_clear_history(GtkMenuItem *w, gpointer d) { clear_history(d); }
static void on_copy_dataset(GtkMenuItem *w, gpointer d) { copy_dataset(d); }
static void on_clear_dataset(GtkMenuItem *w, gpointer d) { command(d, CMD_STAT_CAD); }
static void on_help(GtkMenuItem *w, gpointer d) { show_help(d); }
static void on_about(GtkMenuItem *w, gpointer d) { show_about(d); }

static GtkWidget *build_menu(App *a)
{
    GtkWidget *bar = gtk_menu_bar_new();
    GtkWidget *m = submenu(bar, "_View", NULL);
    GtkWidget *group = NULL;
    for (int i = 0; i < 4; i++) {
        GtkWidget *it = menu_item(a, m, MODES[i].label, G_CALLBACK(on_menu_mode), GINT_TO_POINTER(MODES[i].mode),
                                  MODES[i].accel, MI_RADIO, group);
        g_object_set_data(G_OBJECT(it), "w7-app", a);
        if (!group)
            group = it;
        a->mode_items[MODES[i].mode] = it;
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    a->history_item = menu_item(a, m, "Histor_y", G_CALLBACK(on_menu_history), a, "<Ctrl>h", MI_CHECK, NULL);
    a->grouping_item = menu_item(a, m, "D_igit grouping", G_CALLBACK(on_menu_grouping), a, NULL, MI_CHECK, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    static const char *const ext_labels[3] = {"_Basic", "_Unit conversion", "_Date calculation"};
    static const char *const ext_accels[3] = {"<Ctrl>F4", "<Ctrl>u", "<Ctrl>e"};
    for (int i = 0; i < 3; i++)
        a->ext_items[i] = menu_item(a, m, ext_labels[i], G_CALLBACK(on_menu_ext), GINT_TO_POINTER(i),
                                    ext_accels[i], MI_RADIO, i ? a->ext_items[0] : NULL);
    GtkWidget *ws_top = gtk_menu_item_new_with_mnemonic("_Worksheets"), *ws = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(ws_top), ws);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), ws_top);
    for (int i = 0; i < N_WS; i++)
        a->ext_items[3 + i] = menu_item(a, ws, WORKSHEET_LABELS[i], G_CALLBACK(on_menu_ext),
                                        GINT_TO_POINTER(3 + i), NULL, MI_RADIO, a->ext_items[0]);
    for (int i = 0; i < N_EXT; i++)
        g_object_set_data(G_OBJECT(a->ext_items[i]), "w7-app", a);

    m = submenu(bar, "_Edit", NULL);
    menu_item(a, m, "_Copy", G_CALLBACK(on_copy), a, "<Ctrl>c", MI_PLAIN, NULL);
    menu_item(a, m, "_Paste", G_CALLBACK(on_paste), a, "<Ctrl>v", MI_PLAIN, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    GtkWidget *hm = submenu(m, "_History", &a->edit_history);
    menu_item(a, hm, "Copy h_istory", G_CALLBACK(on_copy_history), a, NULL, MI_PLAIN, NULL);
    menu_item(a, hm, "C_lear", G_CALLBACK(on_clear_history), a, "<Ctrl><Shift>d", MI_PLAIN, NULL);
    GtkWidget *dm = submenu(m, "_Dataset", &a->edit_dataset);
    menu_item(a, dm, "Copy d_ataset", G_CALLBACK(on_copy_dataset), a, NULL, MI_PLAIN, NULL);
    menu_item(a, dm, "C_lear", G_CALLBACK(on_clear_dataset), a, NULL, MI_PLAIN, NULL);

    m = submenu(bar, "_Help", NULL);
    menu_item(a, m, "_View Help", G_CALLBACK(on_help), a, "F1", MI_PLAIN, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    menu_item(a, m, "_About Calculator", G_CALLBACK(on_about), a, NULL, MI_PLAIN, NULL);
    return bar;
}

/* ------------------------------------------------------------------ mode / layout */

static void relayout(App *a)
{
    layout_build(&a->lay, engine_mode(a->eng), a->ui.history, false);
    gtk_widget_set_size_request(a->canvas, (int)py_round(a->lay.width * a->scale),
                                (int)py_round(a->lay.height * a->scale));
    if (a->mode_items[0])
        sync_menu(a);
    gtk_window_resize(GTK_WINDOW(a->win), 1, 1);
    gtk_widget_queue_draw(a->canvas);
}

static void set_mode(App *a, CalcMode mode)
{
    engine_set_mode(a->eng, mode);
    a->ui.list_top = view_max_top(a->eng);
    relayout(a);
    sync_menu(a);
}

static void push_panel(App *a)
{
    if (!a->panel || engine_error(a->eng))
        return;
    if (engine_int_mode(a->eng)) {
        char *v = engine_current_value(a->eng);
        panel_set_value(a->panel, v ? v : "");
        free(v);
    } else {
        GString *s = g_string_new(NULL);
        for (const char *p = engine_display(a->eng); *p; p++)
            if (*p != ',')
                g_string_append_c(s, *p);
        panel_set_value(a->panel, s->str);
        g_string_free(s, TRUE);
    }
}

static void set_ext(App *a, const char *ext_in)
{
    char *ext = g_strdup(ext_in ? ext_in : "basic");
    if (a->panel) {
        gtk_container_remove(GTK_CONTAINER(a->client), a->panel);
        a->panel = NULL;
    }
    g_free(a->ext);
    a->ext = ext;
    if (strcmp(ext, "unit") == 0) {
        a->panel = panel_unit_new(a->scale);
    } else if (strcmp(ext, "date") == 0) {
        a->panel = panel_date_new(a->scale);
    } else if (g_str_has_prefix(ext, "worksheet:")) {
        for (int i = 0; i < N_WS; i++)
            if (strcmp(ext + 10, WORKSHEETS[i]) == 0)
                a->panel = panel_worksheet_new(WORKSHEETS[i], a->scale);
    }
    if (!a->panel) {
        g_free(a->ext);
        a->ext = g_strdup("basic");
    }
    if (a->panel) {
        int m = (int)py_round(LAYOUT_MARGIN * a->scale);
        gtk_widget_set_valign(a->panel, GTK_ALIGN_START);
        gtk_widget_set_margin_top(a->panel, m);
        gtk_widget_set_margin_end(a->panel, m);
        gtk_widget_set_margin_bottom(a->panel, m);
        gtk_box_pack_start(GTK_BOX(a->client), a->panel, FALSE, FALSE, 0);
        gtk_widget_show_all(a->panel);
        push_panel(a);
    }
    relayout(a);
    sync_menu(a);
}

/* ------------------------------------------------------------------ painting / mouse */

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer data)
{
    App *a = data;
    cairo_scale(cr, a->scale, a->scale);
    view_paint(cr, &a->lay, a->eng, &a->ui);
    return TRUE;
}

static ItemKey hit(App *a, double ex, double ey)
{
    double x = ex / a->scale, y = ey / a->scale;
    Item di[8];
    int nd = view_display_items(&a->lay, a->eng, &a->ui, di);
    for (int i = 0; i < nd + a->lay.n_items; i++) {
        const Item *it = i < nd ? &di[i] : &a->lay.items[i - nd];
        ItemKind k = it->kind;
        if ((k == IT_BUTTON || k == IT_RADIO || k == IT_BIT || k == IT_ARROW || k == IT_ROW) &&
            item_contains(it, x, y)) {
            if (k == IT_BIT && it->id >= engine_bits(a->eng))
                return KEY_NONE;
            return item_key(it);
        }
    }
    return KEY_NONE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *ev, gpointer data)
{
    App *a = data;
    ItemKey h = hit(a, ev->x, ev->y);
    if (!key_eq(h, a->ui.hot)) {
        a->ui.hot = h;
        gtk_widget_queue_draw(w);
    }
    return TRUE;
}

static gboolean on_leave(GtkWidget *w, GdkEventCrossing *ev, gpointer data)
{
    App *a = data;
    a->ui.hot = KEY_NONE;
    gtk_widget_queue_draw(w);
    return TRUE;
}

static gboolean on_press(GtkWidget *w, GdkEventButton *ev, gpointer data)
{
    App *a = data;
    gtk_widget_grab_focus(w);
    if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS) {
        a->ui.pressed = hit(a, ev->x, ev->y);
        gtk_widget_queue_draw(w);
    }
    return TRUE;
}

static void scroll_list(App *a, int delta)
{
    int t = a->ui.list_top + delta, mx = view_max_top(a->eng);
    a->ui.list_top = t < 0 ? 0 : t > mx ? mx : t;
    gtk_widget_queue_draw(a->canvas);
}

static void activate(App *a, ItemKey k)
{
    switch (k.kind) {
    case IT_BUTTON:
    case IT_RADIO:
        command(a, k.id);
        break;
    case IT_BIT:
        engine_toggle_bit(a->eng, k.id);
        changed(a);
        break;
    case IT_ARROW:
        scroll_list(a, k.id == VIEW_UP ? -1 : 1);
        break;
    case IT_ROW: {
        int idx = a->ui.list_top + k.id;
        CalcMode m = engine_mode(a->eng);
        if ((m == MODE_STD || m == MODE_SCI) && idx < engine_history_count(a->eng)) {
            char *v = g_strdup(engine_history_value(a->eng, idx));
            engine_set_value(a->eng, v);
            g_free(v);
            changed(a);
        }
        break;
    }
    default:
        break;
    }
}

static gboolean on_release(GtkWidget *w, GdkEventButton *ev, gpointer data)
{
    App *a = data;
    if (ev->button != 1)
        return TRUE;
    ItemKey target = hit(a, ev->x, ev->y), pressed = a->ui.pressed;
    a->ui.pressed = KEY_NONE;
    if (target.kind != IT_NONE && key_eq(target, pressed))
        activate(a, target);
    gtk_widget_queue_draw(w);
    return TRUE;
}

static gboolean on_scroll(GtkWidget *w, GdkEventScroll *ev, gpointer data)
{
    if (ev->direction == GDK_SCROLL_UP)
        scroll_list(data, -1);
    else if (ev->direction == GDK_SCROLL_DOWN)
        scroll_list(data, 1);
    return TRUE;
}

static gboolean on_query_tooltip(GtkWidget *w, gint x, gint y, gboolean kbd, GtkTooltip *tip, gpointer data)
{
    App *a = data;
    if (kbd)
        return FALSE;
    ItemKey k = hit(a, x, y);
    if (k.kind != IT_BUTTON)
        return FALSE;
    const char *t = layout_tooltip(k.id);
    if (!t)
        return FALSE;
    gtk_tooltip_set_text(tip, t);
    return TRUE;
}

/* ------------------------------------------------------------------ commands */

static void changed(App *a)
{
    int mx = view_max_top(a->eng);
    if (a->ui.list_top > mx)
        a->ui.list_top = mx;
    gtk_widget_queue_draw(a->canvas);
    push_panel(a);
}

static void command(App *a, int cid)
{
    int n_hist = engine_history_count(a->eng), n_data = engine_dataset_count(a->eng);
    engine_command(a->eng, cid);
    if (engine_history_count(a->eng) != n_hist || engine_dataset_count(a->eng) != n_data)
        a->ui.list_top = view_max_top(a->eng);
    changed(a);
}

static gboolean clear_flash(gpointer data)
{
    App *a = data;
    a->ui.flash = KEY_NONE;
    a->flash_id = 0;
    gtk_widget_queue_draw(a->canvas);
    return G_SOURCE_REMOVE;
}

static void flash(App *a, int cid)
{
    a->ui.flash = (ItemKey){IT_BUTTON, cid};
    gtk_widget_queue_draw(a->canvas);
    if (a->flash_id)
        g_source_remove(a->flash_id);
    a->flash_id = g_timeout_add(110, clear_flash, a);
}

/* ------------------------------------------------------------------ keyboard */

static int map_char(const CharMap *m, gunichar ch)
{
    for (; m && m->ch; m++)
        if (m->ch == ch)
            return m->cid;
    return -1;
}

static int map_name(const NameMap *m, const char *name)
{
    for (; m && m->name; m++)
        if (strcmp(m->name, name) == 0)
            return m->cid;
    return -1;
}

static const CharMap *mode_chars(CalcMode m)
{
    switch (m) {
    case MODE_STD: return STD_CHARS;
    case MODE_SCI: return SCI_CHARS;
    case MODE_PROG: return PROG_CHARS;
    default: return STAT_CHARS;
    }
}

static int char_command(App *a, gunichar ch)
{
    if (!ch)
        return -1;
    if (ch >= '0' && ch <= '9')
        return CMD_DIGIT0 + (int)(ch - '0');
    CalcMode mode = engine_mode(a->eng);
    const CharMap *mm = mode_chars(mode);
    int cid = map_char(mm, ch);
    if (cid >= 0)
        return cid;
    if (mode == MODE_PROG && (cid = map_char(mm, g_unichar_tolower(ch))) >= 0)
        return cid;
    return map_char(COMMON_CHARS, ch);
}

/* Command for a key press; -1 = none, -2 / -3 = scroll the list up / down. */
static int key_command(App *a, guint keyval, GdkModifierType state)
{
    CalcMode mode = engine_mode(a->eng);
    const char *name = gdk_keyval_name(keyval);
    if (!name)
        name = "";
    if (state & GDK_MOD1_MASK)
        return -1;
    gunichar ch = gdk_keyval_to_unicode(keyval);
    if (state & GDK_CONTROL_MASK) {
        gunichar low = g_unichar_tolower(ch);
        if (!ch) {
            if (!name[0] || name[1])
                return -1;
            low = g_ascii_tolower(name[0]);
        }
        const CharMap *mc = mode == MODE_SCI ? SCI_CTRL : mode == MODE_STAT ? STAT_CTRL : NULL;
        int cid = map_char(mc, low);
        return cid >= 0 ? cid : map_char(COMMON_CTRL, low);
    }
    const NameMap *fk = mode == MODE_SCI ? SCI_FKEYS : mode == MODE_PROG ? PROG_FKEYS : NULL;
    int cid = map_name(fk, name);
    if (cid >= 0)
        return cid;
    if ((strcmp(name, "Return") == 0 || strcmp(name, "KP_Enter") == 0) && mode == MODE_STAT)
        return CMD_STAT_ADD;
    if (strcmp(name, "Up") == 0 || strcmp(name, "Page_Up") == 0)
        return -2;
    if (strcmp(name, "Down") == 0 || strcmp(name, "Page_Down") == 0)
        return -3;
    if ((cid = map_name(NAMED_KEYS, name)) >= 0)
        return cid;
    return char_command(a, ch);
}

static gboolean on_key(GtkWidget *w, GdkEventKey *ev, gpointer data)
{
    App *a = data;
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(a->win));
    if (focus && (GTK_IS_ENTRY(focus) || (focus != a->canvas && gtk_widget_get_ancestor(focus, GTK_TYPE_POPOVER))))
        return FALSE;
    int cid = key_command(a, ev->keyval, (GdkModifierType)ev->state);
    if (cid == -1)
        return FALSE;
    if (cid == -2 || cid == -3) {
        scroll_list(a, cid == -2 ? -1 : 1);
        return TRUE;
    }
    command(a, cid);
    for (int i = 0; i < a->lay.n_items; i++)
        if (a->lay.items[i].kind == IT_BUTTON && a->lay.items[i].id == cid) {
            flash(a, cid);
            break;
        }
    return TRUE;
}

static gboolean on_close(GtkWidget *w, GdkEvent *ev, gpointer data)
{
    save_settings(data);
    return FALSE;
}

/* ------------------------------------------------------------------ window */

static App *app_new(double scale)
{
    App *a = g_new0(App, 1);
    a->scale = scale;
    a->eng = engine_new();
    ui_state_init(&a->ui);
    a->ext = g_strdup("basic");

    a->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(a->win), "Calculator");
    gtk_window_set_resizable(GTK_WINDOW(a->win), FALSE);
    gtk_window_set_icon_name(GTK_WINDOW(a->win), "accessories-calculator");

    char pt[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_formatd(pt, sizeof pt, "%.1f", 9 * scale);
    GString *css = g_string_new(CSS);
    g_string_replace(css, "@PT@", pt, 0);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css->str, -1, NULL);
    g_string_free(css, TRUE);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    a->accel = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(a->win), a->accel);
    g_object_unref(a->accel);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(a->win), vbox);
    gtk_box_pack_start(GTK_BOX(vbox), build_menu(a), FALSE, FALSE, 0);

    a->client = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(a->client), "w7-client");
    gtk_box_pack_start(GTK_BOX(vbox), a->client, TRUE, TRUE, 0);

    a->canvas = gtk_drawing_area_new();
    gtk_widget_set_can_focus(a->canvas, TRUE);
    gtk_widget_add_events(a->canvas, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
                                         GDK_LEAVE_NOTIFY_MASK | GDK_SCROLL_MASK);
    gtk_widget_set_has_tooltip(a->canvas, TRUE);
    g_signal_connect(a->canvas, "draw", G_CALLBACK(on_draw), a);
    g_signal_connect(a->canvas, "button-press-event", G_CALLBACK(on_press), a);
    g_signal_connect(a->canvas, "button-release-event", G_CALLBACK(on_release), a);
    g_signal_connect(a->canvas, "motion-notify-event", G_CALLBACK(on_motion), a);
    g_signal_connect(a->canvas, "leave-notify-event", G_CALLBACK(on_leave), a);
    g_signal_connect(a->canvas, "scroll-event", G_CALLBACK(on_scroll), a);
    g_signal_connect(a->canvas, "query-tooltip", G_CALLBACK(on_query_tooltip), a);
    gtk_box_pack_start(GTK_BOX(a->client), a->canvas, FALSE, FALSE, 0);

    g_signal_connect(a->win, "key-press-event", G_CALLBACK(on_key), a);
    g_signal_connect(a->win, "delete-event", G_CALLBACK(on_close), a);

    Settings s;
    load_settings(&s);
    engine_set_grouping(a->eng, s.grouping);
    a->ui.history = s.history;
    CalcMode mode = MODE_STD;
    if (s.mode)
        layout_mode_by_name(s.mode, &mode);
    set_mode(a, mode);
    set_ext(a, s.have_ext ? (s.ext ? s.ext : "basic") : "basic");
    sync_menu(a);
    gtk_widget_grab_focus(a->canvas);
    g_free(s.mode);
    g_free(s.ext);
    return a;
}

static void app_free(App *a)
{
    if (a->flash_id)
        g_source_remove(a->flash_id);
    engine_free(a->eng);
    g_free(a->ext);
    g_free(a);
}

/* ------------------------------------------------------------------ test hook: window screenshot */

static int winshot(App *a, const char *out, int nsteps, char **steps)
{
    for (int i = 0; i < nsteps; i++) {
        const char *st = steps[i];
        if (g_str_has_prefix(st, "mode=")) {
            CalcMode m;
            if (layout_mode_by_name(st + 5, &m))
                set_mode(a, m);
        } else if (g_str_has_prefix(st, "ext=")) {
            set_ext(a, st + 4);
        } else if (g_str_has_prefix(st, "keys=")) {
            for (const char *p = st + 5; *p; p = g_utf8_next_char(p)) {
                int cid = char_command(a, g_utf8_get_char(p));
                if (cid >= 0)
                    command(a, cid);
            }
        } else if (g_str_has_prefix(st, "paste=")) {
            paste_text(a, st + 6);
        } else if (strcmp(st, "history") == 0) {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(a->history_item), TRUE);
        } else if (strcmp(st, "grouping") == 0) {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(a->grouping_item), TRUE);
        } else {
            int cid = layout_cmd_by_name(st);
            if (cid < 0) {
                fprintf(stderr, "win7calc: unknown step %s\n", st);
                return 2;
            }
            command(a, cid);
        }
    }
    for (int i = 0; i < 50; i++)
        while (gtk_events_pending())
            gtk_main_iteration();
    GtkAllocation al;
    gtk_widget_get_allocation(a->win, &al);
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, al.width, al.height);
    cairo_t *cr = cairo_create(surf);
    gtk_widget_draw(a->win, cr);
    cairo_destroy(cr);
    cairo_status_t rc = cairo_surface_write_to_png(surf, out);
    cairo_surface_destroy(surf);
    printf("%s %d %d display= '%s' expr= '%s'\n", out, al.width, al.height, engine_display(a->eng),
           engine_expression(a->eng));
    return rc == CAIRO_STATUS_SUCCESS ? 0 : 1;
}

/* ------------------------------------------------------------------ entry point */

static void usage(FILE *f)
{
    fprintf(f, "usage: win7calc [-h] [--scale SCALE] [--mode {std,sci,prog,stat}]\n");
}

int app_main(int argc, char **argv)
{
    const char *env = g_getenv("WIN7CALC_SCALE");
    double scale = 1.0;
    if (env) {
        char *end;
        double v = g_ascii_strtod(env, &end);
        if (end != env && !*end)
            scale = v;
    }
    const char *mode_arg = NULL, *shot = NULL;
    int nsteps = 0;
    char **steps = NULL;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--winshot") == 0 && i + 2 < argc) {   /* hidden test hook */
            shot = argv[i + 1];
            scale = g_ascii_strtod(argv[i + 2], NULL);
            steps = argv + i + 3;
            nsteps = argc - i - 3;
            break;
        }
        /* argparse-style long options: unique prefixes and --opt=value are accepted. */
        const char *eq = strchr(arg, '=');
        size_t len = eq ? (size_t)(eq - arg) : strlen(arg);
        int opt = 0;    /* 1 help, 2 scale, 3 mode */
        if (strcmp(arg, "-h") == 0)
            opt = 1;
        else if (len >= 3 && strncmp(arg, "--", 2) == 0) {
            static const char *const longs[] = {"--help", "--scale", "--mode"};
            for (int k = 0; k < 3; k++)
                if (len <= strlen(longs[k]) && strncmp(arg, longs[k], len) == 0)
                    opt = k + 1;
        }
        if (opt == 1 && !eq) {
            usage(stdout);
            printf("\nWindows 7 Calculator replica for Linux\n\noptions:\n"
                   "  -h, --help            show this help message and exit\n"
                   "  --scale SCALE         UI zoom factor (e.g. 1.25 or 1.5), default 1.0 = original Win7 size\n"
                   "  --mode {std,sci,prog,stat}\n"
                   "                        start in this mode\n");
            return 0;
        }
        if (opt == 2 || opt == 3) {
            const char *name = opt == 2 ? "--scale" : "--mode", *val;
            if (eq)
                val = eq + 1;
            else if (i + 1 < argc)
                val = argv[++i];
            else {
                usage(stderr);
                fprintf(stderr, "win7calc: error: argument %s: expected one argument\n", name);
                return 2;
            }
            if (opt == 2) {
                char *end;
                double v = g_ascii_strtod(val, &end);
                if (end == val || *end) {
                    usage(stderr);
                    fprintf(stderr, "win7calc: error: argument --scale: invalid float value: '%s'\n", val);
                    return 2;
                }
                scale = v;
            } else {
                CalcMode m;
                if (!layout_mode_by_name(val, &m)) {
                    usage(stderr);
                    fprintf(stderr, "win7calc: error: argument --mode: invalid choice: '%s' "
                                    "(choose from 'std', 'sci', 'prog', 'stat')\n", val);
                    return 2;
                }
                mode_arg = val;
            }
            continue;
        }
        usage(stderr);
        fprintf(stderr, "win7calc: error: unrecognized arguments: %s\n", arg);
        return 2;
    }
    if (!gtk_init_check(&argc, &argv)) {
        fputs("win7calc: cannot open a display (DISPLAY/WAYLAND_DISPLAY not set).\n"
              "  Over SSH, either reconnect with X forwarding:  ssh -X user@host\n"
              "  or serve it to a web browser (GTK Broadway):\n"
              "      broadwayd --address 127.0.0.1 :5 &   # http://localhost:8085/\n"
              "      GDK_BACKEND=broadway BROADWAY_DISPLAY=:5 win7calc\n",
              stderr);
        return 1;
    }
    if (!(scale >= 0.5))
        scale = 0.5;
    if (scale > 4.0)
        scale = 4.0;
    App *a = app_new(scale);
    int rc = 0;
    if (shot) {
        gtk_widget_show_all(a->win);
        rc = winshot(a, shot, nsteps, steps);
        gtk_widget_destroy(a->win);
    } else {
        CalcMode m;
        if (mode_arg && layout_mode_by_name(mode_arg, &m))
            set_mode(a, m);
        g_signal_connect(a->win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
        gtk_widget_show_all(a->win);
        gtk_main();
    }
    app_free(a);
    return rc;
}
