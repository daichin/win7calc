"""Render the calculator to PNG without a display: python3 -m tools.snapshot OUT.png MODE [keys...]"""
import sys

import cairo

from win7calc import engine as E
from win7calc import layout, view

SCALE = 2


def main():
    out, mode, *cmds = sys.argv[1:]
    eng = E.Engine()
    eng.set_mode(mode)
    ui = view.UIState()
    for c in cmds:
        if c == "history":
            ui.history = True
        elif c == "grouping":
            eng.grouping = True
        elif c.startswith("hot="):
            ui.hot = ("button", getattr(E, c[4:]))
        elif c.startswith("press="):
            ui.hot = ui.pressed = ("button", getattr(E, c[6:]))
        elif c.isdigit():
            for d in c:
                eng.command(E.DIGIT0 + int(d))
        else:
            eng.command(getattr(E, c))
    lay = layout.build(mode, ui.history, False)
    surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, lay.width * SCALE, lay.height * SCALE)
    cr = cairo.Context(surf)
    cr.scale(SCALE, SCALE)
    view.paint(cr, lay, eng, ui)
    surf.write_to_png(out)


if __name__ == "__main__":
    main()
