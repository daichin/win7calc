/* Window geometry, taken from win32calc's dialog templates (port of win7calc/layout.py). */
#include "layout.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

double py_round(double v) { return rint(v); }

static int ux(double v) { return (int)py_round(v / 2); }
static int uy(double v) { return (int)(v * 0.6275 + 0.01); }
static int uh(double v) { return (int)py_round(v * 0.6275); }

/* (id, x, y, w, h) in dialog units — copied from the .mui DIALOG resources. */
typedef struct { int id, x, y, w, h; } Cell;

static const Cell STD_T[] = {
    {122, 0, 0, 68, 43}, {123, 78, 0, 68, 43}, {124, 156, 0, 68, 43}, {125, 234, 0, 68, 43}, {126, 312, 0, 68, 43},
    {83, 0, 51, 68, 43}, {82, 78, 51, 68, 43}, {81, 156, 51, 68, 43}, {80, 234, 51, 68, 43}, {110, 312, 51, 68, 43},
    {137, 0, 102, 68, 43}, {138, 78, 102, 68, 43}, {139, 156, 102, 68, 43}, {91, 234, 102, 68, 43}, {118, 312, 102, 68, 43},
    {134, 0, 154, 68, 43}, {135, 78, 154, 68, 43}, {136, 156, 154, 68, 43}, {92, 234, 154, 68, 43}, {114, 312, 154, 68, 43},
    {131, 0, 205, 68, 43}, {132, 78, 205, 68, 43}, {133, 156, 205, 68, 43}, {94, 234, 205, 68, 43}, {121, 312, 205, 68, 94},
    {130, 0, 256, 146, 43}, {84, 156, 256, 68, 43}, {93, 234, 256, 68, 43},
};

static const Cell SCI_LEFT[] = {
    {98, 0, 102, 68, 43}, {115, 0, 154, 68, 43}, {120, 0, 205, 68, 43}, {119, 0, 256, 68, 43},
    {146, 78, 51, 68, 43}, {105, 78, 102, 68, 43}, {106, 78, 154, 68, 43}, {107, 78, 205, 68, 43}, {127, 78, 256, 68, 43},
    {108, 156, 51, 68, 43}, {102, 156, 102, 68, 43}, {103, 156, 154, 68, 43}, {104, 156, 205, 68, 43}, {95, 156, 256, 68, 43},
    {128, 234, 51, 68, 43}, {111, 234, 102, 68, 43}, {97, 234, 154, 68, 43}, {112, 234, 205, 68, 43}, {109, 234, 256, 68, 43},
    {129, 312, 51, 68, 43}, {113, 312, 102, 68, 43}, {96, 312, 154, 68, 43}, {116, 312, 205, 68, 43}, {117, 312, 256, 68, 43},
};

static const Cell PROG_LEFT[] = {
    {128, 156, 157, 68, 43}, {99, 156, 208, 68, 43}, {87, 156, 259, 68, 43}, {89, 156, 310, 68, 43}, {101, 156, 361, 68, 43},
    {95, 234, 106, 68, 43}, {129, 234, 157, 68, 43}, {100, 234, 208, 68, 43}, {88, 234, 259, 68, 43}, {90, 234, 310, 68, 43},
    {86, 234, 361, 68, 43},
    {140, 312, 106, 68, 43}, {141, 312, 157, 68, 43}, {142, 312, 208, 68, 43}, {143, 312, 259, 68, 43},
    {144, 312, 310, 68, 43}, {145, 312, 361, 68, 43},
};

static const Cell STAT_T[] = {
    {122, 0, 248, 68, 43}, {123, 78, 248, 68, 43}, {124, 156, 248, 68, 43}, {125, 234, 248, 68, 43}, {126, 312, 248, 68, 43},
    {83, 0, 300, 68, 43}, {1056, 78, 300, 68, 43}, {81, 156, 300, 68, 43}, {119, 234, 300, 68, 43}, {127, 312, 300, 68, 43},
    {137, 0, 351, 68, 43}, {138, 78, 351, 68, 43}, {139, 156, 351, 68, 43}, {171, 234, 351, 68, 43}, {172, 312, 351, 68, 43},
    {134, 0, 402, 68, 43}, {135, 78, 402, 68, 43}, {136, 156, 402, 68, 43}, {173, 234, 402, 68, 43}, {174, 312, 402, 68, 43},
    {131, 0, 453, 68, 43}, {132, 78, 453, 68, 43}, {133, 156, 453, 68, 43}, {175, 234, 453, 68, 43}, {176, 312, 453, 68, 43},
    {130, 0, 504, 146, 43}, {84, 156, 504, 68, 43}, {80, 234, 504, 68, 43}, {168, 312, 504, 68, 43},
};

#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

typedef struct { int id; const char *text; } Str;

static const Str LABELS[] = {
    {CMD_MC, "MC"}, {CMD_MR, "MR"}, {CMD_MS, "MS"}, {CMD_MPLUS, "M+"}, {CMD_MMINUS, "M-"},
    {CMD_BACK, "←"}, {CMD_CE, "CE"}, {CMD_CLEAR, "C"}, {CMD_SIGN, "±"}, {CMD_SQRT, "√"},
    {CMD_DIV, "/"}, {CMD_MUL, "*"}, {CMD_SUB, "-"}, {CMD_ADD, "+"}, {CMD_EQU, "="}, {CMD_POINT, "."},
    {CMD_PERCENT, "%"}, {CMD_RECIP, "1/<i>x</i>"},
    {CMD_INT, "Int"}, {CMD_DMS, "dms"}, {CMD_PI, "π"}, {CMD_FE, "F-E"}, {CMD_INV, "Inv"}, {CMD_EXP, "Exp"},
    {CMD_MOD, "Mod"},
    {CMD_SINH, "sinh"}, {CMD_COSH, "cosh"}, {CMD_TANH, "tanh"}, {CMD_SIN, "sin"}, {CMD_COS, "cos"},
    {CMD_TAN, "tan"},
    {CMD_LN, "ln"}, {CMD_LOG, "log"}, {CMD_OPENP, "("}, {CMD_CLOSEP, ")"}, {CMD_FACT, "n!"},
    {CMD_SQR, "<i>x</i>²"}, {CMD_PWR, "<i>x</i>ʸ"}, {CMD_CUBE, "<i>x</i>³"}, {CMD_ROOT, "ʸ√<i>x</i>"},
    {CMD_CUBEROOT, "∛<i>x</i>"}, {CMD_POW10, "10ˣ"},
    {CMD_ROL, "RoL"}, {CMD_ROR, "RoR"}, {CMD_OR, "Or"}, {CMD_XOR, "Xor"}, {CMD_LSH, "Lsh"}, {CMD_RSH, "Rsh"},
    {CMD_NOT, "Not"}, {CMD_AND, "And"},
    {CMD_STAT_CAD, "CAD"}, {CMD_STAT_ADD, "Add"},
    {CMD_STAT_AVE, "<i>x̄</i>"}, {CMD_STAT_AVE2, "<i>x̄</i>²"}, {CMD_STAT_SUM, "∑<i>x</i>"},
    {CMD_STAT_SUM2, "∑<i>x</i>²"}, {CMD_STAT_SDEV, "σₙ"}, {CMD_STAT_SDEV1, "σₙ₋₁"},
    {130, "0"}, {131, "1"}, {132, "2"}, {133, "3"}, {134, "4"}, {135, "5"}, {136, "6"}, {137, "7"},
    {138, "8"}, {139, "9"}, {140, "A"}, {141, "B"}, {142, "C"}, {143, "D"}, {144, "E"}, {145, "F"},
};

/* Labels shown while Inv is checked (strings 700-709). */
static const Str INV_LABELS[] = {
    {CMD_PI, "2*π"}, {CMD_LN, "eˣ"}, {CMD_SINH, "sinh⁻¹"}, {CMD_SIN, "sin⁻¹"}, {CMD_COSH, "cosh⁻¹"},
    {CMD_COS, "cos⁻¹"}, {CMD_TANH, "tanh⁻¹"}, {CMD_TAN, "tan⁻¹"}, {CMD_DMS, "deg"}, {CMD_INT, "Frac"},
};

static const Str TOOLTIPS[] = {
    {CMD_MC, "Memory clear"}, {CMD_MR, "Memory recall"}, {CMD_MS, "Memory store"}, {CMD_MPLUS, "Memory add"},
    {CMD_MMINUS, "Memory subtract"}, {CMD_CE, "Clear entry"}, {CMD_CLEAR, "Clear"}, {CMD_BACK, "Backspace"},
    {CMD_SIGN, "Negate"}, {CMD_MUL, "Multiply"}, {CMD_DIV, "Divide"}, {CMD_ADD, "Add"}, {CMD_SUB, "Subtract"},
    {CMD_SQRT, "Square root"}, {CMD_RECIP, "Reciprocal"}, {CMD_INV, "Inverse functions"},
    {CMD_INT, "Integer part"},
    {CMD_DMS, "Degree minute second"}, {CMD_FACT, "Factorial"}, {CMD_MOD, "Modulo"}, {CMD_PI, "Pi"},
    {CMD_FE, "Exponential notation"}, {CMD_EXP, "Exponential"}, {CMD_SIN, "Sine"}, {CMD_COS, "Cosine"},
    {CMD_TAN, "Tangent"},
    {CMD_SINH, "Hyperbolic sine"}, {CMD_COSH, "Hyperbolic cosine"}, {CMD_TANH, "Hyperbolic tangent"},
    {CMD_SQR, "Square"}, {CMD_CUBE, "Cube"}, {CMD_PWR, "Raise to y power"}, {CMD_LN, "Natural log"},
    {CMD_LOG, "Log"},
    {CMD_CUBEROOT, "Cube root"}, {CMD_ROOT, "Order of y root"}, {CMD_POW10, "Anti log"},
    {CMD_ROL, "Rotate on left"}, {CMD_ROR, "Rotate on right"}, {CMD_OR, "Or"}, {CMD_XOR, "Exclusive or"},
    {CMD_LSH, "Left shift"}, {CMD_RSH, "Right shift"}, {CMD_NOT, "Not"}, {CMD_AND, "And"},
    {CMD_STAT_AVE, "Average"}, {CMD_STAT_AVE2, "Average of square"}, {CMD_STAT_SUM, "Sum"},
    {CMD_STAT_SUM2, "Sum of square"},
    {CMD_STAT_SDEV, "Standard deviation population"}, {CMD_STAT_SDEV1, "Standard deviation"},
    {CMD_STAT_CAD, "Clear all dataset values"}, {CMD_STAT_ADD, "Add dataset value"},
    {CMD_POINT, "Decimal separator"},
    {CMD_OPENP, "Open parenthesis"}, {CMD_CLOSEP, "Close parenthesis"}, {CMD_PERCENT, "Percentage"},
    {CMD_EQU, "Equals"},
};

static const char *lookup(const Str *t, int n, int id)
{
    for (int i = 0; i < n; i++)
        if (t[i].id == id)
            return t[i].text;
    return NULL;
}

const char *layout_label(int cid)
{
    const char *s = lookup(LABELS, N(LABELS), cid);
    return s ? s : "";
}

const char *layout_inv_label(int cid) { return lookup(INV_LABELS, N(INV_LABELS), cid); }
const char *layout_tooltip(int cid) { return lookup(TOOLTIPS, N(TOOLTIPS), cid); }
bool layout_is_toggle(int cid) { return cid == CMD_INV || cid == CMD_FE; }

bool item_contains(const Item *it, double px, double py)
{
    return it->x <= px && px < it->x + it->w && it->y <= py && py < it->y + it->h;
}

static int style_of(int cid)
{
    if ((cid >= CMD_DIGIT0 && cid <= CMD_DIGIT0 + 15) || cid == CMD_POINT)
        return STYLE_DIGIT;
    if (cid >= CMD_MC && cid <= CMD_MMINUS)
        return STYLE_MEM;
    return STYLE_OP;
}

static Item *add(Layout *l, ItemKind kind, int id, double x, double y, double w, double h)
{
    Item *it = &l->items[l->n_items++];
    it->kind = kind;
    it->id = id;
    it->x = x;
    it->y = y;
    it->w = w;
    it->h = h;
    it->style = STYLE_OP;
    it->text = "";
    return it;
}

static void buttons(Layout *l, const Cell *t, int n, int dx, int dy, double ox, double oy)
{
    for (int i = 0; i < n; i++) {
        const Cell *c = &t[i];
        Item *it = add(l, IT_BUTTON, c->id, ox + ux(c->x + dx), oy + uy(c->y + dy), ux(c->w), uh(c->h));
        it->style = style_of(c->id);
        it->text = layout_label(c->id);
    }
}

static void bit_items(Layout *l, double ox, double oy)
{
    for (int i = 0; i < 64; i++) {
        int row = i >= 32 ? 0 : 1;               /* bits 63..32 on the top row */
        int k = 31 - (i % 32);                   /* column from the left */
        int x = 15 + 15 * k + 36 * (k / 4);
        int y = row == 0 ? 8 : 49;
        add(l, IT_BIT, i, ox + x / 2.0, oy + uy(y), 15 / 2.0, uh(19));
    }
    static const struct { const char *text; int x, y; bool right; } lbl[] = {
        {"63", 15, 27, false}, {"47", 399, 27, false}, {"32", 717, 27, true},
        {"31", 15, 69, false}, {"15", 399, 69, false}, {"0", 717, 69, true},
    };
    for (int i = 0; i < N(lbl); i++) {
        Item *it = add(l, IT_BITLABEL, 0, ox + lbl[i].x / 2.0, oy + uy(lbl[i].y), 14, uh(21));
        it->text = lbl[i].text;
        it->style = lbl[i].right ? 1 : 0;
    }
}

void layout_build(Layout *l, CalcMode mode, bool history, bool panel)
{
    const int m = LAYOUT_MARGIN;
    int kw, dh, ky, kh;
    l->n_items = 0;
    l->has_bitpanel = false;
    if (mode == MODE_STAT) {
        kw = ux(380);
        dh = LAYOUT_STAT_DISPLAY_H;
        ky = m + dh + LAYOUT_GAP;
        buttons(l, STAT_T, N(STAT_T), 0, -248, m, ky);
        kh = uy(504 - 248) + uh(43);
    } else if (mode == MODE_STD) {
        kw = ux(380);
        dh = history ? LAYOUT_HISTORY_DISPLAY_H : LAYOUT_DISPLAY_H;
        ky = m + dh + LAYOUT_GAP;
        buttons(l, STD_T, N(STD_T), 0, 0, m, ky);
        kh = uy(256) + uh(43);
    } else if (mode == MODE_SCI) {
        kw = ux(770);
        dh = history ? LAYOUT_HISTORY_DISPLAY_H : LAYOUT_DISPLAY_H;
        ky = m + dh + LAYOUT_GAP;
        add(l, IT_BOX, 1079, m, ky, ux(380), uh(43));
        static const struct { int id, x, w; const char *text; } r[] = {
            {CMD_DEG, 15, 128, "Degrees"}, {CMD_RAD, 144, 132, "Radians"}, {CMD_GRAD, 277, 99, "Grads"},
        };
        for (int i = 0; i < N(r); i++)
            add(l, IT_RADIO, r[i].id, m + ux(r[i].x), ky + uy(8), ux(r[i].w), uh(26))->text = r[i].text;
        add(l, IT_PARENS, 402, m, ky + uy(51), ux(68), uh(43));
        buttons(l, SCI_LEFT, N(SCI_LEFT), 0, 0, m, ky);
        buttons(l, STD_T, N(STD_T), 390, 0, m, ky);
        kh = uy(256) + uh(43);
    } else {  /* programmer */
        kw = ux(770);
        dh = LAYOUT_DISPLAY_H;
        ky = m + dh + LAYOUT_GAP;
        l->has_bitpanel = true;
        l->bitpanel[0] = m;
        l->bitpanel[1] = ky;
        l->bitpanel[2] = ux(770);
        l->bitpanel[3] = uh(98);
        add(l, IT_BOX, 1077, m, ky, ux(770), uh(98));
        bit_items(l, m, ky);
        add(l, IT_BOX, 1079, m, ky + uy(106), ux(146), uh(146));
        add(l, IT_BOX, 1080, m, ky + uy(259), ux(146), uh(146));
        static const struct { int id; const char *text; } rx[] = {
            {CMD_HEX, "Hex"}, {CMD_DEC, "Dec"}, {CMD_OCT, "Oct"}, {CMD_BIN, "Bin"},
        }, wd[] = {
            {CMD_QWORD, "Qword"}, {CMD_DWORD, "Dword"}, {CMD_WORD, "Word"}, {CMD_BYTE, "Byte"},
        };
        for (int i = 0; i < 4; i++)
            add(l, IT_RADIO, rx[i].id, m + ux(15), ky + uy(118 + 32 * i), ux(120), uh(24))->text = rx[i].text;
        for (int i = 0; i < 4; i++)
            add(l, IT_RADIO, wd[i].id, m + ux(15), ky + uy(270 + 32 * i), ux(126), uh(24))->text = wd[i].text;
        add(l, IT_PARENS, 402, m + ux(156), ky + uy(106), ux(68), uh(43));
        buttons(l, PROG_LEFT, N(PROG_LEFT), 0, 0, m, ky);
        buttons(l, STD_T, N(STD_T), 390, 106, m, ky);
        kh = uy(361) + uh(43);
    }
    l->width = m + kw + m;
    l->height = ky + kh + m;
    l->display[0] = m;
    l->display[1] = m;
    l->display[2] = kw;
    l->display[3] = dh;
    l->has_panel = panel;
    if (panel) {
        l->panel[0] = l->width;
        l->panel[1] = m;
        l->panel[2] = LAYOUT_PANEL_W;
        l->panel[3] = LAYOUT_PANEL_H;
        l->width += LAYOUT_PANEL_W + m;
        if (l->height < m + LAYOUT_PANEL_H + m)
            l->height = m + LAYOUT_PANEL_H + m;
    }
}

/* Engine command ids by their Python constant names (win7calc.engine), for test hooks. */
int layout_cmd_by_name(const char *name)
{
    static const Str names[] = {
        {CMD_SIGN, "SIGN"}, {CMD_CLEAR, "CLEAR"}, {CMD_CE, "CE"}, {CMD_BACK, "BACK"}, {CMD_POINT, "POINT"},
        {CMD_AND, "AND"}, {CMD_OR, "OR"}, {CMD_XOR, "XOR"}, {CMD_LSH, "LSH"}, {CMD_RSH, "RSH"},
        {CMD_DIV, "DIV"}, {CMD_MUL, "MUL"}, {CMD_ADD, "ADD"}, {CMD_SUB, "SUB"}, {CMD_MOD, "MOD"},
        {CMD_ROOT, "ROOT"}, {CMD_PWR, "PWR"}, {CMD_INT, "INT"}, {CMD_ROL, "ROL"}, {CMD_ROR, "ROR"},
        {CMD_NOT, "NOT"}, {CMD_SIN, "SIN"}, {CMD_COS, "COS"}, {CMD_TAN, "TAN"}, {CMD_SINH, "SINH"},
        {CMD_COSH, "COSH"}, {CMD_TANH, "TANH"}, {CMD_LN, "LN"}, {CMD_LOG, "LOG"}, {CMD_SQRT, "SQRT"},
        {CMD_SQR, "SQR"}, {CMD_CUBE, "CUBE"}, {CMD_FACT, "FACT"}, {CMD_RECIP, "RECIP"}, {CMD_DMS, "DMS"},
        {CMD_CUBEROOT, "CUBEROOT"}, {CMD_POW10, "POW10"}, {CMD_PERCENT, "PERCENT"}, {CMD_FE, "FE"},
        {CMD_PI, "PI"}, {CMD_EQU, "EQU"}, {CMD_MC, "MC"}, {CMD_MR, "MR"}, {CMD_MS, "MS"},
        {CMD_MPLUS, "MPLUS"}, {CMD_MMINUS, "MMINUS"}, {CMD_EXP, "EXP"}, {CMD_OPENP, "OPENP"},
        {CMD_CLOSEP, "CLOSEP"}, {CMD_DIGIT0, "DIGIT0"}, {CMD_INV, "INV"}, {CMD_STAT_ADD, "STAT_ADD"},
        {CMD_STAT_AVE, "STAT_AVE"}, {CMD_STAT_AVE2, "STAT_AVE2"}, {CMD_STAT_SUM, "STAT_SUM"},
        {CMD_STAT_SUM2, "STAT_SUM2"}, {CMD_STAT_SDEV, "STAT_SDEV"}, {CMD_STAT_SDEV1, "STAT_SDEV1"},
        {CMD_STAT_CAD, "STAT_CAD"}, {CMD_HEX, "HEX"}, {CMD_DEC, "DEC"}, {CMD_OCT, "OCT"}, {CMD_BIN, "BIN"},
        {CMD_QWORD, "QWORD"}, {CMD_DWORD, "DWORD"}, {CMD_WORD, "WORD"}, {CMD_BYTE, "BYTE"},
        {CMD_DEG, "DEG"}, {CMD_RAD, "RAD"}, {CMD_GRAD, "GRAD"},
    };
    for (int i = 0; i < N(names); i++)
        if (strcmp(names[i].text, name) == 0)
            return names[i].id;
    return -1;
}

bool layout_mode_by_name(const char *name, CalcMode *out)
{
    static const char *const modes[] = {"std", "sci", "prog", "stat"};
    for (int i = 0; i < 4; i++)
        if (strcmp(name, modes[i]) == 0) {
            *out = (CalcMode)i;
            return true;
        }
    return false;
}

const char *layout_mode_name(CalcMode m)
{
    static const char *const modes[] = {"std", "sci", "prog", "stat"};
    return (unsigned)m < 4 ? modes[m] : "std";
}
