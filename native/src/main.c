/* win7calc entry point.
 *   win7calc [--scale S] [--mode M]             GTK window
 *   win7calc --snapshot OUT.png MODE [CMD...]   headless render (port of tools/snapshot.py)
 */
#include <stdio.h>
#include <string.h>

#include <cairo.h>

#include "app.h"
#include "engine.h"
#include "layout.h"
#include "view.h"

#define SNAPSHOT_SCALE 2

/* Render the calculator to PNG without a display: no gtk_init, Pango/cairo only. */
int snapshot_main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: win7calc --snapshot OUT.png MODE [CMD...]\n");
        return 2;
    }
    const char *out = argv[0];
    CalcMode mode;
    if (!layout_mode_by_name(argv[1], &mode)) {
        fprintf(stderr, "win7calc: unknown mode %s\n", argv[1]);
        return 2;
    }
    Engine *eng = engine_new();
    engine_set_mode(eng, mode);
    UIState ui;
    ui_state_init(&ui);
    int rc = 0;
    for (int i = 2; i < argc && rc == 0; i++) {
        const char *c = argv[i];
        if (strcmp(c, "history") == 0) {
            ui.history = true;
        } else if (strcmp(c, "grouping") == 0) {
            engine_set_grouping(eng, true);
        } else if (strncmp(c, "hot=", 4) == 0 || strncmp(c, "press=", 6) == 0) {
            bool press = c[0] == 'p';
            int cid = layout_cmd_by_name(c + (press ? 6 : 4));
            if (cid < 0) {
                fprintf(stderr, "win7calc: unknown command %s\n", c);
                rc = 2;
                break;
            }
            ui.hot = (ItemKey){IT_BUTTON, cid};
            if (press)
                ui.pressed = ui.hot;
        } else if (c[0] && strspn(c, "0123456789") == strlen(c)) {
            for (const char *d = c; *d; d++)
                engine_command(eng, CMD_DIGIT0 + (*d - '0'));
        } else {
            int cid = layout_cmd_by_name(c);
            if (cid < 0) {
                fprintf(stderr, "win7calc: unknown command %s\n", c);
                rc = 2;
                break;
            }
            engine_command(eng, cid);
        }
    }
    if (rc == 0) {
        static Layout lay;
        layout_build(&lay, mode, ui.history, false);
        cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, lay.width * SNAPSHOT_SCALE,
                                                           lay.height * SNAPSHOT_SCALE);
        cairo_t *cr = cairo_create(surf);
        cairo_scale(cr, SNAPSHOT_SCALE, SNAPSHOT_SCALE);
        view_paint(cr, &lay, eng, &ui);
        cairo_destroy(cr);
        if (cairo_surface_write_to_png(surf, out) != CAIRO_STATUS_SUCCESS) {
            fprintf(stderr, "win7calc: cannot write %s\n", out);
            rc = 1;
        }
        cairo_surface_destroy(surf);
    }
    engine_free(eng);
    return rc;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--snapshot") == 0)
        return snapshot_main(argc - 2, argv + 2);
    return app_main(argc, argv);
}
