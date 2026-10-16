"""Unit conversion data and math (Win7 calc "Unit conversion" panel). No GTK."""
from __future__ import annotations

from decimal import Decimal, localcontext
from fractions import Fraction

_PI = Fraction(Decimal("3.14159265358979323846264338327950288419716939937510582097494"))


def _f(s: str) -> Fraction:
    return Fraction(Decimal(s))


_BTU = _f("1055.05585262")
_FTLB = _f("1.3558179483314004")  # 0.3048 m * 4.4482216152605 N

# category -> ordered {unit: factor to base unit}; Temperature handled separately.
_TABLE: dict[str, dict[str, Fraction]] = {
    "Angle": {  # base: degree
        "Degree": Fraction(1),
        "Gradian": Fraction(9, 10),
        "Radian": 180 / _PI,
    },
    "Area": {  # base: m^2
        "Acres": _f("4046.8564224"),
        "Hectares": Fraction(10000),
        "Square centimeter": _f("0.0001"),
        "Square feet": _f("0.09290304"),
        "Square inch": _f("0.00064516"),
        "Square kilometer": Fraction(10**6),
        "Square meters": Fraction(1),
        "Square mile": _f("2589988.110336"),
        "Square millimeter": _f("0.000001"),
        "Square Yard": _f("0.83612736"),
    },
    "Energy": {  # base: joule
        "British Thermal Unit": _BTU,
        "Calorie": _f("4.1868"),
        "Electron-Volts": _f("1.602176634e-19"),
        "Foot-Pound": _FTLB,
        "Joule": Fraction(1),
        "Kilocalorie": _f("4186.8"),
        "Kilojoule": Fraction(1000),
    },
    "Length": {  # base: meter
        "Angstrom": _f("1e-10"),
        "Centimeters": _f("0.01"),
        "Chain": _f("20.1168"),
        "Fathom": _f("1.8288"),
        "Feet": _f("0.3048"),
        "Hand": _f("0.1016"),
        "Inch": _f("0.0254"),
        "Kilometers": Fraction(1000),
        "Link": _f("0.201168"),
        "Meter": Fraction(1),
        "Microns": _f("1e-6"),
        "Mile": _f("1609.344"),
        "Millimeters": _f("0.001"),
        "Nanometer": _f("1e-9"),
        "Nautical Miles": Fraction(1852),
        "PICA": _f("0.0254") / 6,
        "Rods": _f("5.0292"),
        "Span": _f("0.2286"),
        "Yard": _f("0.9144"),
    },
    "Power": {  # base: watt
        "BTU/minute": _BTU / 60,
        "Foot-Pound/minute": _FTLB / 60,
        "Horsepower": _f("745.69987158227022"),
        "Kilowatt": Fraction(1000),
        "Watt": Fraction(1),
    },
    "Pressure": {  # base: pascal
        "Atmosphere": Fraction(101325),
        "Bar": Fraction(100000),
        "Kilo Pascal": Fraction(1000),
        "Millimeter of mercury": _f("133.322387415"),
        "Pascal": Fraction(1),
        "Pound per square inch (PSI)": _f("6894.757293168"),
    },
    "Temperature": {
        "Degrees Celsius": Fraction(1),
        "Degrees Fahrenheit": Fraction(1),
        "Kelvin": Fraction(1),
    },
    "Time": {  # base: second
        "Day": Fraction(86400),
        "Hour": Fraction(3600),
        "Microsecond": _f("1e-6"),
        "Millisecond": _f("0.001"),
        "Minute": Fraction(60),
        "Second": Fraction(1),
        "Week": Fraction(604800),
    },
    "Velocity": {  # base: m/s
        "Centimeter per second": _f("0.01"),
        "Feet per second": _f("0.3048"),
        "Kilometer per hour": Fraction(1000, 3600),
        "Knots": Fraction(1852, 3600),
        "Mach (at std. atm)": _f("340.3"),
        "Meter per second": Fraction(1),
        "Miles per hour": _f("0.44704"),
    },
    "Volume": {  # base: m^3
        "Cubic centimeter": _f("1e-6"),
        "Cubic feet": _f("0.028316846592"),
        "Cubic inch": _f("0.000016387064"),
        "Cubic meter": Fraction(1),
        "Cubic yard": _f("0.764554857984"),
        "Fluid ounce (UK)": _f("0.0000284130625"),
        "Fluid ounce (US)": _f("0.0000295735295625"),
        "Gallon (UK)": _f("0.00454609"),
        "Gallon (US)": _f("0.003785411784"),
        "Liter": _f("0.001"),
        "Pint (UK)": _f("0.00056826125"),
        "Pint (US)": _f("0.000473176473"),
        "Quart (UK)": _f("0.0011365225"),
        "Quart (US)": _f("0.000946352946"),
    },
    "Weight/Mass": {  # base: kg
        "Carat": _f("0.0002"),
        "Centigram": _f("0.00001"),
        "Decigram": _f("0.0001"),
        "Dekagram": _f("0.01"),
        "Gram": _f("0.001"),
        "Hectogram": _f("0.1"),
        "Kilogram": Fraction(1),
        "Long ton": _f("1016.0469088"),
        "Milligram": _f("0.000001"),
        "Ounce": _f("0.028349523125"),
        "Pound": _f("0.45359237"),
        "Short ton": _f("907.18474"),
        "Stone": _f("6.35029318"),
        "Tonne": Fraction(1000),
    },
}

CATEGORIES: list[str] = list(_TABLE)

_DEFAULTS: dict[str, tuple[str, str]] = {
    "Angle": ("Degree", "Radian"),
    "Area": ("Square meters", "Square feet"),
    "Energy": ("Joule", "Calorie"),
    "Length": ("Meter", "Feet"),
    "Power": ("Kilowatt", "Horsepower"),
    "Pressure": ("Kilo Pascal", "Pound per square inch (PSI)"),
    "Temperature": ("Degrees Celsius", "Degrees Fahrenheit"),
    "Time": ("Hour", "Minute"),
    "Velocity": ("Kilometer per hour", "Miles per hour"),
    "Volume": ("Liter", "Gallon (US)"),
    "Weight/Mass": ("Kilogram", "Pound"),
}

PRECISION = 32  # significant digits of convert() results


def units(category: str) -> list[str]:
    return list(_TABLE[category])


def default_units(category: str) -> tuple[str, str]:
    return _DEFAULTS[category]


def _to_kelvin(v: Fraction, unit: str) -> Fraction:
    if unit == "Degrees Celsius":
        return v + _f("273.15")
    if unit == "Degrees Fahrenheit":
        return (v - 32) * Fraction(5, 9) + _f("273.15")
    return v


def _from_kelvin(k: Fraction, unit: str) -> Fraction:
    if unit == "Degrees Celsius":
        return k - _f("273.15")
    if unit == "Degrees Fahrenheit":
        return (k - _f("273.15")) * Fraction(9, 5) + 32
    return k


def convert(value: Decimal, category: str, from_unit: str, to_unit: str) -> Decimal:
    """Convert exactly (rational arithmetic), rounded to PRECISION significant digits."""
    table = _TABLE[category]
    if from_unit not in table or to_unit not in table:
        raise KeyError(f"unknown unit for {category}: {from_unit!r} / {to_unit!r}")
    v = Fraction(value)
    if category == "Temperature":
        r = _from_kelvin(_to_kelvin(v, from_unit), to_unit)
    else:
        r = v * table[from_unit] / table[to_unit]
    with localcontext() as ctx:
        ctx.prec = PRECISION
        out = Decimal(r.numerator) / Decimal(r.denominator)
    return out if out else Decimal(0)


def format_number(d: Decimal, sig: int = 16) -> str:
    """Display a Decimal Win7-style: <= `sig` significant digits, trailing zeros stripped,
    plain notation for ordinary magnitudes, 'd.ddde+N' otherwise."""
    if not d.is_finite():
        raise ValueError("non-finite")
    if d.is_zero():
        return "0"
    with localcontext() as ctx:
        ctx.prec = sig
        d = +d  # round to `sig` digits
    adj = d.adjusted()
    if -10 <= adj < sig:
        s = format(d, "f")
        if "." in s:
            s = s.rstrip("0").rstrip(".")
        return s
    sign, digits, _ = d.as_tuple()
    ds = "".join(map(str, digits)).rstrip("0") or "0"
    mant = ds[0] + ("." + ds[1:] if len(ds) > 1 else "")
    return f"{'-' if sign else ''}{mant}e{'+' if adj >= 0 else '-'}{abs(adj)}"
