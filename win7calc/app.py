"""GTK3 front end: window, menus, mouse and keyboard handling."""
from __future__ import annotations

import json
import os
import sys

import gi

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gdk, GLib, Gtk  # noqa: E402

from . import engine as E  # noqa: E402
from . import layout, view  # noqa: E402
from .numfmt import parse_number  # noqa: E402

CONFIG = os.path.join(GLib.get_user_config_dir(), "win7calc", "settings.json")

CSS = """
.w7-client { background-image: linear-gradient(to bottom, #edf4fc, #d9e4f1 75px, #d9e4f1); }
menubar {
  background-image: linear-gradient(to bottom, #ffffff, #f2f5fa 50%, #e3e9f3);
  border-bottom: 1px solid #b9c5d6; padding: 0; box-shadow: none;
}
menubar, menu, menuitem, .w7-panel, .w7-panel * { font-family: "Segoe UI", Selawik, "Noto Sans", sans-serif; font-size: @PT@pt; }
menubar > menuitem { padding: 2px 7px; color: #000; border: 1px solid transparent; border-radius: 3px; }
menubar > menuitem:hover {
  background-image: linear-gradient(to bottom, #f8fbfe, #e1ecfa); border-color: #a8c3e5; color: #000; box-shadow: none;
}
menu, .menu, .context-menu { background-color: #f0f0f0; border: 1px solid #979797; padding: 2px; }
menu menuitem { padding: 3px 18px 3px 6px; color: #000; border: 1px solid transparent; border-radius: 3px; min-height: 16px; }
menu menuitem:hover { background-image: linear-gradient(to bottom, #fbfdff, #e3edfb); border-color: #a8c8ef; color: #000; }
menu menuitem:disabled, menu menuitem:disabled label { color: #6d6d6d; }
menu menuitem accelerator { color: #000; margin-left: 24px; }
menu separator { background: #e0e0e0; margin: 3px 2px 3px 26px; min-height: 1px; }
.w7-panel { background-color: #ffffff; border: 1px solid #8e9cad; border-radius: 2px; }
.w7-panel label { color: #000; }
.w7-panel entry { min-height: 18px; padding: 1px 4px; border-radius: 0; border: 1px solid #abadb3;
                  background: #fff; color: #000; box-shadow: none; }
.w7-panel entry.w7-readonly { background: #f4f7fb; }
.w7-panel combobox button, .w7-panel button { min-height: 18px; padding: 1px 6px; border-radius: 3px;
  border: 1px solid #707070; color: #000; box-shadow: none;
  background-image: linear-gradient(to bottom, #f2f2f2, #ebebeb 50%, #dddddd 51%, #cfcfcf); }
.w7-panel button:hover { border-color: #3c7fb1;
  background-image: linear-gradient(to bottom, #eaf6fd, #d9f0fc 50%, #bee6fd 51%, #a7d9f5); }
.w7-panel spinbutton { min-height: 18px; }
"""

MODES = [(E.STD, "S_tandard", "<Alt>1"), (E.SCI, "_Scientific", "<Alt>2"),
         (E.PROG, "_Programmer", "<Alt>3"), (E.STAT, "St_atistics", "<Alt>4")]
WORKSHEETS = ["Mortgage", "Vehicle lease", "Fuel economy (mpg)", "Fuel economy (L/100 km)"]
WORKSHEET_LABELS = ["_Mortgage", "_Vehicle lease", "_Fuel economy (mpg)", "F_uel economy (L/100 km)"]

# Character keys shared by all modes (accelerator table 906 and the common part of 900-903).
COMMON_CHARS = {".": E.POINT, ",": E.POINT, "+": E.ADD, "-": E.SUB, "*": E.MUL, "/": E.DIV, "=": E.EQU}
MODE_CHARS = {
    E.STD: {"r": E.RECIP, "@": E.SQRT, "%": E.PERCENT},
    E.SCI: {"!": E.FACT, "s": E.SIN, "o": E.COS, "t": E.TAN, "r": E.RECIP, "y": E.PWR, "#": E.CUBE,
            "@": E.SQRT, "q": E.SQR, "i": E.INV, "m": E.DMS, "n": E.LN, "l": E.LOG, "v": E.FE, "x": E.EXP,
            "e": E.EXP, "p": E.PI, "(": E.OPENP, ")": E.CLOSEP, ";": E.INT, "d": E.MOD},
    E.PROG: {"a": 140, "b": 141, "c": 142, "d": 143, "e": 144, "f": 145, "j": E.ROL, "k": E.ROR,
             "%": E.MOD, "&": E.AND, "|": E.OR, "~": E.NOT, "^": E.XOR, "(": E.OPENP, ")": E.CLOSEP,
             "<": E.LSH, ">": E.RSH},
    E.STAT: {"d": E.STAT_CAD, "v": E.FE, "a": E.STAT_AVE, "s": E.STAT_SUM, "t": E.STAT_SDEV,
             "x": E.EXP, "e": E.EXP},
}
MODE_CTRL = {
    E.SCI: {"s": E.SINH, "o": E.COSH, "t": E.TANH, "y": E.ROOT, "b": E.CUBEROOT, "g": E.POW10},
    E.STAT: {"a": E.STAT_AVE2, "s": E.STAT_SUM2, "t": E.STAT_SDEV1},
}
COMMON_CTRL = {"l": E.MC, "r": E.MR, "m": E.MS, "p": E.MPLUS, "q": E.MMINUS}
MODE_FKEYS = {
    E.SCI: {"F3": E.DEG, "F4": E.RAD, "F5": E.GRAD},
    E.PROG: {"F5": E.HEX, "F6": E.DEC, "F7": E.OCT, "F8": E.BIN, "F12": E.QWORD, "F2": E.DWORD,
             "F3": E.WORD, "F4": E.BYTE},
}
NAMED_KEYS = {"Return": E.EQU, "KP_Enter": E.EQU, "Escape": E.CLEAR, "Delete": E.CE, "KP_Delete": E.CE,
              "BackSpace": E.BACK, "F9": E.SIGN, "KP_Add": E.ADD, "KP_Subtract": E.SUB,
              "KP_Multiply": E.MUL, "KP_Divide": E.DIV, "KP_Decimal": E.POINT, "KP_Separator": E.POINT}

HELP_TEXT = """Keyboard shortcuts (same as Windows 7 Calculator)

All modes
  0-9 . +  -  *  /  =  Enter        number entry and operators
  Esc  C      Del  CE      Backspace  ←      F9  ±
  Ctrl+L MC   Ctrl+R MR   Ctrl+M MS   Ctrl+P M+   Ctrl+Q M-
  Ctrl+C copy   Ctrl+V paste   Ctrl+H history
  Alt+1..4 Standard / Scientific / Programmer / Statistics
  Ctrl+F4 Basic   Ctrl+U Unit conversion   Ctrl+E Date calculation

Standard      R 1/x   @ √   % percent
Scientific    S sin  O cos  T tan  Ctrl+S/O/T sinh/cosh/tanh  I Inv
              N ln  L log  Q x²  # x³  Y xʸ  Ctrl+Y ʸ√x  Ctrl+B ∛x
              Ctrl+G 10ˣ  ! n!  M dms  ; Int  D Mod  P π  V F-E  X/E Exp
              ( )   F3 Degrees  F4 Radians  F5 Grads
Programmer    A-F digits  F5-F8 Hex/Dec/Oct/Bin  F12/F2/F3/F4 Qword/Dword/Word/Byte
              & And  | Or  ^ Xor  ~ Not  < Lsh  > Rsh  % Mod  J RoL  K RoR
Statistics    Enter Add  D CAD  A x̄  Ctrl+A x̄²  S ∑x  Ctrl+S ∑x²  T σn  Ctrl+T σn-1"""


def load_settings() -> dict:
    try:
        with open(CONFIG) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


class CalcWindow(Gtk.Window):
    def __init__(self, scale: float):
        super().__init__(title="Calculator")
        self.scale = scale
        self.eng = E.Engine()
        self.ui = view.UIState()
        self.ext = "basic"            # basic | unit | date | worksheet:<name>
        self.panel_widget = None
        self._syncing = False
        self._flash_id = 0

        self.set_resizable(False)
        self.set_icon_name("accessories-calculator")
        provider = Gtk.CssProvider()
        provider.load_from_data(CSS.replace("@PT@", str(round(9 * scale, 1))).encode())
        Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(), provider,
                                                 Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

        self.accel = Gtk.AccelGroup()
        self.add_accel_group(self.accel)
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.add(vbox)
        vbox.pack_start(self._build_menu(), False, False, 0)

        self.client = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        self.client.get_style_context().add_class("w7-client")
        vbox.pack_start(self.client, True, True, 0)

        self.canvas = Gtk.DrawingArea()
        self.canvas.set_can_focus(True)
        self.canvas.add_events(Gdk.EventMask.BUTTON_PRESS_MASK | Gdk.EventMask.BUTTON_RELEASE_MASK
                               | Gdk.EventMask.POINTER_MOTION_MASK | Gdk.EventMask.LEAVE_NOTIFY_MASK
                               | Gdk.EventMask.SCROLL_MASK)
        self.canvas.connect("draw", self._on_draw)
        self.canvas.connect("button-press-event", self._on_press)
        self.canvas.connect("button-release-event", self._on_release)
        self.canvas.connect("motion-notify-event", self._on_motion)
        self.canvas.connect("leave-notify-event", self._on_leave)
        self.canvas.connect("scroll-event", self._on_scroll)
        self.client.pack_start(self.canvas, False, False, 0)

        self.connect("key-press-event", self._on_key)
        self.connect("delete-event", self._on_close)

        s = load_settings()
        self.eng.grouping = bool(s.get("grouping", False))
        self.ui.history = bool(s.get("history", False))
        mode = s.get("mode", E.STD)
        self._set_mode(mode if mode in {m[0] for m in MODES} else E.STD)
        self._set_ext(s.get("ext", "basic"))
        self._sync_menu()
        self.canvas.grab_focus()

    # ------------------------------------------------------------ menus
    def _item(self, menu, label, cb=None, accel=None, kind=None, group=None):
        if kind == "radio":
            it = Gtk.RadioMenuItem.new_with_mnemonic_from_widget(group, label)
        elif kind == "check":
            it = Gtk.CheckMenuItem.new_with_mnemonic(label)
        else:
            it = Gtk.MenuItem.new_with_mnemonic(label)
        if cb:
            it.connect("toggled" if kind else "activate", cb)
        if accel:
            k, mods = Gtk.accelerator_parse(accel)
            it.add_accelerator("activate", self.accel, k, mods, Gtk.AccelFlags.VISIBLE)
        menu.append(it)
        return it

    def _submenu(self, bar, label):
        top = Gtk.MenuItem.new_with_mnemonic(label)
        menu = Gtk.Menu()
        top.set_submenu(menu)
        bar.append(top)
        return top, menu

    def _build_menu(self):
        bar = Gtk.MenuBar()
        _, m = self._submenu(bar, "_View")
        self.mode_items = {}
        group = None
        for mode, label, accel in MODES:
            it = self._item(m, label, lambda w, md=mode: self._menu_mode(w, md), accel, "radio", group)
            group = group or it
            self.mode_items[mode] = it
        m.append(Gtk.SeparatorMenuItem())
        self.history_item = self._item(m, "Histor_y", self._menu_history, "<Ctrl>h", "check")
        self.grouping_item = self._item(m, "D_igit grouping", self._menu_grouping, None, "check")
        m.append(Gtk.SeparatorMenuItem())
        self.ext_items = {}
        g2 = self._item(m, "_Basic", lambda w: self._menu_ext(w, "basic"), "<Ctrl>F4", "radio")
        self.ext_items["basic"] = g2
        self.ext_items["unit"] = self._item(m, "_Unit conversion", lambda w: self._menu_ext(w, "unit"),
                                            "<Ctrl>u", "radio", g2)
        self.ext_items["date"] = self._item(m, "_Date calculation", lambda w: self._menu_ext(w, "date"),
                                            "<Ctrl>e", "radio", g2)
        ws_top, ws = Gtk.MenuItem.new_with_mnemonic("_Worksheets"), Gtk.Menu()
        ws_top.set_submenu(ws)
        m.append(ws_top)
        for name, label in zip(WORKSHEETS, WORKSHEET_LABELS):
            key = "worksheet:" + name
            self.ext_items[key] = self._item(ws, label, lambda w, k=key: self._menu_ext(w, k), None, "radio", g2)

        _, m = self._submenu(bar, "_Edit")
        self._item(m, "_Copy", lambda w: self.copy(), "<Ctrl>c")
        self._item(m, "_Paste", lambda w: self.paste(), "<Ctrl>v")
        m.append(Gtk.SeparatorMenuItem())
        self.edit_history, hm = self._submenu(m, "_History")
        self._item(hm, "Copy h_istory", lambda w: self.copy_history())
        self._item(hm, "C_lear", lambda w: self.clear_history(), "<Ctrl><Shift>d")
        self.edit_dataset, dm = self._submenu(m, "_Dataset")
        self._item(dm, "Copy d_ataset", lambda w: self.copy_dataset())
        self._item(dm, "C_lear", lambda w: self.command(E.STAT_CAD))

        _, m = self._submenu(bar, "_Help")
        self._item(m, "_View Help", lambda w: self.show_help(), "F1")
        m.append(Gtk.SeparatorMenuItem())
        self._item(m, "_About Calculator", lambda w: self.show_about())
        return bar

    def _sync_menu(self):
        self._syncing = True
        self.mode_items[self.eng.mode].set_active(True)
        self.history_item.set_active(self.ui.history)
        self.grouping_item.set_active(self.eng.grouping)
        self.ext_items.get(self.ext, self.ext_items["basic"]).set_active(True)
        has_hist = self.eng.mode in (E.STD, E.SCI)
        self.history_item.set_sensitive(has_hist)
        self.edit_history.set_sensitive(has_hist and self.ui.history)
        self.edit_dataset.set_sensitive(self.eng.mode == E.STAT)
        self._syncing = False

    def _menu_mode(self, item, mode):
        if not self._syncing and item.get_active():
            self._set_mode(mode)

    def _menu_history(self, item):
        if not self._syncing:
            self.ui.history = item.get_active()
            self.ui.list_top = view.max_top(self.eng)
            self._relayout()

    def _menu_grouping(self, item):
        if not self._syncing:
            self.eng.grouping = item.get_active()
            self._changed()

    def _menu_ext(self, item, ext):
        if not self._syncing and item.get_active():
            self._set_ext(ext)

    # ------------------------------------------------------------ mode / layout
    def _set_mode(self, mode):
        self.eng.set_mode(mode)
        self.ui.list_top = view.max_top(self.eng)
        self._relayout()
        self._sync_menu()

    def _set_ext(self, ext):
        from . import panels
        if self.panel_widget is not None:
            self.client.remove(self.panel_widget)
            self.panel_widget = None
        self.ext = ext
        if ext == "unit":
            self.panel_widget = panels.UnitPanel(scale=self.scale)
        elif ext == "date":
            self.panel_widget = panels.DatePanel(scale=self.scale)
        elif ext.startswith("worksheet:") and ext[10:] in WORKSHEETS:
            self.panel_widget = panels.WorksheetPanel(ext[10:], scale=self.scale)
        else:
            self.ext = "basic"
        if self.panel_widget is not None:
            m = round(layout.MARGIN * self.scale)
            self.panel_widget.set_valign(Gtk.Align.START)
            self.panel_widget.set_margin_top(m)
            self.panel_widget.set_margin_end(m)
            self.panel_widget.set_margin_bottom(m)
            self.client.pack_start(self.panel_widget, False, False, 0)
            self.panel_widget.show_all()
            self._push_panel()
        self._relayout()
        self._sync_menu()

    def _relayout(self):
        self.lay = layout.build(self.eng.mode, self.ui.history, False)
        self.canvas.set_size_request(round(self.lay.width * self.scale), round(self.lay.height * self.scale))
        if hasattr(self, "mode_items"):
            self._sync_menu()
        self.resize(1, 1)
        self.canvas.queue_draw()

    # ------------------------------------------------------------ painting
    def _on_draw(self, widget, cr):
        cr.scale(self.scale, self.scale)
        view.paint(cr, self.lay, self.eng, self.ui)
        return True

    def _hit(self, ex, ey):
        x, y = ex / self.scale, ey / self.scale
        for it in view.display_items(self.lay, self.eng, self.ui) + self.lay.items:
            if it.kind in ("button", "radio", "bit", "arrow", "row") and it.contains(x, y):
                if it.kind == "bit" and it.id >= self.eng.bits:
                    return None
                return view.key(it)
        return None

    def _on_motion(self, widget, ev):
        hot = self._hit(ev.x, ev.y)
        if hot != self.ui.hot:
            self.ui.hot = hot
            widget.queue_draw()
        return True

    def _on_leave(self, widget, ev):
        self.ui.hot = None
        widget.queue_draw()
        return True

    def _on_press(self, widget, ev):
        widget.grab_focus()
        if ev.button == 1 and ev.type == Gdk.EventType.BUTTON_PRESS:
            self.ui.pressed = self._hit(ev.x, ev.y)
            widget.queue_draw()
        return True

    def _on_release(self, widget, ev):
        if ev.button != 1:
            return True
        target = self._hit(ev.x, ev.y)
        pressed, self.ui.pressed = self.ui.pressed, None
        if target is not None and target == pressed:
            self._activate(*target)
        widget.queue_draw()
        return True

    def _on_scroll(self, widget, ev):
        if ev.direction == Gdk.ScrollDirection.UP:
            self._scroll(-1)
        elif ev.direction == Gdk.ScrollDirection.DOWN:
            self._scroll(1)
        return True

    def _scroll(self, delta):
        self.ui.list_top = max(0, min(view.max_top(self.eng), self.ui.list_top + delta))
        self.canvas.queue_draw()

    def _activate(self, kind, ident):
        if kind == "button" or kind == "radio":
            self.command(ident)
        elif kind == "bit":
            self.eng.toggle_bit(ident)
            self._changed()
        elif kind == "arrow":
            self._scroll(-1 if ident == view.UP else 1)
        elif kind == "row":
            idx = self.ui.list_top + ident
            if self.eng.mode in (E.STD, E.SCI) and idx < len(self.eng.history):
                self.eng.set_value(self.eng.history[idx][1])
                self._changed()

    # ------------------------------------------------------------ commands
    def command(self, cid):
        n_hist, n_data = len(self.eng.history), len(self.eng.dataset)
        self.eng.command(cid)
        if len(self.eng.history) != n_hist or len(self.eng.dataset) != n_data:
            self.ui.list_top = view.max_top(self.eng)
        self._changed()

    def _changed(self):
        self.ui.list_top = min(self.ui.list_top, view.max_top(self.eng))
        self.canvas.queue_draw()
        self._push_panel()

    def _push_panel(self):
        if self.panel_widget is None:
            return
        if self.eng.error:
            return
        v = self.eng.current()
        self.panel_widget.set_value(str(v) if self.eng.int_mode else self.eng.display.replace(",", ""))

    def _flash(self, cid):
        self.ui.flash = ("button", cid)
        self.canvas.queue_draw()
        if self._flash_id:
            GLib.source_remove(self._flash_id)

        def clear():
            self.ui.flash = None
            self._flash_id = 0
            self.canvas.queue_draw()
            return False
        self._flash_id = GLib.timeout_add(110, clear)

    # ------------------------------------------------------------ keyboard
    def key_command(self, keyval: int, state) -> int | None:
        mode = self.eng.mode
        name = Gdk.keyval_name(keyval) or ""
        ctrl = bool(state & Gdk.ModifierType.CONTROL_MASK)
        alt = bool(state & Gdk.ModifierType.MOD1_MASK)
        if alt:
            return None
        ch = chr(Gdk.keyval_to_unicode(keyval)) if Gdk.keyval_to_unicode(keyval) else ""
        if ctrl:
            low = ch.lower() if ch else name.lower()
            return MODE_CTRL.get(mode, {}).get(low) or COMMON_CTRL.get(low)
        if name in MODE_FKEYS.get(mode, {}):
            return MODE_FKEYS[mode][name]
        if name in ("Return", "KP_Enter") and mode == E.STAT:
            return E.STAT_ADD
        if name in ("Up", "Page_Up", "Down", "Page_Down"):
            return -1 if name in ("Up", "Page_Up") else -2
        if name in NAMED_KEYS:
            return NAMED_KEYS[name]
        return self.char_command(ch)

    def char_command(self, ch: str) -> int | None:
        if not ch:
            return None
        if len(ch) == 1 and ch in "0123456789":
            return E.DIGIT0 + int(ch)
        mode_map = MODE_CHARS.get(self.eng.mode, {})
        if ch in mode_map:
            return mode_map[ch]
        if ch.lower() in mode_map and self.eng.mode == E.PROG:
            return mode_map[ch.lower()]
        return COMMON_CHARS.get(ch)

    def _on_key(self, widget, ev):
        focus = self.get_focus()
        if isinstance(focus, (Gtk.Entry, Gtk.SpinButton)) or (focus is not None and focus is not self.canvas
                                                              and focus.get_ancestor(Gtk.Popover)):
            return False
        cid = self.key_command(ev.keyval, ev.state)
        if cid is None:
            return False
        if cid in (-1, -2):
            self._scroll(-1 if cid == -1 else 1)
            return True
        self.command(cid)
        if any(it.kind == "button" and it.id == cid for it in self.lay.items):
            self._flash(cid)
        return True

    # ------------------------------------------------------------ clipboard
    def _clipboard(self):
        return Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD)

    def copy(self):
        text = self.eng.display
        if not self.eng.error:
            text = text.replace(",", "") if not self.eng.int_mode or self.eng.radix == 10 else text.replace(" ", "")
        self._clipboard().set_text(text, -1)

    def copy_history(self):
        lines = [f"{t} = {self.eng.fmt(v)}" for t, v in self.eng.history]
        self._clipboard().set_text("\n".join(lines), -1)

    def copy_dataset(self):
        self._clipboard().set_text("\n".join(self.eng.fmt(v, False) for v in self.eng.dataset), -1)

    def clear_history(self):
        self.eng.clear_history()
        self.ui.list_top = 0
        self.canvas.queue_draw()

    def paste(self):
        text = self._clipboard().wait_for_text()
        if text:
            self.paste_text(text)

    def paste_text(self, text: str):
        """Win7 treats pasted text as typed keys; a plain number is loaded directly."""
        num = parse_number(text) if not self.eng.int_mode else None
        if num is not None:
            self.eng.set_value(num)
            return self._changed()
        for ch in text:
            if ch in " \t\r\n,":
                continue
            cid = self.char_command(ch)
            if cid is None and ch in "eE" and not self.eng.int_mode:
                cid = E.EXP
            if cid is not None:
                self.eng.command(cid)
        self.ui.list_top = view.max_top(self.eng)
        self._changed()

    # ------------------------------------------------------------ dialogs
    def show_help(self):
        dlg = Gtk.Dialog(title="Calculator Help", transient_for=self, modal=True)
        dlg.add_button("_Close", Gtk.ResponseType.CLOSE)
        lbl = Gtk.Label()
        lbl.set_markup("<tt>" + GLib.markup_escape_text(HELP_TEXT) + "</tt>")
        lbl.set_margin_start(14)
        lbl.set_margin_end(14)
        lbl.set_margin_top(10)
        lbl.set_margin_bottom(10)
        lbl.set_selectable(True)
        dlg.get_content_area().add(lbl)
        dlg.show_all()
        dlg.run()
        dlg.destroy()

    def show_about(self):
        dlg = Gtk.AboutDialog(transient_for=self, modal=True)
        dlg.set_program_name("Calculator")
        dlg.set_logo_icon_name("accessories-calculator")
        dlg.set_comments("A Linux (GTK3) replica of the classic Windows 7 Calculator.\n"
                         "Layout, key bindings and behaviour follow win32calc.exe.")
        dlg.set_version("1.0")
        dlg.run()
        dlg.destroy()

    # ------------------------------------------------------------ persistence
    def _on_close(self, *a):
        try:
            os.makedirs(os.path.dirname(CONFIG), exist_ok=True)
            with open(CONFIG, "w") as f:
                json.dump({"mode": self.eng.mode, "grouping": self.eng.grouping,
                           "history": self.ui.history, "ext": self.ext}, f)
        except OSError:
            pass
        return False


def main(argv=None):
    import argparse
    p = argparse.ArgumentParser(prog="win7calc", description="Windows 7 Calculator replica for Linux")
    p.add_argument("--scale", type=float, default=float(os.environ.get("WIN7CALC_SCALE", "1.0")),
                   help="UI zoom factor (e.g. 1.25 or 1.5), default 1.0 = original Win7 size")
    p.add_argument("--mode", choices=[m[0] for m in MODES], help="start in this mode")
    args = p.parse_args(argv)
    if not Gtk.init_check(sys.argv)[0]:
        sys.stderr.write(
            "win7calc: cannot open a display (DISPLAY/WAYLAND_DISPLAY not set).\n"
            "  Over SSH, either reconnect with X forwarding:  ssh -X user@host\n"
            "  or run it in a web browser:                    ./win7calc.sh --web\n")
        return 1
    win = CalcWindow(max(0.5, min(args.scale, 4.0)))
    if args.mode:
        win._set_mode(args.mode)
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    Gtk.main()


if __name__ == "__main__":
    sys.exit(main())
