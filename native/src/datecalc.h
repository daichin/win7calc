/* Date calculation logic (Win7 calc "Date calculation" panel).  C port of win7calc/datecalc.py.
 * Returned `char *` strings are owned by the caller (free()). */
#ifndef WIN7CALC_DATECALC_H
#define WIN7CALC_DATECALC_H

#include <stdbool.h>

#define DATE_MIN_YEAR 1601
#define DATE_MAX_YEAR 9999
#define DATE_OUT_OF_BOUND "Date out of bound"

typedef struct { int year, month, day; } CDate;

/* Order-independent difference between two dates. */
void date_difference(CDate d1, CDate d2, int *years, int *months, int *weeks, int *days,
                     long *total_days);
char *date_format_difference(int years, int months, int weeks, int days);
char *date_format_total_days(long total_days);
/* Shift by years+months (day clamped), then days.  false = "Date out of bound". */
bool date_add(CDate d, int years, int months, int days, bool subtract, CDate *out);
int date_days_in_month(int year, int month);

#endif
