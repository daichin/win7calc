/* Date calculation logic.  C port of win7calc/datecalc.py (proleptic Gregorian,
 * like Python's datetime.date). */
#include "datecalc.h"

#include "numfmt.h"

#include <stdlib.h>

static bool is_leap(long y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int date_days_in_month(int year, int month)
{
    static const int dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12)
        return 0;
    return month == 2 && is_leap(year) ? 29 : dim[month - 1];
}

static long floordiv(long a, long b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

/* Days since 0001-01-01 (== date.toordinal() - 1). */
static long to_days(long y, long m, long d)
{
    static const int before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    long y1 = y - 1;
    long n = y1 * 365 + floordiv(y1, 4) - floordiv(y1, 100) + floordiv(y1, 400);
    n += before[m - 1] + (m > 2 && is_leap(y)) + d - 1;
    return n;
}

static CDate from_days(long n)
{
    /* inverse of to_days for n >= 0 */
    long n400 = n / 146097, r = n % 146097;
    long n100 = r / 36524;
    r %= 36524;
    long n4 = r / 1461;
    r %= 1461;
    long n1 = r / 365;
    r %= 365;
    long year = n400 * 400 + n100 * 100 + n4 * 4 + n1 + 1;
    CDate out;
    if (n1 == 4 || n100 == 4) {
        out.year = (int)(year - 1);
        out.month = 12;
        out.day = 31;
        return out;
    }
    int m = 1;
    while (r >= date_days_in_month((int)year, m)) {
        r -= date_days_in_month((int)year, m);
        m++;
    }
    out.year = (int)year;
    out.month = m;
    out.day = (int)r + 1;
    return out;
}

static int cmp_date(CDate a, CDate b)
{
    if (a.year != b.year)
        return a.year < b.year ? -1 : 1;
    if (a.month != b.month)
        return a.month < b.month ? -1 : 1;
    if (a.day != b.day)
        return a.day < b.day ? -1 : 1;
    return 0;
}

/* _add_months without the bounds check (callers check). */
static CDate add_months_raw(CDate d, long months, long *year_out)
{
    long total = (long)d.year * 12 + (d.month - 1) + months;
    long y = floordiv(total, 12), m0 = total - y * 12;
    *year_out = y;
    CDate r = {(int)y, (int)m0 + 1, 0};
    int dim = date_days_in_month((int)y, (int)m0 + 1);
    r.day = d.day < dim ? d.day : dim;
    return r;
}

void date_difference(CDate d1, CDate d2, int *years, int *months, int *weeks, int *days,
                     long *total_days)
{
    CDate a = d1, b = d2;
    if (cmp_date(a, b) > 0) {
        a = d2;
        b = d1;
    }
    long y;
    long mo = (long)(b.year - a.year) * 12 + (b.month - a.month);
    if (cmp_date(add_months_raw(a, mo, &y), b) > 0)
        mo--;
    CDate am = add_months_raw(a, mo, &y);
    long rem = to_days(b.year, b.month, b.day) - to_days(am.year, am.month, am.day);
    if (years)
        *years = (int)floordiv(mo, 12);
    if (months)
        *months = (int)(mo - floordiv(mo, 12) * 12);
    if (weeks)
        *weeks = (int)floordiv(rem, 7);
    if (days)
        *days = (int)(rem - floordiv(rem, 7) * 7);
    if (total_days)
        *total_days = to_days(b.year, b.month, b.day) - to_days(a.year, a.month, a.day);
}

static void plural(StrBuf *b, long n, const char *word)
{
    sb_printf(b, "%ld %s%s", n, word, n == 1 ? "" : "s");
}

char *date_format_difference(int years, int months, int weeks, int days)
{
    StrBuf b;
    sb_init(&b);
    const int vals[4] = {years, months, weeks, days};
    const char *words[4] = {"year", "month", "week", "day"};
    bool any = false;
    for (int i = 0; i < 4; i++) {
        if (!vals[i])
            continue;
        if (any)
            sb_puts(&b, ", ");
        plural(&b, vals[i], words[i]);
        any = true;
    }
    if (!any)
        sb_puts(&b, "Same dates");
    return sb_take(&b);
}

char *date_format_total_days(long total_days)
{
    StrBuf b;
    sb_init(&b);
    if (total_days == 0)
        sb_puts(&b, "Same dates");
    else
        plural(&b, total_days, "day");
    return sb_take(&b);
}

bool date_add(CDate d, int years, int months, int days, bool subtract, CDate *out)
{
    long sign = subtract ? -1 : 1;
    long y;
    CDate r = add_months_raw(d, sign * ((long)years * 12 + months), &y);
    if (y < DATE_MIN_YEAR || y > DATE_MAX_YEAR)
        return false;
    long n = to_days(r.year, r.month, r.day) + sign * (long)days;
    /* outside date.min..date.max -> OverflowError; then the 1601..9999 check */
    if (n < to_days(DATE_MIN_YEAR, 1, 1) || n > to_days(DATE_MAX_YEAR, 12, 31))
        return false;
    if (out)
        *out = from_days(n);
    return true;
}
