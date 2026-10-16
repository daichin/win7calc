"""Window geometry, taken from win32calc's dialog templates.

Dialog coordinates are converted to 96-dpi pixels: x/2, y*0.6275 — this
reproduces Win7's 34x27 buttons on a 39x32 pitch exactly.
"""
from __future__ import annotations

from dataclasses import dataclass

from . import engine as E

MARGIN = 11      # window edge to display / keypad
GAP = 8          # display to keypad
DISPLAY_H = 50
HISTORY_DISPLAY_H = 151
STAT_DISPLAY_H = 150
PANEL_W, PANEL_H = 363, 251


def ux(v: float) -> int:
    return round(v / 2)


def uy(v: float) -> int:
    return int(v * 0.6275 + 0.01)


def uh(v: float) -> int:
    return round(v * 0.6275)


@dataclass
class Item:
    kind: str            # button | radio | box | parens | bit | bitlabel
    id: int
    x: float
    y: float
    w: float
    h: float
    style: str = "op"    # button style: digit | op | mem
    text: str = ""

    def contains(self, px: float, py: float) -> bool:
        return self.x <= px < self.x + self.w and self.y <= py < self.y + self.h


# (id, x, y, w, h) in dialog units — copied from the .mui DIALOG resources.
_STD = [
    (122, 0, 0, 68, 43), (123, 78, 0, 68, 43), (124, 156, 0, 68, 43), (125, 234, 0, 68, 43), (126, 312, 0, 68, 43),
    (83, 0, 51, 68, 43), (82, 78, 51, 68, 43), (81, 156, 51, 68, 43), (80, 234, 51, 68, 43), (110, 312, 51, 68, 43),
    (137, 0, 102, 68, 43), (138, 78, 102, 68, 43), (139, 156, 102, 68, 43), (91, 234, 102, 68, 43), (118, 312, 102, 68, 43),
    (134, 0, 154, 68, 43), (135, 78, 154, 68, 43), (136, 156, 154, 68, 43), (92, 234, 154, 68, 43), (114, 312, 154, 68, 43),
    (131, 0, 205, 68, 43), (132, 78, 205, 68, 43), (133, 156, 205, 68, 43), (94, 234, 205, 68, 43), (121, 312, 205, 68, 94),
    (130, 0, 256, 146, 43), (84, 156, 256, 68, 43), (93, 234, 256, 68, 43),
]

_SCI_LEFT = [
    (98, 0, 102, 68, 43), (115, 0, 154, 68, 43), (120, 0, 205, 68, 43), (119, 0, 256, 68, 43),
    (146, 78, 51, 68, 43), (105, 78, 102, 68, 43), (106, 78, 154, 68, 43), (107, 78, 205, 68, 43), (127, 78, 256, 68, 43),
    (108, 156, 51, 68, 43), (102, 156, 102, 68, 43), (103, 156, 154, 68, 43), (104, 156, 205, 68, 43), (95, 156, 256, 68, 43),
    (128, 234, 51, 68, 43), (111, 234, 102, 68, 43), (97, 234, 154, 68, 43), (112, 234, 205, 68, 43), (109, 234, 256, 68, 43),
    (129, 312, 51, 68, 43), (113, 312, 102, 68, 43), (96, 312, 154, 68, 43), (116, 312, 205, 68, 43), (117, 312, 256, 68, 43),
]

_PROG_LEFT = [
    (128, 156, 157, 68, 43), (99, 156, 208, 68, 43), (87, 156, 259, 68, 43), (89, 156, 310, 68, 43), (101, 156, 361, 68, 43),
    (95, 234, 106, 68, 43), (129, 234, 157, 68, 43), (100, 234, 208, 68, 43), (88, 234, 259, 68, 43), (90, 234, 310, 68, 43),
    (86, 234, 361, 68, 43),
    (140, 312, 106, 68, 43), (141, 312, 157, 68, 43), (142, 312, 208, 68, 43), (143, 312, 259, 68, 43),
    (144, 312, 310, 68, 43), (145, 312, 361, 68, 43),
]

_STAT = [
    (122, 0, 248, 68, 43), (123, 78, 248, 68, 43), (124, 156, 248, 68, 43), (125, 234, 248, 68, 43), (126, 312, 248, 68, 43),
    (83, 0, 300, 68, 43), (1056, 78, 300, 68, 43), (81, 156, 300, 68, 43), (119, 234, 300, 68, 43), (127, 312, 300, 68, 43),
    (137, 0, 351, 68, 43), (138, 78, 351, 68, 43), (139, 156, 351, 68, 43), (171, 234, 351, 68, 43), (172, 312, 351, 68, 43),
    (134, 0, 402, 68, 43), (135, 78, 402, 68, 43), (136, 156, 402, 68, 43), (173, 234, 402, 68, 43), (174, 312, 402, 68, 43),
    (131, 0, 453, 68, 43), (132, 78, 453, 68, 43), (133, 156, 453, 68, 43), (175, 234, 453, 68, 43), (176, 312, 453, 68, 43),
    (130, 0, 504, 146, 43), (84, 156, 504, 68, 43), (80, 234, 504, 68, 43), (168, 312, 504, 68, 43),
]

TOGGLE_IDS = {E.INV, E.FE}

LABELS = {
    E.MC: "MC", E.MR: "MR", E.MS: "MS", E.MPLUS: "M+", E.MMINUS: "M-",
    E.BACK: "←", E.CE: "CE", E.CLEAR: "C", E.SIGN: "±", E.SQRT: "√",
    E.DIV: "/", E.MUL: "*", E.SUB: "-", E.ADD: "+", E.EQU: "=", E.POINT: ".",
    E.PERCENT: "%", E.RECIP: "1/<i>x</i>",
    E.INT: "Int", E.DMS: "dms", E.PI: "π", E.FE: "F-E", E.INV: "Inv", E.EXP: "Exp", E.MOD: "Mod",
    E.SINH: "sinh", E.COSH: "cosh", E.TANH: "tanh", E.SIN: "sin", E.COS: "cos", E.TAN: "tan",
    E.LN: "ln", E.LOG: "log", E.OPENP: "(", E.CLOSEP: ")", E.FACT: "n!",
    E.SQR: "<i>x</i>²", E.PWR: "<i>x</i>ʸ", E.CUBE: "<i>x</i>³", E.ROOT: "ʸ√<i>x</i>",
    E.CUBEROOT: "∛<i>x</i>", E.POW10: "10ˣ",
    E.ROL: "RoL", E.ROR: "RoR", E.OR: "Or", E.XOR: "Xor", E.LSH: "Lsh", E.RSH: "Rsh", E.NOT: "Not", E.AND: "And",
    E.STAT_CAD: "CAD", E.STAT_ADD: "Add",
    E.STAT_AVE: "<i>x̄</i>", E.STAT_AVE2: "<i>x̄</i>²", E.STAT_SUM: "∑<i>x</i>", E.STAT_SUM2: "∑<i>x</i>²",
    E.STAT_SDEV: "σₙ", E.STAT_SDEV1: "σₙ₋₁",
}
for _d in range(16):
    LABELS[E.DIGIT0 + _d] = "0123456789ABCDEF"[_d]

# Labels shown while Inv is checked (strings 700-709).
INV_LABELS = {
    E.PI: "2*π", E.LN: "eˣ", E.SINH: "sinh⁻¹", E.SIN: "sin⁻¹", E.COSH: "cosh⁻¹",
    E.COS: "cos⁻¹", E.TANH: "tanh⁻¹", E.TAN: "tan⁻¹", E.DMS: "deg", E.INT: "Frac",
}

TOOLTIPS = {
    E.MC: "Memory clear", E.MR: "Memory recall", E.MS: "Memory store", E.MPLUS: "Memory add",
    E.MMINUS: "Memory subtract", E.CE: "Clear entry", E.CLEAR: "Clear", E.BACK: "Backspace",
    E.SIGN: "Negate", E.MUL: "Multiply", E.DIV: "Divide", E.ADD: "Add", E.SUB: "Subtract",
    E.SQRT: "Square root", E.RECIP: "Reciprocal", E.INV: "Inverse functions", E.INT: "Integer part",
    E.DMS: "Degree minute second", E.FACT: "Factorial", E.MOD: "Modulo", E.PI: "Pi",
    E.FE: "Exponential notation", E.EXP: "Exponential", E.SIN: "Sine", E.COS: "Cosine", E.TAN: "Tangent",
    E.SINH: "Hyperbolic sine", E.COSH: "Hyperbolic cosine", E.TANH: "Hyperbolic tangent",
    E.SQR: "Square", E.CUBE: "Cube", E.PWR: "Raise to y power", E.LN: "Natural log", E.LOG: "Log",
    E.CUBEROOT: "Cube root", E.ROOT: "Order of y root", E.POW10: "Anti log",
    E.ROL: "Rotate on left", E.ROR: "Rotate on right", E.OR: "Or", E.XOR: "Exclusive or",
    E.LSH: "Left shift", E.RSH: "Right shift", E.NOT: "Not", E.AND: "And",
    E.STAT_AVE: "Average", E.STAT_AVE2: "Average of square", E.STAT_SUM: "Sum", E.STAT_SUM2: "Sum of square",
    E.STAT_SDEV: "Standard deviation population", E.STAT_SDEV1: "Standard deviation",
    E.STAT_CAD: "Clear all dataset values", E.STAT_ADD: "Add dataset value", E.POINT: "Decimal separator",
    E.OPENP: "Open parenthesis", E.CLOSEP: "Close parenthesis", E.PERCENT: "Percentage", E.EQU: "Equals",
}


def _style(cid: int) -> str:
    if E.DIGIT0 <= cid <= E.DIGIT0 + 15 or cid == E.POINT:
        return "digit"
    if E.MC <= cid <= E.MMINUS:
        return "mem"
    return "op"


def _buttons(table, dx=0, dy=0, ox=0.0, oy=0.0):
    return [Item("button", cid, ox + ux(x + dx), oy + uy(y + dy), ux(w), uh(h), _style(cid), LABELS.get(cid, ""))
            for cid, x, y, w, h in table]


def _shift(table, dx):
    return [(cid, x + dx, y, w, h) for cid, x, y, w, h in table]


@dataclass
class Layout:
    width: int
    height: int
    display: tuple            # x, y, w, h
    items: list
    panel: tuple | None       # x, y, w, h for the extension panel
    bitpanel: tuple | None = None


def build(mode: str, history: bool, panel: bool) -> Layout:
    m = MARGIN
    items: list[Item] = []
    bitpanel = None
    if mode == E.STAT:
        kw, dh = ux(380), STAT_DISPLAY_H
        ky = m + dh + GAP
        items += _buttons(_STAT, dy=-248, ox=m, oy=ky)
        kh = uy(504 - 248) + uh(43)
    elif mode == E.STD:
        kw, dh = ux(380), HISTORY_DISPLAY_H if history else DISPLAY_H
        ky = m + dh + GAP
        items += _buttons(_STD, ox=m, oy=ky)
        kh = uy(256) + uh(43)
    elif mode == E.SCI:
        kw, dh = ux(770), HISTORY_DISPLAY_H if history else DISPLAY_H
        ky = m + dh + GAP
        items.append(Item("box", 1079, m, ky, ux(380), uh(43)))
        for cid, x, w in ((E.DEG, 15, 128), (E.RAD, 144, 132), (E.GRAD, 277, 99)):
            items.append(Item("radio", cid, m + ux(x), ky + uy(8), ux(w), uh(26),
                              text={E.DEG: "Degrees", E.RAD: "Radians", E.GRAD: "Grads"}[cid]))
        items.append(Item("parens", 402, m, ky + uy(51), ux(68), uh(43)))
        items += _buttons(_SCI_LEFT, ox=m, oy=ky)
        items += _buttons(_shift(_STD, 390), ox=m, oy=ky)
        kh = uy(256) + uh(43)
    else:  # programmer
        kw, dh = ux(770), DISPLAY_H
        ky = m + dh + GAP
        bitpanel = (m, ky, ux(770), uh(98))
        items.append(Item("box", 1077, *bitpanel))
        items += _bit_items(m, ky)
        items.append(Item("box", 1079, m, ky + uy(106), ux(146), uh(146)))
        items.append(Item("box", 1080, m, ky + uy(259), ux(146), uh(146)))
        for i, (cid, text) in enumerate(((E.HEX, "Hex"), (E.DEC, "Dec"), (E.OCT, "Oct"), (E.BIN, "Bin"))):
            items.append(Item("radio", cid, m + ux(15), ky + uy(118 + 32 * i), ux(120), uh(24), text=text))
        for i, (cid, text) in enumerate(((E.QWORD, "Qword"), (E.DWORD, "Dword"), (E.WORD, "Word"), (E.BYTE, "Byte"))):
            items.append(Item("radio", cid, m + ux(15), ky + uy(270 + 32 * i), ux(126), uh(24), text=text))
        items.append(Item("parens", 402, m + ux(156), ky + uy(106), ux(68), uh(43)))
        items += _buttons(_PROG_LEFT, ox=m, oy=ky)
        items += _buttons(_shift([(c, x, y + 106, w, h) for c, x, y, w, h in _STD], 390), ox=m, oy=ky)
        kh = uy(361) + uh(43)

    width = m + kw + m
    height = ky + kh + m
    pnl = None
    if panel:
        pnl = (width, m, PANEL_W, PANEL_H)
        width += PANEL_W + m
        height = max(height, m + PANEL_H + m)
    return Layout(width, height, (m, m, kw, dh), items, pnl, bitpanel)


def _bit_items(ox: float, oy: float) -> list[Item]:
    out = []
    for i in range(64):
        row = 0 if i >= 32 else 1               # bits 63..32 on the top row
        k = 31 - (i % 32)                       # column from the left
        x = 15 + 15 * k + 36 * (k // 4)
        y = 8 if row == 0 else 49
        out.append(Item("bit", i, ox + x / 2, oy + uy(y), 15 / 2, uh(19)))
    for text, x, y, right in (("63", 15, 27, False), ("47", 399, 27, False), ("32", 717, 27, True),
                              ("31", 15, 69, False), ("15", 399, 69, False), ("0", 717, 69, True)):
        out.append(Item("bitlabel", 0, ox + x / 2, oy + uy(y), 14, uh(21), text=text, style="r" if right else "l"))
    return out
