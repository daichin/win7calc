/* High-precision math on libmpdec decimals.  C port of win7calc/mathfn.py.
 *
 * Every Python expression is mirrored operation by operation, in the same
 * decimal context (precision) Python evaluates it in, so results -- including
 * exponents, which show up in str(Decimal) -- are identical.  Python code that
 * runs outside `with ctx()` only compares or uses exact operations
 * (copy_abs), so nothing is ever rounded to Decimal's default 28 digits. */
#include "mathfn.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ basics */

void mf_ctx(mpd_context_t *c, mpd_ssize_t prec)
{
    memset(c, 0, sizeof *c);
    c->prec = prec;
    c->emax = 999999;
    c->emin = -999999;
    c->traps = MF_TRAPS;
    c->round = MPD_ROUND_HALF_EVEN;
    c->clamp = 0;
    c->allcr = 1;
}

void mf_exact_ctx(mpd_context_t *c)
{
    mpd_maxcontext(c);
    c->traps = 0;
    c->status = 0;
    c->allcr = 1;
}

mpd_t *mf_new(void)
{
    mpd_t *r = mpd_qnew();
    if (!r) {
        fputs("win7calc: out of memory\n", stderr);
        abort();
    }
    return r;
}

void mf_free(mpd_t *a)
{
    if (a)
        mpd_del(a);
}

void mf_copy(mpd_t *r, const mpd_t *a)
{
    uint32_t st = 0;
    if (r != a && !mpd_qcopy(r, a, &st)) {
        fputs("win7calc: out of memory\n", stderr);
        abort();
    }
}

mpd_t *mf_dup(const mpd_t *a)
{
    mpd_t *r = mf_new();
    mf_copy(r, a);
    return r;
}

void mf_set_i64(mpd_t *r, int64_t v)
{
    uint32_t st = 0;
    mpd_qset_i64_exact(r, v, &st);
}

bool mf_set_str(mpd_t *r, const char *s)
{
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1]))
        n--;
    while (n && isspace((unsigned char)*s))
        s++, n--;
    char *t = malloc(n + 1);
    if (!t)
        abort();
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (s[i] != '_')
            t[k++] = s[i];
    t[k] = 0;
    uint32_t st = 0;
    mpd_t *tmp = mf_new();
    mpd_qset_string_exact(tmp, t, &st);
    free(t);
    if (st & (MPD_Conversion_syntax | MPD_Malloc_error)) {
        mpd_del(tmp);
        return false;
    }
    mf_copy(r, tmp);
    mpd_del(tmp);
    return true;
}

char *mf_to_str(const mpd_t *a)
{
    char *s = mpd_to_sci(a, 1);
    if (!s)
        abort();
    return s;
}

static mpd_t *g_pi, *g_1e45, *g_1e40, *g_tenth;

static void init_consts(void)
{
    if (g_pi)
        return;
    g_pi = mf_new();
    mf_set_str(g_pi, "3.14159265358979323846264338327950288419716939937510582097494459230781640628620899");
    g_1e45 = mf_new();
    mf_set_str(g_1e45, "1e-45");
    g_1e40 = mf_new();
    mf_set_str(g_1e40, "1e-40");
    g_tenth = mf_new();
    mf_set_str(g_tenth, "0.1");
}

const mpd_t *mf_pi(void)
{
    init_consts();
    return g_pi;
}

int64_t mf_wrap(int64_t v, int bits)
{
    uint64_t u = (uint64_t)v;
    if (bits >= 64)
        return (int64_t)u;
    uint64_t mask = (1ULL << bits) - 1;
    u &= mask;
    if (u >> (bits - 1))
        u |= ~mask;
    return (int64_t)u;
}

/* Magnitude of trunc(x) modulo 2**64. */
static uint64_t trunc_mag_mod64(const mpd_t *x)
{
    if (mpd_isspecial(x) || mpd_iszero(x))
        return 0;
    uint32_t st = 0;
    mpd_t *c = mf_new();
    mf_copy(c, x);
    mpd_ssize_t e = c->exp;
    if (e < 0) {
        mpd_qshiftr(c, x, -e, &st); /* coefficient of the integer part */
        e = 0;
    }
    uint64_t v = 0;
    if (!(st & MPD_Malloc_error)) {
        for (mpd_ssize_t i = c->len - 1; i >= 0; i--)
            v = v * (uint64_t)MPD_RADIX + c->data[i];
        for (mpd_ssize_t i = 0; i < e && i < 64; i++)
            v *= 10;
        if (e >= 64)
            v = 0;
    }
    mpd_del(c);
    return v;
}

int64_t mf_to_wrapped_int(const mpd_t *x, int bits)
{
    uint64_t v = trunc_mag_mod64(x);
    if (mpd_isnegative(x))
        v = (uint64_t)0 - v;
    return mf_wrap((int64_t)v, bits);
}

int mf_int_mod(const mpd_t *x, int m)
{
    /* m must divide 100 (we only need 2 and 4). */
    unsigned last2;
    if (mpd_iszero(x)) {
        last2 = 0;
    } else if (x->exp >= 2) {
        last2 = 0;
    } else if (x->exp == 1) {
        last2 = (unsigned)(x->data[0] % 10) * 10;
    } else if (x->exp == 0) {
        last2 = (unsigned)(x->data[0] % 100);
    } else {
        uint32_t st = 0;
        mpd_t *c = mf_new();
        mpd_qshiftr(c, x, -x->exp, &st);
        last2 = mpd_iszero(c) ? 0 : (unsigned)(c->data[0] % 100);
        mpd_del(c);
    }
    int r = (int)(last2 % (unsigned)m);
    if (mpd_isnegative(x) && r)
        r = m - r;
    return r;
}

/* ------------------------------------------------------------------ op helpers */

/* A pool of temporaries freed together. */
typedef struct {
    mpd_t *v[64];
    int n;
} Pool;

static mpd_t *T(Pool *p)
{
    if (p->n >= 64)
        abort();
    return p->v[p->n++] = mf_new();
}

static mpd_t *TI(Pool *p, int64_t v)
{
    mpd_t *r = T(p);
    mf_set_i64(r, v);
    return r;
}

static void pool_free(Pool *p)
{
    for (int i = 0; i < p->n; i++)
        mpd_del(p->v[i]);
    p->n = 0;
}

/* Run one mpdecimal operation (which must use `&st`) and bail out on a trap. */
#define Q(call)                                                                                    \
    do {                                                                                           \
        uint32_t st = 0;                                                                           \
        call;                                                                                      \
        if (st & MF_TRAPS) {                                                                       \
            err = trap_err(st);                                                                    \
            goto out;                                                                              \
        }                                                                                          \
    } while (0)
/* Propagate an error from a nested mathfn call. */
#define E(call)                                                                                    \
    do {                                                                                           \
        if ((err = (call)) != NULL)                                                                \
            goto out;                                                                              \
    } while (0)

/* Python surfaces a decimal trap as ArithmeticError -> the engine shows "Invalid input". */
static const char *trap_err(uint32_t st)
{
    (void)st;
    return MF_ERR_INVALID;
}

static int cmp(const mpd_t *a, const mpd_t *b)
{
    uint32_t st = 0;
    return mpd_qcmp(a, b, &st);
}

static int cmp_i(const mpd_t *a, int64_t b)
{
    mpd_t *t = mf_new();
    mf_set_i64(t, b);
    int r = cmp(a, t);
    mpd_del(t);
    return r;
}

static void set_zero(mpd_t *r) { mf_set_i64(r, 0); }

void mf_negate(mpd_t *r, const mpd_t *x, mpd_ssize_t prec)
{
    mpd_context_t c;
    mf_ctx(&c, prec);
    uint32_t st = 0;
    mpd_qminus(r, x, &c, &st);
}

/* Exact |x| (Python's x.copy_abs()). */
static void copy_abs(mpd_t *r, const mpd_t *x)
{
    uint32_t st = 0;
    mpd_qcopy_abs(r, x, &st);
}

static bool is_int(const mpd_t *x) { return mpd_isinteger(x); }

/* Decimal(int(x)): exact integer with exponent 0. */
static void int_value(mpd_t *r, const mpd_t *x)
{
    mpd_context_t c;
    mf_exact_ctx(&c);
    uint32_t st = 0;
    mpd_t *t = mf_new();
    mpd_qtrunc(t, x, &c, &st);
    if (mpd_iszero(t))
        mf_set_i64(r, 0);
    else
        mpd_qrescale(r, t, 0, &c, &st);
    mpd_del(t);
}

/* ------------------------------------------------------------------ check */

const char *mf_check(mpd_t *r, const mpd_t *x)
{
    if (mpd_isnan(x))
        return MF_ERR_INVALID;
    if (mpd_isinfinite(x))
        return MF_ERR_OVERFLOW;
    if (!mpd_iszero(x) && mpd_adjexp(x) >= MF_MAX_EXP + 1)
        return MF_ERR_OVERFLOW;
    if (!mpd_iszero(x) && mpd_adjexp(x) < -MF_MAX_EXP) {
        set_zero(r);
        return NULL;
    }
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    uint32_t st = 0;
    mpd_qplus(r, x, &c, &st);
    if (st & MF_TRAPS)
        return MF_ERR_INVALID;
    return NULL;
}

/* ------------------------------------------------------------------ arithmetic */

typedef void (*binop_fn)(mpd_t *, const mpd_t *, const mpd_t *, const mpd_context_t *, uint32_t *);

static const char *binop50(mpd_t *r, const mpd_t *a, const mpd_t *b, binop_fn f)
{
    const char *err = NULL;
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    mpd_t *t = mf_new();
    {
        uint32_t st = 0;
        f(t, a, b, &c, &st);
        if (st & MF_TRAPS) {
            err = MF_ERR_INVALID;
            goto out;
        }
    }
    err = mf_check(r, t);
out:
    mpd_del(t);
    return err;
}

const char *mf_add(mpd_t *r, const mpd_t *a, const mpd_t *b) { return binop50(r, a, b, mpd_qadd); }
const char *mf_sub(mpd_t *r, const mpd_t *a, const mpd_t *b) { return binop50(r, a, b, mpd_qsub); }
const char *mf_mul(mpd_t *r, const mpd_t *a, const mpd_t *b) { return binop50(r, a, b, mpd_qmul); }

const char *mf_div(mpd_t *r, const mpd_t *a, const mpd_t *b)
{
    if (mpd_iszero(b))
        return mpd_iszero(a) ? MF_ERR_UNDEFINED : MF_ERR_DIV0;
    return binop50(r, a, b, mpd_qdiv);
}

const char *mf_mod(mpd_t *r, const mpd_t *a, const mpd_t *b)
{
    if (mpd_iszero(b))
        return mpd_iszero(a) ? MF_ERR_UNDEFINED : MF_ERR_DIV0;
    return binop50(r, a, b, mpd_qrem);
}

/* power() maps decimal Overflow -> "Overflow", InvalidOperation -> "Invalid input";
 * DivisionByZero escapes to the engine's ArithmeticError handler -> "Invalid input". */
static const char *pow_trap_err(uint32_t st)
{
    if (st & MPD_IEEE_Invalid_operation)
        return MF_ERR_INVALID;
    if (st & MPD_Division_by_zero)
        return MF_ERR_INVALID;
    return MF_ERR_OVERFLOW;
}

#define QP(call)                                                                                   \
    do {                                                                                           \
        uint32_t st = 0;                                                                           \
        call;                                                                                      \
        if (st & MF_TRAPS) {                                                                       \
            err = pow_trap_err(st);                                                                \
            goto out;                                                                              \
        }                                                                                          \
    } while (0)

const char *mf_power(mpd_t *r, const mpd_t *x, const mpd_t *y)
{
    const char *err = NULL;
    Pool p = {0};
    if (mpd_iszero(x)) {
        if (mpd_isnegative(y) && !mpd_iszero(y))
            return MF_ERR_DIV0;
        if (mpd_iszero(y))
            return MF_ERR_UNDEFINED;
        set_zero(r);
        return NULL;
    }
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    if (is_int(y)) {
        mpd_t *n = T(&p);
        int_value(n, y);
        mpd_t *an = T(&p);
        mpd_t *lim = TI(&p, 100000);
        uint32_t s0 = 0;
        mpd_qcopy_abs(an, n, &s0);
        if (cmp(an, lim) <= 0) {
            mpd_t *t = T(&p);
            QP(mpd_qpow(t, x, n, &c, &st));
            E(mf_check(r, t));
            goto out;
        }
    }
    if (mpd_isnegative(x)) {
        mpd_t *ax = T(&p), *t = T(&p);
        if (is_int(y)) {
            QP(mpd_qabs(ax, x, &c, &st));
            QP(mpd_qpow(t, ax, y, &c, &st));
            if (mf_int_mod(y, 2))
                QP(mpd_qminus(t, t, &c, &st));
            E(mf_check(r, t));
            goto out;
        }
        mpd_t *inv = T(&p);
        QP(mpd_qdiv(inv, TI(&p, 1), y, &c, &st));
        if (is_int(inv) && mf_int_mod(inv, 2)) {
            QP(mpd_qabs(ax, x, &c, &st));
            QP(mpd_qpow(t, ax, y, &c, &st));
            QP(mpd_qminus(t, t, &c, &st));
            E(mf_check(r, t));
            goto out;
        }
        err = MF_ERR_INVALID;
        goto out;
    }
    {
        mpd_t *t = T(&p);
        QP(mpd_qpow(t, x, y, &c, &st));
        E(mf_check(r, t));
    }
out:
    pool_free(&p);
    return err;
}

const char *mf_root(mpd_t *r, const mpd_t *x, const mpd_t *y)
{
    const char *err = NULL;
    Pool p = {0};
    if (mpd_iszero(y))
        return MF_ERR_INVALID;
    if (mpd_isnegative(x) && !mpd_iszero(x)) {
        if (is_int(y) && mf_int_mod(y, 2)) {
            mpd_t *nx = T(&p);
            mf_negate(nx, x, MF_WORK_PREC);
            E(mf_root(r, nx, y));
            mf_negate(r, r, MF_WORK_PREC);
            goto out;
        }
        err = MF_ERR_INVALID;
        goto out;
    }
    if (mpd_iszero(x)) {
        if (mpd_isnegative(y))
            err = MF_ERR_DIV0;
        else
            set_zero(r);
        goto out;
    }
    {
        mpd_context_t c;
        mf_ctx(&c, MF_WORK_PREC);
        mpd_t *inv = T(&p), *t = T(&p), *rr = T(&p);
        Q(mpd_qdiv(inv, TI(&p, 1), y, &c, &st));
        Q(mpd_qpow(t, x, inv, &c, &st));
        E(mf_check(rr, t));
        if (is_int(y)) {
            mpd_t *n = T(&p), *iy = T(&p), *pw = T(&p);
            Q(mpd_qround_to_int(n, rr, &c, &st));
            if (!mpd_iszero(n)) {
                int_value(iy, y);
                Q(mpd_qpow(pw, n, iy, &c, &st));
                if (cmp(pw, x) == 0) {
                    Q(mpd_qadd(r, n, TI(&p, 0), &c, &st));
                    goto out;
                }
            }
        }
        mf_copy(r, rr);
    }
out:
    pool_free(&p);
    return err;
}

/* ------------------------------------------------------------------ unary */

const char *mf_sqrt(mpd_t *r, const mpd_t *x)
{
    const char *err = NULL;
    Pool p = {0};
    if (mpd_isnegative(x) && !mpd_iszero(x))
        return MF_ERR_INVALID;
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    mpd_t *t = T(&p);
    Q(mpd_qsqrt(t, x, &c, &st));
    E(mf_check(r, t));
out:
    pool_free(&p);
    return err;
}

const char *mf_cbrt(mpd_t *r, const mpd_t *x)
{
    const char *err = NULL;
    Pool p = {0};
    if (mpd_iszero(x)) {
        set_zero(r);
        return NULL;
    }
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    mpd_t *a = T(&p), *t = T(&p);
    Q(mpd_qabs(a, x, &c, &st));
    E(mf_root(t, a, TI(&p, 3)));
    if (mpd_isnegative(x))
        Q(mpd_qminus(t, t, &c, &st));
    mf_copy(r, t);
out:
    pool_free(&p);
    return err;
}

const char *mf_reciprocal(mpd_t *r, const mpd_t *x)
{
    if (mpd_iszero(x))
        return MF_ERR_DIV0;
    mpd_t *one = mf_new();
    mf_set_i64(one, 1);
    const char *err = binop50(r, one, x, mpd_qdiv);
    mpd_del(one);
    return err;
}

const char *mf_square(mpd_t *r, const mpd_t *x) { return mf_mul(r, x, x); }

const char *mf_cube(mpd_t *r, const mpd_t *x)
{
    const char *err = NULL;
    Pool p = {0};
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    mpd_t *t = T(&p);
    Q(mpd_qmul(t, x, x, &c, &st));
    Q(mpd_qmul(t, t, x, &c, &st));
    E(mf_check(r, t));
out:
    pool_free(&p);
    return err;
}

typedef void (*unop_fn)(mpd_t *, const mpd_t *, const mpd_context_t *, uint32_t *);

static const char *unop50(mpd_t *r, const mpd_t *x, unop_fn f)
{
    const char *err = NULL;
    Pool p = {0};
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    mpd_t *t = T(&p);
    Q(f(t, x, &c, &st));
    E(mf_check(r, t));
out:
    pool_free(&p);
    return err;
}

const char *mf_ln(mpd_t *r, const mpd_t *x)
{
    if (mpd_iszero(x) || mpd_isnegative(x))
        return MF_ERR_INVALID;
    return unop50(r, x, mpd_qln);
}

const char *mf_log10(mpd_t *r, const mpd_t *x)
{
    if (mpd_iszero(x) || mpd_isnegative(x))
        return MF_ERR_INVALID;
    return unop50(r, x, mpd_qlog10);
}

const char *mf_exp(mpd_t *r, const mpd_t *x)
{
    if (cmp_i(x, 23027) > 0)
        return MF_ERR_OVERFLOW;
    return unop50(r, x, mpd_qexp);
}

const char *mf_pow10(mpd_t *r, const mpd_t *x)
{
    mpd_t *ten = mf_new();
    mf_set_i64(ten, 10);
    const char *err = mf_power(r, ten, x);
    mpd_del(ten);
    return err;
}

/* ---- CPython 3.10 math.gamma (Modules/mathmodule.c), reproduced exactly. */

static const double py_pi = 3.141592653589793238462643383279502884197;

#define NGAMMA_INTEGRAL 23
static const double gamma_integral[NGAMMA_INTEGRAL] = {
    1.0, 1.0, 2.0, 6.0, 24.0, 120.0, 720.0, 5040.0, 40320.0, 362880.0,
    3628800.0, 39916800.0, 479001600.0, 6227020800.0, 87178291200.0,
    1307674368000.0, 20922789888000.0, 355687428096000.0,
    6402373705728000.0, 121645100408832000.0, 2432902008176640000.0,
    51090942171709440000.0, 1124000727777607680000.0,
};

#define LANCZOS_N 13
static const double lanczos_g = 6.024680040776729583740234375;
static const double lanczos_g_minus_half = 5.524680040776729583740234375;
static const double lanczos_num_coeffs[LANCZOS_N] = {
    23531376880.410759688572007674451636754734846804940,
    42919803642.649098768957899047001988850926355848959,
    35711959237.355668049440185451547166705960488635843,
    17921034426.037209699919755754458931112671403265390,
    6039542586.3520280050642916443072979210699388420708,
    1439720407.3117216736632230727949123939715485786772,
    248874557.86205415651146038641322942321632125127801,
    31426415.585400194380614231628318205362874684987640,
    2876370.6289353724412254090516208496135991145378768,
    186056.26539522349504029498971604569928220784236328,
    8071.6720023658162106380029022722506138218516325024,
    210.82427775157934587250973392071336271166969580291,
    2.5066282746310002701649081771338373386264310793408
};
static const double lanczos_den_coeffs[LANCZOS_N] = {
    0.0, 39916800.0, 120543840.0, 150917976.0, 105258076.0, 45995730.0,
    13339535.0, 2637558.0, 357423.0, 32670.0, 1925.0, 66.0, 1.0};

static double lanczos_sum(double x)
{
    double num = 0.0, den = 0.0;
    int i;
    if (x < 5.0) {
        for (i = LANCZOS_N; --i >= 0;) {
            num = num * x + lanczos_num_coeffs[i];
            den = den * x + lanczos_den_coeffs[i];
        }
    } else {
        for (i = 0; i < LANCZOS_N; i++) {
            num = num / x + lanczos_num_coeffs[i];
            den = den / x + lanczos_den_coeffs[i];
        }
    }
    return num / den;
}

static double m_sinpi(double x)
{
    double y, r;
    int n;
    y = fmod(fabs(x), 2.0);
    n = (int)round(2.0 * y);
    switch (n) {
    case 0: r = sin(py_pi * y); break;
    case 1: r = cos(py_pi * (y - 0.5)); break;
    case 2: r = sin(py_pi * (1.0 - y)); break;
    case 3: r = -cos(py_pi * (y - 1.5)); break;
    case 4: r = sin(py_pi * (y - 2.0)); break;
    default: r = 0; break;
    }
    return copysign(1.0, x) * r;
}

static double m_tgamma(double x)
{
    double absx, r, y, z, sqrtpow;
    if (!isfinite(x)) {
        if (isnan(x) || x > 0.0)
            return x;
        errno = EDOM;
        return NAN;
    }
    if (x == 0.0) {
        errno = EDOM;
        return copysign(HUGE_VAL, x);
    }
    if (x == floor(x)) {
        if (x < 0.0) {
            errno = EDOM;
            return NAN;
        }
        if (x <= NGAMMA_INTEGRAL)
            return gamma_integral[(int)x - 1];
    }
    absx = fabs(x);
    if (absx < 1e-20) {
        r = 1.0 / x;
        if (isinf(r))
            errno = ERANGE;
        return r;
    }
    if (absx > 200.0) {
        if (x < 0.0) {
            return 0.0 / m_sinpi(x);
        } else {
            errno = ERANGE;
            return HUGE_VAL;
        }
    }
    y = absx + lanczos_g_minus_half;
    if (absx > lanczos_g_minus_half) {
        volatile double q = y - absx;
        z = q - lanczos_g_minus_half;
    } else {
        volatile double q = y - lanczos_g_minus_half;
        z = q - absx;
    }
    z = z * lanczos_g / y;
    if (x < 0.0) {
        r = -py_pi / m_sinpi(absx) / absx * exp(y) / lanczos_sum(absx);
        r -= z * r;
        if (absx < 140.0) {
            r /= pow(y, absx - 0.5);
        } else {
            sqrtpow = pow(y, absx / 2.0 - 0.25);
            r /= sqrtpow;
            r /= sqrtpow;
        }
    } else {
        r = lanczos_sum(absx) / exp(y);
        r += z * r;
        if (absx < 140.0) {
            r *= pow(y, absx - 0.5);
        } else {
            sqrtpow = pow(y, absx / 2.0 - 0.25);
            r *= sqrtpow;
            r *= sqrtpow;
        }
    }
    if (isinf(r))
        errno = ERANGE;
    return r;
}

double mf_py_gamma(double x, int *err)
{
    *err = 0;
    errno = 0;
    double r = m_tgamma(x);
    if (errno) {
        if (errno == ERANGE) {
            if (!(fabs(r) < 1.5))
                *err = 2;
        } else {
            *err = 1;
        }
    }
    return r;
}

void mf_py_float_repr(double x, char *buf, int n)
{
    /* Shortest round-tripping digits, laid out like Python's repr(). */
    char tmp[64];
    int prec;
    for (prec = 1; prec <= 17; prec++) {
        snprintf(tmp, sizeof tmp, "%.*e", prec - 1, x);
        if (strtod(tmp, NULL) == x)
            break;
    }
    if (prec > 17)
        prec = 17;
    snprintf(tmp, sizeof tmp, "%.*e", prec - 1, x);
    /* tmp = [-]d[.ddd]e[+-]XX */
    const char *s = tmp;
    bool neg = false;
    if (*s == '-') {
        neg = true;
        s++;
    }
    char digits[32];
    int nd = 0;
    const char *e = strchr(s, 'e');
    for (const char *q = s; q < e; q++)
        if (isdigit((unsigned char)*q))
            digits[nd++] = *q;
    while (nd > 1 && digits[nd - 1] == '0')
        nd--;
    digits[nd] = 0;
    int exp10 = atoi(e + 1);
    int decpt = exp10 + 1;
    char out[64];
    int k = 0;
    if (neg)
        out[k++] = '-';
    if (decpt <= -4 || decpt > 16) {
        out[k++] = digits[0];
        if (nd > 1) {
            out[k++] = '.';
            for (int i = 1; i < nd; i++)
                out[k++] = digits[i];
        }
        k += snprintf(out + k, sizeof out - k, "e%c%02d", exp10 < 0 ? '-' : '+', abs(exp10));
    } else if (decpt <= 0) {
        out[k++] = '0';
        out[k++] = '.';
        for (int i = 0; i < -decpt; i++)
            out[k++] = '0';
        for (int i = 0; i < nd; i++)
            out[k++] = digits[i];
    } else if (decpt >= nd) {
        for (int i = 0; i < nd; i++)
            out[k++] = digits[i];
        for (int i = nd; i < decpt; i++)
            out[k++] = '0';
        out[k++] = '.';
        out[k++] = '0';
    } else {
        for (int i = 0; i < nd; i++) {
            if (i == decpt)
                out[k++] = '.';
            out[k++] = digits[i];
        }
    }
    out[k] = 0;
    snprintf(buf, n, "%s", out);
}

const char *mf_factorial(mpd_t *r, const mpd_t *x)
{
    const char *err = NULL;
    Pool p = {0};
    if (is_int(x)) {
        if (mpd_isnegative(x) && !mpd_iszero(x))
            return MF_ERR_INVALID;
        if (cmp_i(x, 3249) > 0)
            return MF_ERR_OVERFLOW;
        uint32_t st = 0;
        int64_t n = mpd_qget_i64(x, &st);
        if (st) { /* integral but with a fractional-looking exponent, e.g. 5.00 */
            mpd_t *iv = T(&p);
            int_value(iv, x);
            st = 0;
            n = mpd_qget_i64(iv, &st);
        }
        mpd_context_t ec;
        mf_exact_ctx(&ec);
        mpd_t *f = TI(&p, 1), *k = T(&p);
        for (int64_t i = 2; i <= n; i++) {
            mf_set_i64(k, i);
            mpd_qmul(f, f, k, &ec, &st);
        }
        E(mf_check(r, f));
        goto out;
    }
    {
        char *s = mf_to_str(x);
        double fx = strtod(s, NULL);
        free(s);
        int gerr;
        double g = mf_py_gamma(fx + 1, &gerr);
        bool neg = mpd_isnegative(x) && !mpd_iszero(x);
        if (gerr) {
            err = neg ? MF_ERR_INVALID : MF_ERR_OVERFLOW;
            goto out;
        }
        if (isinf(g) || isnan(g)) {
            err = MF_ERR_OVERFLOW;
            goto out;
        }
        char buf[64];
        mf_py_float_repr(g, buf, sizeof buf);
        mpd_t *d = T(&p);
        mf_set_str(d, buf);
        E(mf_check(r, d));
    }
out:
    pool_free(&p);
    return err;
}

const char *mf_int_part(mpd_t *r, const mpd_t *x, mpd_ssize_t prec)
{
    if (mpd_iszero(x)) {
        set_zero(r);
        return NULL;
    }
    mpd_context_t down, c;
    mf_ctx(&down, prec);
    down.round = MPD_ROUND_DOWN;
    mf_ctx(&c, prec);
    uint32_t st = 0;
    mpd_t *t = mf_new(), *z = mf_new();
    mf_set_i64(z, 0);
    mpd_qround_to_int(t, x, &down, &st);
    mpd_qadd(r, t, z, &c, &st);
    mpd_del(t);
    mpd_del(z);
    return (st & MF_TRAPS) ? MF_ERR_INVALID : NULL;
}

const char *mf_frac_part(mpd_t *r, const mpd_t *x)
{
    const char *err = NULL;
    Pool p = {0};
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    mpd_t *ip = T(&p), *t = T(&p);
    E(mf_int_part(ip, x, MF_WORK_PREC));
    Q(mpd_qsub(t, x, ip, &c, &st));
    E(mf_check(r, t));
out:
    pool_free(&p);
    return err;
}

/* dms (k1=60, d1=100, d2=10000) and degrees (k1=100, d1=60, d2=3600). */
static const char *dms_like(mpd_t *r, const mpd_t *x, int k1, int d1, int d2)
{
    const char *err = NULL;
    Pool p = {0};
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    bool neg = mpd_isnegative(x) && !mpd_iszero(x);
    mpd_t *ax = T(&p), *d = T(&p), *rest = T(&p), *m = T(&p), *s = T(&p), *t1 = T(&p),
          *t2 = T(&p), *t3 = T(&p), *res = T(&p);
    Q(mpd_qabs(ax, x, &c, &st));
    E(mf_int_part(d, ax, MF_WORK_PREC));
    Q(mpd_qsub(rest, ax, d, &c, &st));
    Q(mpd_qmul(rest, rest, TI(&p, k1), &c, &st));
    E(mf_int_part(m, rest, MF_WORK_PREC));
    Q(mpd_qsub(s, rest, m, &c, &st));
    Q(mpd_qmul(s, s, TI(&p, k1), &c, &st));
    Q(mpd_qdiv(t1, m, TI(&p, d1), &c, &st));
    Q(mpd_qadd(t2, d, t1, &c, &st));
    Q(mpd_qdiv(t3, s, TI(&p, d2), &c, &st));
    Q(mpd_qadd(res, t2, t3, &c, &st));
    if (neg)
        Q(mpd_qminus(res, res, &c, &st));
    E(mf_check(r, res));
out:
    pool_free(&p);
    return err;
}

const char *mf_dms(mpd_t *r, const mpd_t *x) { return dms_like(r, x, 60, 100, 10000); }
const char *mf_degrees(mpd_t *r, const mpd_t *x) { return dms_like(r, x, 100, 60, 3600); }

/* ------------------------------------------------------------------ trigonometry */

static const char *to_radians(mpd_t *r, const mpd_t *x, MfAngle u)
{
    const char *err = NULL;
    Pool p = {0};
    init_consts();
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC + 10);
    if (u == MF_RAD) {
        Q(mpd_qplus(r, x, &c, &st));
    } else {
        mpd_t *t = T(&p);
        Q(mpd_qmul(t, x, g_pi, &c, &st));
        Q(mpd_qdiv(r, t, TI(&p, u == MF_DEG ? 180 : 200), &c, &st));
    }
out:
    pool_free(&p);
    return err;
}

static const char *from_radians(mpd_t *r, const mpd_t *x, MfAngle u)
{
    const char *err = NULL;
    Pool p = {0};
    init_consts();
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC + 10);
    if (u == MF_RAD) {
        Q(mpd_qplus(r, x, &c, &st));
    } else {
        mpd_t *t = T(&p);
        Q(mpd_qmul(t, x, TI(&p, u == MF_DEG ? 180 : 200), &c, &st));
        Q(mpd_qdiv(r, t, g_pi, &c, &st));
    }
out:
    pool_free(&p);
    return err;
}

/* k (0..3) if x is an exact multiple of a right angle in deg/grad, else -1. */
static int quadrant(const mpd_t *x, MfAngle u)
{
    if (u == MF_RAD)
        return -1;
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC + 10);
    mpd_t *q = mf_new(), *r = mf_new();
    mf_set_i64(q, u == MF_DEG ? 90 : 100);
    uint32_t st = 0;
    mpd_qdiv(r, x, q, &c, &st);
    int k = -1;
    if (is_int(r))
        k = mf_int_mod(r, 4);
    mpd_del(q);
    mpd_del(r);
    return k;
}

static const char *sin_cos_rad(mpd_t *s_out, mpd_t *c_out, const mpd_t *xin)
{
    const char *err = NULL;
    Pool p = {0};
    init_consts();
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC + 15);
    mpd_t *twopi = T(&p), *x = T(&p), *x2 = T(&p), *s = T(&p), *term = T(&p), *ns = T(&p),
          *den = T(&p);
    Q(mpd_qmul(twopi, TI(&p, 2), g_pi, &c, &st));
    Q(mpd_qrem(x, xin, twopi, &c, &st));
    Q(mpd_qmul(x2, x, x, &c, &st));
    mf_copy(s, x);
    mf_copy(term, x);
    int64_t n = 1;
    for (;;) {
        Q(mpd_qminus(term, term, &c, &st));
        Q(mpd_qmul(term, term, x2, &c, &st));
        mf_set_i64(den, (n + 1) * (n + 2));
        Q(mpd_qdiv(term, term, den, &c, &st));
        n += 2;
        Q(mpd_qadd(ns, s, term, &c, &st));
        if (cmp(ns, s) == 0)
            break;
        mf_copy(s, ns);
    }
    mf_copy(s_out, s);
    mf_set_i64(s, 1); /* reuse s as c */
    mf_set_i64(term, 1);
    n = 0;
    for (;;) {
        Q(mpd_qminus(term, term, &c, &st));
        Q(mpd_qmul(term, term, x2, &c, &st));
        mf_set_i64(den, (n + 1) * (n + 2));
        Q(mpd_qdiv(term, term, den, &c, &st));
        n += 2;
        Q(mpd_qadd(ns, s, term, &c, &st));
        if (cmp(ns, s) == 0)
            break;
        mf_copy(s, ns);
    }
    mf_copy(c_out, s);
out:
    pool_free(&p);
    return err;
}

static const char *snap(mpd_t *r, const mpd_t *v)
{
    const char *err = NULL;
    Pool p = {0};
    init_consts();
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    mpd_t *a = T(&p);
    Q(mpd_qabs(a, v, &c, &st));
    if (cmp(a, g_1e45) < 0)
        set_zero(r);
    else
        Q(mpd_qplus(r, v, &c, &st));
out:
    pool_free(&p);
    return err;
}

static const int SIN_Q[4] = {0, 1, 0, -1};
static const int COS_Q[4] = {1, 0, -1, 0};

static const char *sin_or_cos(mpd_t *r, const mpd_t *x, MfAngle u, bool want_sin)
{
    const char *err = NULL;
    Pool p = {0};
    int q = quadrant(x, u);
    if (q >= 0) {
        mf_set_i64(r, want_sin ? SIN_Q[q] : COS_Q[q]);
        return NULL;
    }
    mpd_t *rad = T(&p), *s = T(&p), *c = T(&p), *sn = T(&p);
    E(to_radians(rad, x, u));
    E(sin_cos_rad(s, c, rad));
    E(snap(sn, want_sin ? s : c));
    E(mf_check(r, sn));
out:
    pool_free(&p);
    return err;
}

const char *mf_sin(mpd_t *r, const mpd_t *x, MfAngle u) { return sin_or_cos(r, x, u, true); }
const char *mf_cos(mpd_t *r, const mpd_t *x, MfAngle u) { return sin_or_cos(r, x, u, false); }

const char *mf_tan(mpd_t *r, const mpd_t *x, MfAngle u)
{
    const char *err = NULL;
    Pool p = {0};
    init_consts();
    int q = quadrant(x, u);
    if (q >= 0) {
        if (q % 2)
            return MF_ERR_INVALID;
        set_zero(r);
        return NULL;
    }
    mpd_t *rad = T(&p), *s = T(&p), *c = T(&p), *ac = T(&p), *t = T(&p), *sn = T(&p);
    E(to_radians(rad, x, u));
    E(sin_cos_rad(s, c, rad));
    copy_abs(ac, c);
    if (cmp(ac, g_1e45) < 0) {
        err = MF_ERR_INVALID;
        goto out;
    }
    {
        mpd_context_t cx;
        mf_ctx(&cx, MF_WORK_PREC);
        Q(mpd_qdiv(t, s, c, &cx, &st));
    }
    E(snap(sn, t));
    E(mf_check(r, sn));
out:
    pool_free(&p);
    return err;
}

static const char *atan_rad(mpd_t *r, const mpd_t *xin)
{
    const char *err = NULL;
    Pool p = {0};
    init_consts();
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC + 15);
    mpd_t *one = TI(&p, 1);
    if (mpd_isnegative(xin) && !mpd_iszero(xin)) {
        mpd_t *nx = T(&p), *t = T(&p);
        Q(mpd_qminus(nx, xin, &c, &st));
        E(atan_rad(t, nx));
        Q(mpd_qminus(r, t, &c, &st));
        goto out;
    }
    if (cmp(xin, one) > 0) {
        mpd_t *h = T(&p), *inv = T(&p), *t = T(&p);
        Q(mpd_qdiv(h, g_pi, TI(&p, 2), &c, &st));
        Q(mpd_qdiv(inv, one, xin, &c, &st));
        E(atan_rad(t, inv));
        Q(mpd_qsub(r, h, t, &c, &st));
        goto out;
    }
    {
        mpd_t *x = T(&p), *t = T(&p), *x2 = T(&p), *s = T(&p), *term = T(&p), *ns = T(&p),
              *q = T(&p), *nd = T(&p);
        mf_copy(x, xin);
        int k = 0;
        while (cmp(x, g_tenth) > 0) {
            Q(mpd_qmul(t, x, x, &c, &st));
            Q(mpd_qadd(t, one, t, &c, &st));
            Q(mpd_qsqrt(t, t, &c, &st));
            Q(mpd_qadd(t, one, t, &c, &st));
            Q(mpd_qdiv(x, x, t, &c, &st));
            k++;
        }
        Q(mpd_qmul(x2, x, x, &c, &st));
        mf_copy(s, x);
        mf_copy(term, x);
        int64_t n = 1;
        for (;;) {
            Q(mpd_qminus(term, term, &c, &st));
            Q(mpd_qmul(term, term, x2, &c, &st));
            n += 2;
            mf_set_i64(nd, n);
            Q(mpd_qdiv(q, term, nd, &c, &st));
            Q(mpd_qadd(ns, s, q, &c, &st));
            if (cmp(ns, s) == 0)
                break;
            mf_copy(s, ns);
        }
        Q(mpd_qmul(r, s, TI(&p, (int64_t)1 << k), &c, &st));
    }
out:
    pool_free(&p);
    return err;
}

static const char *clean(mpd_t *r, const mpd_t *v)
{
    const char *err = NULL;
    Pool p = {0};
    init_consts();
    mpd_context_t c45, cdef, c50;
    mf_ctx(&c45, MF_WORK_PREC - 5);
    mf_ctx(&cdef, MF_DEFAULT_PREC);
    mf_ctx(&c50, MF_WORK_PREC);
    mpd_t *rr = T(&p), *n = T(&p), *d = T(&p);
    Q(mpd_qplus(rr, v, &c45, &st));
    Q(mpd_qround_to_int(n, rr, &cdef, &st));
    Q(mpd_qsub(d, rr, n, &c50, &st));
    Q(mpd_qabs(d, d, &c50, &st));
    if (cmp(d, g_1e40) < 0)
        Q(mpd_qadd(r, n, TI(&p, 0), &c50, &st));
    else
        Q(mpd_qplus(r, rr, &c50, &st));
out:
    pool_free(&p);
    return err;
}

static const char *finish_inverse_trig(mpd_t *r, const mpd_t *rad, MfAngle u)
{
    const char *err = NULL;
    mpd_t *t = mf_new(), *t2 = mf_new();
    if ((err = from_radians(t, rad, u)) == NULL && (err = clean(t2, t)) == NULL)
        err = mf_check(r, t2);
    mpd_del(t);
    mpd_del(t2);
    return err;
}

const char *mf_atan(mpd_t *r, const mpd_t *x, MfAngle u)
{
    mpd_t *t = mf_new();
    const char *err = atan_rad(t, x);
    if (!err)
        err = finish_inverse_trig(r, t, u);
    mpd_del(t);
    return err;
}

const char *mf_asin(mpd_t *r, const mpd_t *x, MfAngle u)
{
    const char *err = NULL;
    Pool p = {0};
    init_consts();
    mpd_t *one = TI(&p, 1), *ax = T(&p), *rad = T(&p);
    copy_abs(ax, x);
    if (cmp(ax, one) > 0)
        return pool_free(&p), MF_ERR_INVALID;
    {
        mpd_context_t c;
        mf_ctx(&c, MF_WORK_PREC + 15);
        mpd_t *a = T(&p), *t = T(&p);
        Q(mpd_qabs(a, x, &c, &st));
        if (cmp(a, one) == 0) {
            Q(mpd_qdiv(t, g_pi, TI(&p, 2), &c, &st));
            bool pos = !mpd_isnegative(x) && !mpd_iszero(x);
            Q(mpd_qmul(rad, t, TI(&p, pos ? 1 : -1), &c, &st));
        } else {
            Q(mpd_qmul(t, x, x, &c, &st));
            Q(mpd_qsub(t, one, t, &c, &st));
            Q(mpd_qsqrt(t, t, &c, &st));
            Q(mpd_qdiv(t, x, t, &c, &st));
            E(atan_rad(rad, t));
        }
    }
    E(finish_inverse_trig(r, rad, u));
out:
    pool_free(&p);
    return err;
}

const char *mf_acos(mpd_t *r, const mpd_t *x, MfAngle u)
{
    const char *err = NULL;
    Pool p = {0};
    init_consts();
    mpd_t *one = TI(&p, 1), *ax = T(&p), *rad = T(&p);
    copy_abs(ax, x);
    if (cmp(ax, one) > 0)
        return pool_free(&p), MF_ERR_INVALID;
    {
        mpd_context_t c;
        mf_ctx(&c, MF_WORK_PREC + 15);
        mpd_t *t = T(&p);
        if (mpd_iszero(x)) {
            Q(mpd_qdiv(rad, g_pi, TI(&p, 2), &c, &st));
        } else {
            Q(mpd_qmul(t, x, x, &c, &st));
            Q(mpd_qsub(t, one, t, &c, &st));
            Q(mpd_qsqrt(t, t, &c, &st));
            Q(mpd_qdiv(t, t, x, &c, &st));
            E(atan_rad(rad, t));
            if (mpd_isnegative(x))
                Q(mpd_qadd(rad, rad, g_pi, &c, &st));
        }
    }
    E(finish_inverse_trig(r, rad, u));
out:
    pool_free(&p);
    return err;
}

/* ------------------------------------------------------------------ hyperbolic */

const char *mf_sinh(mpd_t *r, const mpd_t *x)
{
    const char *err = NULL;
    Pool p = {0};
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    mpd_t *e = T(&p), *t = T(&p);
    E(mf_exp(e, x));
    Q(mpd_qdiv(t, TI(&p, 1), e, &c, &st));
    Q(mpd_qsub(t, e, t, &c, &st));
    Q(mpd_qdiv(t, t, TI(&p, 2), &c, &st));
    E(mf_check(r, t));
out:
    pool_free(&p);
    return err;
}

const char *mf_cosh(mpd_t *r, const mpd_t *x)
{
    const char *err = NULL;
    Pool p = {0};
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC);
    mpd_t *a = T(&p), *e = T(&p), *t = T(&p);
    copy_abs(a, x);
    E(mf_exp(e, a));
    Q(mpd_qdiv(t, TI(&p, 1), e, &c, &st));
    Q(mpd_qadd(t, e, t, &c, &st));
    Q(mpd_qdiv(t, t, TI(&p, 2), &c, &st));
    E(mf_check(r, t));
out:
    pool_free(&p);
    return err;
}

const char *mf_tanh(mpd_t *r, const mpd_t *x)
{
    const char *err = NULL;
    Pool p = {0};
    mpd_t *a = T(&p);
    copy_abs(a, x);
    if (cmp_i(a, 200) > 0) {
        mf_set_i64(r, (!mpd_isnegative(x) && !mpd_iszero(x)) ? 1 : -1);
        goto out;
    }
    {
        mpd_context_t c;
        mf_ctx(&c, MF_WORK_PREC);
        mpd_t *x2 = T(&p), *e2 = T(&p), *n = T(&p), *d = T(&p), *t = T(&p), *one = TI(&p, 1);
        Q(mpd_qmul(x2, TI(&p, 2), x, &c, &st));
        E(mf_exp(e2, x2));
        Q(mpd_qsub(n, e2, one, &c, &st));
        Q(mpd_qadd(d, e2, one, &c, &st));
        Q(mpd_qdiv(t, n, d, &c, &st));
        E(mf_check(r, t));
    }
out:
    pool_free(&p);
    return err;
}

const char *mf_asinh(mpd_t *r, const mpd_t *x)
{
    const char *err = NULL;
    Pool p = {0};
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC + 10);
    mpd_t *ax = T(&p), *t = T(&p);
    Q(mpd_qabs(ax, x, &c, &st));
    Q(mpd_qmul(t, ax, ax, &c, &st));
    Q(mpd_qadd(t, t, TI(&p, 1), &c, &st));
    Q(mpd_qsqrt(t, t, &c, &st));
    Q(mpd_qadd(t, ax, t, &c, &st));
    Q(mpd_qln(t, t, &c, &st));
    if (mpd_isnegative(x) && !mpd_iszero(x))
        Q(mpd_qminus(t, t, &c, &st));
    E(mf_check(r, t));
out:
    pool_free(&p);
    return err;
}

const char *mf_acosh(mpd_t *r, const mpd_t *x)
{
    const char *err = NULL;
    Pool p = {0};
    if (cmp_i(x, 1) < 0)
        return MF_ERR_INVALID;
    mpd_context_t c;
    mf_ctx(&c, MF_WORK_PREC + 10);
    mpd_t *t = T(&p);
    Q(mpd_qmul(t, x, x, &c, &st));
    Q(mpd_qsub(t, t, TI(&p, 1), &c, &st));
    Q(mpd_qsqrt(t, t, &c, &st));
    Q(mpd_qadd(t, x, t, &c, &st));
    Q(mpd_qln(t, t, &c, &st));
    E(mf_check(r, t));
out:
    pool_free(&p);
    return err;
}

const char *mf_atanh(mpd_t *r, const mpd_t *x)
{
    const char *err = NULL;
    Pool p = {0};
    mpd_t *a = T(&p);
    copy_abs(a, x);
    if (cmp_i(a, 1) >= 0) {
        err = MF_ERR_INVALID;
        goto out;
    }
    {
        mpd_context_t c;
        mf_ctx(&c, MF_WORK_PREC + 10);
        mpd_t *n = T(&p), *d = T(&p), *t = T(&p), *one = TI(&p, 1);
        Q(mpd_qadd(n, one, x, &c, &st));
        Q(mpd_qsub(d, one, x, &c, &st));
        Q(mpd_qdiv(t, n, d, &c, &st));
        Q(mpd_qln(t, t, &c, &st));
        Q(mpd_qdiv(t, t, TI(&p, 2), &c, &st));
        E(mf_check(r, t));
    }
out:
    pool_free(&p);
    return err;
}
