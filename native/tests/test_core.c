/* Unit tests for the calculator core: a port of tests/test_*.py (57 tests). */
#include "datecalc.h"
#include "engine.h"
#include "units.h"
#include "worksheets.h"
#include "mathfn.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_test_failed, g_passed, g_failed;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);               \
            g_test_failed = 1;                                                                     \
        }                                                                                          \
    } while (0)

#define EQ_STR(actual, expected)                                                                   \
    do {                                                                                           \
        const char *a_ = (actual), *e_ = (expected);                                               \
        if (!a_ || strcmp(a_, e_) != 0) {                                                          \
            fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"  (%s)\n", __FILE__, __LINE__, e_,  \
                    a_ ? a_ : "(null)", #actual);                                                  \
            g_test_failed = 1;                                                                     \
        }                                                                                          \
    } while (0)

/* Compare and free a heap string. */
#define EQ_OWN(actual, expected)                                                                   \
    do {                                                                                           \
        char *o_ = (actual);                                                                       \
        EQ_STR(o_, expected);                                                                      \
        free(o_);                                                                                  \
    } while (0)

#define EQ_INT(actual, expected)                                                                   \
    do {                                                                                           \
        long long a_ = (long long)(actual), e_ = (long long)(expected);                            \
        if (a_ != e_) {                                                                            \
            fprintf(stderr, "%s:%d: expected %lld, got %lld  (%s)\n", __FILE__, __LINE__, e_, a_,  \
                    #actual);                                                                      \
            g_test_failed = 1;                                                                     \
        }                                                                                          \
    } while (0)

#define ALMOST(actual, expected, places)                                                           \
    do {                                                                                           \
        double a_ = (actual), e_ = (expected);                                                     \
        if (!(round(fabs(a_ - e_) * pow(10, places)) == 0)) {                                      \
            fprintf(stderr, "%s:%d: expected %.12g ~= %.12g (places=%d)  (%s)\n", __FILE__,        \
                    __LINE__, a_, e_, places, #actual);                                            \
            g_test_failed = 1;                                                                     \
        }                                                                                          \
    } while (0)


/* Decimal equality (Python's assertEqual on Decimal values); frees `actual`. */
#define EQ_DEC(actual, expected)                                                                   \
    do {                                                                                           \
        char *o_ = (actual);                                                                       \
        if (!o_ || !dec_eq(o_, expected)) {                                                        \
            fprintf(stderr, "%s:%d: expected Decimal %s, got \"%s\"  (%s)\n", __FILE__,          \
                    __LINE__, expected, o_ ? o_ : "(null)", #actual);                              \
            g_test_failed = 1;                                                                     \
        }                                                                                          \
        free(o_);                                                                                  \
    } while (0)

static bool dec_eq(const char *a, const char *b)
{
    mpd_t *x = mf_new(), *y = mf_new();
    bool ok = mf_set_str(x, a) && mf_set_str(y, b);
    uint32_t st = 0;
    ok = ok && mpd_qcmp(x, y, &st) == 0;
    mpd_del(x);
    mpd_del(y);
    return ok;
}

static void run_test(const char *name, void (*fn)(void))
{
    g_test_failed = 0;
    fn();
    if (g_test_failed) {
        fprintf(stderr, "FAIL: %s\n", name);
        g_failed++;
    } else {
        g_passed++;
    }
}
#define RUN(fn) run_test(#fn, fn)

/* ------------------------------------------------------------------ engine helpers */

static const struct {
    const char *name;
    int id;
} NAMES[] = {
    {"+", CMD_ADD}, {"-", CMD_SUB}, {"*", CMD_MUL}, {"/", CMD_DIV}, {"=", CMD_EQU},
    {".", CMD_POINT}, {"(", CMD_OPENP}, {")", CMD_CLOSEP}, {"%", CMD_PERCENT}, {"~", CMD_SIGN},
    {"<", CMD_BACK}, {"C", CMD_CLEAR}, {"E", CMD_CE}, {"^", CMD_PWR}, {"r", CMD_SQRT},
    {"MS", CMD_MS}, {"MR", CMD_MR}, {"MC", CMD_MC}, {"MPLUS", CMD_MPLUS}, {"MMINUS", CMD_MMINUS},
    {"EXP", CMD_EXP}, {"ADD", CMD_ADD}, {"EQU", CMD_EQU}, {"SIN", CMD_SIN}, {"COS", CMD_COS},
    {"TAN", CMD_TAN}, {"INV", CMD_INV}, {"LN", CMD_LN}, {"RAD", CMD_RAD}, {"FACT", CMD_FACT},
    {"CUBEROOT", CMD_CUBEROOT}, {"ROOT", CMD_ROOT}, {"POW10", CMD_POW10}, {"LOG", CMD_LOG},
    {"MOD", CMD_MOD}, {"INT", CMD_INT}, {"DMS", CMD_DMS}, {"FE", CMD_FE}, {"PI", CMD_PI},
    {"HEX", CMD_HEX}, {"BYTE", CMD_BYTE}, {"AND", CMD_AND}, {"OR", CMD_OR}, {"LSH", CMD_LSH},
    {"NOT", CMD_NOT}, {"ROR", CMD_ROR}, {"BIN", CMD_BIN}, {"STAT_ADD", CMD_STAT_ADD},
    {"STAT_AVE", CMD_STAT_AVE}, {"STAT_SUM", CMD_STAT_SUM}, {"STAT_SDEV", CMD_STAT_SDEV},
    {"STAT_CAD", CMD_STAT_CAD},
};

static int cmd_id(const char *k)
{
    for (size_t i = 0; i < sizeof NAMES / sizeof NAMES[0]; i++)
        if (strcmp(NAMES[i].name, k) == 0)
            return NAMES[i].id;
    fprintf(stderr, "unknown key %s\n", k);
    abort();
}

/* run(keys, mode, grouping): keys separated by spaces; digit runs are typed digit by digit. */
static Engine *run_g(const char *keys, CalcMode mode, bool grouping)
{
    Engine *c = engine_new();
    engine_set_mode(c, mode);
    engine_set_grouping(c, grouping);
    char *dup = strdup(keys), *save = NULL;
    for (char *k = strtok_r(dup, " ", &save); k; k = strtok_r(NULL, " ", &save)) {
        bool digits = true;
        for (const char *p = k; *p; p++)
            if (!isdigit((unsigned char)*p))
                digits = false;
        if (digits) {
            for (const char *p = k; *p; p++)
                engine_command(c, CMD_DIGIT0 + (*p - '0'));
        } else {
            engine_command(c, cmd_id(k));
        }
    }
    free(dup);
    return c;
}

static Engine *run(const char *keys, CalcMode mode) { return run_g(keys, mode, false); }

/* display of a fresh run, freeing the engine */
static char *disp(const char *keys, CalcMode mode)
{
    Engine *c = run(keys, mode);
    char *s = strdup(engine_display(c));
    engine_free(c);
    return s;
}

/* ------------------------------------------------------------------ StandardMode */

static void test_no_precedence(void) { EQ_OWN(disp("2 + 3 * 4 =", MODE_STD), "20"); }

static void test_intermediate_result_shown(void)
{
    Engine *c = run("2 + 3 +", MODE_STD);
    EQ_STR(engine_display(c), "5");
    EQ_STR(engine_expression(c), "2 + 3 +");
    engine_free(c);
}

static void test_repeat_equals(void) { EQ_OWN(disp("5 + 3 = = =", MODE_STD), "14"); }
static void test_new_number_repeats_last_op(void) { EQ_OWN(disp("2 + 3 = 4 =", MODE_STD), "7"); }
static void test_operand_defaults_to_display(void) { EQ_OWN(disp("5 + =", MODE_STD), "10"); }
static void test_operator_replacement(void) { EQ_OWN(disp("8 - * 2 =", MODE_STD), "16"); }

static void test_division_precision(void)
{
    EQ_OWN(disp("1 / 3 =", MODE_STD), "0.3333333333333333");
    EQ_OWN(disp("2 / 3 =", MODE_STD), "0.6666666666666667");
}

static void test_divide_by_zero(void)
{
    EQ_OWN(disp("5 / 0 =", MODE_STD), "Cannot divide by zero");
    EQ_OWN(disp("0 / 0 =", MODE_STD), "Result is undefined");
    EQ_OWN(disp("5 / 0 = 7", MODE_STD), "7");
}

static void test_sqrt_expression(void)
{
    Engine *c = run("9 + 16 r", MODE_STD);
    EQ_STR(engine_display(c), "4");
    EQ_STR(engine_expression(c), "9 + sqrt(16)");
    engine_command(c, CMD_EQU);
    EQ_STR(engine_display(c), "13");
    EQ_INT(engine_history_count(c), 1);
    EQ_STR(engine_history_text(c, engine_history_count(c) - 1), "9 + sqrt(16)");
    char *cur = engine_current_value(c);
    EQ_STR(engine_history_value(c, engine_history_count(c) - 1), cur);
    free(cur);
    engine_free(c);
}

static void test_memory_keeps_expression(void)
{
    Engine *c = run("12 + 7 r MS", MODE_STD);
    EQ_STR(engine_expression(c), "12 + sqrt(7)");
    engine_free(c);
}

static void test_invalid_sqrt(void) { EQ_OWN(disp("4 ~ r", MODE_STD), "Invalid input"); }

static void test_percent(void)
{
    EQ_OWN(disp("200 + 10 % =", MODE_STD), "220");
    EQ_OWN(disp("50 %", MODE_STD), "0");
}

static void test_entry_editing(void)
{
    EQ_OWN(disp("123 <", MODE_STD), "12");
    EQ_OWN(disp("1 . 50", MODE_STD), "1.50");
    EQ_OWN(disp("12 ~", MODE_STD), "-12");
    EQ_OWN(disp("5 + 3 E 4 =", MODE_STD), "9");
}

static void test_digit_limit(void) { EQ_OWN(disp("12345678901234567", MODE_STD), "1234567890123456"); }

static void test_grouping(void)
{
    Engine *c = run_g("1234567 . 5", MODE_STD, true);
    EQ_STR(engine_display(c), "1,234,567.5");
    engine_free(c);
}

static void test_memory(void)
{
    Engine *c = run("5 MS C 3 MPLUS MR", MODE_STD);
    EQ_STR(engine_display(c), "8");
    CHECK(engine_has_memory(c));
    engine_command(c, CMD_MC);
    CHECK(!engine_has_memory(c));
    engine_free(c);
}

static void test_exponent_entry(void)
{
    Engine *c = run("1 EXP 5", MODE_STD);
    EQ_STR(engine_display(c), "1.e+5");
    engine_command(c, CMD_ADD);
    EQ_STR(engine_display(c), engine_grouping(c) ? "100,000" : "100000");
    engine_free(c);
}

/* ------------------------------------------------------------------ ScientificMode */

static void test_precedence(void)
{
    EQ_OWN(disp("2 + 3 * 4 =", MODE_SCI), "14");
    EQ_OWN(disp("2 * 3 ^ 2 =", MODE_SCI), "18");
    EQ_OWN(disp("8 - 3 * + 1 =", MODE_SCI), "6");
}

static void test_parens(void)
{
    Engine *c = run("( 2 + 3 ) * 4", MODE_SCI);
    EQ_STR(engine_expression(c), "(2 + 3) *");
    EQ_INT(engine_paren_count(c), 0);
    engine_command(c, CMD_EQU);
    EQ_STR(engine_display(c), "20");
    engine_free(c);
    EQ_OWN(disp("2 * ( 3 + 4 =", MODE_SCI), "14");
}

static void test_precision_32(void)
{
    EQ_OWN(disp("1 / 3 =", MODE_SCI), "0.33333333333333333333333333333333");
}

static void test_trig_degrees(void)
{
    EQ_OWN(disp("30 SIN", MODE_SCI), "0.5");
    EQ_OWN(disp("180 SIN", MODE_SCI), "0");
    EQ_OWN(disp("90 TAN", MODE_SCI), "Invalid input");
    EQ_OWN(disp("45 TAN", MODE_SCI), "1");
    EQ_OWN(disp("0 . 5 INV SIN", MODE_SCI), "30");
}

static void test_inv_resets(void)
{
    Engine *c = run("1 INV LN", MODE_SCI);
    const char *d = engine_display(c);
    CHECK(strncmp(d, "2.7182818284590452353602874713527", 33) == 0);
    CHECK(!engine_inv(c));
    EQ_STR(engine_expression(c), "powe(1)");
    engine_free(c);
}

static void test_trig_radians_and_expr(void)
{
    Engine *c = run("RAD 0 COS", MODE_SCI);
    EQ_STR(engine_display(c), "1");
    EQ_STR(engine_expression(c), "cosr(0)");
    engine_free(c);
}

static void test_misc_functions(void)
{
    EQ_OWN(disp("5 FACT", MODE_SCI), "120");
    EQ_OWN(disp("27 CUBEROOT", MODE_SCI), "3");
    EQ_OWN(disp("27 ROOT 3 =", MODE_SCI), "3");
    EQ_OWN(disp("2 POW10", MODE_SCI), "100");
    EQ_OWN(disp("1000 LOG", MODE_SCI), "3");
    EQ_OWN(disp("7 MOD 3 =", MODE_SCI), "1");
    EQ_OWN(disp("2 . 75 INT", MODE_SCI), "2");
    EQ_OWN(disp("2 . 75 INV INT", MODE_SCI), "0.75");
    EQ_OWN(disp("1 . 5 DMS", MODE_SCI), "1.3");
    EQ_OWN(disp("1 . 3 INV DMS", MODE_SCI), "1.5");
}

static void test_fe(void) { EQ_OWN(disp("1234 FE", MODE_SCI), "1.234e+3"); }

static void test_pi(void)
{
    char *d = disp("PI", MODE_SCI);
    CHECK(strncmp(d, "3.1415926535897932384626433832795", 33) == 0);
    free(d);
}

static void test_overflow(void) { EQ_OWN(disp("10 ^ 10000 =", MODE_SCI), "Overflow"); }

/* ------------------------------------------------------------------ ProgrammerMode */

static void test_hex_entry_and_ops(void)
{
    Engine *c = run("HEX", MODE_PROG);
    engine_command(c, CMD_DIGIT0 + 15);
    engine_command(c, CMD_DIGIT0 + 15);
    engine_command(c, CMD_ADD);
    engine_command(c, CMD_DIGIT0 + 1);
    engine_command(c, CMD_EQU);
    EQ_STR(engine_display(c), "100");
    engine_free(c);
}

static void test_word_wrap(void)
{
    Engine *c = run("BYTE 127 + 1 =", MODE_PROG);
    EQ_STR(engine_display(c), "-128");
    engine_command(c, CMD_HEX);
    EQ_STR(engine_display(c), "80");
    engine_free(c);
}

static void test_bitwise(void)
{
    EQ_OWN(disp("12 AND 10 =", MODE_PROG), "8");
    EQ_OWN(disp("12 OR 3 =", MODE_PROG), "15");
    EQ_OWN(disp("1 LSH 4 =", MODE_PROG), "16");
    EQ_OWN(disp("0 NOT", MODE_PROG), "-1");
    EQ_OWN(disp("BYTE 1 ROR HEX", MODE_PROG), "80");
}

static void test_integer_division(void) { EQ_OWN(disp("7 / 2 =", MODE_PROG), "3"); }

static void test_disabled(void)
{
    Engine *c = run("BIN", MODE_PROG);
    CHECK(!engine_is_enabled(c, CMD_DIGIT0 + 2));
    CHECK(!engine_is_enabled(c, CMD_POINT));
    engine_free(c);
}

static void test_toggle_bit(void)
{
    Engine *c = run("", MODE_PROG);
    engine_toggle_bit(c, 3);
    EQ_STR(engine_display(c), "8");
    engine_free(c);
}

/* ------------------------------------------------------------------ StatisticsMode */

static void test_stats(void)
{
    Engine *c = run("2 STAT_ADD 4 STAT_ADD 4 STAT_ADD 4 STAT_ADD 5 STAT_ADD 5 STAT_ADD 7 STAT_ADD "
                    "9 STAT_ADD",
                    MODE_STAT);
    EQ_INT(engine_dataset_count(c), 8);
    engine_command(c, CMD_STAT_AVE);
    EQ_STR(engine_display(c), "5");
    engine_command(c, CMD_STAT_SUM);
    EQ_STR(engine_display(c), "40");
    engine_command(c, CMD_STAT_SDEV);
    EQ_STR(engine_display(c), "2");
    engine_command(c, CMD_STAT_CAD);
    EQ_INT(engine_dataset_count(c), 0);
    engine_free(c);
}

static void test_empty(void) { EQ_OWN(disp("STAT_AVE", MODE_STAT), "Result not defined"); }

/* ------------------------------------------------------------------ ModeSwitch */

static void test_keeps_value(void)
{
    Engine *c = run("12 . 7", MODE_SCI);
    engine_set_mode(c, MODE_PROG);
    EQ_STR(engine_display(c), "12");
    engine_set_mode(c, MODE_STD);
    EQ_STR(engine_display(c), "12");
    engine_free(c);
}

/* ------------------------------------------------------------------ FullPrecision */

/* Load `value` in Sci mode, then apply cmds (0-terminated; a negative entry
 * means "set_value(operand)"). */
static void full_prec(const char *value, const int *cmds, const char *operand, const char *expected,
                      int line)
{
    Engine *c = engine_new();
    engine_set_mode(c, MODE_SCI);
    engine_set_value(c, value);
    for (; *cmds; cmds++) {
        if (*cmds < 0)
            engine_set_value(c, operand);
        else
            engine_command(c, *cmds);
    }
    const char *got = engine_display(c);
    if (strcmp(got, expected) != 0) {
        fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"\n", __FILE__, line, expected, got);
        g_test_failed = 1;
    }
    engine_free(c);
}

static void test_full_precision(void)
{
    full_prec("0.33333333333333333333333333333333", (const int[]){CMD_SIGN, 0}, NULL,
              "-0.33333333333333333333333333333333", __LINE__);
    full_prec("1234567890123456789012345678901.5", (const int[]){CMD_INT, 0}, NULL,
              "1234567890123456789012345678901", __LINE__);
    full_prec("0.99999999999999999999999999999999", (const int[]){CMD_INV, CMD_TANH, 0}, NULL,
              "37.187935078184703598996479335679", __LINE__);
    full_prec("-2.0000000000000000000000000000001", (const int[]){CMD_ROOT, -1, CMD_EQU, 0}, "3",
              "-1.2599210498948731647672106072782", __LINE__);
    full_prec("-1.0000000000000000000000000000001", (const int[]){CMD_COSH, 0}, NULL,
              "1.5430806348152437784779056207572", __LINE__);
}

/* ------------------------------------------------------------------ Units */

static void test_categories_order(void)
{
    static const char *const cats[] = {"Angle", "Area", "Energy", "Length", "Power", "Pressure",
                                       "Temperature", "Time", "Velocity", "Volume", "Weight/Mass"};
    EQ_INT(units_category_count(), 11);
    for (int i = 0; i < 11; i++)
        EQ_STR(units_category(i), cats[i]);
    EQ_INT(units_count("Length"), 19);
    EQ_INT(units_count("Angle"), 3);
    EQ_STR(units_name("Angle", 0), "Degree");
    EQ_STR(units_name("Angle", 1), "Gradian");
    EQ_STR(units_name("Angle", 2), "Radian");
}

static bool unit_in(const char *cat, const char *u)
{
    for (int i = 0; i < units_count(cat); i++)
        if (u && strcmp(units_name(cat, i), u) == 0)
            return true;
    return false;
}

static void test_defaults_valid(void)
{
    for (int i = 0; i < units_category_count(); i++) {
        const char *c = units_category(i), *f, *t;
        units_default(c, &f, &t);
        CHECK(unit_in(c, f));
        CHECK(unit_in(c, t));
    }
}

static void test_length(void)
{
    EQ_OWN(units_convert("1", "Length", "Mile", "Kilometers"), "1.609344");
    EQ_OWN(units_convert("1", "Length", "Feet", "Inch"), "12");
    EQ_OWN(units_convert("6", "Length", "PICA", "Inch"), "1");
}

static void test_temperature(void)
{
    EQ_OWN(units_convert("100", "Temperature", "Degrees Celsius", "Degrees Fahrenheit"), "212");
    EQ_OWN(units_convert("-40", "Temperature", "Degrees Fahrenheit", "Degrees Celsius"), "-40");
    EQ_OWN(units_convert("0", "Temperature", "Kelvin", "Degrees Celsius"), "-273.15");
}

static void test_units_misc(void)
{
    EQ_OWN(units_convert("3.6", "Velocity", "Kilometer per hour", "Meter per second"), "1");
    EQ_OWN(units_convert("1", "Weight/Mass", "Pound", "Ounce"), "16");
    EQ_OWN(units_convert("1", "Pressure", "Atmosphere", "Kilo Pascal"), "101.325");
    EQ_OWN(units_convert("1", "Time", "Week", "Hour"), "168");
    char *r = units_convert("180", "Angle", "Degree", "Radian");
    EQ_OWN(units_format_number(r, 16), "3.141592653589793");
    free(r);
    EQ_OWN(units_convert("1", "Volume", "Gallon (US)", "Quart (US)"), "4");
}

static void test_units_roundtrip(void)
{
    for (int i = 0; i < units_category_count(); i++) {
        const char *c = units_category(i);
        for (int a = 0; a < units_count(c); a++)
            for (int b = 0; b < units_count(c); b++) {
                const char *ua = units_name(c, a), *ub = units_name(c, b);
                char *x = units_convert("123.456", c, ua, ub);
                char *y = x ? units_convert(x, c, ub, ua) : NULL;
                CHECK(y != NULL);
                if (y)
                    ALMOST(strtod(y, NULL), 123.456, 9);
                free(x);
                free(y);
            }
    }
}

static void test_units_format(void)
{
    EQ_OWN(units_format_number("1.50000", 16), "1.5");
    EQ_OWN(units_format_number("100", 16), "100");
    EQ_OWN(units_format_number("1E+2", 16), "100");
    EQ_OWN(units_format_number("0.1234567890123456789", 16), "0.1234567890123457");
    EQ_OWN(units_format_number("1.5E+20", 16), "1.5e+20");
    EQ_OWN(units_format_number("-2E-15", 16), "-2e-15");
    EQ_OWN(units_format_number("0", 16), "0");
}

/* ------------------------------------------------------------------ Datecalc */

static CDate D(int y, int m, int d) { return (CDate){y, m, d}; }

static void diff(CDate a, CDate b, int r[5])
{
    long t;
    date_difference(a, b, &r[0], &r[1], &r[2], &r[3], &t);
    r[4] = (int)t;
}

#define EQ_DIFF(a, b, y, m, w, d, t)                                                               \
    do {                                                                                           \
        int r_[5];                                                                                 \
        diff(a, b, r_);                                                                            \
        EQ_INT(r_[0], y);                                                                          \
        EQ_INT(r_[1], m);                                                                          \
        EQ_INT(r_[2], w);                                                                          \
        EQ_INT(r_[3], d);                                                                          \
        EQ_INT(r_[4], t);                                                                          \
    } while (0)

#define EQ_DATE(actual, y, m, d)                                                                   \
    do {                                                                                           \
        CDate dt_ = (actual);                                                                       \
        EQ_INT(dt_.year, y);                                                                        \
        EQ_INT(dt_.month, m);                                                                       \
        EQ_INT(dt_.day, d);                                                                         \
    } while (0)

static void test_difference(void)
{
    EQ_DIFF(D(2024, 1, 1), D(2025, 3, 15), 1, 2, 2, 0, 439);
    EQ_DIFF(D(2025, 3, 15), D(2024, 1, 1), 1, 2, 2, 0, 439);
    EQ_OWN(date_format_difference(1, 2, 2, 0), "1 year, 2 months, 2 weeks");
    EQ_OWN(date_format_total_days(439), "439 days");
}

static void test_difference_month_end(void)
{
    EQ_DIFF(D(2024, 1, 31), D(2024, 2, 29), 0, 1, 0, 0, 29);
    EQ_DIFF(D(2024, 1, 30), D(2024, 2, 28), 0, 0, 4, 1, 29);
    EQ_DIFF(D(2024, 1, 31), D(2024, 3, 31), 0, 2, 0, 0, 60);
}

static void test_date_format(void)
{
    EQ_OWN(date_format_difference(1, 2, 3, 4), "1 year, 2 months, 3 weeks, 4 days");
    EQ_OWN(date_format_difference(2, 0, 1, 0), "2 years, 1 week");
    EQ_OWN(date_format_difference(0, 0, 0, 1), "1 day");
    EQ_OWN(date_format_difference(0, 0, 0, 0), "Same dates");
    EQ_OWN(date_format_total_days(1), "1 day");
    EQ_DIFF(D(2026, 10, 16), D(2026, 10, 16), 0, 0, 0, 0, 0);
}

static CDate add(CDate d, int y, int m, int days, bool sub)
{
    CDate r = {0, 0, 0};
    CHECK(date_add(d, y, m, days, sub, &r));
    return r;
}

static void test_add(void)
{
    EQ_DATE(add(D(2023, 1, 31), 0, 1, 0, false), 2023, 2, 28);
    EQ_DATE(add(D(2024, 1, 31), 0, 1, 0, false), 2024, 2, 29);
    EQ_DATE(add(D(2024, 2, 29), 1, 0, 0, false), 2025, 2, 28);
    EQ_DATE(add(D(2024, 3, 31), 0, 1, 0, true), 2024, 2, 29);
    EQ_DATE(add(D(2026, 10, 16), 1, 2, 3, false), 2027, 12, 19);
    EQ_DATE(add(D(2026, 10, 16), 0, 0, 16, true), 2026, 9, 30);
}

static void test_bounds(void)
{
    CDate r;
    CHECK(!date_add(D(2026, 1, 1), 9999, 0, 0, false, &r));
    CHECK(!date_add(D(2026, 1, 1), 500, 0, 0, true, &r));
    CHECK(!date_add(D(2026, 1, 1), 9000, 0, 99999, false, &r));
    CHECK(!date_add(D(1601, 1, 5), 0, 0, 10, true, &r));
}

/* ------------------------------------------------------------------ Worksheets */

typedef struct {
    const char *names[8];
    const char *values[8];
    int n;
} In;

static void in_set(In *in, const char *name, const char *value)
{
    for (int i = 0; i < in->n; i++)
        if (strcmp(in->names[i], name) == 0) {
            in->values[i] = value;
            return;
        }
    in->names[in->n] = name;
    in->values[in->n++] = value;
}

static void in_del(In *in, const char *name)
{
    for (int i = 0; i < in->n; i++)
        if (strcmp(in->names[i], name) == 0) {
            in->names[i] = in->names[in->n - 1];
            in->values[i] = in->values[in->n - 1];
            in->n--;
            return;
        }
}

static const char *in_get(const In *in, const char *name)
{
    for (int i = 0; i < in->n; i++)
        if (strcmp(in->names[i], name) == 0)
            return in->values[i];
    return NULL;
}

static char *solve(const char *ws, const char *t, const In *in, const char **msg, const char **fld)
{
    const char *m = NULL, *f = NULL;
    char *r = ws_solve(ws, t, in->names, in->values, in->n, &m, &f);
    if (msg)
        *msg = m;
    if (fld)
        *fld = f;
    return r;
}

static double solve_f(const char *ws, const char *t, const In *in)
{
    char *r = solve(ws, t, in, NULL, NULL);
    CHECK(r != NULL);
    double v = r ? strtod(r, NULL) : NAN;
    free(r);
    return v;
}

static In mortgage_base(void)
{
    In in = {.n = 0};
    in_set(&in, "Purchase price", "200000");
    in_set(&in, "Down payment", "40000");
    in_set(&in, "Term (years)", "30");
    in_set(&in, "Interest rate (%)", "6");
    return in;
}

static void test_mortgage_payment(void)
{
    In in = mortgage_base();
    ALMOST(solve_f("Mortgage", "Monthly payment", &in), 959.2808, 3);
}

static void test_mortgage_inverse(void)
{
    In in = mortgage_base();
    char *pmt = solve("Mortgage", "Monthly payment", &in, NULL, NULL);
    CHECK(pmt != NULL);
    In full = in;
    in_set(&full, "Monthly payment", pmt);
    for (int i = 0; i < ws_calculable_count("Mortgage"); i++) {
        const char *tgt = ws_calculable("Mortgage", i);
        In inp = full;
        in_del(&inp, tgt);
        ALMOST(solve_f("Mortgage", tgt, &inp), strtod(in_get(&full, tgt), NULL), 6);
    }
    free(pmt);
}

static void test_mortgage_zero_rate(void)
{
    In in = mortgage_base();
    in_set(&in, "Interest rate (%)", "0");
    ALMOST(solve_f("Mortgage", "Monthly payment", &in), 160000.0 / 360, 9);
}

static void test_mortgage_errors(void)
{
    In in = mortgage_base();
    const char *m, *f;
    in_set(&in, "Down payment", "");
    CHECK(solve("Mortgage", "Monthly payment", &in, &m, &f) == NULL);
    EQ_STR(m, "This value cannot be blank.");
    EQ_STR(f, "Down payment");
    in_set(&in, "Down payment", "abc");
    CHECK(solve("Mortgage", "Monthly payment", &in, &m, &f) == NULL);
    EQ_STR(m, "This value is not valid.");
    In in2 = {.n = 0};
    in_set(&in2, "Purchase price", "200000");
    in_set(&in2, "Down payment", "0");
    in_set(&in2, "Interest rate (%)", "6");
    in_set(&in2, "Monthly payment", "500");
    CHECK(solve("Mortgage", "Term (years)", &in2, &m, &f) == NULL);
    EQ_STR(m, "Unable to calculate");
}

static In lease_base(void)
{
    In in = {.n = 0};
    in_set(&in, "Lease value", "30000");
    in_set(&in, "Lease period", "3");
    in_set(&in, "Payments per year", "12");
    in_set(&in, "Residual value", "15000");
    in_set(&in, "Interest rate (%)", "5");
    return in;
}

static void test_lease_roundtrip(void)
{
    In in = lease_base();
    char *pmt = solve("Vehicle lease", "Periodic payment", &in, NULL, NULL);
    CHECK(pmt != NULL);
    double i = 0.05 / 12, v = pow(1 + i, -36);
    ALMOST(strtod(pmt, NULL), (30000 - 15000 * v) * i / (1 - v), 6);
    In full = in;
    in_set(&full, "Periodic payment", pmt);
    for (int k = 0; k < ws_calculable_count("Vehicle lease"); k++) {
        const char *tgt = ws_calculable("Vehicle lease", k);
        In inp = full;
        in_del(&inp, tgt);
        ALMOST(solve_f("Vehicle lease", tgt, &inp), strtod(in_get(&full, tgt), NULL), 6);
    }
    free(pmt);
}

static void test_lease_zero_ppy(void)
{
    In in = lease_base();
    in_set(&in, "Payments per year", "0");
    const char *m;
    CHECK(solve("Vehicle lease", "Periodic payment", &in, &m, NULL) == NULL);
    EQ_STR(m, "Unable to calculate");
}

static void test_mpg(void)
{
    const char *w = "Fuel economy (mpg)", *m;
    In a = {.n = 0}, b = {.n = 0}, c = {.n = 0}, d = {.n = 0};
    in_set(&a, "Distance (miles)", "300");
    in_set(&a, "Fuel used (gallons)", "12");
    EQ_DEC(solve(w, "Fuel economy (mpg)", &a, NULL, NULL), "25");
    in_set(&b, "Fuel economy (mpg)", "25");
    in_set(&b, "Fuel used (gallons)", "12");
    EQ_DEC(solve(w, "Distance (miles)", &b, NULL, NULL), "300");
    in_set(&c, "Distance (miles)", "300");
    in_set(&c, "Fuel economy (mpg)", "25");
    EQ_DEC(solve(w, "Fuel used (gallons)", &c, NULL, NULL), "12");
    in_set(&d, "Distance (miles)", "300");
    in_set(&d, "Fuel used (gallons)", "0");
    CHECK(solve(w, "Fuel economy (mpg)", &d, &m, NULL) == NULL);
    EQ_STR(m, "Unable to calculate");
}

static void test_l100(void)
{
    const char *w = "Fuel economy (L/100 km)";
    In a = {.n = 0}, b = {.n = 0}, c = {.n = 0};
    in_set(&a, "Distance (kilometers)", "500");
    in_set(&a, "Fuel used (liters)", "40");
    EQ_DEC(solve(w, "Fuel economy (L/100 km)", &a, NULL, NULL), "8");
    in_set(&b, "Fuel economy (L/100 km)", "8");
    in_set(&b, "Fuel used (liters)", "40");
    EQ_DEC(solve(w, "Distance (kilometers)", &b, NULL, NULL), "500");
    in_set(&c, "Distance (kilometers)", "500");
    in_set(&c, "Fuel economy (L/100 km)", "8");
    EQ_DEC(solve(w, "Fuel used (liters)", &c, NULL, NULL), "40");
}

static void test_specs(void)
{
    EQ_INT(ws_count(), 4);
    EQ_STR(ws_name(0), "Mortgage");
    EQ_STR(ws_name(1), "Vehicle lease");
    EQ_STR(ws_name(2), "Fuel economy (mpg)");
    EQ_STR(ws_name(3), "Fuel economy (L/100 km)");
}

int main(void)
{
    /* test_engine.py */
    RUN(test_no_precedence);
    RUN(test_intermediate_result_shown);
    RUN(test_repeat_equals);
    RUN(test_new_number_repeats_last_op);
    RUN(test_operand_defaults_to_display);
    RUN(test_operator_replacement);
    RUN(test_division_precision);
    RUN(test_divide_by_zero);
    RUN(test_sqrt_expression);
    RUN(test_memory_keeps_expression);
    RUN(test_invalid_sqrt);
    RUN(test_percent);
    RUN(test_entry_editing);
    RUN(test_digit_limit);
    RUN(test_grouping);
    RUN(test_memory);
    RUN(test_exponent_entry);
    RUN(test_precedence);
    RUN(test_parens);
    RUN(test_precision_32);
    RUN(test_trig_degrees);
    RUN(test_inv_resets);
    RUN(test_trig_radians_and_expr);
    RUN(test_misc_functions);
    RUN(test_fe);
    RUN(test_pi);
    RUN(test_overflow);
    RUN(test_hex_entry_and_ops);
    RUN(test_word_wrap);
    RUN(test_bitwise);
    RUN(test_integer_division);
    RUN(test_disabled);
    RUN(test_toggle_bit);
    RUN(test_stats);
    RUN(test_empty);
    RUN(test_keeps_value);
    RUN(test_full_precision);
    /* test_units.py */
    RUN(test_categories_order);
    RUN(test_defaults_valid);
    RUN(test_length);
    RUN(test_temperature);
    RUN(test_units_misc);
    RUN(test_units_roundtrip);
    RUN(test_units_format);
    /* test_datecalc.py */
    RUN(test_difference);
    RUN(test_difference_month_end);
    RUN(test_date_format);
    RUN(test_add);
    RUN(test_bounds);
    /* test_worksheets.py */
    RUN(test_mortgage_payment);
    RUN(test_mortgage_inverse);
    RUN(test_mortgage_zero_rate);
    RUN(test_mortgage_errors);
    RUN(test_lease_roundtrip);
    RUN(test_lease_zero_ppy);
    RUN(test_mpg);
    RUN(test_l100);
    RUN(test_specs);
    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
