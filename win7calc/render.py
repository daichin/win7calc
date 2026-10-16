"""Cairo drawing of the Win7 Aero look. Colours were sampled from win32calc's own bitmaps."""
from __future__ import annotations

import gi

gi.require_version("Pango", "1.0")
gi.require_version("PangoCairo", "1.0")
from gi.repository import Pango, PangoCairo  # noqa: E402

UI_FONT = "Segoe UI, Selawik, Noto Sans, Open Sans, DejaVu Sans"
NUM_FONT = "Consolas, Inconsolata, Noto Sans Mono, DejaVu Sans Mono, monospace"


def hexrgb(h: str):
    return int(h[0:2], 16) / 255, int(h[2:4], 16) / 255, int(h[4:6], 16) / 255


# Vertical gradients: (stop, colour). Sharp steps at ~0.48 give the glassy split.
G_DIGIT = [(0, "f7fbff"), (.42, "f1f5f9"), (.475, "edf3f8"), (.48, "e7edf5"), (.75, "ecf4fa"), (1, "f2fbff")]
G_OP = [(0, "f6fafe"), (.05, "f0f6fb"), (.475, "e2eaf3"), (.48, "d5e0ed"), (1, "d7e2ef")]
G_MEM = [(0, "d4e0ed"), (1, "d7e2ef")]
G_HOT = [(0, "fffbf6"), (.475, "fee2c2"), (.48, "ffcf6a"), (.7, "ffda74"), (.82, "ffeb81"), (.92, "fffab4"), (1, "fffff1")]
G_HOT_MEM = [(0, "ffda78"), (.1, "ffce68"), (.48, "ffcf6a"), (.78, "ffea7f"), (.9, "fff9b3"), (1, "fffff2")]
G_PRESS = [(0, "e3c185"), (.08, "eecb8e"), (.475, "f5c779"), (.48, "f5bb57"), (.73, "f4cd61"), (.85, "f3e37c"),
           (.93, "f2f0b0"), (1, "f2f2bb")]
G_PRESS_MEM = [(0, "d1a75c"), (.08, "e2b15d"), (.2, "ecb85e"), (.48, "ecb85f"), (.75, "eacd66"), (.85, "eadd7d"),
               (.93, "e9e9b0"), (1, "e9ebc0")]
G_DISABLED = [(0, "d9e4f1"), (1, "d9e4f1")]
G_DISPLAY = [(0, "e5eefb"), (.5, "f0f5fc"), (1, "ffffff")]
G_WINDOW = [(0, "edf4fc"), (1, "d9e4f1")]

BORDER = {"normal": "8797aa", "hot": "ffdb00", "pressed": "c29b29", "disabled": "b4c2d1"}
TEXT = (0, 0, 0)
TEXT_DISABLED = hexrgb("8d9db0")
PANEL_FILL = hexrgb("d9e4f1")
PANEL_BORDER = hexrgb("8e9cad")


def gradient(y0: float, y1: float, stops):
    import cairo
    g = cairo.LinearGradient(0, y0, 0, y1)
    for t, c in stops:
        g.add_color_stop_rgb(t, *hexrgb(c))
    return g


def rrect(cr, x, y, w, h, r):
    import math
    cr.new_sub_path()
    cr.arc(x + w - r, y + r, r, -math.pi / 2, 0)
    cr.arc(x + w - r, y + h - r, r, 0, math.pi / 2)
    cr.arc(x + r, y + h - r, r, math.pi / 2, math.pi)
    cr.arc(x + r, y + r, r, math.pi, 3 * math.pi / 2)
    cr.close_path()


def window_bg(cr, w, h):
    cr.set_source(gradient(0, 75, G_WINDOW))
    cr.rectangle(0, 0, w, h)
    cr.fill()


def button(cr, x, y, w, h, style: str, state: str):
    """state: normal | hot | pressed | disabled"""
    if state == "disabled":
        stops = G_DISABLED
    elif state == "hot":
        stops = G_HOT_MEM if style == "mem" else G_HOT
    elif state == "pressed":
        stops = G_PRESS_MEM if style == "mem" else G_PRESS
    else:
        stops = {"digit": G_DIGIT, "mem": G_MEM}.get(style, G_OP)
    cr.save()
    rrect(cr, x + .5, y + .5, w - 1, h - 1, 2.5)
    cr.set_source(gradient(y, y + h, stops))
    cr.fill_preserve()
    cr.set_line_width(1)
    cr.set_source_rgb(*hexrgb(BORDER[state]))
    cr.stroke()
    if state in ("normal", "hot"):
        # 1px inner highlight.
        rrect(cr, x + 1.5, y + 1.5, w - 3, h - 3, 1.5)
        cr.set_source_rgba(1, 1, 1, .55 if state == "normal" else .45)
        cr.stroke()
    cr.restore()


def panel(cr, x, y, w, h, fill=None, border=PANEL_BORDER, radius=2):
    cr.save()
    rrect(cr, x + .5, y + .5, w - 1, h - 1, radius)
    if fill is None:
        cr.set_source_rgb(*PANEL_FILL)
    elif isinstance(fill, tuple):
        cr.set_source_rgb(*fill)
    else:
        cr.set_source(fill)
    cr.fill_preserve()
    cr.set_source_rgb(*border)
    cr.set_line_width(1)
    cr.stroke()
    cr.restore()


def display_panel(cr, x, y, w, h):
    panel(cr, x, y, w, h, fill=gradient(y, y + h, G_DISPLAY))
    cr.save()
    cr.set_source_rgba(1, 1, 1, .9)
    cr.set_line_width(1)
    cr.move_to(x + 1.5, y + h - 2)
    cr.line_to(x + 1.5, y + 1.5)
    cr.line_to(x + w - 2, y + 1.5)
    cr.stroke()
    cr.restore()


# ---------------------------------------------------------------- text

def layout(cr, text: str, family: str, px: float, markup=False, bold=False):
    lo = PangoCairo.create_layout(cr)
    fd = Pango.FontDescription.from_string(family)
    fd.set_absolute_size(px * Pango.SCALE)
    if bold:
        fd.set_weight(Pango.Weight.BOLD)
    lo.set_font_description(fd)
    if markup:
        lo.set_markup(text, -1)
    else:
        lo.set_text(text, -1)
    return lo


def text_size(lo):
    _, logical = lo.get_pixel_extents()
    return logical.width, logical.height


def draw_text(cr, text, x, y, w, h, family=UI_FONT, px=12, align="center", color=TEXT, markup=False,
              valign="center"):
    lo = layout(cr, text, family, px, markup)
    tw, th = text_size(lo)
    if align == "center":
        tx = x + (w - tw) / 2
    elif align == "right":
        tx = x + w - tw
    else:
        tx = x
    ty = y + (h - th) / 2 if valign == "center" else (y + h - th if valign == "bottom" else y)
    cr.save()
    cr.set_source_rgb(*color)
    cr.move_to(round(tx), round(ty))
    PangoCairo.show_layout(cr, lo)
    cr.restore()
    return tw


def fit_text_px(cr, text, family, sizes, max_w):
    """Largest font size from `sizes` at which `text` fits in max_w (Win7 shrinks long numbers)."""
    for px in sizes:
        if text_size(layout(cr, text, family, px))[0] <= max_w:
            return px
    return sizes[-1]


def radio(cr, x, y, h, text, checked, hot, px=12):
    """Win7-style radio button with its label; returns nothing."""
    import math
    r = 6.5
    cx, cy = x + r + .5, y + h / 2
    cr.save()
    cr.new_path()
    cr.arc(cx, cy, r, 0, 2 * math.pi)
    g = gradient(cy - r, cy + r, [(0, "f4f4f4" if not hot else "e8f6fd"), (1, "ffffff" if not hot else "dff2fc")])
    cr.set_source(g)
    cr.fill_preserve()
    cr.set_source_rgb(*hexrgb("3c7fb1" if hot else "8e8f8f"))
    cr.set_line_width(1)
    cr.stroke()
    cr.arc(cx, cy, r - 2, 0, 2 * math.pi)
    cr.set_source_rgb(*hexrgb("dfe6ee" if not hot else "c6e9fb"))
    cr.fill()
    if checked:
        cr.arc(cx, cy, 3.2, 0, 2 * math.pi)
        g = gradient(cy - 3, cy + 3, [(0, "8ad0f2"), (.5, "1f6ba8"), (1, "0d3a6b")])
        cr.set_source(g)
        cr.fill()
    cr.restore()
    draw_text(cr, text, x + 2 * r + 5, y, 200, h, px=px, align="left")


def arrow_button(cr, x, y, w, h, up: bool, state: str):
    button(cr, x, y, w, h, "op", state)
    cr.save()
    cx, cy = x + w / 2, y + h / 2
    cr.set_source_rgb(*hexrgb("aab8c8" if state == "disabled" else "1e395b"))
    if up:
        cr.move_to(cx - 4, cy + 2)
        cr.line_to(cx + 4, cy + 2)
        cr.line_to(cx, cy - 2)
    else:
        cr.move_to(cx - 4, cy - 2)
        cr.line_to(cx + 4, cy - 2)
        cr.line_to(cx, cy + 2)
    cr.close_path()
    cr.fill()
    cr.restore()
