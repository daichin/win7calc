/* Unit conversion data and math.  C port of win7calc/units.py.
 *
 * Python converts with exact Fractions; here every factor is an exact ratio of
 * two decimal strings and the arithmetic runs in an (effectively) unbounded
 * libmpdec context, so the final 32-digit division sees the same rational. */
#include "units.h"

#include "mathfn.h"
#include "numfmt.h"

#include <stdlib.h>
#include <string.h>

#define PRECISION 32

typedef struct {
    const char *name;
    const char *num, *den; /* factor to the base unit = num / den */
} Unit;

typedef struct {
    const char *name;
    const char *def_from, *def_to;
    int n;
    Unit units[20];
} Category;

#define PI_STR "3.14159265358979323846264338327950288419716939937510582097494"
#define BTU "1055.05585262"
#define FTLB "1.3558179483314004"

static const Category CATS[] = {
    {"Angle", "Degree", "Radian", 3,
     {{"Degree", "1", "1"}, {"Gradian", "9", "10"}, {"Radian", "180", PI_STR}}},
    {"Area", "Square meters", "Square feet", 10,
     {{"Acres", "4046.8564224", "1"},
      {"Hectares", "10000", "1"},
      {"Square centimeter", "0.0001", "1"},
      {"Square feet", "0.09290304", "1"},
      {"Square inch", "0.00064516", "1"},
      {"Square kilometer", "1000000", "1"},
      {"Square meters", "1", "1"},
      {"Square mile", "2589988.110336", "1"},
      {"Square millimeter", "0.000001", "1"},
      {"Square Yard", "0.83612736", "1"}}},
    {"Energy", "Joule", "Calorie", 7,
     {{"British Thermal Unit", BTU, "1"},
      {"Calorie", "4.1868", "1"},
      {"Electron-Volts", "1.602176634e-19", "1"},
      {"Foot-Pound", FTLB, "1"},
      {"Joule", "1", "1"},
      {"Kilocalorie", "4186.8", "1"},
      {"Kilojoule", "1000", "1"}}},
    {"Length", "Meter", "Feet", 19,
     {{"Angstrom", "1e-10", "1"},
      {"Centimeters", "0.01", "1"},
      {"Chain", "20.1168", "1"},
      {"Fathom", "1.8288", "1"},
      {"Feet", "0.3048", "1"},
      {"Hand", "0.1016", "1"},
      {"Inch", "0.0254", "1"},
      {"Kilometers", "1000", "1"},
      {"Link", "0.201168", "1"},
      {"Meter", "1", "1"},
      {"Microns", "1e-6", "1"},
      {"Mile", "1609.344", "1"},
      {"Millimeters", "0.001", "1"},
      {"Nanometer", "1e-9", "1"},
      {"Nautical Miles", "1852", "1"},
      {"PICA", "0.0254", "6"},
      {"Rods", "5.0292", "1"},
      {"Span", "0.2286", "1"},
      {"Yard", "0.9144", "1"}}},
    {"Power", "Kilowatt", "Horsepower", 5,
     {{"BTU/minute", BTU, "60"},
      {"Foot-Pound/minute", FTLB, "60"},
      {"Horsepower", "745.69987158227022", "1"},
      {"Kilowatt", "1000", "1"},
      {"Watt", "1", "1"}}},
    {"Pressure", "Kilo Pascal", "Pound per square inch (PSI)", 6,
     {{"Atmosphere", "101325", "1"},
      {"Bar", "100000", "1"},
      {"Kilo Pascal", "1000", "1"},
      {"Millimeter of mercury", "133.322387415", "1"},
      {"Pascal", "1", "1"},
      {"Pound per square inch (PSI)", "6894.757293168", "1"}}},
    {"Temperature", "Degrees Celsius", "Degrees Fahrenheit", 3,
     {{"Degrees Celsius", "1", "1"}, {"Degrees Fahrenheit", "1", "1"}, {"Kelvin", "1", "1"}}},
    {"Time", "Hour", "Minute", 7,
     {{"Day", "86400", "1"},
      {"Hour", "3600", "1"},
      {"Microsecond", "1e-6", "1"},
      {"Millisecond", "0.001", "1"},
      {"Minute", "60", "1"},
      {"Second", "1", "1"},
      {"Week", "604800", "1"}}},
    {"Velocity", "Kilometer per hour", "Miles per hour", 7,
     {{"Centimeter per second", "0.01", "1"},
      {"Feet per second", "0.3048", "1"},
      {"Kilometer per hour", "1000", "3600"},
      {"Knots", "1852", "3600"},
      {"Mach (at std. atm)", "340.3", "1"},
      {"Meter per second", "1", "1"},
      {"Miles per hour", "0.44704", "1"}}},
    {"Volume", "Liter", "Gallon (US)", 14,
     {{"Cubic centimeter", "1e-6", "1"},
      {"Cubic feet", "0.028316846592", "1"},
      {"Cubic inch", "0.000016387064", "1"},
      {"Cubic meter", "1", "1"},
      {"Cubic yard", "0.764554857984", "1"},
      {"Fluid ounce (UK)", "0.0000284130625", "1"},
      {"Fluid ounce (US)", "0.0000295735295625", "1"},
      {"Gallon (UK)", "0.00454609", "1"},
      {"Gallon (US)", "0.003785411784", "1"},
      {"Liter", "0.001", "1"},
      {"Pint (UK)", "0.00056826125", "1"},
      {"Pint (US)", "0.000473176473", "1"},
      {"Quart (UK)", "0.0011365225", "1"},
      {"Quart (US)", "0.000946352946", "1"}}},
    {"Weight/Mass", "Kilogram", "Pound", 14,
     {{"Carat", "0.0002", "1"},
      {"Centigram", "0.00001", "1"},
      {"Decigram", "0.0001", "1"},
      {"Dekagram", "0.01", "1"},
      {"Gram", "0.001", "1"},
      {"Hectogram", "0.1", "1"},
      {"Kilogram", "1", "1"},
      {"Long ton", "1016.0469088", "1"},
      {"Milligram", "0.000001", "1"},
      {"Ounce", "0.028349523125", "1"},
      {"Pound", "0.45359237", "1"},
      {"Short ton", "907.18474", "1"},
      {"Stone", "6.35029318", "1"},
      {"Tonne", "1000", "1"}}},
};

#define NCATS ((int)(sizeof CATS / sizeof CATS[0]))

static const Category *find_cat(const char *name)
{
    if (!name)
        return NULL;
    for (int i = 0; i < NCATS; i++)
        if (strcmp(CATS[i].name, name) == 0)
            return &CATS[i];
    return NULL;
}

static const Unit *find_unit(const Category *c, const char *name)
{
    if (!name)
        return NULL;
    for (int i = 0; i < c->n; i++)
        if (strcmp(c->units[i].name, name) == 0)
            return &c->units[i];
    return NULL;
}

int units_category_count(void) { return NCATS; }
const char *units_category(int i) { return i >= 0 && i < NCATS ? CATS[i].name : NULL; }

int units_count(const char *category)
{
    const Category *c = find_cat(category);
    return c ? c->n : -1;
}

const char *units_name(const char *category, int i)
{
    const Category *c = find_cat(category);
    return c && i >= 0 && i < c->n ? c->units[i].name : NULL;
}

void units_default(const char *category, const char **from, const char **to)
{
    const Category *c = find_cat(category);
    if (from)
        *from = c ? c->def_from : NULL;
    if (to)
        *to = c ? c->def_to : NULL;
}

/* ------------------------------------------------------------------ exact rationals */

typedef struct {
    mpd_t *n, *d;
} Frac;

static mpd_context_t XC; /* exact context */

static Frac fr_new(const char *n, const char *d)
{
    Frac f = {mf_new(), mf_new()};
    mf_set_str(f.n, n);
    mf_set_str(f.d, d);
    return f;
}

static void fr_free(Frac *f)
{
    mf_free(f->n);
    mf_free(f->d);
    f->n = f->d = NULL;
}

static void fr_mul(Frac *r, const Frac *a, const Frac *b) /* r may alias a */
{
    uint32_t st = 0;
    mpd_qmul(r->n, a->n, b->n, &XC, &st);
    mpd_qmul(r->d, a->d, b->d, &XC, &st);
}

static void fr_div(Frac *r, const Frac *a, const Frac *b)
{
    uint32_t st = 0;
    mpd_t *n = mf_new(), *d = mf_new();
    mpd_qmul(n, a->n, b->d, &XC, &st);
    mpd_qmul(d, a->d, b->n, &XC, &st);
    mf_copy(r->n, n);
    mf_copy(r->d, d);
    mpd_del(n);
    mpd_del(d);
}

static void fr_add(Frac *r, const Frac *a, const Frac *b, bool sub)
{
    uint32_t st = 0;
    mpd_t *x = mf_new(), *y = mf_new(), *d = mf_new();
    mpd_qmul(x, a->n, b->d, &XC, &st);
    mpd_qmul(y, b->n, a->d, &XC, &st);
    mpd_qmul(d, a->d, b->d, &XC, &st);
    if (sub)
        mpd_qsub(r->n, x, y, &XC, &st);
    else
        mpd_qadd(r->n, x, y, &XC, &st);
    mf_copy(r->d, d);
    mpd_del(x);
    mpd_del(y);
    mpd_del(d);
}

static void fr_add_s(Frac *r, const Frac *a, const char *n, const char *d, bool sub)
{
    Frac b = fr_new(n, d);
    fr_add(r, a, &b, sub);
    fr_free(&b);
}

static void fr_mul_s(Frac *r, const Frac *a, const char *n, const char *d)
{
    Frac b = fr_new(n, d);
    fr_mul(r, a, &b);
    fr_free(&b);
}

static void to_kelvin(Frac *v, const char *unit)
{
    if (strcmp(unit, "Degrees Celsius") == 0) {
        fr_add_s(v, v, "273.15", "1", false);
    } else if (strcmp(unit, "Degrees Fahrenheit") == 0) {
        fr_add_s(v, v, "32", "1", true);
        fr_mul_s(v, v, "5", "9");
        fr_add_s(v, v, "273.15", "1", false);
    }
}

static void from_kelvin(Frac *k, const char *unit)
{
    if (strcmp(unit, "Degrees Celsius") == 0) {
        fr_add_s(k, k, "273.15", "1", true);
    } else if (strcmp(unit, "Degrees Fahrenheit") == 0) {
        fr_add_s(k, k, "273.15", "1", true);
        fr_mul_s(k, k, "9", "5");
        fr_add_s(k, k, "32", "1", false);
    }
}

/* Make n and d integers with exponent 0 (same ratio), like Fraction's numerator/denominator. */
static void fr_integralize(Frac *f)
{
    uint32_t st = 0;
    mpd_ssize_t k = 0;
    if (!mpd_iszero(f->n) && -f->n->exp > k)
        k = -f->n->exp;
    if (-f->d->exp > k)
        k = -f->d->exp;
    f->n->exp += k;
    f->d->exp += k;
    mpd_t *t = mf_new();
    if (mpd_iszero(f->n))
        mf_set_i64(f->n, 0);
    else {
        mpd_qrescale(t, f->n, 0, &XC, &st);
        mf_copy(f->n, t);
    }
    mpd_qrescale(t, f->d, 0, &XC, &st);
    mf_copy(f->d, t);
    mpd_del(t);
}

char *units_convert(const char *value, const char *category, const char *from, const char *to)
{
    const Category *c = find_cat(category);
    if (!c || !value)
        return NULL;
    const Unit *fu = find_unit(c, from), *tu = find_unit(c, to);
    if (!fu || !tu)
        return NULL;
    mf_exact_ctx(&XC);
    mpd_t *val = mf_new();
    if (!mf_set_str(val, value) || !mpd_isfinite(val)) {
        mpd_del(val);
        return NULL;
    }
    Frac v = {val, mf_new()};
    mf_set_i64(v.d, 1);
    if (strcmp(category, "Temperature") == 0) {
        to_kelvin(&v, from);
        from_kelvin(&v, to);
    } else {
        Frac a = fr_new(fu->num, fu->den), b = fr_new(tu->num, tu->den);
        fr_mul(&v, &v, &a);
        fr_div(&v, &v, &b);
        fr_free(&a);
        fr_free(&b);
    }
    fr_integralize(&v);
    mpd_context_t ctx;
    mf_ctx(&ctx, PRECISION);
    uint32_t st = 0;
    mpd_t *out = mf_new();
    mpd_qdiv(out, v.n, v.d, &ctx, &st);
    fr_free(&v);
    char *s = NULL;
    if (!(st & MF_TRAPS)) {
        if (mpd_iszero(out))
            mf_set_i64(out, 0);
        s = mf_to_str(out);
    }
    mpd_del(out);
    return s;
}

char *units_format_number(const char *value, int sig)
{
    if (!value)
        return NULL;
    mpd_t *d = mf_new();
    if (!mf_set_str(d, value) || !mpd_isfinite(d)) {
        mpd_del(d);
        return NULL;
    }
    if (mpd_iszero(d)) {
        mpd_del(d);
        return xstrdup("0");
    }
    mpd_context_t ctx;
    mf_ctx(&ctx, sig);
    uint32_t st = 0;
    mpd_qplus(d, d, &ctx, &st);
    if (st & MF_TRAPS) {
        mpd_del(d);
        return NULL;
    }
    mpd_ssize_t adj = mpd_adjexp(d);
    bool neg = mpd_isnegative(d);
    mpd_t *c = mf_dup(d);
    c->exp = 0;
    mpd_set_positive(c);
    char *cd = mf_to_str(c);
    mpd_del(c);
    StrBuf b;
    sb_init(&b);
    if (neg)
        sb_putc(&b, '-');
    if (adj >= -10 && adj < sig) {
        /* format(d, "f"), then strip trailing zeros / point */
        mpd_ssize_t e = d->exp, len = (mpd_ssize_t)strlen(cd);
        if (e >= 0) {
            sb_puts(&b, cd);
            for (mpd_ssize_t i = 0; i < e; i++)
                sb_putc(&b, '0');
        } else {
            mpd_ssize_t pos = len + e;
            StrBuf f;
            sb_init(&f);
            if (pos > 0) {
                sb_putn(&f, cd, (size_t)pos);
                sb_putc(&f, '.');
                sb_puts(&f, cd + pos);
            } else {
                sb_puts(&f, "0.");
                for (mpd_ssize_t i = 0; i < -pos; i++)
                    sb_putc(&f, '0');
                sb_puts(&f, cd);
            }
            char *fs = sb_take(&f);
            size_t n = strlen(fs);
            while (n && fs[n - 1] == '0')
                n--;
            if (n && fs[n - 1] == '.')
                n--;
            sb_putn(&b, fs, n);
            free(fs);
        }
    } else {
        size_t n = strlen(cd);
        while (n > 1 && cd[n - 1] == '0')
            n--;
        sb_putc(&b, cd[0]);
        if (n > 1) {
            sb_putc(&b, '.');
            sb_putn(&b, cd + 1, n - 1);
        }
        sb_printf(&b, "e%c%lld", adj >= 0 ? '+' : '-', (long long)(adj >= 0 ? adj : -adj));
    }
    free(cd);
    mpd_del(d);
    return sb_take(&b);
}
