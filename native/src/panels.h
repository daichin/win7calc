/* GTK3 side panels: Unit conversion, Date calculation, Worksheets (port of win7calc/panels.py).
 *
 * Layout uses GtkFixed with positions taken from the Win7 dialog templates
 * (dialog units -> px: x*0.5, y*0.6275), multiplied by `scale`.
 */
#ifndef WIN7CALC_PANELS_H
#define WIN7CALC_PANELS_H

#include <gtk/gtk.h>

#define PANEL_W 363     /* natural size at scale 1.0 */
#define PANEL_H 251

GtkWidget *panel_unit_new(double scale);
GtkWidget *panel_date_new(double scale);
GtkWidget *panel_worksheet_new(const char *worksheet, double scale);

/* Feed the calculator's current value into the panel (no-op where meaningless). */
void panel_set_value(GtkWidget *panel, const char *text);

/* Parse display/user text ("1,234.5", " -3 ", "1.e+5") the way Python's Decimal does.
 * Returns a normalised plain number string (g_free) or NULL if invalid, blank or not finite. */
char *parse_number(const char *text);

#endif
