"""Number <-> display-string conversion following win32calc's rules."""
from __future__ import annotations

from decimal import Decimal, ROUND_HALF_UP, localcontext

# Win7 switches to scientific notation once more than this many zeros
# would follow the decimal point (MAX_ZEROS_AFTER_DECIMAL in ratpak).
MAX_ZEROS_AFTER_DECIMAL = 2


def group_int(digits: str, size: int = 3, sep: str = ",") -> str:
    out = []
    while len(digits) > size:
        out.append(digits[-size:])
        digits = digits[:-size]
    out.append(digits)
    return sep.join(reversed(out))


def _sci(sign: str, digits: str, exp: int) -> str:
    mant = digits[0] + "." + digits[1:].rstrip("0")
    return f"{sign}{mant}e{'+' if exp >= 0 else '-'}{abs(exp)}"


def format_decimal(x: Decimal, precision: int, grouping: bool = False, fe: bool = False) -> str:
    """Format x with at most `precision` significant digits, like win32calc's display."""
    with localcontext() as c:
        c.prec = 100
        c.Emax, c.Emin = 999999, -999999
        return _format_decimal(x, precision, grouping, fe)


def _format_decimal(x: Decimal, precision: int, grouping: bool, fe: bool) -> str:
    if x == 0:
        return "0.e+0" if fe else "0"
    sign = "-" if x < 0 else ""
    x = abs(x)
    # Round to `precision` significant digits.
    q = Decimal(1).scaleb(x.adjusted() - precision + 1)
    r = x.quantize(q, rounding=ROUND_HALF_UP)
    if r.adjusted() != x.adjusted():  # rounding carried, e.g. 9.99.. -> 10.0
        q = Decimal(1).scaleb(r.adjusted() - precision + 1)
        r = r.quantize(q, rounding=ROUND_HALF_UP)
    exp = r.adjusted()
    digits = str(r.scaleb(-r.as_tuple().exponent).to_integral_value())  # mantissa digits
    digits = digits.ljust(precision, "0")[:precision]
    sig = digits.rstrip("0") or "0"

    if fe or exp >= precision:
        return _sci(sign, sig, exp)
    if exp < 0:
        # Number of fractional digits needed to show all significant digits.
        frac_len = -exp - 1 + len(sig)
        if frac_len > precision:
            if -exp - 1 > MAX_ZEROS_AFTER_DECIMAL:
                return _sci(sign, sig, exp)
            # Too long: re-round so that only `precision` fractional digits remain.
            r2 = x.quantize(Decimal(1).scaleb(-precision), rounding=ROUND_HALF_UP)
            if r2 == 0:
                return _sci(sign, sig, exp)
            return _format_decimal(-r2 if sign else r2, precision, grouping, False)
        frac = "0" * (-exp - 1) + sig
        return f"{sign}0.{frac}"
    int_digits = sig[: exp + 1].ljust(exp + 1, "0")
    frac = sig[exp + 1:]
    if grouping:
        int_digits = group_int(int_digits)
    return f"{sign}{int_digits}.{frac}" if frac else f"{sign}{int_digits}"


_RADIX_GROUP = {16: (4, " "), 10: (3, ","), 8: (3, " "), 2: (4, " ")}


def format_int(v: int, radix: int, bits: int, grouping: bool = False) -> str:
    """Programmer-mode display: signed in decimal, two's complement otherwise."""
    if radix == 10:
        s = str(abs(v))
        if grouping:
            s = group_int(s)
        return ("-" if v < 0 else "") + s
    u = v & ((1 << bits) - 1)
    s = {16: "%X", 8: "%o"}.get(radix, None)
    s = (s % u) if s else bin(u)[2:]
    if grouping:
        size, sep = _RADIX_GROUP[radix]
        s = group_int(s, size, sep)
    return s


def format_entry(digits: str, neg: bool, exp: str | None, exp_neg: bool, grouping: bool,
                 group_size: int = 3, group_sep: str = ",") -> str:
    """Format a number as it is being typed (keeps trailing '.' and zeros)."""
    if "." in digits:
        ip, fp = digits.split(".", 1)
    else:
        ip, fp = digits, None
    if grouping:
        ip = group_int(ip, group_size, group_sep)
    s = ("-" if neg else "") + ip
    if fp is not None:
        s += "." + fp
    if exp is not None:
        if fp is None:
            s += "."
        s += "e" + ("-" if exp_neg else "+") + (exp or "0")
    return s


def parse_number(text: str) -> Decimal | None:
    """Parse a pasted/display number ("1,234.5", "1.e+5"); None if invalid."""
    t = text.strip().replace(",", "").replace(" ", "")
    if not t:
        return None
    try:
        d = Decimal(t)
    except Exception:
        return None
    return d if d.is_finite() else None
