/* Worksheet math (mortgage, vehicle lease, fuel economy).  C port of win7calc/worksheets.py. */
#include "worksheets.h"

#include "mathfn.h"
#include "numfmt.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    int nf;
    const char *fields[6];
    int nc;
    const char *calc[4];
} Sheet;

static const Sheet SHEETS[] = {
    {"Mortgage", 5,
     {"Purchase price", "Down payment", "Term (years)", "Interest rate (%)", "Monthly payment"},
     4, {"Down payment", "Monthly payment", "Purchase price", "Term (years)"}},
    {"Vehicle lease", 6,
     {"Lease value", "Lease period", "Payments per year", "Residual value", "Interest rate (%)",
      "Periodic payment"},
     3, {"Lease value", "Periodic payment", "Residual value"}},
    {"Fuel economy (mpg)", 3, {"Distance (miles)", "Fuel used (gallons)", "Fuel economy (mpg)"},
     3, {"Distance (miles)", "Fuel used (gallons)", "Fuel economy (mpg)"}},
    {"Fuel economy (L/100 km)", 3,
     {"Distance (kilometers)", "Fuel used (liters)", "Fuel economy (L/100 km)"}, 3,
     {"Distance (kilometers)", "Fuel used (liters)", "Fuel economy (L/100 km)"}},
};
#define NSHEETS ((int)(sizeof SHEETS / sizeof SHEETS[0]))

static const Sheet *find(const char *ws)
{
    if (!ws)
        return NULL;
    for (int i = 0; i < NSHEETS; i++)
        if (strcmp(SHEETS[i].name, ws) == 0)
            return &SHEETS[i];
    return NULL;
}

int ws_count(void) { return NSHEETS; }
const char *ws_name(int i) { return i >= 0 && i < NSHEETS ? SHEETS[i].name : NULL; }

int ws_field_count(const char *ws)
{
    const Sheet *s = find(ws);
    return s ? s->nf : -1;
}

const char *ws_field(const char *ws, int i)
{
    const Sheet *s = find(ws);
    return s && i >= 0 && i < s->nf ? s->fields[i] : NULL;
}

int ws_calculable_count(const char *ws)
{
    const Sheet *s = find(ws);
    return s ? s->nc : -1;
}

const char *ws_calculable(const char *ws, int i)
{
    const Sheet *s = find(ws);
    return s && i >= 0 && i < s->nc ? s->calc[i] : NULL;
}

/* ------------------------------------------------------------------ solving */

typedef struct {
    const char *names[6];
    mpd_t *vals[6];
    int n;
} Inputs;

static const mpd_t *V(const Inputs *in, const char *name)
{
    for (int i = 0; i < in->n; i++)
        if (strcmp(in->names[i], name) == 0)
            return in->vals[i];
    return NULL;
}

/* _parse: NULL on success, else the message. */
static const char *parse(const char *raw, mpd_t *out)
{
    if (!raw)
        return WS_BLANK;
    const char *s = raw;
    while (isspace((unsigned char)*s))
        s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1]))
        n--;
    if (!n)
        return WS_BLANK;
    StrBuf b;
    sb_init(&b);
    for (size_t i = 0; i < n; i++)
        if (s[i] != ',')
            sb_putc(&b, s[i]);
    char *t = sb_take(&b);
    bool ok = mf_set_str(out, t);
    free(t);
    if (!ok || !mpd_isfinite(out) || (mpd_isnegative(out) && !mpd_iszero(out)))
        return WS_INVALID;
    return NULL;
}

typedef struct {
    mpd_context_t c;
    bool trapped;
    mpd_t *tmp[48];
    int nt;
} S;

static mpd_t *T(S *s)
{
    if (s->nt >= 48)
        abort();
    return s->tmp[s->nt++] = mf_new();
}

static mpd_t *TI(S *s, int64_t v)
{
    mpd_t *r = T(s);
    mf_set_i64(r, v);
    return r;
}

#define OP(call)                                                                                   \
    do {                                                                                           \
        uint32_t st = 0;                                                                           \
        call;                                                                                      \
        if (st & MF_TRAPS) {                                                                       \
            s->trapped = true;                                                                     \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

static bool lt0(const mpd_t *x) { return mpd_isnegative(x) && !mpd_iszero(x); }
static bool le0(const mpd_t *x) { return mpd_iszero(x) || mpd_isnegative(x); }

/* Each solver returns the result (a pool temporary) or NULL; NULL with !s->trapped = UNABLE. */
static mpd_t *annuity(S *s, const mpd_t *i, const mpd_t *n)
{
    if (mpd_iszero(i))
        return (mpd_t *)n;
    mpd_t *t = T(s), *mn = T(s), *one = TI(s, 1);
    OP(mpd_qadd(t, one, i, &s->c, &st));
    OP(mpd_qminus(mn, n, &s->c, &st));
    OP(mpd_qpow(t, t, mn, &s->c, &st));
    OP(mpd_qsub(t, one, t, &s->c, &st));
    OP(mpd_qdiv(t, t, i, &s->c, &st));
    return t;
}

static mpd_t *mortgage(S *s, const char *target, const Inputs *v)
{
    mpd_t *i = T(s), *one = TI(s, 1);
    OP(mpd_qdiv(i, V(v, "Interest rate (%)"), TI(s, 1200), &s->c, &st));
    if (strcmp(target, "Term (years)") == 0) {
        mpd_t *principal = T(s);
        OP(mpd_qsub(principal, V(v, "Purchase price"), V(v, "Down payment"), &s->c, &st));
        const mpd_t *pmt = V(v, "Monthly payment");
        if (lt0(principal) || le0(pmt))
            return NULL;
        mpd_t *r = T(s);
        if (mpd_iszero(i)) {
            OP(mpd_qdiv(r, principal, pmt, &s->c, &st));
            OP(mpd_qdiv(r, r, TI(s, 12), &s->c, &st));
            return r;
        }
        mpd_t *x = T(s), *t = T(s);
        OP(mpd_qmul(t, principal, i, &s->c, &st));
        OP(mpd_qdiv(t, t, pmt, &s->c, &st));
        OP(mpd_qsub(x, one, t, &s->c, &st));
        if (le0(x))
            return NULL;
        OP(mpd_qln(r, x, &s->c, &st));
        OP(mpd_qminus(r, r, &s->c, &st));
        OP(mpd_qadd(t, one, i, &s->c, &st));
        OP(mpd_qln(t, t, &s->c, &st));
        OP(mpd_qdiv(r, r, t, &s->c, &st));
        OP(mpd_qdiv(r, r, TI(s, 12), &s->c, &st));
        return r;
    }
    mpd_t *n = T(s);
    OP(mpd_qmul(n, V(v, "Term (years)"), TI(s, 12), &s->c, &st));
    if (le0(n))
        return NULL;
    mpd_t *a = annuity(s, i, n);
    if (!a)
        return NULL;
    mpd_t *r = T(s), *principal = T(s);
    if (strcmp(target, "Monthly payment") == 0) {
        OP(mpd_qsub(principal, V(v, "Purchase price"), V(v, "Down payment"), &s->c, &st));
        if (lt0(principal))
            return NULL;
        OP(mpd_qdiv(r, principal, a, &s->c, &st));
        return r;
    }
    OP(mpd_qmul(principal, V(v, "Monthly payment"), a, &s->c, &st));
    if (strcmp(target, "Down payment") == 0)
        OP(mpd_qsub(r, V(v, "Purchase price"), principal, &s->c, &st));
    else
        OP(mpd_qadd(r, principal, V(v, "Down payment"), &s->c, &st));
    return r;
}

static mpd_t *lease(S *s, const char *target, const Inputs *v)
{
    const mpd_t *ppy = V(v, "Payments per year");
    mpd_t *n = T(s), *i = T(s), *vn = T(s), *t = T(s), *r = T(s), *one = TI(s, 1);
    OP(mpd_qmul(n, V(v, "Lease period"), ppy, &s->c, &st));
    if (le0(ppy) || le0(n))
        return NULL;
    OP(mpd_qdiv(i, V(v, "Interest rate (%)"), TI(s, 100), &s->c, &st));
    OP(mpd_qdiv(i, i, ppy, &s->c, &st));
    OP(mpd_qadd(vn, one, i, &s->c, &st));
    OP(mpd_qminus(t, n, &s->c, &st));
    OP(mpd_qpow(vn, vn, t, &s->c, &st));
    mpd_t *a = annuity(s, i, n);
    if (!a)
        return NULL;
    if (strcmp(target, "Periodic payment") == 0) {
        OP(mpd_qmul(t, V(v, "Residual value"), vn, &s->c, &st));
        OP(mpd_qsub(t, V(v, "Lease value"), t, &s->c, &st));
        OP(mpd_qdiv(r, t, a, &s->c, &st));
    } else if (strcmp(target, "Lease value") == 0) {
        mpd_t *u = T(s);
        OP(mpd_qmul(t, V(v, "Periodic payment"), a, &s->c, &st));
        OP(mpd_qmul(u, V(v, "Residual value"), vn, &s->c, &st));
        OP(mpd_qadd(r, t, u, &s->c, &st));
    } else {
        OP(mpd_qmul(t, V(v, "Periodic payment"), a, &s->c, &st));
        OP(mpd_qsub(t, V(v, "Lease value"), t, &s->c, &st));
        OP(mpd_qdiv(r, t, vn, &s->c, &st));
    }
    return r;
}

static mpd_t *ratio(S *s, const char *target, const Inputs *v, const char *num, const char *den,
                    const char *out, int64_t kk)
{
    mpd_t *k = TI(s, kk), *r = T(s);
    if (strcmp(target, out) == 0) {
        OP(mpd_qdiv(r, V(v, num), V(v, den), &s->c, &st));
        OP(mpd_qmul(r, r, k, &s->c, &st));
    } else if (strcmp(target, num) == 0) {
        OP(mpd_qmul(r, V(v, out), V(v, den), &s->c, &st));
        OP(mpd_qdiv(r, r, k, &s->c, &st));
    } else {
        OP(mpd_qmul(r, V(v, num), k, &s->c, &st));
        OP(mpd_qdiv(r, r, V(v, out), &s->c, &st));
    }
    return r;
}

char *ws_solve(const char *ws, const char *target, const char *const *names,
               const char *const *values, int n, const char **err_msg, const char **err_field)
{
    const char *dummy_m, *dummy_f;
    if (!err_msg)
        err_msg = &dummy_m;
    if (!err_field)
        err_field = &dummy_f;
    *err_msg = NULL;
    *err_field = NULL;
    const Sheet *sh = find(ws);
    bool calculable = false;
    if (sh && target)
        for (int i = 0; i < sh->nc; i++)
            if (strcmp(sh->calc[i], target) == 0)
                calculable = true;
    if (!calculable) {
        *err_msg = WS_UNABLE; /* Python raises a plain ValueError here */
        return NULL;
    }
    Inputs in = {.n = 0};
    char *result = NULL;
    for (int f = 0; f < sh->nf; f++) {
        if (strcmp(sh->fields[f], target) == 0)
            continue;
        const char *raw = NULL;
        for (int j = 0; j < n; j++)
            if (names && names[j] && strcmp(names[j], sh->fields[f]) == 0) {
                raw = values ? values[j] : NULL;
                break;
            }
        mpd_t *d = mf_new();
        in.names[in.n] = sh->fields[f];
        in.vals[in.n++] = d;
        const char *e = parse(raw, d);
        if (e) {
            *err_msg = e;
            *err_field = sh->fields[f];
            goto out;
        }
    }
    {
        S s = {.trapped = false, .nt = 0};
        mf_ctx(&s.c, 34);
        mpd_t *r;
        if (sh == &SHEETS[0])
            r = mortgage(&s, target, &in);
        else if (sh == &SHEETS[1])
            r = lease(&s, target, &in);
        else if (sh == &SHEETS[2])
            r = ratio(&s, target, &in, "Distance (miles)", "Fuel used (gallons)",
                      "Fuel economy (mpg)", 1);
        else
            r = ratio(&s, target, &in, "Fuel used (liters)", "Distance (kilometers)",
                      "Fuel economy (L/100 km)", 100);
        if (!r || !mpd_isfinite(r) || lt0(r)) {
            *err_msg = WS_UNABLE;
        } else {
            uint32_t st = 0;
            mpd_t *p = mf_new();
            mpd_qplus(p, r, &s.c, &st);
            result = mf_to_str(p);
            mpd_del(p);
        }
        for (int i = 0; i < s.nt; i++)
            mpd_del(s.tmp[i]);
    }
out:
    for (int i = 0; i < in.n; i++)
        mpd_del(in.vals[i]);
    return result;
}
