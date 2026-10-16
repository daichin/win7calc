/* Worksheet math (mortgage, vehicle lease, fuel economy).  C port of win7calc/worksheets.py.
 * Returned `char *` strings are owned by the caller (free()). */
#ifndef WIN7CALC_WORKSHEETS_H
#define WIN7CALC_WORKSHEETS_H

#define WS_BLANK "This value cannot be blank."
#define WS_INVALID "This value is not valid."
#define WS_UNABLE "Unable to calculate"

int ws_count(void);
const char *ws_name(int i);                              /* "Mortgage", ... */
int ws_field_count(const char *ws);
const char *ws_field(const char *ws, int i);             /* all fields, spec order */
int ws_calculable_count(const char *ws);
const char *ws_calculable(const char *ws, int i);        /* the "Calculate" choices */

/* Solve for `target`.  names/values are parallel arrays of n field inputs
 * (a NULL or empty value means blank).  On success returns a plain number
 * string at full precision.  On failure returns NULL and sets *err_msg to one
 * of the WS_* messages and *err_field to the offending field name (or NULL). */
char *ws_solve(const char *ws, const char *target, const char *const *names,
               const char *const *values, int n, const char **err_msg, const char **err_field);

#endif
