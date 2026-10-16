"""Paints the whole calculator client area (independent of GTK widgets, so it can render headless)."""
from __future__ import annotations

from dataclasses import dataclass

from . import engine as E
from . import render as R
from .layout import Item, Layout, INV_LABELS, TOGGLE_IDS

NUM_SIZES = [24, 20, 16, 13, 11]
ROW_H = 18
ARROW_W, ARROW_H = 17, 16
UP, DOWN = 1051, 1052


@dataclass
class UIState:
    hot: tuple | None = None        # (kind, id) under the mouse
    pressed: tuple | None = None    # (kind, id) held by the mouse
    flash: tuple | None = None      # (kind, id) briefly shown pressed after a key press
    history: bool = False
    list_top: int = 0               # first visible row of the history / dataset list


def key(item: Item) -> tuple:
    return item.kind, item.id


def display_items(lay: Layout, eng: E.Engine, ui: UIState) -> list[Item]:
    """Interactive items inside the display: list scroll arrows and rows."""
    x, y, w, h = lay.display
    if not _has_list(eng, ui):
        return []
    items = [Item("arrow", UP, x + w - 52, y + 5, ARROW_W, ARROW_H),
             Item("arrow", DOWN, x + w - 30, y + 5, ARROW_W, ARROW_H)]
    for i in range(visible_rows()):
        items.append(Item("row", i, x + 6, y + 27 + i * ROW_H, w - 12, ROW_H))
    return items


def visible_rows() -> int:
    return 4


def _has_list(eng: E.Engine, ui: UIState) -> bool:
    return eng.mode == E.STAT or (ui.history and eng.mode in (E.STD, E.SCI))


def list_entries(eng: E.Engine) -> list[str]:
    if eng.mode == E.STAT:
        return [eng.fmt(v) for v in eng.dataset]
    return [text for text, _ in eng.history]


def max_top(eng: E.Engine) -> int:
    return max(0, len(list_entries(eng)) - visible_rows())


def item_state(item: Item, eng: E.Engine, ui: UIState) -> str:
    k = key(item)
    if item.kind == "button":
        if not eng.is_enabled(item.id):
            return "disabled"
        if item.id in TOGGLE_IDS and ((item.id == E.INV and eng.inv) or (item.id == E.FE and eng.fe)):
            return "pressed"
    if ui.flash == k or (ui.pressed == k and ui.hot == k):
        return "pressed"
    if ui.hot == k and (ui.pressed is None or ui.pressed == k):
        return "hot"
    return "normal"


def paint(cr, lay: Layout, eng: E.Engine, ui: UIState):
    R.window_bg(cr, lay.width, lay.height)
    _paint_display(cr, lay, eng, ui)
    for it in lay.items:
        _paint_item(cr, it, eng, ui)


def _paint_item(cr, it: Item, eng: E.Engine, ui: UIState):
    if it.kind == "button":
        st = item_state(it, eng, ui)
        R.button(cr, it.x, it.y, it.w, it.h, it.style, st)
        text = INV_LABELS.get(it.id, it.text) if eng.inv else it.text
        R.draw_text(cr, text, it.x, it.y, it.w, it.h, px=12, markup=True,
                    color=R.TEXT_DISABLED if st == "disabled" else R.TEXT)
    elif it.kind == "box":
        R.panel(cr, it.x, it.y, it.w, it.h, border=R.hexrgb("8696aa"))
    elif it.kind == "parens":
        R.panel(cr, it.x, it.y, it.w, it.h)
        if eng.paren_count:
            R.draw_text(cr, f"(={eng.paren_count}", it.x, it.y, it.w, it.h, px=12)
    elif it.kind == "radio":
        checked = (eng.angle == E.ANGLE_IDS.get(it.id) or eng.radix == E.RADIX_IDS.get(it.id)
                   or eng.bits == E.WORD_IDS.get(it.id))
        R.radio(cr, it.x, it.y, it.h, it.text, checked, ui.hot == key(it), px=11)
    elif it.kind == "bit":
        enabled = it.id < eng.bits
        bit = (eng.unsigned_value() >> it.id) & 1
        color = R.TEXT if enabled else R.hexrgb("9aa8b8")
        if enabled and ui.hot == key(it):
            color = R.hexrgb("1a5fb4")
        R.draw_text(cr, str(bit), it.x - 2, it.y, it.w + 4, it.h, family=R.NUM_FONT, px=13, color=color)
    elif it.kind == "bitlabel":
        R.draw_text(cr, it.text, it.x if it.style == "l" else it.x - it.w + 7.5, it.y, it.w, it.h,
                    family=R.NUM_FONT, px=10, align="left" if it.style == "l" else "right",
                    color=R.hexrgb("4e5d6e"))


def _paint_display(cr, lay: Layout, eng: E.Engine, ui: UIState):
    x, y, w, h = lay.display
    R.display_panel(cr, x, y, w, h)
    has_list = _has_list(eng, ui)
    if has_list:
        cr.save()
        cr.set_source_rgb(1, 1, 1)
        cr.set_line_width(1)
        for ly in (y + 26.5, y + 100.5):
            cr.move_to(x + 1, ly)
            cr.line_to(x + w - 1, ly)
        cr.stroke()
        cr.restore()
        entries = list_entries(eng)
        for it in display_items(lay, eng, ui):
            if it.kind == "arrow":
                can = ui.list_top > 0 if it.id == UP else ui.list_top < max_top(eng)
                R.arrow_button(cr, it.x, it.y, it.w, it.h, it.id == UP,
                               item_state(it, eng, ui) if can else "disabled")
            else:
                idx = ui.list_top + it.id
                if idx < len(entries):
                    if ui.hot == key(it):
                        R.panel(cr, it.x, it.y + 1, it.w, it.h - 1, fill=R.hexrgb("e5f1fb"),
                                border=R.hexrgb("a8cbef"))
                    R.draw_text(cr, _elide_left(cr, entries[idx], it.w - 8, 12), it.x, it.y, it.w - 4, it.h,
                                px=12, align="right")
        if eng.mode == E.STAT:
            R.draw_text(cr, f"Count = {len(eng.dataset)}", x + 5, y + 101, 160, 18, px=12, align="left")
        y += 100
        h = 50
    # Expression line (control 404) and memory indicator (401).
    if eng.mode != E.STAT:
        expr = _elide_left(cr, eng.expression, w - 12, 12)
        R.draw_text(cr, expr, x + 6, y + 2, w - 12, 16, px=12, align="right")
    if eng.has_memory:
        R.draw_text(cr, "M", x + 5, y + h - 25, 14, 18, px=12, align="left")
    # Main number (control 150), shrinking the font when it is long.
    text = eng.display
    avail = w - 24
    px = R.fit_text_px(cr, text, R.NUM_FONT, NUM_SIZES, avail)
    if R.text_size(R.layout(cr, text, R.NUM_FONT, px))[0] > avail:
        text = _elide_left(cr, text, avail, px, R.NUM_FONT)
    R.draw_text(cr, text, x + 18, y + 17, avail, h - 19, family=R.NUM_FONT, px=px, align="right")


def _elide_left(cr, text: str, max_w: float, px: float, family: str = R.UI_FONT) -> str:
    """Keep the tail of `text`, prefixed with '«' like win32calc, when it is too wide."""
    if not text or R.text_size(R.layout(cr, text, family, px))[0] <= max_w:
        return text
    lo, hi = 0, len(text)
    while lo < hi:
        mid = (lo + hi) // 2
        if R.text_size(R.layout(cr, "«" + text[mid:], family, px))[0] <= max_w:
            hi = mid
        else:
            lo = mid + 1
    return "«" + text[lo:]
