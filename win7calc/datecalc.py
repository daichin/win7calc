"""Date calculation logic (Win7 calc "Date calculation" panel). No GTK."""
from __future__ import annotations

import calendar
from datetime import date, timedelta

MIN_YEAR, MAX_YEAR = 1601, 9999
OUT_OF_BOUND = "Date out of bound"


def _add_months(d: date, months: int) -> date:
    """Add (possibly negative) months, clamping the day to the target month's length."""
    total = d.year * 12 + (d.month - 1) + months
    y, m0 = divmod(total, 12)
    if not MIN_YEAR <= y <= MAX_YEAR:
        raise ValueError(OUT_OF_BOUND)
    return date(y, m0 + 1, min(d.day, calendar.monthrange(y, m0 + 1)[1]))


def date_difference(d1: date, d2: date) -> tuple[int, int, int, int, int]:
    """(years, months, weeks, days, total_days) between two dates, order-independent.
    Whole calendar months first (anchored at the earlier date), remainder as weeks+days."""
    a, b = sorted((d1, d2))
    months = (b.year - a.year) * 12 + (b.month - a.month)
    if _add_months(a, months) > b:
        months -= 1
    rem = (b - _add_months(a, months)).days
    years, months = divmod(months, 12)
    weeks, days = divmod(rem, 7)
    return years, months, weeks, days, (b - a).days


def _plural(n: int, word: str) -> str:
    return f"{n} {word}" + ("" if n == 1 else "s")


def format_difference(years: int, months: int, weeks: int, days: int) -> str:
    parts = [_plural(n, w) for n, w in
             ((years, "year"), (months, "month"), (weeks, "week"), (days, "day")) if n]
    return ", ".join(parts) if parts else "Same dates"


def format_total_days(total_days: int) -> str:
    return "Same dates" if total_days == 0 else _plural(total_days, "day")


def add_to_date(d: date, years: int = 0, months: int = 0, days: int = 0,
                subtract: bool = False) -> date:
    """Shift by years+months (day-of-month clamped), then by days.
    Raises ValueError('Date out of bound') outside years 1601..9999."""
    sign = -1 if subtract else 1
    r = _add_months(d, sign * (years * 12 + months))
    try:
        r = r + timedelta(days=sign * days)
    except OverflowError:
        raise ValueError(OUT_OF_BOUND) from None
    if not MIN_YEAR <= r.year <= MAX_YEAR:
        raise ValueError(OUT_OF_BOUND)
    return r
