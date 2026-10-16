/* Calculator state machine replicating win32calc (Windows 7 Calculator).
 * C port of win7calc/engine.py; behaviour must match it exactly.
 *
 * Command ids are the original control ids from win32calc's resources.
 * Every `char *` returned by this API is heap-allocated and owned by the
 * caller (release with free()).  `const char *` results are owned by the
 * engine and stay valid until the next call that mutates the engine.
 */
#ifndef WIN7CALC_ENGINE_H
#define WIN7CALC_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

enum {
    CMD_SIGN = 80, CMD_CLEAR = 81, CMD_CE = 82, CMD_BACK = 83, CMD_POINT = 84,
    CMD_AND = 86, CMD_OR = 87, CMD_XOR = 88, CMD_LSH = 89, CMD_RSH = 90, CMD_DIV = 91,
    CMD_MUL = 92, CMD_ADD = 93, CMD_SUB = 94, CMD_MOD = 95, CMD_ROOT = 96, CMD_PWR = 97,
    CMD_INT = 98, CMD_ROL = 99, CMD_ROR = 100, CMD_NOT = 101,
    CMD_SIN = 102, CMD_COS = 103, CMD_TAN = 104, CMD_SINH = 105, CMD_COSH = 106,
    CMD_TANH = 107, CMD_LN = 108, CMD_LOG = 109,
    CMD_SQRT = 110, CMD_SQR = 111, CMD_CUBE = 112, CMD_FACT = 113, CMD_RECIP = 114,
    CMD_DMS = 115, CMD_CUBEROOT = 116, CMD_POW10 = 117, CMD_PERCENT = 118, CMD_FE = 119,
    CMD_PI = 120, CMD_EQU = 121,
    CMD_MC = 122, CMD_MR = 123, CMD_MS = 124, CMD_MPLUS = 125, CMD_MMINUS = 126,
    CMD_EXP = 127, CMD_OPENP = 128, CMD_CLOSEP = 129,
    CMD_DIGIT0 = 130,          /* 130..139 = 0..9, 140..145 = A..F */
    CMD_INV = 146,
    CMD_STAT_ADD = 168, CMD_STAT_AVE = 171, CMD_STAT_AVE2 = 172, CMD_STAT_SUM = 173,
    CMD_STAT_SUM2 = 174, CMD_STAT_SDEV = 175, CMD_STAT_SDEV1 = 176,
    CMD_STAT_CAD = 1056,
    CMD_HEX = 313, CMD_DEC = 314, CMD_OCT = 315, CMD_BIN = 316,
    CMD_QWORD = 317, CMD_DWORD = 318, CMD_WORD = 319, CMD_BYTE = 320,
    CMD_DEG = 321, CMD_RAD = 322, CMD_GRAD = 323,
};

typedef enum { MODE_STD, MODE_SCI, MODE_PROG, MODE_STAT } CalcMode;
typedef enum { ANGLE_DEG, ANGLE_RAD, ANGLE_GRAD } AngleUnit;

typedef struct Engine Engine;

Engine *engine_new(void);
void engine_free(Engine *e);

/* Execute one command id; errors become the display text (never aborts). */
void engine_command(Engine *e, int cid);
bool engine_is_enabled(const Engine *e, int cid);

/* Text views (engine-owned). */
const char *engine_display(Engine *e);      /* main number line, or the error text */
const char *engine_expression(Engine *e);   /* upper "2 + sqrt(9) -" line */
const char *engine_error(const Engine *e);  /* NULL when there is no error */

/* State. */
CalcMode engine_mode(const Engine *e);
void engine_set_mode(Engine *e, CalcMode m);
bool engine_int_mode(const Engine *e);      /* == MODE_PROG */
int engine_radix(const Engine *e);          /* 16/10/8/2 */
int engine_bits(const Engine *e);           /* 64/32/16/8 */
AngleUnit engine_angle(const Engine *e);
bool engine_inv(const Engine *e);
bool engine_fe(const Engine *e);
bool engine_grouping(const Engine *e);
void engine_set_grouping(Engine *e, bool on);
int engine_paren_count(const Engine *e);
bool engine_has_memory(const Engine *e);

/* Programmer mode. */
uint64_t engine_unsigned_value(Engine *e);  /* current value as unsigned word */
void engine_toggle_bit(Engine *e, int bit);

/* Value exchange.  Number strings are plain ("-12.5", "1.5E+20", "255"):
 * in programmer mode a signed decimal integer, otherwise full precision. */
char *engine_current_value(Engine *e);      /* value currently shown */
void engine_set_value(Engine *e, const char *num);  /* paste / history recall */
/* Format a number string the way the display would (grouping: 0/1, or -1 = current setting). */
char *engine_format_value(Engine *e, const char *num, int grouping);

/* History (Std/Sci) and statistics dataset. */
int engine_history_count(const Engine *e);
const char *engine_history_text(const Engine *e, int i);   /* "2 + 3" */
const char *engine_history_value(const Engine *e, int i);  /* plain number string */
void engine_clear_history(Engine *e);
int engine_dataset_count(const Engine *e);
const char *engine_dataset_value(const Engine *e, int i);  /* plain number string */

#endif
