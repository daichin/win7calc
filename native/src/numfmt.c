/* Number <-> display-string conversion.  C port of win7calc/numfmt.py. */
#include "numfmt.h"

#include "mathfn.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ strings */

void sb_init(StrBuf *b)
{
    b->s = NULL;
    b->len = b->cap = 0;
}

static void sb_reserve(StrBuf *b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap)
        return;
    size_t cap = b->cap ? b->cap : 32;
    while (cap < b->len + extra + 1)
        cap *= 2;
    char *s = realloc(b->s, cap);
    if (!s)
        abort();
    b->s = s;
    b->cap = cap;
}

void sb_putn(StrBuf *b, const char *s, size_t n)
{
    sb_reserve(b, n);
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = 0;
}

void sb_putc(StrBuf *b, char c) { sb_putn(b, &c, 1); }
void sb_puts(StrBuf *b, const char *s) { sb_putn(b, s, strlen(s)); }

void sb_printf(StrBuf *b, const char *fmt, ...)
{
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    sb_reserve(b, (size_t)n);
    vsnprintf(b->s + b->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)n;
}

char *sb_take(StrBuf *b)
{
    char *s = b->s;
    if (!s)
        s = xstrdup("");
    sb_init(b);
    return s;
}

char *xstrdup(const char *s)
{
    char *r = strdup(s);
    if (!r)
        abort();
    return r;
}

char *xasprintf(const char *fmt, ...)
{
    StrBuf b;
    sb_init(&b);
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    sb_reserve(&b, (size_t)n);
    va_start(ap, fmt);
    vsnprintf(b.s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    b.len = (size_t)n;
    return sb_take(&b);
}

/* ------------------------------------------------------------------ formatting */

char *nf_group_int(const char *digits, int size, char sep)
{
    /* The first group holds the leftover digits (Python peels groups off the right). */
    size_t n = strlen(digits), sz = size > 0 ? (size_t)size : 1;
    size_t first = n % sz ? n % sz : sz;
    StrBuf b;
    sb_init(&b);
    for (size_t i = 0; i < n; i++) {
        if (i >= first && (i - first) % sz == 0)
            sb_putc(&b, sep);
        sb_putc(&b, digits[i]);
    }
    return sb_take(&b);
}

/* _sci(sign, digits, exp) */
static char *sci(bool neg, const char *digits, mpd_ssize_t exp)
{
    StrBuf b;
    sb_init(&b);
    if (neg)
        sb_putc(&b, '-');
    sb_putc(&b, digits[0]);
    sb_putc(&b, '.');
    size_t n = strlen(digits);
    while (n > 1 && digits[n - 1] == '0')
        n--;
    if (n > 1)
        sb_putn(&b, digits + 1, n - 1);
    sb_printf(&b, "e%c%lld", exp >= 0 ? '+' : '-', (long long)(exp >= 0 ? exp : -exp));
    return sb_take(&b);
}

static void fmt_ctx(mpd_context_t *c, int round)
{
    mf_ctx(c, 100); /* localcontext() copy of the default context, prec=100 */
    c->round = round;
}

/* Coefficient digits of a finite decimal as a string. */
static char *coeff_digits(const mpd_t *r)
{
    mpd_t *t = mf_dup(r);
    t->exp = 0;
    mpd_set_positive(t);
    char *s = mf_to_str(t);
    mpd_del(t);
    return s;
}

static char *format_decimal(const mpd_t *x, int precision, bool grouping, bool fe)
{
    if (mpd_iszero(x))
        return xstrdup(fe ? "0.e+0" : "0");
    bool neg = mpd_isnegative(x);
    mpd_context_t c;
    fmt_ctx(&c, MPD_ROUND_HALF_UP);
    uint32_t st = 0;
    mpd_t *ax = mf_new(), *q = mf_new(), *r = mf_new();
    mpd_qabs(ax, x, &c, &st);
    mf_set_i64(q, 1);
    q->exp = mpd_adjexp(ax) - precision + 1;
    mpd_qquantize(r, ax, q, &c, &st);
    if (mpd_adjexp(r) != mpd_adjexp(ax)) {
        mf_set_i64(q, 1);
        q->exp = mpd_adjexp(r) - precision + 1;
        mpd_qquantize(r, r, q, &c, &st);
    }
    mpd_ssize_t exp = mpd_adjexp(r);
    char *cd = coeff_digits(r);
    /* digits = digits.ljust(precision, "0")[:precision]; sig = digits.rstrip("0") or "0" */
    char *digits = calloc((size_t)precision + 1, 1);
    if (!digits)
        abort();
    size_t cl = strlen(cd);
    for (int i = 0; i < precision; i++)
        digits[i] = (size_t)i < cl ? cd[i] : '0';
    free(cd);
    size_t sl = strlen(digits);
    while (sl > 0 && digits[sl - 1] == '0')
        sl--;
    char *sig = sl ? strndup(digits, sl) : xstrdup("0");
    free(digits);
    char *out = NULL;

    if (fe || exp >= precision) {
        out = sci(neg, sig, exp);
        goto done;
    }
    if (exp < 0) {
        long frac_len = -exp - 1 + (long)strlen(sig);
        if (frac_len > precision) {
            if (-exp - 1 > NF_MAX_ZEROS_AFTER_DECIMAL) {
                out = sci(neg, sig, exp);
                goto done;
            }
            mpd_t *r2 = mf_new();
            mf_set_i64(q, 1);
            q->exp = -precision;
            mpd_qquantize(r2, ax, q, &c, &st);
            if (mpd_iszero(r2)) {
                mpd_del(r2);
                out = sci(neg, sig, exp);
                goto done;
            }
            if (neg)
                mpd_qminus(r2, r2, &c, &st);
            out = format_decimal(r2, precision, grouping, false);
            mpd_del(r2);
            goto done;
        }
        StrBuf b;
        sb_init(&b);
        if (neg)
            sb_putc(&b, '-');
        sb_puts(&b, "0.");
        for (long i = 0; i < -exp - 1; i++)
            sb_putc(&b, '0');
        sb_puts(&b, sig);
        out = sb_take(&b);
        goto done;
    }
    {
        size_t il = (size_t)exp + 1, sgl = strlen(sig);
        char *ip = calloc(il + 1, 1);
        if (!ip)
            abort();
        for (size_t i = 0; i < il; i++)
            ip[i] = i < sgl ? sig[i] : '0';
        const char *frac = sgl > il ? sig + il : "";
        if (grouping) {
            char *g = nf_group_int(ip, 3, ',');
            free(ip);
            ip = g;
        }
        out = *frac ? xasprintf("%s%s.%s", neg ? "-" : "", ip, frac)
                    : xasprintf("%s%s", neg ? "-" : "", ip);
        free(ip);
    }
done:
    free(sig);
    mpd_del(ax);
    mpd_del(q);
    mpd_del(r);
    return out;
}

char *nf_format_decimal(const mpd_t *x, int precision, bool grouping, bool fe)
{
    return format_decimal(x, precision, grouping, fe);
}

void nf_radix_group(int radix, int *size, char *sep)
{
    switch (radix) {
    case 16: *size = 4; *sep = ' '; break;
    case 8: *size = 3; *sep = ' '; break;
    case 2: *size = 4; *sep = ' '; break;
    default: *size = 3; *sep = ','; break;
    }
}

char *nf_format_int(int64_t v, int radix, int bits, bool grouping)
{
    char buf[80];
    if (radix == 10) {
        uint64_t a = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
        snprintf(buf, sizeof buf, "%llu", (unsigned long long)a);
        char *s = grouping ? nf_group_int(buf, 3, ',') : xstrdup(buf);
        if (v < 0) {
            char *t = xasprintf("-%s", s);
            free(s);
            s = t;
        }
        return s;
    }
    uint64_t mask = bits >= 64 ? ~(uint64_t)0 : (((uint64_t)1 << bits) - 1);
    uint64_t u = (uint64_t)v & mask;
    if (radix == 16) {
        snprintf(buf, sizeof buf, "%llX", (unsigned long long)u);
    } else if (radix == 8) {
        snprintf(buf, sizeof buf, "%llo", (unsigned long long)u);
    } else {
        int n = 0;
        char tmp[70];
        do {
            tmp[n++] = (char)('0' + (u & 1));
            u >>= 1;
        } while (u);
        for (int i = 0; i < n; i++)
            buf[i] = tmp[n - 1 - i];
        buf[n] = 0;
    }
    if (grouping) {
        int size;
        char sep;
        nf_radix_group(radix, &size, &sep);
        return nf_group_int(buf, size, sep);
    }
    return xstrdup(buf);
}

char *nf_format_entry(const char *digits, bool neg, const char *exp, bool exp_neg, bool grouping,
                      int group_size, char group_sep)
{
    const char *dot = strchr(digits, '.');
    char *ip = dot ? strndup(digits, (size_t)(dot - digits)) : xstrdup(digits);
    if (!ip)
        abort();
    if (grouping) {
        char *g = nf_group_int(ip, group_size, group_sep);
        free(ip);
        ip = g;
    }
    StrBuf b;
    sb_init(&b);
    if (neg)
        sb_putc(&b, '-');
    sb_puts(&b, ip);
    free(ip);
    if (dot) {
        sb_putc(&b, '.');
        sb_puts(&b, dot + 1);
    }
    if (exp) {
        if (!dot)
            sb_putc(&b, '.');
        sb_putc(&b, 'e');
        sb_putc(&b, exp_neg ? '-' : '+');
        sb_puts(&b, *exp ? exp : "0");
    }
    return sb_take(&b);
}

mpd_t *nf_parse_number(const char *text)
{
    StrBuf b;
    sb_init(&b);
    for (const char *p = text; *p; p++)
        if (*p != ',' && *p != ' ')
            sb_putc(&b, *p);
    char *t = sb_take(&b);
    /* strip() then the remaining whitespace check: Decimal() strips as well */
    const char *s = t;
    while (*s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f')
        s++;
    if (!*s) {
        free(t);
        return NULL;
    }
    mpd_t *d = mf_new();
    bool ok = mf_set_str(d, s);
    free(t);
    if (!ok || !mpd_isfinite(d)) {
        mpd_del(d);
        return NULL;
    }
    return d;
}
