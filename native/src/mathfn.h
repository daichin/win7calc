/* High-precision math on libmpdec decimals, mirroring win32calc's error semantics.
 * C port of win7calc/mathfn.py.
 *
 * Conventions: every function writes its result into a caller-owned `mpd_t *r`
 * (allocated with mf_new()) and returns NULL on success or one of the MF_ERR_*
 * messages on failure (r is then unspecified).  Decimal-context traps
 * (InvalidOperation / DivisionByZero / Overflow), which Python surfaces as
 * ArithmeticError, are reported as MF_ERR_INVALID, except inside power() where
 * Overflow becomes MF_ERR_OVERFLOW -- exactly like the Python code.
 */
#ifndef WIN7CALC_MATHFN_H
#define WIN7CALC_MATHFN_H

#include <stdbool.h>
#include <stdint.h>
#include <mpdecimal.h>

#define MF_WORK_PREC 50
#define MF_MAX_EXP 9999
#define MF_DEFAULT_PREC 28 /* Python's default decimal context precision */

#define MF_ERR_DIV0 "Cannot divide by zero"
#define MF_ERR_INVALID "Invalid input"
#define MF_ERR_UNDEFINED "Result is undefined"
#define MF_ERR_OVERFLOW "Overflow"

/* Status bits Python traps (InvalidOperation, DivisionByZero, Overflow). */
#define MF_TRAPS (MPD_IEEE_Invalid_operation | MPD_Division_by_zero | MPD_Overflow)

typedef enum { MF_DEG, MF_RAD, MF_GRAD } MfAngle;

/* Context(prec, Emax=999999, Emin=-999999, ROUND_HALF_EVEN), like mathfn.ctx(). */
void mf_ctx(mpd_context_t *c, mpd_ssize_t prec);
/* A context large enough for exact integer/rational arithmetic. */
void mf_exact_ctx(mpd_context_t *c);

/* Allocation helpers (abort on OOM). */
mpd_t *mf_new(void);
mpd_t *mf_dup(const mpd_t *a);
void mf_free(mpd_t *a); /* NULL-safe */
void mf_copy(mpd_t *r, const mpd_t *a);
void mf_set_i64(mpd_t *r, int64_t v);           /* exact, exponent 0 */
/* Decimal(str) semantics: surrounding whitespace and '_' ignored; exact.
 * Returns false (r untouched) for syntax errors.  Accepts inf/nan. */
bool mf_set_str(mpd_t *r, const char *s);
char *mf_to_str(const mpd_t *a);                 /* str(Decimal), caller frees */
const mpd_t *mf_pi(void);                        /* the 80-digit PI constant */

/* Python int(x) (truncation) wrapped to a signed `bits`-wide two's complement int. */
int64_t mf_to_wrapped_int(const mpd_t *x, int bits);
/* Python int(x) % m (floor modulo) for an integral x and small m > 0. */
int mf_int_mod(const mpd_t *x, int m);
int64_t mf_wrap(int64_t v, int bits);

const char *mf_check(mpd_t *r, const mpd_t *x);

const char *mf_add(mpd_t *r, const mpd_t *a, const mpd_t *b);
const char *mf_sub(mpd_t *r, const mpd_t *a, const mpd_t *b);
const char *mf_mul(mpd_t *r, const mpd_t *a, const mpd_t *b);
const char *mf_div(mpd_t *r, const mpd_t *a, const mpd_t *b);
const char *mf_mod(mpd_t *r, const mpd_t *a, const mpd_t *b);
const char *mf_power(mpd_t *r, const mpd_t *x, const mpd_t *y);
const char *mf_root(mpd_t *r, const mpd_t *x, const mpd_t *y);

const char *mf_sqrt(mpd_t *r, const mpd_t *x);
const char *mf_cbrt(mpd_t *r, const mpd_t *x);
const char *mf_reciprocal(mpd_t *r, const mpd_t *x);
const char *mf_square(mpd_t *r, const mpd_t *x);
const char *mf_cube(mpd_t *r, const mpd_t *x);
const char *mf_ln(mpd_t *r, const mpd_t *x);
const char *mf_log10(mpd_t *r, const mpd_t *x);
const char *mf_exp(mpd_t *r, const mpd_t *x);
const char *mf_pow10(mpd_t *r, const mpd_t *x);
const char *mf_factorial(mpd_t *r, const mpd_t *x);
/* int_part's trailing "+ D0" runs at `prec` digits (MF_WORK_PREC). */
const char *mf_int_part(mpd_t *r, const mpd_t *x, mpd_ssize_t prec);
const char *mf_frac_part(mpd_t *r, const mpd_t *x);
const char *mf_dms(mpd_t *r, const mpd_t *x);
const char *mf_degrees(mpd_t *r, const mpd_t *x);

const char *mf_sin(mpd_t *r, const mpd_t *x, MfAngle u);
const char *mf_cos(mpd_t *r, const mpd_t *x, MfAngle u);
const char *mf_tan(mpd_t *r, const mpd_t *x, MfAngle u);
const char *mf_asin(mpd_t *r, const mpd_t *x, MfAngle u);
const char *mf_acos(mpd_t *r, const mpd_t *x, MfAngle u);
const char *mf_atan(mpd_t *r, const mpd_t *x, MfAngle u);
const char *mf_sinh(mpd_t *r, const mpd_t *x);
const char *mf_cosh(mpd_t *r, const mpd_t *x);
const char *mf_tanh(mpd_t *r, const mpd_t *x);
const char *mf_asinh(mpd_t *r, const mpd_t *x);
const char *mf_acosh(mpd_t *r, const mpd_t *x);
const char *mf_atanh(mpd_t *r, const mpd_t *x);

/* Unary minus in a context of the given precision (Python `-x`). */
void mf_negate(mpd_t *r, const mpd_t *x, mpd_ssize_t prec);

/* CPython's math.gamma (Lanczos), bit-for-bit; sets *err to 0 ok, 1 ValueError, 2 OverflowError. */
double mf_py_gamma(double x, int *err);
/* repr(float) digits: writes Python's repr() of a finite double into buf. */
void mf_py_float_repr(double x, char *buf, int n);

#endif
