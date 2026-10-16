"""High-precision math on decimal.Decimal, mirroring win32calc's error semantics."""
from __future__ import annotations

import math
from decimal import Context, Decimal, localcontext, InvalidOperation, Overflow, DivisionByZero

WORK_PREC = 50
# Results at or beyond this magnitude are reported as "Overflow" (matches Win7's ~1e10000 limit).
MAX_EXP = 9999

ERR_DIV0 = "Cannot divide by zero"
ERR_INVALID = "Invalid input"
ERR_UNDEFINED = "Result is undefined"
ERR_OVERFLOW = "Overflow"

D0, D1, D2 = Decimal(0), Decimal(1), Decimal(2)
PI = Decimal("3.14159265358979323846264338327950288419716939937510582097494459230781640628620899")


class CalcError(Exception):
    """A calculation error; str() is the message shown in the display."""


def ctx(prec: int = WORK_PREC):
    return localcontext(Context(prec=prec, Emax=999999, Emin=-999999,
                                traps=[InvalidOperation, Overflow, DivisionByZero]))


def check(x: Decimal) -> Decimal:
    """Validate a result: raise Overflow beyond the display range, flush tiny values to zero."""
    if x.is_nan():
        raise CalcError(ERR_INVALID)
    if x.is_infinite():
        raise CalcError(ERR_OVERFLOW)
    if x and x.adjusted() >= MAX_EXP + 1:
        raise CalcError(ERR_OVERFLOW)
    if x and x.adjusted() < -MAX_EXP:
        return D0
    with ctx():
        return +x


def is_int(x: Decimal) -> bool:
    return x == x.to_integral_value()


# ---------------------------------------------------------------- arithmetic

def add(a, b):
    with ctx():
        return check(a + b)


def sub(a, b):
    with ctx():
        return check(a - b)


def mul(a, b):
    with ctx():
        return check(a * b)


def div(a, b):
    if b == 0:
        raise CalcError(ERR_UNDEFINED if a == 0 else ERR_DIV0)
    with ctx():
        return check(a / b)


def mod(a, b):
    """Win7 Mod: remainder with the sign of the dividend."""
    if b == 0:
        raise CalcError(ERR_UNDEFINED if a == 0 else ERR_DIV0)
    with ctx():
        return check(a % b)  # Decimal % keeps the dividend's sign


def power(x: Decimal, y: Decimal) -> Decimal:
    if x == 0:
        if y < 0:
            raise CalcError(ERR_DIV0)
        if y == 0:
            raise CalcError(ERR_UNDEFINED)
        return D0
    try:
        with ctx():
            if is_int(y):
                n = int(y)
                if abs(n) <= 100000:
                    return check(x ** n)
            if x < 0:
                # Negative base: allowed only when y is an integer or 1/odd.
                if is_int(y):
                    r = abs(x) ** y
                    return check(-r if int(y) % 2 else r)
                inv = D1 / y
                if is_int(inv) and int(inv) % 2:
                    return check(-(abs(x) ** y))
                raise CalcError(ERR_INVALID)
            return check(x ** y)
    except (Overflow,):
        raise CalcError(ERR_OVERFLOW)
    except InvalidOperation:
        raise CalcError(ERR_INVALID)


def root(x: Decimal, y: Decimal) -> Decimal:
    """y-th root of x (x yroot y)."""
    if y == 0:
        raise CalcError(ERR_INVALID)
    if x < 0:
        if is_int(y) and int(y) % 2:
            return -root(-x, y)
        raise CalcError(ERR_INVALID)
    if x == 0:
        if y < 0:
            raise CalcError(ERR_DIV0)
        return D0
    with ctx():
        r = check(x ** (D1 / y))
        # Snap exact integer roots (e.g. 27 yroot 3 = 3).
        if is_int(y):
            n = r.to_integral_value()
            if n and n ** int(y) == x:
                return n + D0
        return r


# ---------------------------------------------------------------- unary

def sqrt(x):
    if x < 0:
        raise CalcError(ERR_INVALID)
    with ctx():
        return check(x.sqrt())


def cbrt(x):
    if x == 0:
        return D0
    with ctx():
        r = root(abs(x), Decimal(3))
        return -r if x < 0 else r


def reciprocal(x):
    if x == 0:
        raise CalcError(ERR_DIV0)
    with ctx():
        return check(D1 / x)


def square(x):
    return mul(x, x)


def cube(x):
    with ctx():
        return check(x * x * x)


def ln(x):
    if x <= 0:
        raise CalcError(ERR_INVALID)
    with ctx():
        return check(x.ln())


def log10(x):
    if x <= 0:
        raise CalcError(ERR_INVALID)
    with ctx():
        return check(x.log10())


def exp(x):
    if x > 23027:  # e^23027 > 1e10000
        raise CalcError(ERR_OVERFLOW)
    with ctx():
        return check(x.exp())


def pow10(x):
    return power(Decimal(10), x)


def factorial(x: Decimal) -> Decimal:
    if is_int(x):
        n = int(x)
        if n < 0:
            raise CalcError(ERR_INVALID)
        if n > 3249:
            raise CalcError(ERR_OVERFLOW)
        with ctx():
            return check(Decimal(math.factorial(n)))
    # Non-integers use the gamma function (double precision is good enough here).
    try:
        g = math.gamma(float(x) + 1)
    except (ValueError, OverflowError):
        raise CalcError(ERR_INVALID if x < 0 else ERR_OVERFLOW)
    if math.isinf(g) or math.isnan(g):
        raise CalcError(ERR_OVERFLOW)
    return check(Decimal(repr(g)))


def int_part(x):
    return x.to_integral_value(rounding="ROUND_DOWN") + D0 if x else D0


def frac_part(x):
    with ctx():
        return check(x - int_part(x))


def dms(x: Decimal) -> Decimal:
    """Decimal degrees -> d.mmss format."""
    with ctx():
        neg = x < 0
        x = abs(x)
        d = int_part(x)
        rest = (x - d) * 60
        m = int_part(rest)
        s = (rest - m) * 60
        r = d + m / 100 + s / 10000
        return check(-r if neg else r)


def degrees(x: Decimal) -> Decimal:
    """d.mmss format -> decimal degrees (Inv dms)."""
    with ctx():
        neg = x < 0
        x = abs(x)
        d = int_part(x)
        rest = (x - d) * 100
        m = int_part(rest)
        s = (rest - m) * 100
        r = d + m / 60 + s / 3600
        return check(-r if neg else r)


# ---------------------------------------------------------------- trigonometry

def _angle_period(unit: str) -> Decimal:
    return {"deg": Decimal(360), "grad": Decimal(400)}.get(unit, D2 * PI)


def to_radians(x: Decimal, unit: str) -> Decimal:
    with ctx(WORK_PREC + 10):
        if unit == "deg":
            return x * PI / 180
        if unit == "grad":
            return x * PI / 200
        return +x


def from_radians(x: Decimal, unit: str) -> Decimal:
    with ctx(WORK_PREC + 10):
        if unit == "deg":
            return x * 180 / PI
        if unit == "grad":
            return x * 200 / PI
        return +x


def _quadrant(x: Decimal, unit: str):
    """For deg/grad, return k if x is an exact multiple of a right angle, else None."""
    if unit == "rad":
        return None
    q = Decimal(90) if unit == "deg" else Decimal(100)
    with ctx(WORK_PREC + 10):
        r = x / q
    if is_int(r):
        return int(r) % 4
    return None


def _sin_cos_rad(x: Decimal):
    with ctx(WORK_PREC + 15):
        twopi = 2 * PI
        x = x % twopi
        # Taylor series for sin and cos together.
        x2 = x * x
        s = term = x
        n = 1
        while True:
            term = -term * x2 / ((n + 1) * (n + 2))
            n += 2
            ns = s + term
            if ns == s:
                break
            s = ns
        c = term = D1
        n = 0
        while True:
            term = -term * x2 / ((n + 1) * (n + 2))
            n += 2
            nc = c + term
            if nc == c:
                break
            c = nc
    return s, c


def _snap(v: Decimal) -> Decimal:
    with ctx():
        return D0 if abs(v) < Decimal("1e-45") else +v


_SIN_Q = [0, 1, 0, -1]
_COS_Q = [1, 0, -1, 0]


def sin(x, unit):
    q = _quadrant(x, unit)
    if q is not None:
        return Decimal(_SIN_Q[q])
    s, _ = _sin_cos_rad(to_radians(x, unit))
    return check(_snap(s))


def cos(x, unit):
    q = _quadrant(x, unit)
    if q is not None:
        return Decimal(_COS_Q[q])
    _, c = _sin_cos_rad(to_radians(x, unit))
    return check(_snap(c))


def tan(x, unit):
    q = _quadrant(x, unit)
    if q is not None:
        if q % 2:
            raise CalcError(ERR_INVALID)
        return D0
    s, c = _sin_cos_rad(to_radians(x, unit))
    if abs(c) < Decimal("1e-45"):
        raise CalcError(ERR_INVALID)
    with ctx():
        return check(_snap(s / c))


def _atan_rad(x: Decimal) -> Decimal:
    with ctx(WORK_PREC + 15):
        if x < 0:
            return -_atan_rad(-x)
        if x > 1:
            return PI / 2 - _atan_rad(D1 / x)
        # Halve the argument twice for fast convergence: atan(x) = 2 atan(x / (1 + sqrt(1 + x^2)))
        k = 0
        while x > Decimal("0.1"):
            x = x / (1 + (1 + x * x).sqrt())
            k += 1
        x2 = x * x
        s = term = x
        n = 1
        while True:
            term = -term * x2
            n += 2
            ns = s + term / n
            if ns == s:
                break
            s = ns
        return s * (2 ** k)


def atan(x, unit):
    return check(_clean(from_radians(_atan_rad(x), unit)))


def asin(x, unit):
    if abs(x) > 1:
        raise CalcError(ERR_INVALID)
    with ctx(WORK_PREC + 15):
        if abs(x) == 1:
            r = PI / 2 * (1 if x > 0 else -1)
        else:
            r = _atan_rad(x / (1 - x * x).sqrt())
    return check(_clean(from_radians(r, unit)))


def acos(x, unit):
    if abs(x) > 1:
        raise CalcError(ERR_INVALID)
    with ctx(WORK_PREC + 15):
        if x == 0:
            r = PI / 2
        else:
            r = _atan_rad((1 - x * x).sqrt() / x)
            if x < 0:
                r += PI
    return check(_clean(from_radians(r, unit)))


def _clean(v: Decimal) -> Decimal:
    """Round to working precision and snap values that are integers to 40+ digits."""
    with ctx(WORK_PREC - 5):
        r = +v
    n = r.to_integral_value()
    with ctx():
        if abs(r - n) < Decimal("1e-40"):
            return n + D0
        return +r


def sinh(x):
    e = exp(x)
    with ctx():
        return check((e - D1 / e) / 2)


def cosh(x):
    e = exp(abs(x))
    with ctx():
        return check((e + D1 / e) / 2)


def tanh(x):
    if abs(x) > 200:
        return Decimal(1 if x > 0 else -1)
    e2 = exp(2 * x)
    with ctx():
        return check((e2 - 1) / (e2 + 1))


def asinh(x):
    with ctx(WORK_PREC + 10):
        ax = abs(x)
        r = (ax + (ax * ax + 1).sqrt()).ln()
        return check(-r if x < 0 else r)


def acosh(x):
    if x < 1:
        raise CalcError(ERR_INVALID)
    with ctx(WORK_PREC + 10):
        return check((x + (x * x - 1).sqrt()).ln())


def atanh(x):
    if abs(x) >= 1:
        raise CalcError(ERR_INVALID)
    with ctx(WORK_PREC + 10):
        return check(((1 + x) / (1 - x)).ln() / 2)
