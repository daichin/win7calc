"""Worksheet math (Win7 calc "Worksheets" panels). No GTK.

Conventions:
- Mortgage: standard amortization, monthly compounding at rate%/12, n = term*12.
- Vehicle lease: ordinary annuity (payments in ARREARS, i.e. end of period),
  n = lease period (years) * payments per year, periodic rate = rate% / payments per year,
  PV = lease value, FV = residual value:
      PMT = (PV - FV*v^n) * i / (1 - v^n),  v = 1/(1+i)
- Fuel economy: plain ratios.
"""
from __future__ import annotations

from collections import OrderedDict
from decimal import Decimal, InvalidOperation, localcontext
from typing import Mapping, Union

BLANK = "This value cannot be blank."
INVALID = "This value is not valid."
UNABLE = "Unable to calculate"


class WorksheetError(ValueError):
    """ValueError carrying the offending field name (None for a general failure)."""

    def __init__(self, message: str, field: str | None = None):
        super().__init__(message)
        self.field = field


WORKSHEETS: "OrderedDict[str, dict[str, list[str]]]" = OrderedDict([
    ("Mortgage", {
        "fields": ["Purchase price", "Down payment", "Term (years)",
                   "Interest rate (%)", "Monthly payment"],
        "calculable": ["Down payment", "Monthly payment", "Purchase price", "Term (years)"],
    }),
    ("Vehicle lease", {
        "fields": ["Lease value", "Lease period", "Payments per year",
                   "Residual value", "Interest rate (%)", "Periodic payment"],
        "calculable": ["Lease value", "Periodic payment", "Residual value"],
    }),
    ("Fuel economy (mpg)", {
        "fields": ["Distance (miles)", "Fuel used (gallons)", "Fuel economy (mpg)"],
        "calculable": ["Distance (miles)", "Fuel used (gallons)", "Fuel economy (mpg)"],
    }),
    ("Fuel economy (L/100 km)", {
        "fields": ["Distance (kilometers)", "Fuel used (liters)", "Fuel economy (L/100 km)"],
        "calculable": ["Distance (kilometers)", "Fuel used (liters)", "Fuel economy (L/100 km)"],
    }),
])


def input_fields(worksheet: str, target: str) -> list[str]:
    """Fields the user must fill when calculating `target` (spec order)."""
    return [f for f in WORKSHEETS[worksheet]["fields"] if f != target]


Value = Union[Decimal, str, int, float, None]


def _parse(name: str, raw: Value) -> Decimal:
    if raw is None or (isinstance(raw, str) and not raw.strip()):
        raise WorksheetError(BLANK, name)
    try:
        d = Decimal(raw.strip().replace(",", "")) if isinstance(raw, str) else Decimal(raw)
    except (InvalidOperation, ValueError):
        raise WorksheetError(INVALID, name) from None
    if not d.is_finite() or d < 0:
        raise WorksheetError(INVALID, name)
    return d


def solve(worksheet: str, target: str, inputs: Mapping[str, Value]) -> Decimal:
    """Compute `target` from the other fields of `worksheet`.
    Raises WorksheetError (a ValueError) with BLANK / INVALID / UNABLE messages."""
    spec = WORKSHEETS[worksheet]
    if target not in spec["calculable"]:
        raise ValueError(f"{target!r} is not calculable in {worksheet!r}")
    v = {f: _parse(f, inputs.get(f)) for f in input_fields(worksheet, target)}
    with localcontext() as ctx:
        ctx.prec = 34
        try:
            r = _SOLVERS[worksheet](target, v)
        except (ZeroDivisionError, InvalidOperation, OverflowError):
            raise WorksheetError(UNABLE) from None
        if not r.is_finite() or r < 0:
            raise WorksheetError(UNABLE)
        return +r


def _annuity_factor(i: Decimal, n: Decimal) -> Decimal:
    """(1 - (1+i)^-n) / i ; equals n when i == 0."""
    if i == 0:
        return n
    return (1 - (1 + i) ** -n) / i


def _mortgage(target: str, v: dict[str, Decimal]) -> Decimal:
    i = v["Interest rate (%)"] / 1200
    if target == "Term (years)":
        principal = v["Purchase price"] - v["Down payment"]
        pmt = v["Monthly payment"]
        if principal < 0 or pmt <= 0:
            raise WorksheetError(UNABLE)
        if i == 0:
            return principal / pmt / 12
        x = 1 - principal * i / pmt
        if x <= 0:
            raise WorksheetError(UNABLE)  # payment never covers the interest
        return -x.ln() / (1 + i).ln() / 12
    n = v["Term (years)"] * 12
    if n <= 0:
        raise WorksheetError(UNABLE)
    a = _annuity_factor(i, n)
    if target == "Monthly payment":
        principal = v["Purchase price"] - v["Down payment"]
        if principal < 0:
            raise WorksheetError(UNABLE)
        return principal / a
    principal = v["Monthly payment"] * a
    if target == "Down payment":
        return v["Purchase price"] - principal
    return principal + v["Down payment"]  # Purchase price


def _lease(target: str, v: dict[str, Decimal]) -> Decimal:
    ppy = v["Payments per year"]
    n = v["Lease period"] * ppy
    if ppy <= 0 or n <= 0:
        raise WorksheetError(UNABLE)
    i = v["Interest rate (%)"] / 100 / ppy
    vn = (1 + i) ** -n  # discount factor over the whole lease
    a = _annuity_factor(i, n)
    if target == "Periodic payment":
        return (v["Lease value"] - v["Residual value"] * vn) / a
    if target == "Lease value":
        return v["Periodic payment"] * a + v["Residual value"] * vn
    return (v["Lease value"] - v["Periodic payment"] * a) / vn  # Residual value


def _ratio(num: str, den: str, out: str, k: Decimal):
    # out = num / den * k
    def f(target: str, v: dict[str, Decimal]) -> Decimal:
        if target == out:
            return v[num] / v[den] * k
        if target == num:
            return v[out] * v[den] / k
        return v[num] * k / v[out]
    return f


_SOLVERS = {
    "Mortgage": _mortgage,
    "Vehicle lease": _lease,
    "Fuel economy (mpg)": _ratio("Distance (miles)", "Fuel used (gallons)",
                                 "Fuel economy (mpg)", Decimal(1)),
    "Fuel economy (L/100 km)": _ratio("Fuel used (liters)", "Distance (kilometers)",
                                      "Fuel economy (L/100 km)", Decimal(100)),
}
