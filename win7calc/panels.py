"""GTK3 side panels: Unit conversion, Date calculation, Worksheets.

Layout uses Gtk.Fixed with positions taken from the Win7 dialog templates
(dialog units -> px: x*0.5, y*0.6275), multiplied by `scale`.
"""
from __future__ import annotations

from datetime import date
from decimal import Decimal, InvalidOperation

import gi

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk  # noqa: E402

from . import datecalc, units, worksheets  # noqa: E402

PANEL_W, PANEL_H = 363, 251  # natural size at scale 1.0


def parse_decimal(text: str) -> Decimal | None:
    """Parse display/user text ('1,234.5', ' -3 ') -> Decimal, or None if invalid/blank."""
    t = text.strip().replace(",", "").replace(" ", "")
    if not t:
        return None
    try:
        d = Decimal(t)
    except InvalidOperation:
        return None
    return d if d.is_finite() else None


def short_date(d: date) -> str:
    return f"{d.month}/{d.day}/{d.year}"  # Win7 en-US short date (M/d/yyyy)


class _Panel(Gtk.Box):
    def __init__(self, scale: float = 1.0):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.scale = scale
        self.get_style_context().add_class("w7-panel")
        self.fixed = Gtk.Fixed()
        self.fixed.set_size_request(self.px(PANEL_W), self.px(PANEL_H))
        self.pack_start(self.fixed, True, True, 0)

    def px(self, v: float) -> int:
        return int(round(v * self.scale))

    def put(self, w: Gtk.Widget, x: float, y: float, wd: float = -1, ht: float = -1,
            du: bool = True) -> Gtk.Widget:
        """Place `w`; coordinates in dialog units if `du`, else in unscaled px."""
        kx, ky = (0.5, 0.6275) if du else (1.0, 1.0)
        self.fixed.put(w, self.px(x * kx), self.px(y * ky))
        w.set_size_request(self.px(wd * kx) if wd > 0 else -1,
                           self.px(ht * ky) if ht > 0 else -1)
        return w

    def move(self, w: Gtk.Widget, x: float, y: float, du: bool = True) -> None:
        kx, ky = (0.5, 0.6275) if du else (1.0, 1.0)
        self.fixed.move(w, self.px(x * kx), self.px(y * ky))

    @staticmethod
    def label(text: str) -> Gtk.Label:
        lab = Gtk.Label(label=text, xalign=0.0)
        lab.get_style_context().add_class("w7-panel-label")
        return lab

    @staticmethod
    def readonly_entry() -> Gtk.Entry:
        e = Gtk.Entry(editable=False, can_focus=True)
        e.get_style_context().add_class("w7-readonly")
        return e

    def set_value(self, text: str) -> None:  # overridden where meaningful
        pass


# --------------------------------------------------------------------------- units

class UnitPanel(_Panel):
    def __init__(self, scale: float = 1.0):
        super().__init__(scale)
        self.put(self.label("Select the type of unit you want to convert"), 18, 16)
        self.category = Gtk.ComboBoxText()
        for c in units.CATEGORIES:
            self.category.append_text(c)
        self.put(self.category, 18, 68, 690)

        self.put(self.label("From"), 18, 117)
        self.from_entry = Gtk.Entry()
        self.put(self.from_entry, 18, 148, 690, 36)
        self.from_unit = Gtk.ComboBoxText()
        self.put(self.from_unit, 18, 190, 690)

        self.put(self.label("To"), 18, 247)
        self.to_entry = self.readonly_entry()
        self.put(self.to_entry, 18, 278, 690, 36)
        self.to_unit = Gtk.ComboBoxText()
        self.put(self.to_unit, 18, 320, 690)

        self._loading = False
        self.category.connect("changed", self._on_category)
        self.from_entry.connect("changed", self._recompute)
        self.from_unit.connect("changed", self._recompute)
        self.to_unit.connect("changed", self._recompute)
        self.category.set_active(0)

    def _on_category(self, *_):
        cat = self.category.get_active_text()
        if cat is None:
            return
        self._loading = True
        names = units.units(cat)
        frm, to = units.default_units(cat)
        for combo, sel in ((self.from_unit, frm), (self.to_unit, to)):
            combo.remove_all()
            for n in names:
                combo.append_text(n)
            combo.set_active(names.index(sel))
        self._loading = False
        self._recompute()

    def _recompute(self, *_):
        if self._loading:
            return
        cat, fu, tu = (self.category.get_active_text(), self.from_unit.get_active_text(),
                       self.to_unit.get_active_text())
        val = parse_decimal(self.from_entry.get_text())
        out = ""
        if None not in (cat, fu, tu) and val is not None:
            try:
                out = units.format_number(units.convert(val, cat, fu, tu))
            except (ValueError, ArithmeticError, KeyError):
                out = ""
        self.to_entry.set_text(out)

    def set_value(self, text: str) -> None:
        self.from_entry.set_text(text)  # 'changed' recomputes To


# --------------------------------------------------------------------------- dates

class DatePicker(Gtk.MenuButton):
    """Button showing a short date; click opens a popover calendar."""

    def __init__(self, d: date | None = None):
        super().__init__()
        self._date = d or date.today()
        self._label = Gtk.Label(xalign=0.0)
        box = Gtk.Box(spacing=4)
        box.pack_start(self._label, True, True, 0)
        box.pack_end(Gtk.Arrow(arrow_type=Gtk.ArrowType.DOWN), False, False, 0)
        self.add(box)
        self.cal = Gtk.Calendar()
        pop = Gtk.Popover()
        pop.add(self.cal)
        self.cal.show()
        self.set_popover(pop)
        self.cal.connect("day-selected", self._on_day)
        self.cal.connect("day-selected-double-click", lambda *_: pop.popdown())
        self.set_date(self._date)

    def get_date(self) -> date:
        return self._date

    def set_date(self, d: date) -> None:
        self._date = d
        self._label.set_text(short_date(d))
        y, m, dd = self.cal.get_date()
        if (y, m + 1, dd) != (d.year, d.month, d.day):
            self.cal.select_month(d.month - 1, d.year)
            self.cal.select_day(d.day)

    def _on_day(self, cal: Gtk.Calendar):
        y, m, d = cal.get_date()
        if d == 0:  # no day selected (transient during month change)
            return
        try:
            self._date = date(y, m + 1, d)
        except ValueError:
            return
        self._label.set_text(short_date(self._date))


class DatePanel(_Panel):
    MODES = ["Calculate the difference between two dates",
             "Add or subtract days to a specified date"]

    def __init__(self, scale: float = 1.0):
        super().__init__(scale)
        # coordinates here are unscaled px (du=False)
        self.put(self.label("Select the date calculation you want"), 9, 8, du=False)
        self.mode = Gtk.ComboBoxText()
        for m in self.MODES:
            self.mode.append_text(m)
        self.put(self.mode, 9, 26, 345, du=False)

        self.stack = Gtk.Stack()
        self.put(self.stack, 0, 58, PANEL_W, PANEL_H - 58, du=False)
        self.stack.add_named(self._build_diff(), "diff")
        self.stack.add_named(self._build_add(), "add")
        self.mode.connect("changed", self._on_mode)
        self.mode.set_active(0)

    def _sub(self) -> tuple[Gtk.Fixed, callable]:
        fx = Gtk.Fixed()

        def put(w, x, y, wd=-1, ht=-1):
            fx.put(w, self.px(x), self.px(y))
            w.set_size_request(self.px(wd) if wd > 0 else -1, self.px(ht) if ht > 0 else -1)
            return w
        return fx, put

    def _build_diff(self) -> Gtk.Widget:
        fx, put = self._sub()
        put(self.label("From"), 9, 2)
        self.diff_from = put(DatePicker(), 9, 18, 168)
        put(self.label("To"), 186, 2)
        self.diff_to = put(DatePicker(), 186, 18, 168)
        put(self.label("Difference (years, months, weeks, days)"), 9, 54)
        self.diff_ymwd = put(self.readonly_entry(), 9, 70, 345)
        put(self.label("Difference (days)"), 9, 102)
        self.diff_days = put(self.readonly_entry(), 9, 118, 240)
        btn = put(Gtk.Button(label="Calculate"), 259, 152, 95)
        btn.connect("clicked", self.calculate_difference)
        return fx

    def _build_add(self) -> Gtk.Widget:
        fx, put = self._sub()
        put(self.label("From"), 9, 2)
        self.add_from = put(DatePicker(), 9, 18, 168)
        self.rb_add = put(Gtk.RadioButton.new_with_label(None, "Add"), 9, 52)
        self.rb_sub = put(Gtk.RadioButton.new_with_label_from_widget(self.rb_add, "Subtract"),
                          90, 52)
        self.spins: list[Gtk.SpinButton] = []
        for k, (name, hi) in enumerate((("Year(s)", 9999), ("Month(s)", 9999),
                                        ("Day(s)", 99999))):
            x = 9 + k * 117
            put(self.label(name), x, 80)
            sp = Gtk.SpinButton.new_with_range(0, hi, 1)
            sp.set_numeric(True)
            sp.set_width_chars(3)
            self.spins.append(put(sp, x, 96, 111))
        put(self.label("Date"), 9, 134)
        self.add_result = put(self.readonly_entry(), 9, 152, 240)
        btn = put(Gtk.Button(label="Calculate"), 259, 152, 95)
        btn.connect("clicked", self.calculate_add)
        return fx

    def _on_mode(self, *_):
        self.stack.set_visible_child_name("diff" if self.mode.get_active() == 0 else "add")

    def calculate_difference(self, *_):
        y, m, w, d, total = datecalc.date_difference(self.diff_from.get_date(),
                                                     self.diff_to.get_date())
        self.diff_ymwd.set_text(datecalc.format_difference(y, m, w, d))
        self.diff_days.set_text(datecalc.format_total_days(total))

    def calculate_add(self, *_):
        y, m, d = (int(s.get_value()) for s in self.spins)
        try:
            r = datecalc.add_to_date(self.add_from.get_date(), y, m, d,
                                     subtract=self.rb_sub.get_active())
            self.add_result.set_text(r.strftime("%A, %B ") + f"{r.day}, {r.year}")
        except ValueError as e:
            self.add_result.set_text(str(e))


# --------------------------------------------------------------------------- worksheets

class WorksheetPanel(_Panel):
    ROW_Y = [96, 136, 176, 216, 256, 296]  # dialog units

    def __init__(self, worksheet_name: str, scale: float = 1.0):
        super().__init__(scale)
        self.worksheet = worksheet_name
        spec = worksheets.WORKSHEETS[worksheet_name]
        self.put(self.label("Select the value you want to calculate"), 18, 16)
        self.target = Gtk.ComboBoxText()
        for f in spec["calculable"]:
            self.target.append_text(f)
        self.put(self.target, 18, 40, 690)

        self.labels: dict[str, Gtk.Label] = {}
        self.entries: dict[str, Gtk.Entry] = {}
        for f in spec["fields"]:
            lab, ent = self.label(f), Gtk.Entry()
            lab.set_ellipsize(3)  # Pango.EllipsizeMode.END
            self.put(lab, 18, 96, 340)
            self.put(ent, 360, 96, 346, 36)
            ent.connect("focus-in-event", self._on_focus, f)
            ent.connect("grab-focus", lambda _w, f=f: setattr(self, "_last_focus", f))
            ent.connect("activate", self.calculate)
            self.labels[f], self.entries[f] = lab, ent
        self.calc_btn = Gtk.Button(label="Calculate")
        self.put(self.calc_btn, 18, 336, 330)
        self.result = self.readonly_entry()
        self.put(self.result, 360, 338, 346, 36)
        self.calc_btn.connect("clicked", self.calculate)

        self._last_focus: str | None = None
        self.target.connect("changed", self._on_target)
        self.target.set_active(0)

    def input_names(self) -> list[str]:
        return worksheets.input_fields(self.worksheet, self.target.get_active_text())

    def _on_target(self, *_):
        tgt = self.target.get_active_text()
        names = self.input_names()
        for f in self.entries:
            visible = f != tgt
            self.labels[f].set_visible(visible)
            self.entries[f].set_visible(visible)
            self.labels[f].set_no_show_all(not visible)
            self.entries[f].set_no_show_all(not visible)
        for row, f in enumerate(names):
            y = self.ROW_Y[row]
            self.move(self.labels[f], 18, y + 6)
            self.move(self.entries[f], 360, y)
        if self._last_focus not in names:
            self._last_focus = None
        self.result.set_text("")

    def _on_focus(self, _w, _ev, field: str):
        self._last_focus = field
        return False

    def set_value(self, text: str) -> None:
        names = self.input_names()
        field = self._last_focus if self._last_focus in names else names[0]
        self.entries[field].set_text(text)

    def calculate(self, *_):
        tgt = self.target.get_active_text()
        inputs = {f: self.entries[f].get_text() for f in self.input_names()}
        try:
            self.result.set_text(units.format_number(
                worksheets.solve(self.worksheet, tgt, inputs)))
        except ValueError as e:
            self.result.set_text(str(e))
            field = getattr(e, "field", None)
            if field:
                self.entries[field].grab_focus()
