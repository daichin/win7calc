/* Number <-> display-string conversion following win32calc's rules.
 * C port of win7calc/numfmt.py.  All returned `char *` are caller-owned (free()). */
#ifndef WIN7CALC_NUMFMT_H
#define WIN7CALC_NUMFMT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <mpdecimal.h>

#define NF_MAX_ZEROS_AFTER_DECIMAL 2

/* Growable string buffer. */
typedef struct {
    char *s;
    size_t len, cap;
} StrBuf;
void sb_init(StrBuf *b);
void sb_putc(StrBuf *b, char c);
void sb_puts(StrBuf *b, const char *s);
void sb_putn(StrBuf *b, const char *s, size_t n);
void sb_printf(StrBuf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
char *sb_take(StrBuf *b); /* returns the malloc'd string; buffer is reset */
char *xstrdup(const char *s);
char *xasprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* group_int(digits, size, sep) */
char *nf_group_int(const char *digits, int size, char sep);
/* format_decimal(x, precision, grouping, fe) */
char *nf_format_decimal(const mpd_t *x, int precision, bool grouping, bool fe);
/* format_int(v, radix, bits, grouping) */
char *nf_format_int(int64_t v, int radix, int bits, bool grouping);
/* _RADIX_GROUP: group size / separator for a radix */
void nf_radix_group(int radix, int *size, char *sep);
/* format_entry(digits, neg, exp (NULL = None), exp_neg, grouping, group_size, group_sep) */
char *nf_format_entry(const char *digits, bool neg, const char *exp, bool exp_neg, bool grouping,
                      int group_size, char group_sep);
/* parse_number(text): "1,234.5", "1.e+5" -> new decimal, or NULL if invalid/non-finite. */
mpd_t *nf_parse_number(const char *text);

#endif
