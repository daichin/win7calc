/* Differential-testing driver: reads commands from stdin, prints engine state after each.
 *
 *   R            new engine              c <id>       engine_command
 *   m <0..3>     engine_set_mode         g <0|1>      engine_set_grouping
 *   t <bit>      engine_toggle_bit       v <num>      engine_set_value
 *   h            engine_clear_history    e            is_enabled for all ids
 *   f <g> <num>  engine_format_value     U/D/W ...    units / dates / worksheets (see below)
 *
 * Build: see the scratchpad fuzz notes (links build/ objects + libmpdec).
 */
#include "datecalc.h"
#include "engine.h"
#include "units.h"
#include "worksheets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *nz(const char *s) { return s ? s : "None"; }

static void state(Engine *e)
{
    const char *disp = engine_display(e);
    const char *expr = engine_expression(e);
    int nh = engine_history_count(e), nd = engine_dataset_count(e);
    char *cur = engine_current_value(e);
    printf("%s|%s|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%s|%s|%d|%s|%llu|%s|%s\n", disp, expr,
           engine_paren_count(e), engine_inv(e), engine_fe(e), engine_grouping(e),
           engine_has_memory(e), (int)engine_mode(e), engine_radix(e), engine_bits(e),
           (int)engine_angle(e), nh, nh ? engine_history_text(e, nh - 1) : "None",
           nh ? engine_history_value(e, nh - 1) : "None", nd,
           nd ? engine_dataset_value(e, nd - 1) : "None",
           (unsigned long long)engine_unsigned_value(e), cur, nz(engine_error(e)));
    free(cur);
}

static char *trim(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = 0;
    return s;
}

/* split on '|' in place */
static int split(char *s, char **out, int max)
{
    int n = 0;
    out[n++] = s;
    for (char *p = s; *p && n < max; p++)
        if (*p == '|') {
            *p = 0;
            out[n++] = p + 1;
        }
    return n;
}

int main(void)
{
    static char line[1 << 16];
    Engine *e = engine_new();
    while (fgets(line, sizeof line, stdin)) {
        trim(line);
        char *arg = line + (line[0] && line[1] ? 2 : 1);
        switch (line[0]) {
        case 'R':
            engine_free(e);
            e = engine_new();
            break;
        case 'c': engine_command(e, atoi(arg)); state(e); break;
        case 'm': engine_set_mode(e, (CalcMode)atoi(arg)); state(e); break;
        case 'g': engine_set_grouping(e, atoi(arg) != 0); state(e); break;
        case 't': engine_toggle_bit(e, atoi(arg)); state(e); break;
        case 'v': engine_set_value(e, arg); state(e); break;
        case 'h': engine_clear_history(e); state(e); break;
        case 'e':
            for (int id = 0; id < 400; id++)
                putchar(engine_is_enabled(e, id) ? '1' : '0');
            putchar('\n');
            break;
        case 'f': {
            int g = atoi(arg);
            char *num = strchr(arg, ' ');
            char *s = engine_format_value(e, num ? num + 1 : "", g);
            printf("%s\n", nz(s));
            free(s);
            break;
        }
        case 'U': { /* U value|category|from|to  -> convert + format_number(16) */
            char *f[4];
            if (split(arg, f, 4) != 4)
                break;
            char *r = units_convert(f[0], f[1], f[2], f[3]);
            char *fm = r ? units_format_number(r, 16) : NULL;
            printf("%s %s\n", nz(r), nz(fm));
            free(r);
            free(fm);
            break;
        }
        case 'N': { /* N sig|value -> format_number */
            char *f[2];
            if (split(arg, f, 2) != 2)
                break;
            char *r = units_format_number(f[1], atoi(f[0]));
            printf("%s\n", nz(r));
            free(r);
            break;
        }
        case 'D': { /* D y m d y m d -> difference */
            CDate a, b;
            if (sscanf(arg, "%d %d %d %d %d %d", &a.year, &a.month, &a.day, &b.year, &b.month,
                       &b.day) != 6)
                break;
            int y, m, w, d;
            long t;
            date_difference(a, b, &y, &m, &w, &d, &t);
            char *s1 = date_format_difference(y, m, w, d), *s2 = date_format_total_days(t);
            printf("%d %d %d %d %ld|%s|%s\n", y, m, w, d, t, s1, s2);
            free(s1);
            free(s2);
            break;
        }
        case 'A': { /* A y m d years months days sub -> add */
            CDate a, r;
            int ys, ms, ds, sub;
            if (sscanf(arg, "%d %d %d %d %d %d %d", &a.year, &a.month, &a.day, &ys, &ms, &ds,
                       &sub) != 7)
                break;
            if (date_add(a, ys, ms, ds, sub != 0, &r))
                printf("%04d-%02d-%02d\n", r.year, r.month, r.day);
            else
                printf("%s\n", DATE_OUT_OF_BOUND);
            break;
        }
        case 'W': { /* W ws|target|name=value|name=value... ('~' value = NULL) */
            char *f[16];
            int n = split(arg, f, 16);
            if (n < 2)
                break;
            const char *names[14], *values[14];
            int k = 0;
            for (int i = 2; i < n; i++) {
                char *eq = strchr(f[i], '=');
                if (!eq)
                    continue;
                *eq = 0;
                names[k] = f[i];
                values[k] = strcmp(eq + 1, "~") == 0 ? NULL : eq + 1;
                k++;
            }
            const char *msg = NULL, *fld = NULL;
            char *r = ws_solve(f[0], f[1], names, values, k, &msg, &fld);
            if (r)
                printf("OK %s\n", r);
            else
                printf("ERR %s|%s\n", nz(msg), nz(fld));
            free(r);
            break;
        }
        }
    }
    engine_free(e);
    return 0;
}
