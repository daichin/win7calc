/* Unit conversion (Win7 calc "Unit conversion" panel).  C port of win7calc/units.py.
 * Returned `char *` strings are owned by the caller (free()). */
#ifndef WIN7CALC_UNITS_H
#define WIN7CALC_UNITS_H

int units_category_count(void);
const char *units_category(int i);                     /* in units.CATEGORIES order */
int units_count(const char *category);                 /* -1 for unknown category */
const char *units_name(const char *category, int i);
void units_default(const char *category, const char **from, const char **to);

/* Convert a plain number string; result rounded to 32 significant digits.
 * Returns NULL for an unparsable value or unknown category/unit. */
char *units_convert(const char *value, const char *category, const char *from, const char *to);
/* Win7-style display of a number string (units.format_number); NULL if not finite. */
char *units_format_number(const char *value, int sig);

#endif
