/* Calculator state machine replicating win32calc (Windows 7 Calculator).
 * C port of win7calc/engine.py -- keep the two in lock-step. */
#include "engine.h"

#include "mathfn.h"
#include "numfmt.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PARENS 25
#define ERR_NOT_DEFINED "Result not defined"

/* ------------------------------------------------------------------ values */

/* A calculator value: a decimal (d != NULL, Std/Sci/Stat) or a programmer-mode int. */
typedef struct {
    int64_t i;
    mpd_t *d;
} Val;

static Val vint(int64_t i) { return (Val){i, NULL}; }
static Val vdec(mpd_t *d) { return (Val){0, d}; }
static Val vcopy(Val v) { return v.d ? vdec(mf_dup(v.d)) : v; }
static void vfree(Val *v)
{
    mf_free(v->d);
    v->d = NULL;
    v->i = 0;
}
static void vset(Val *dst, Val src) /* takes ownership of src */
{
    vfree(dst);
    *dst = src;
}
static Val vdec_i(int64_t i)
{
    mpd_t *d = mf_new();
    mf_set_i64(d, i);
    return vdec(d);
}

typedef struct {
    Val lhs;
    int op;
} PendOp;

typedef struct {
    PendOp *ops;
    int nops, cap_ops;
    char **tokens;
    int ntok, cap_tok;
} Frame;

typedef struct {
    char digits[160];
    bool neg;
    bool has_exp;
    char exp[8];
    bool exp_neg;
} Entry;

enum { PREV_NONE, PREV_DIGIT, PREV_BINOP, PREV_EQU, PREV_OPEN, PREV_CLOSE, PREV_UNARY, PREV_MEM };

struct Engine {
    CalcMode mode;
    AngleUnit angle;
    int radix, bits;
    bool grouping, fe, inv;
    bool has_mem;
    Val memory;

    int nhist, cap_hist;
    char **hist_text, **hist_val;
    int ndata, cap_data;
    mpd_t **data;
    char **data_str;

    /* clear_all() state */
    Val value;
    bool has_entry;
    Entry entry;
    Frame *frames;
    int nframes, cap_frames;
    char *operand_expr;
    bool has_last_binop;
    int lb_op;
    Val lb_rhs;
    const char *error;
    int prev;
    bool has_last_applied;
    int la_op;
    Val la_rhs;

    char *disp_cache, *expr_cache;
};

/* ------------------------------------------------------------------ tables */

static bool is_binop(int c)
{
    return c == CMD_AND || c == CMD_OR || c == CMD_XOR || c == CMD_LSH || c == CMD_RSH ||
           c == CMD_DIV || c == CMD_MUL || c == CMD_ADD || c == CMD_SUB || c == CMD_MOD ||
           c == CMD_ROOT || c == CMD_PWR;
}

static const char *op_symbol(int op)
{
    switch (op) {
    case CMD_ADD: return "+";
    case CMD_SUB: return "-";
    case CMD_MUL: return "*";
    case CMD_DIV: return "/";
    case CMD_MOD: return "Mod";
    case CMD_PWR: return "^";
    case CMD_ROOT: return "yroot";
    case CMD_AND: return "And";
    case CMD_OR: return "Or";
    case CMD_XOR: return "Xor";
    case CMD_LSH: return "Lsh";
    case CMD_RSH: return "Rsh";
    }
    return "?";
}

static int precedence(int op)
{
    switch (op) {
    case CMD_OR: case CMD_XOR: return 0;
    case CMD_AND: return 1;
    case CMD_ADD: case CMD_SUB: return 2;
    case CMD_RSH: case CMD_LSH: case CMD_MOD: case CMD_DIV: case CMD_MUL: return 3;
    case CMD_PWR: case CMD_ROOT: return 4;
    }
    return 0;
}

static bool is_unary(int c)
{
    switch (c) {
    case CMD_SQRT: case CMD_SQR: case CMD_CUBE: case CMD_RECIP: case CMD_FACT: case CMD_LOG:
    case CMD_POW10: case CMD_CUBEROOT: case CMD_SINH: case CMD_COSH: case CMD_TANH: case CMD_LN:
    case CMD_INT: case CMD_DMS: case CMD_SIN: case CMD_COS: case CMD_TAN: case CMD_NOT:
    case CMD_ROL: case CMD_ROR:
        return true;
    }
    return false;
}

static bool inv_affected(int c)
{
    switch (c) {
    case CMD_SIN: case CMD_COS: case CMD_TAN: case CMD_SINH: case CMD_COSH: case CMD_TANH:
    case CMD_LN: case CMD_INT: case CMD_DMS: case CMD_PI:
        return true;
    }
    return false;
}

static bool is_stat_cmd(int c)
{
    return c == CMD_STAT_ADD || c == CMD_STAT_AVE || c == CMD_STAT_AVE2 || c == CMD_STAT_SUM ||
           c == CMD_STAT_SUM2 || c == CMD_STAT_SDEV || c == CMD_STAT_SDEV1;
}

/* ------------------------------------------------------------------ frames */

static void frame_free(Frame *f)
{
    for (int i = 0; i < f->nops; i++)
        vfree(&f->ops[i].lhs);
    free(f->ops);
    for (int i = 0; i < f->ntok; i++)
        free(f->tokens[i]);
    free(f->tokens);
    memset(f, 0, sizeof *f);
}

static void frame_push_op(Frame *f, Val lhs, int op)
{
    if (f->nops == f->cap_ops) {
        f->cap_ops = f->cap_ops ? f->cap_ops * 2 : 4;
        f->ops = realloc(f->ops, sizeof *f->ops * (size_t)f->cap_ops);
        if (!f->ops)
            abort();
    }
    f->ops[f->nops++] = (PendOp){lhs, op};
}

static void frame_push_tok(Frame *f, char *tok) /* takes ownership */
{
    if (f->ntok == f->cap_tok) {
        f->cap_tok = f->cap_tok ? f->cap_tok * 2 : 4;
        f->tokens = realloc(f->tokens, sizeof *f->tokens * (size_t)f->cap_tok);
        if (!f->tokens)
            abort();
    }
    f->tokens[f->ntok++] = tok;
}

static void frames_push(Engine *e)
{
    if (e->nframes == e->cap_frames) {
        e->cap_frames = e->cap_frames ? e->cap_frames * 2 : 4;
        e->frames = realloc(e->frames, sizeof *e->frames * (size_t)e->cap_frames);
        if (!e->frames)
            abort();
    }
    memset(&e->frames[e->nframes++], 0, sizeof(Frame));
}

static void frames_reset(Engine *e)
{
    for (int i = 0; i < e->nframes; i++)
        frame_free(&e->frames[i]);
    e->nframes = 0;
    frames_push(e);
}

/* ------------------------------------------------------------------ state helpers */

static bool int_mode(const Engine *e) { return e->mode == MODE_PROG; }
static int precision(const Engine *e) { return e->mode == MODE_STD ? 16 : 32; }
static Val zero(const Engine *e) { return int_mode(e) ? vint(0) : vdec_i(0); }

static void set_operand_expr(Engine *e, char *s) /* takes ownership */
{
    free(e->operand_expr);
    e->operand_expr = s;
}

static void clear_last_binop(Engine *e)
{
    if (e->has_last_binop)
        vfree(&e->lb_rhs);
    e->has_last_binop = false;
}

static void clear_last_applied(Engine *e)
{
    if (e->has_last_applied)
        vfree(&e->la_rhs);
    e->has_last_applied = false;
}

static void clear_all(Engine *e)
{
    vset(&e->value, zero(e));
    e->has_entry = false;
    frames_reset(e);
    set_operand_expr(e, NULL);
    clear_last_binop(e);
    e->error = NULL;
    e->prev = PREV_NONE;
    clear_last_applied(e);
}

static void set_error(Engine *e, const char *msg)
{
    clear_all(e);
    e->error = msg;
}

static Entry new_entry(void)
{
    Entry en;
    memset(&en, 0, sizeof en);
    strcpy(en.digits, "0");
    return en;
}

static int digit_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0;
}

/* int(digits, radix); false if it does not fit in 64 bits. */
static bool parse_radix(const char *s, int radix, uint64_t *out)
{
    uint64_t v = 0;
    bool ok = true;
    for (; *s; s++) {
        uint64_t d = (uint64_t)digit_value(*s);
        if (v > (UINT64_MAX - d) / (uint64_t)radix)
            ok = false;
        v = v * (uint64_t)radix + d;
    }
    *out = v;
    return ok;
}

static const char *entry_value(Engine *e, const Entry *en, Val *out)
{
    if (int_mode(e)) {
        uint64_t u;
        parse_radix(en->digits, e->radix, &u); /* entries never exceed the word size */
        *out = vint(mf_wrap((int64_t)u, e->bits));
        return NULL;
    }
    StrBuf b;
    sb_init(&b);
    if (en->neg)
        sb_putc(&b, '-');
    size_t n = strlen(en->digits);
    while (n && en->digits[n - 1] == '.')
        n--;
    sb_putn(&b, en->digits, n);
    if (en->has_exp && en->exp[0]) {
        sb_putc(&b, 'e');
        if (en->exp_neg)
            sb_putc(&b, '-');
        sb_puts(&b, en->exp);
    }
    char *s = sb_take(&b);
    mpd_t *d = mf_new(), *r = mf_new();
    mf_set_str(d, s);
    free(s);
    const char *err = mf_check(r, d);
    mpd_del(d);
    if (err) {
        mpd_del(r);
        return err;
    }
    *out = vdec(r);
    return NULL;
}

/* current(): the value shown, committing nothing.  *out is a new Val. */
static const char *current(Engine *e, Val *out)
{
    if (e->has_entry)
        return entry_value(e, &e->entry, out);
    *out = vcopy(e->value);
    return NULL;
}

static const char *commit(Engine *e)
{
    if (e->has_entry) {
        Val v;
        const char *err = entry_value(e, &e->entry, &v);
        if (err)
            return err;
        vset(&e->value, v);
        e->has_entry = false;
    }
    return NULL;
}

static char *fmt(Engine *e, Val v, int grouping)
{
    bool g = grouping < 0 ? e->grouping : grouping != 0;
    if (int_mode(e)) {
        int64_t i = v.d ? mf_to_wrapped_int(v.d, 64) : v.i;
        return nf_format_int(i, e->radix, e->bits, g);
    }
    if (!v.d) {
        mpd_t *d = mf_new();
        mf_set_i64(d, v.i);
        char *s = nf_format_decimal(d, precision(e), g, e->fe);
        mpd_del(d);
        return s;
    }
    return nf_format_decimal(v.d, precision(e), g, e->fe);
}

static char *operand_text(Engine *e, Val x)
{
    return e->operand_expr ? xstrdup(e->operand_expr) : fmt(e, x, -1);
}

static char *val_str(Val v)
{
    if (!v.d)
        return xasprintf("%lld", (long long)v.i);
    return mf_to_str(v.d);
}

/* ------------------------------------------------------------------ public views */

Engine *engine_new(void)
{
    Engine *e = calloc(1, sizeof *e);
    if (!e)
        abort();
    e->mode = MODE_STD;
    e->angle = ANGLE_DEG;
    e->radix = 10;
    e->bits = 64;
    e->value = vdec_i(0);
    clear_all(e);
    return e;
}

void engine_free(Engine *e)
{
    if (!e)
        return;
    clear_all(e);
    for (int i = 0; i < e->nframes; i++)
        frame_free(&e->frames[i]);
    free(e->frames);
    vfree(&e->value);
    if (e->has_mem)
        vfree(&e->memory);
    engine_clear_history(e);
    free(e->hist_text);
    free(e->hist_val);
    for (int i = 0; i < e->ndata; i++) {
        mpd_del(e->data[i]);
        free(e->data_str[i]);
    }
    free(e->data);
    free(e->data_str);
    free(e->disp_cache);
    free(e->expr_cache);
    free(e);
}

static char *display_str(Engine *e)
{
    if (e->error)
        return xstrdup(e->error);
    if (e->has_entry) {
        const Entry *en = &e->entry;
        if (int_mode(e)) {
            char d[sizeof en->digits];
            strcpy(d, en->digits);
            for (char *p = d; *p; p++)
                *p = (char)toupper((unsigned char)*p);
            if (e->grouping) {
                int size;
                char sep;
                nf_radix_group(e->radix, &size, &sep);
                return nf_group_int(d, size, sep);
            }
            return xstrdup(d);
        }
        return nf_format_entry(en->digits, en->neg, en->has_exp ? en->exp : NULL, en->exp_neg,
                               e->grouping, 3, ',');
    }
    return fmt(e, e->value, -1);
}

const char *engine_display(Engine *e)
{
    free(e->disp_cache);
    e->disp_cache = display_str(e);
    return e->disp_cache;
}

static char *expression_str(const Engine *e)
{
    StrBuf b;
    sb_init(&b);
    bool first = true;
#define PART(s)                                                                                    \
    do {                                                                                           \
        if (!first)                                                                                \
            sb_putc(&b, ' ');                                                                      \
        sb_puts(&b, (s));                                                                          \
        first = false;                                                                             \
    } while (0)
    for (int i = 0; i < e->nframes; i++) {
        if (i)
            PART("(");
        for (int j = 0; j < e->frames[i].ntok; j++)
            PART(e->frames[i].tokens[j]);
    }
    if (e->operand_expr)
        PART(e->operand_expr);
#undef PART
    char *s = sb_take(&b);
    /* .replace("( ", "(") */
    size_t k = 0;
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == '(' && s[i + 1] == ' ') {
            s[k++] = '(';
            i++;
        } else {
            s[k++] = s[i];
        }
    }
    s[k] = 0;
    return s;
}

const char *engine_expression(Engine *e)
{
    free(e->expr_cache);
    e->expr_cache = expression_str(e);
    return e->expr_cache;
}

const char *engine_error(const Engine *e) { return e->error; }
CalcMode engine_mode(const Engine *e) { return e->mode; }
bool engine_int_mode(const Engine *e) { return int_mode(e); }
int engine_radix(const Engine *e) { return e->radix; }
int engine_bits(const Engine *e) { return e->bits; }
AngleUnit engine_angle(const Engine *e) { return e->angle; }
bool engine_inv(const Engine *e) { return e->inv; }
bool engine_fe(const Engine *e) { return e->fe; }
bool engine_grouping(const Engine *e) { return e->grouping; }
void engine_set_grouping(Engine *e, bool on) { e->grouping = on; }
int engine_paren_count(const Engine *e) { return e->nframes - 1; }
bool engine_has_memory(const Engine *e) { return e->has_mem; }

uint64_t engine_unsigned_value(Engine *e)
{
    Val v;
    if (current(e, &v))
        v = vint(0);
    uint64_t r = 0;
    if (int_mode(e)) {
        int64_t i = v.d ? mf_to_wrapped_int(v.d, 64) : v.i;
        uint64_t mask = e->bits >= 64 ? ~(uint64_t)0 : (((uint64_t)1 << e->bits) - 1);
        r = (uint64_t)i & mask;
    }
    vfree(&v);
    return r;
}

bool engine_is_enabled(const Engine *e, int cid)
{
    if (e->mode == MODE_PROG) {
        if (cid == CMD_POINT || cid == CMD_FE || cid == CMD_EXP || cid == CMD_SQRT ||
            cid == CMD_PERCENT || cid == CMD_RECIP)
            return false;
        if (cid >= CMD_DIGIT0 && cid <= CMD_DIGIT0 + 15)
            return cid - CMD_DIGIT0 < e->radix;
    } else if (cid >= CMD_DIGIT0 + 10 && cid <= CMD_DIGIT0 + 15) {
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ number entry */

static void start_entry(Engine *e)
{
    if (!e->has_entry) {
        set_operand_expr(e, NULL);
        e->entry = new_entry();
        e->has_entry = true;
        e->prev = PREV_DIGIT;
    }
}

static void do_digit(Engine *e, int d)
{
    start_entry(e);
    Entry *en = &e->entry;
    if (en->has_exp) {
        size_t n = strlen(en->exp);
        if (n < 4) {
            char t[8];
            memcpy(t, en->exp, n);
            t[n] = (char)('0' + d);
            t[n + 1] = 0;
            const char *p = t;
            while (*p == '0')
                p++;
            strcpy(en->exp, p);
        }
        return;
    }
    char ch = "0123456789ABCDEF"[d];
    char nw[sizeof en->digits + 2];
    if (strcmp(en->digits, "0") == 0) {
        nw[0] = ch;
        nw[1] = 0;
    } else {
        size_t n = strlen(en->digits);
        if (n + 2 > sizeof en->digits)
            return;
        memcpy(nw, en->digits, n);
        nw[n] = ch;
        nw[n + 1] = 0;
    }
    if (int_mode(e)) {
        uint64_t v;
        if (!parse_radix(nw, e->radix, &v) || (e->bits < 64 && v >> e->bits))
            return;
    } else {
        int count = 0;
        if (strncmp(nw, "0.", 2) == 0) {
            /* len(new.replace(".", "").lstrip("0")) */
            bool lead = true;
            for (const char *p = nw; *p; p++) {
                if (*p == '.')
                    continue;
                if (lead && *p == '0')
                    continue;
                lead = false;
                count++;
            }
        } else {
            for (const char *p = nw; *p; p++)
                if (*p != '.')
                    count++;
        }
        if (count > precision(e))
            return;
    }
    strcpy(en->digits, nw);
}

static void do_point(Engine *e)
{
    start_entry(e);
    Entry *en = &e->entry;
    if (!strchr(en->digits, '.') && !en->has_exp) {
        size_t n = strlen(en->digits);
        if (n + 2 <= sizeof en->digits) {
            en->digits[n] = '.';
            en->digits[n + 1] = 0;
        }
    }
}

static void do_exp(Engine *e)
{
    start_entry(e);
    if (!e->entry.has_exp) {
        e->entry.has_exp = true;
        e->entry.exp[0] = 0;
    }
}

static void do_backspace(Engine *e)
{
    if (!e->has_entry)
        return;
    Entry *en = &e->entry;
    if (en->has_exp) {
        size_t n = strlen(en->exp);
        if (n) {
            en->exp[n - 1] = 0;
        } else {
            en->has_exp = false;
            en->exp_neg = false;
        }
    } else if (strlen(en->digits) > 1) {
        en->digits[strlen(en->digits) - 1] = 0;
    } else {
        strcpy(en->digits, "0");
        en->neg = false;
    }
}

static void finish_unary(Engine *e, const char *name, Val x, Val result)
{
    if (name) {
        char *t = operand_text(e, x);
        set_operand_expr(e, xasprintf("%s(%s)", name, t));
        free(t);
    } else {
        set_operand_expr(e, NULL);
    }
    vset(&e->value, result);
    e->has_entry = false;
    e->prev = PREV_UNARY;
}

static const char *do_sign(Engine *e)
{
    if (e->has_entry && !int_mode(e)) {
        Entry *en = &e->entry;
        if (en->has_exp)
            en->exp_neg = !en->exp_neg;
        else if (strcmp(en->digits, "0") != 0)
            en->neg = !en->neg;
        return NULL;
    }
    const char *err = commit(e);
    if (err)
        return err;
    Val x = vcopy(e->value), r;
    if (int_mode(e)) {
        r = vint(mf_wrap((int64_t)((uint64_t)0 - (uint64_t)x.i), e->bits));
    } else {
        mpd_t *d = mf_new();
        mf_negate(d, x.d, MF_WORK_PREC);
        r = vdec(d);
    }
    finish_unary(e, "negate", x, r);
    vfree(&x);
    return NULL;
}

/* ------------------------------------------------------------------ binary ops */

static const char *apply_int(Engine *e, int64_t a, int op, int64_t b, Val *out)
{
    int bits = e->bits;
    uint64_t mask = bits >= 64 ? ~(uint64_t)0 : (((uint64_t)1 << bits) - 1);
    uint64_t ua = (uint64_t)a, ub = (uint64_t)b, r;
    if ((op == CMD_DIV || op == CMD_MOD) && b == 0)
        return a == 0 ? MF_ERR_UNDEFINED : MF_ERR_DIV0;
    uint64_t abs_a = a < 0 ? (uint64_t)0 - ua : ua, abs_b = b < 0 ? (uint64_t)0 - ub : ub;
    switch (op) {
    case CMD_ADD: r = ua + ub; break;
    case CMD_SUB: r = ua - ub; break;
    case CMD_MUL: r = ua * ub; break;
    case CMD_DIV:
        r = abs_a / abs_b;
        if ((a < 0) != (b < 0))
            r = (uint64_t)0 - r;
        break;
    case CMD_MOD:
        r = abs_a % abs_b;
        if (a < 0)
            r = (uint64_t)0 - r;
        break;
    case CMD_AND: r = (ua & mask) & (ub & mask); break;
    case CMD_OR: r = (ua & mask) | (ub & mask); break;
    case CMD_XOR: r = (ua & mask) ^ (ub & mask); break;
    case CMD_LSH:
    case CMD_RSH:
        if (b < 0)
            return MF_ERR_INVALID;
        if (op == CMD_LSH) {
            r = b < bits ? ua << b : 0;
        } else {
            int64_t s = b < bits ? b : bits;
            if (s >= 64)
                r = a < 0 ? ~(uint64_t)0 : 0;
            else
                r = (uint64_t)(a >> s);
        }
        break;
    default:
        return MF_ERR_INVALID;
    }
    *out = vint(mf_wrap((int64_t)r, bits));
    return NULL;
}

static const char *apply(Engine *e, Val lhs, int op, Val rhs, Val *out)
{
    clear_last_applied(e);
    e->has_last_applied = true;
    e->la_op = op;
    e->la_rhs = vcopy(rhs);
    if (int_mode(e))
        return apply_int(e, lhs.d ? mf_to_wrapped_int(lhs.d, 64) : lhs.i, op,
                         rhs.d ? mf_to_wrapped_int(rhs.d, 64) : rhs.i, out);
    const char *(*f)(mpd_t *, const mpd_t *, const mpd_t *) = NULL;
    switch (op) {
    case CMD_ADD: f = mf_add; break;
    case CMD_SUB: f = mf_sub; break;
    case CMD_MUL: f = mf_mul; break;
    case CMD_DIV: f = mf_div; break;
    case CMD_MOD: f = mf_mod; break;
    case CMD_PWR: f = mf_power; break;
    case CMD_ROOT: f = mf_root; break;
    default: return MF_ERR_INVALID;
    }
    mpd_t *r = mf_new();
    const char *err = f(r, lhs.d, rhs.d);
    if (err) {
        mpd_del(r);
        return err;
    }
    *out = vdec(r);
    return NULL;
}

/* Apply pending ops in `f` binding at least as tightly as `op` (all if op < 0).
 * Consumes *x and replaces it with the result (also on error, *x stays valid). */
static const char *resolve(Engine *e, Frame *f, Val *x, int op)
{
    bool prec = e->mode != MODE_STD;
    while (f->nops) {
        PendOp *top = &f->ops[f->nops - 1];
        if (op >= 0 && prec && precedence(top->op) < precedence(op))
            break;
        PendOp po = *top;
        f->nops--;
        Val r;
        const char *err = apply(e, po.lhs, po.op, *x, &r);
        vfree(&po.lhs);
        if (err)
            return err;
        vset(x, r);
    }
    return NULL;
}

static const char *do_binop(Engine *e, int op)
{
    if (e->mode == MODE_STAT)
        return NULL;
    Frame *f = &e->frames[e->nframes - 1];
    Val x;
    if (e->prev == PREV_BINOP && f->nops) {
        PendOp po = f->ops[--f->nops];
        x = po.lhs;
        free(f->tokens[f->ntok - 1]);
        f->tokens[f->ntok - 1] = xstrdup(op_symbol(op));
    } else {
        const char *err = commit(e);
        if (err)
            return err;
        x = vcopy(e->value);
        frame_push_tok(f, operand_text(e, x));
        frame_push_tok(f, xstrdup(op_symbol(op)));
    }
    const char *err = resolve(e, f, &x, op);
    if (err) {
        vfree(&x);
        return err;
    }
    frame_push_op(f, vcopy(x), op);
    vset(&e->value, x);
    set_operand_expr(e, NULL);
    clear_last_binop(e);
    e->prev = PREV_BINOP;
    return NULL;
}

static void add_history(Engine *e, const char *text, Val v)
{
    if (e->mode != MODE_STD && e->mode != MODE_SCI)
        return;
    if (e->nhist == e->cap_hist) {
        e->cap_hist = e->cap_hist ? e->cap_hist * 2 : 8;
        e->hist_text = realloc(e->hist_text, sizeof(char *) * (size_t)e->cap_hist);
        e->hist_val = realloc(e->hist_val, sizeof(char *) * (size_t)e->cap_hist);
        if (!e->hist_text || !e->hist_val)
            abort();
    }
    e->hist_text[e->nhist] = xstrdup(text);
    e->hist_val[e->nhist] = val_str(v);
    e->nhist++;
}

static char *strip_dup(const char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1]))
        n--;
    char *r = strndup(s, n);
    if (!r)
        abort();
    return r;
}

static const char *do_equals(Engine *e)
{
    if (e->mode == MODE_STAT)
        return NULL;
    const char *err = commit(e);
    if (err)
        return err;
    Val x = vcopy(e->value);
    bool pending = e->nframes > 1 || e->frames[0].nops;
    if (pending) {
        StrBuf b;
        sb_init(&b);
        char *ex = expression_str(e);
        sb_puts(&b, ex);
        free(ex);
        if (!e->operand_expr) {
            char *t = operand_text(e, x);
            sb_putc(&b, ' ');
            sb_puts(&b, t);
            free(t);
        }
        for (int i = 0; i < e->nframes - 1; i++)
            sb_putc(&b, ')');
        char *text = sb_take(&b);
        while (e->nframes > 1) {
            Frame *f = &e->frames[e->nframes - 1];
            err = resolve(e, f, &x, -1);
            frame_free(f);
            e->nframes--;
            if (err)
                break;
        }
        if (!err)
            err = resolve(e, &e->frames[0], &x, -1);
        if (err) {
            free(text);
            vfree(&x);
            return err;
        }
        clear_last_binop(e);
        if (e->has_last_applied) {
            e->has_last_binop = true;
            e->lb_op = e->la_op;
            e->lb_rhs = vcopy(e->la_rhs);
        }
        char *st = strip_dup(text);
        add_history(e, st, x);
        free(st);
        free(text);
    } else if (e->has_last_binop) {
        int op = e->lb_op;
        Val rhs = vcopy(e->lb_rhs);
        char *ot = operand_text(e, x), *rt = fmt(e, rhs, -1);
        char *text = xasprintf("%s %s %s", ot, op_symbol(op), rt);
        free(ot);
        free(rt);
        Val r;
        err = apply(e, x, op, rhs, &r);
        vfree(&rhs);
        if (err) {
            free(text);
            vfree(&x);
            return err;
        }
        vset(&x, r);
        add_history(e, text, x);
        free(text);
    }
    frames_reset(e);
    vset(&e->value, x);
    set_operand_expr(e, NULL);
    e->prev = PREV_EQU;
    return NULL;
}

/* ------------------------------------------------------------------ parentheses */

static void do_open_paren(Engine *e)
{
    if (e->mode == MODE_STD || engine_paren_count(e) >= MAX_PARENS)
        return;
    if (e->prev != PREV_BINOP && e->prev != PREV_OPEN && e->prev != PREV_NONE) {
        e->has_entry = false;
        set_operand_expr(e, NULL);
    }
    frames_push(e);
    e->prev = PREV_OPEN;
}

static const char *do_close_paren(Engine *e)
{
    if (e->mode == MODE_STD || engine_paren_count(e) == 0)
        return NULL;
    const char *err = commit(e);
    if (err)
        return err;
    Val x = vcopy(e->value);
    Frame inner = e->frames[--e->nframes];
    StrBuf b;
    sb_init(&b);
    sb_putc(&b, '(');
    for (int i = 0; i < inner.ntok; i++) {
        sb_puts(&b, inner.tokens[i]);
        sb_putc(&b, ' ');
    }
    char *t = operand_text(e, x);
    sb_puts(&b, t);
    free(t);
    sb_putc(&b, ')');
    char *text = sb_take(&b);
    err = resolve(e, &inner, &x, -1);
    frame_free(&inner);
    if (err) {
        free(text);
        vfree(&x);
        return err;
    }
    vset(&e->value, x);
    set_operand_expr(e, text);
    e->prev = PREV_CLOSE;
    return NULL;
}

/* ------------------------------------------------------------------ unary ops */

static const char *angle_sfx(AngleUnit a) { return a == ANGLE_DEG ? "d" : a == ANGLE_RAD ? "r" : "g"; }
static MfAngle mf_angle(AngleUnit a) { return a == ANGLE_DEG ? MF_DEG : a == ANGLE_RAD ? MF_RAD : MF_GRAD; }

static const char *unary_int(Engine *e, int cid, int64_t x, const char **name, Val *out)
{
    int b = e->bits;
    uint64_t mask = b >= 64 ? ~(uint64_t)0 : (((uint64_t)1 << b) - 1);
    uint64_t ux = (uint64_t)x;
    switch (cid) {
    case CMD_NOT:
        *name = "Not";
        *out = vint(mf_wrap((int64_t)~ux, b));
        return NULL;
    case CMD_ROL: {
        *name = "RoL";
        uint64_t sb = (uint64_t)(x >> (b - 1)) & 1;
        *out = vint(mf_wrap((int64_t)(((ux << 1) | sb) & mask), b));
        return NULL;
    }
    case CMD_ROR:
        *name = "RoR";
        *out = vint(mf_wrap((int64_t)(((ux & mask) >> 1) | ((ux & 1) << (b - 1))), b));
        return NULL;
    case CMD_SQR:
        *name = "sqr";
        *out = vint(mf_wrap((int64_t)(ux * ux), b));
        return NULL;
    case CMD_CUBE:
        *name = "cube";
        *out = vint(mf_wrap((int64_t)(ux * ux * ux), b));
        return NULL;
    case CMD_FACT: {
        *name = "fact";
        mpd_t *d = mf_new(), *r = mf_new();
        mf_set_i64(d, x);
        const char *err = mf_factorial(r, d);
        if (!err)
            *out = vint(mf_to_wrapped_int(r, b));
        mpd_del(d);
        mpd_del(r);
        return err;
    }
    }
    return MF_ERR_INVALID;
}

static const char *unary_dec(Engine *e, int cid, bool inv, const mpd_t *x, char **name, mpd_t *r)
{
    MfAngle a = mf_angle(e->angle);
    const char *sfx = angle_sfx(e->angle);
    switch (cid) {
    case CMD_SQRT: *name = xstrdup("sqrt"); return mf_sqrt(r, x);
    case CMD_SQR: *name = xstrdup("sqr"); return mf_square(r, x);
    case CMD_CUBE: *name = xstrdup("cube"); return mf_cube(r, x);
    case CMD_RECIP: *name = xstrdup("reciproc"); return mf_reciprocal(r, x);
    case CMD_FACT: *name = xstrdup("fact"); return mf_factorial(r, x);
    case CMD_LOG: *name = xstrdup("log"); return mf_log10(r, x);
    case CMD_POW10: *name = xstrdup("powten"); return mf_pow10(r, x);
    case CMD_CUBEROOT: *name = xstrdup("cuberoot"); return mf_cbrt(r, x);
    case CMD_SINH:
        *name = xstrdup(inv ? "asinh" : "sinh");
        return inv ? mf_asinh(r, x) : mf_sinh(r, x);
    case CMD_COSH:
        *name = xstrdup(inv ? "acosh" : "cosh");
        return inv ? mf_acosh(r, x) : mf_cosh(r, x);
    case CMD_TANH:
        *name = xstrdup(inv ? "atanh" : "tanh");
        return inv ? mf_atanh(r, x) : mf_tanh(r, x);
    case CMD_LN:
        *name = xstrdup(inv ? "powe" : "ln");
        return inv ? mf_exp(r, x) : mf_ln(r, x);
    case CMD_INT:
        *name = xstrdup(inv ? "frac" : "Int");
        return inv ? mf_frac_part(r, x) : mf_int_part(r, x, MF_WORK_PREC);
    case CMD_DMS:
        *name = xstrdup(inv ? "degrees" : "dms");
        return inv ? mf_degrees(r, x) : mf_dms(r, x);
    case CMD_SIN:
        *name = xasprintf("%s%s", inv ? "asin" : "sin", sfx);
        return inv ? mf_asin(r, x, a) : mf_sin(r, x, a);
    case CMD_COS:
        *name = xasprintf("%s%s", inv ? "acos" : "cos", sfx);
        return inv ? mf_acos(r, x, a) : mf_cos(r, x, a);
    case CMD_TAN:
        *name = xasprintf("%s%s", inv ? "atan" : "tan", sfx);
        return inv ? mf_atan(r, x, a) : mf_tan(r, x, a);
    }
    return MF_ERR_INVALID;
}

static const char *do_unary(Engine *e, int cid)
{
    const char *err = commit(e);
    if (err)
        return err;
    Val x = vcopy(e->value), result;
    bool inv = e->inv;
    if (int_mode(e)) {
        const char *name = NULL;
        err = unary_int(e, cid, x.d ? mf_to_wrapped_int(x.d, 64) : x.i, &name, &result);
        if (err) {
            vfree(&x);
            return err;
        }
        if (inv_affected(cid))
            e->inv = false;
        finish_unary(e, name, x, result);
    } else {
        char *name = NULL;
        mpd_t *r = mf_new();
        err = unary_dec(e, cid, inv, x.d, &name, r);
        if (err) {
            free(name);
            mpd_del(r);
            vfree(&x);
            return err;
        }
        if (inv_affected(cid))
            e->inv = false;
        finish_unary(e, name, x, vdec(r));
        free(name);
    }
    vfree(&x);
    return NULL;
}

static const char *do_percent(Engine *e)
{
    if (int_mode(e))
        return NULL;
    const char *err = commit(e);
    if (err)
        return err;
    Val x = vcopy(e->value);
    Frame *f = &e->frames[e->nframes - 1];
    mpd_t *r = mf_new();
    if (f->nops) {
        mpd_t *t = mf_new(), *h = mf_new();
        mf_set_i64(h, 100);
        err = mf_mul(t, f->ops[f->nops - 1].lhs.d, x.d);
        if (!err)
            err = mf_div(r, t, h);
        mpd_del(t);
        mpd_del(h);
    } else {
        mf_set_i64(r, 0);
    }
    if (err) {
        mpd_del(r);
        vfree(&x);
        return err;
    }
    finish_unary(e, NULL, x, vdec(r));
    vfree(&x);
    return NULL;
}

static const char *do_pi(Engine *e)
{
    if (int_mode(e))
        return NULL;
    const char *err = commit(e);
    if (err)
        return err;
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    uint32_t st = 0;
    mpd_t *r = mf_new();
    if (e->inv) {
        mpd_t *two = mf_new();
        mf_set_i64(two, 2);
        mpd_qmul(r, mf_pi(), two, &c, &st);
        mpd_qplus(r, r, &c, &st);
        mpd_del(two);
    } else {
        mpd_qplus(r, mf_pi(), &c, &st);
    }
    e->inv = false;
    finish_unary(e, NULL, vint(0), vdec(r));
    return NULL;
}

/* ------------------------------------------------------------------ memory */

static const char *do_memory(Engine *e, int cid)
{
    if (cid == CMD_MR) {
        if (!e->has_mem)
            return NULL;
        e->has_entry = false;
        set_operand_expr(e, NULL);
        vset(&e->value, vcopy(e->memory));
        e->prev = PREV_MEM;
        return NULL;
    }
    const char *err = commit(e);
    if (err)
        return err;
    Val x = e->value;
    if (cid == CMD_MS) {
        if (e->has_mem)
            vfree(&e->memory);
        e->memory = vcopy(x);
        e->has_mem = true;
    } else if (int_mode(e)) {
        uint64_t m = e->has_mem ? (uint64_t)(e->memory.d ? mf_to_wrapped_int(e->memory.d, 64)
                                                         : e->memory.i)
                                : 0;
        uint64_t xv = (uint64_t)(x.d ? mf_to_wrapped_int(x.d, 64) : x.i);
        uint64_t r = cid == CMD_MPLUS ? m + xv : m - xv;
        if (e->has_mem)
            vfree(&e->memory);
        e->memory = vint(mf_wrap((int64_t)r, e->bits));
        e->has_mem = true;
    } else {
        mpd_t *z = NULL;
        const mpd_t *m;
        if (e->has_mem) {
            m = e->memory.d;
        } else {
            z = mf_new();
            mf_set_i64(z, 0);
            m = z;
        }
        mpd_t *r = mf_new();
        err = cid == CMD_MPLUS ? mf_add(r, m, x.d) : mf_sub(r, m, x.d);
        mf_free(z);
        if (err) {
            mpd_del(r);
            return err;
        }
        if (e->has_mem)
            vfree(&e->memory);
        e->memory = vdec(r);
        e->has_mem = true;
    }
    e->prev = PREV_MEM;
    return NULL;
}

/* ------------------------------------------------------------------ statistics */

static void stat_clear(Engine *e)
{
    e->has_entry = false;
    vset(&e->value, vdec_i(0));
    e->error = NULL;
    e->prev = PREV_NONE;
}

static const char *do_stat(Engine *e, int cid)
{
    if (e->mode != MODE_STAT)
        return NULL;
    const char *err = commit(e);
    if (err)
        return err;
    if (cid == CMD_STAT_ADD) {
        if (e->ndata == e->cap_data) {
            e->cap_data = e->cap_data ? e->cap_data * 2 : 8;
            e->data = realloc(e->data, sizeof(mpd_t *) * (size_t)e->cap_data);
            e->data_str = realloc(e->data_str, sizeof(char *) * (size_t)e->cap_data);
            if (!e->data || !e->data_str)
                abort();
        }
        mpd_t *d = mf_dup(e->value.d);
        e->data[e->ndata] = d;
        e->data_str[e->ndata] = mf_to_str(d);
        e->ndata++;
        e->prev = PREV_MEM;
        return NULL;
    }
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    uint32_t st = 0;
    int n = e->ndata;
    mpd_t *s = mf_new(), *s2 = mf_new(), *t = mf_new(), *r = mf_new(), *nd = mf_new();
    mf_set_i64(s, 0);
    mf_set_i64(s2, 0);
    for (int i = 0; i < n; i++)
        mpd_qadd(s, s, e->data[i], &c, &st);
    for (int i = 0; i < n; i++) {
        mpd_qmul(t, e->data[i], e->data[i], &c, &st);
        mpd_qadd(s2, s2, t, &c, &st);
    }
    mf_set_i64(nd, n);
    if (cid == CMD_STAT_SUM) {
        mf_copy(r, s);
    } else if (cid == CMD_STAT_SUM2) {
        mf_copy(r, s2);
    } else if (n == 0) {
        err = ERR_NOT_DEFINED;
    } else if (cid == CMD_STAT_AVE) {
        mpd_qdiv(r, s, nd, &c, &st);
    } else if (cid == CMD_STAT_AVE2) {
        mpd_qdiv(r, s2, nd, &c, &st);
    } else {
        int d = n - (cid == CMD_STAT_SDEV1 ? 1 : 0);
        if (d == 0) {
            err = ERR_NOT_DEFINED;
        } else {
            mpd_t *dd = mf_new();
            mf_set_i64(dd, d);
            mpd_qmul(t, s, s, &c, &st);
            mpd_qdiv(t, t, nd, &c, &st);
            mpd_qsub(t, s2, t, &c, &st);
            mpd_qdiv(t, t, dd, &c, &st);
            mpd_del(dd);
            if (mpd_ispositive(t) && !mpd_iszero(t))
                mpd_qsqrt(r, t, &c, &st);
            else
                mf_set_i64(r, 0);
        }
    }
    if (!err && (st & MF_TRAPS))
        err = MF_ERR_INVALID;
    mpd_t *res = NULL;
    if (!err) {
        res = mf_new();
        err = mf_check(res, r);
    }
    mpd_del(s);
    mpd_del(s2);
    mpd_del(t);
    mpd_del(r);
    mpd_del(nd);
    if (err) {
        mf_free(res);
        return err;
    }
    finish_unary(e, NULL, vint(0), vdec(res));
    return NULL;
}

/* ------------------------------------------------------------------ modes */

void engine_set_mode(Engine *e, CalcMode m)
{
    if (m == e->mode)
        return;
    Val v;
    if (current(e, &v))
        v = int_mode(e) ? vint(0) : vdec_i(0);
    if (e->error) {
        vfree(&v);
        v = int_mode(e) ? vint(0) : vdec_i(0);
    }
    CalcMode old = e->mode;
    e->mode = m;
    clear_all(e);
    if (m == MODE_PROG) {
        Val nv = vint(v.d ? mf_to_wrapped_int(v.d, e->bits) : mf_wrap(v.i, e->bits));
        vset(&e->value, nv);
        if (e->has_mem) {
            Val mm = vint(e->memory.d ? mf_to_wrapped_int(e->memory.d, e->bits)
                                      : mf_wrap(e->memory.i, e->bits));
            vset(&e->memory, mm);
        }
        vfree(&v);
    } else {
        if (old == MODE_PROG) {
            vset(&e->value, v.d ? vcopy(v) : vdec_i(v.i));
            if (e->has_mem && !e->memory.d)
                vset(&e->memory, vdec_i(e->memory.i));
            vfree(&v);
        } else {
            vset(&e->value, v);
        }
    }
    e->inv = false;
}

static const char *set_radix(Engine *e, int radix)
{
    if (!int_mode(e))
        return NULL;
    const char *err = commit(e);
    if (err)
        return err;
    e->radix = radix;
    if (e->prev != PREV_UNARY && e->prev != PREV_CLOSE)
        set_operand_expr(e, NULL);
    return NULL;
}

static const char *set_bits(Engine *e, int bits)
{
    if (!int_mode(e))
        return NULL;
    const char *err = commit(e);
    if (err)
        return err;
    e->bits = bits;
    e->value.i = mf_wrap(e->value.i, bits);
    if (e->has_mem)
        e->memory.i = mf_wrap(e->memory.i, bits);
    for (int i = 0; i < e->nframes; i++)
        for (int j = 0; j < e->frames[i].nops; j++)
            e->frames[i].ops[j].lhs.i = mf_wrap(e->frames[i].ops[j].lhs.i, bits);
    return NULL;
}

void engine_toggle_bit(Engine *e, int bit)
{
    if (!int_mode(e) || bit < 0 || bit >= e->bits)
        return;
    if (e->error)
        clear_all(e);
    if (commit(e))
        return;
    uint64_t x = (uint64_t)e->value.i ^ ((uint64_t)1 << bit);
    e->value.i = mf_wrap((int64_t)x, e->bits);
    set_operand_expr(e, NULL);
    e->prev = PREV_UNARY;
}

/* ------------------------------------------------------------------ dispatcher */

static const char *dispatch(Engine *e, int cid)
{
    if (cid == CMD_CLEAR) {
        if (e->mode != MODE_STAT)
            clear_all(e);
        else
            stat_clear(e);
        return NULL;
    }
    if (cid == CMD_CE) {
        e->error = NULL;
        e->has_entry = false;
        set_operand_expr(e, NULL);
        vset(&e->value, zero(e));
        return NULL;
    }
    if (cid == CMD_DEG || cid == CMD_RAD || cid == CMD_GRAD) {
        e->angle = cid == CMD_DEG ? ANGLE_DEG : cid == CMD_RAD ? ANGLE_RAD : ANGLE_GRAD;
        return NULL;
    }
    if (cid == CMD_HEX || cid == CMD_DEC || cid == CMD_OCT || cid == CMD_BIN)
        return set_radix(e, cid == CMD_HEX ? 16 : cid == CMD_DEC ? 10 : cid == CMD_OCT ? 8 : 2);
    if (cid == CMD_QWORD || cid == CMD_DWORD || cid == CMD_WORD || cid == CMD_BYTE)
        return set_bits(e, cid == CMD_QWORD ? 64 : cid == CMD_DWORD ? 32 : cid == CMD_WORD ? 16 : 8);
    if (cid == CMD_INV) {
        e->inv = !e->inv;
        return NULL;
    }
    if (cid == CMD_FE) {
        if (!e->error) {
            const char *err = commit(e);
            if (err)
                return err;
        }
        e->fe = !e->fe;
        return NULL;
    }
    if (cid == CMD_MC) {
        if (e->has_mem)
            vfree(&e->memory);
        e->has_mem = false;
        return NULL;
    }
    if (cid == CMD_STAT_CAD) {
        for (int i = 0; i < e->ndata; i++) {
            mpd_del(e->data[i]);
            free(e->data_str[i]);
        }
        e->ndata = 0;
        return NULL;
    }

    if (e->error) {
        if ((cid >= CMD_DIGIT0 && cid <= CMD_DIGIT0 + 15) || cid == CMD_POINT || cid == CMD_MR)
            clear_all(e);
        else
            return NULL;
    }

    if (cid >= CMD_DIGIT0 && cid <= CMD_DIGIT0 + 15) {
        do_digit(e, cid - CMD_DIGIT0);
        return NULL;
    }
    if (cid == CMD_POINT) {
        do_point(e);
        return NULL;
    }
    if (cid == CMD_BACK) {
        do_backspace(e);
        return NULL;
    }
    if (cid == CMD_EXP) {
        do_exp(e);
        return NULL;
    }
    if (cid == CMD_SIGN)
        return do_sign(e);
    if (is_binop(cid))
        return do_binop(e, cid);
    if (cid == CMD_EQU)
        return do_equals(e);
    if (cid == CMD_OPENP) {
        do_open_paren(e);
        return NULL;
    }
    if (cid == CMD_CLOSEP)
        return do_close_paren(e);
    if (cid == CMD_PERCENT)
        return do_percent(e);
    if (cid == CMD_PI)
        return do_pi(e);
    if (cid == CMD_MR || cid == CMD_MS || cid == CMD_MPLUS || cid == CMD_MMINUS)
        return do_memory(e, cid);
    if (is_stat_cmd(cid))
        return do_stat(e, cid);
    if (is_unary(cid))
        return do_unary(e, cid);
    return NULL;
}

void engine_command(Engine *e, int cid)
{
    if (!engine_is_enabled(e, cid))
        return;
    const char *err = dispatch(e, cid);
    if (err)
        set_error(e, err);
}

/* ------------------------------------------------------------------ value exchange */

char *engine_current_value(Engine *e)
{
    Val v;
    if (current(e, &v))
        v = zero(e);
    char *s = val_str(v);
    vfree(&v);
    return s;
}

void engine_set_value(Engine *e, const char *num)
{
    mpd_t *d = mf_new();
    if (!num || !mf_set_str(d, num) || !mpd_isfinite(d)) {
        mpd_del(d);
        return;
    }
    if (e->error)
        clear_all(e);
    e->has_entry = false;
    set_operand_expr(e, NULL);
    if (int_mode(e)) {
        vset(&e->value, vint(mf_to_wrapped_int(d, e->bits)));
        mpd_del(d);
    } else {
        mpd_t *r = mf_new();
        const char *err = mf_check(r, d);
        mpd_del(d);
        if (err) {
            mpd_del(r);
            set_error(e, err);
            return;
        }
        vset(&e->value, vdec(r));
    }
    e->prev = PREV_MEM;
}

char *engine_format_value(Engine *e, const char *num, int grouping)
{
    mpd_t *d = mf_new();
    if (!num || !mf_set_str(d, num) || !mpd_isfinite(d)) {
        mpd_del(d);
        return NULL;
    }
    char *s;
    if (int_mode(e))
        s = fmt(e, vint(mf_to_wrapped_int(d, 64)), grouping);
    else
        s = fmt(e, vdec(d), grouping);
    mpd_del(d);
    return s;
}

/* ------------------------------------------------------------------ history / dataset */

int engine_history_count(const Engine *e) { return e->nhist; }

const char *engine_history_text(const Engine *e, int i)
{
    return i >= 0 && i < e->nhist ? e->hist_text[i] : NULL;
}

const char *engine_history_value(const Engine *e, int i)
{
    return i >= 0 && i < e->nhist ? e->hist_val[i] : NULL;
}

void engine_clear_history(Engine *e)
{
    for (int i = 0; i < e->nhist; i++) {
        free(e->hist_text[i]);
        free(e->hist_val[i]);
    }
    e->nhist = 0;
}

int engine_dataset_count(const Engine *e) { return e->ndata; }

const char *engine_dataset_value(const Engine *e, int i)
{
    return i >= 0 && i < e->ndata ? e->data_str[i] : NULL;
}
