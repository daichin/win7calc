import unittest
from decimal import Decimal as D

from win7calc import units


class UnitsTest(unittest.TestCase):
    def test_categories_order(self):
        self.assertEqual(units.CATEGORIES, [
            "Angle", "Area", "Energy", "Length", "Power", "Pressure", "Temperature",
            "Time", "Velocity", "Volume", "Weight/Mass"])
        self.assertEqual(len(units.units("Length")), 19)
        self.assertEqual(units.units("Angle"), ["Degree", "Gradian", "Radian"])

    def test_defaults_valid(self):
        for c in units.CATEGORIES:
            f, t = units.default_units(c)
            self.assertIn(f, units.units(c))
            self.assertIn(t, units.units(c))

    def test_length(self):
        self.assertEqual(units.convert(D(1), "Length", "Mile", "Kilometers"), D("1.609344"))
        self.assertEqual(units.convert(D(1), "Length", "Feet", "Inch"), D(12))
        self.assertEqual(units.convert(D(6), "Length", "PICA", "Inch"), D(1))

    def test_temperature(self):
        self.assertEqual(units.convert(D(100), "Temperature", "Degrees Celsius",
                                       "Degrees Fahrenheit"), D(212))
        self.assertEqual(units.convert(D(-40), "Temperature", "Degrees Fahrenheit",
                                       "Degrees Celsius"), D(-40))
        self.assertEqual(units.convert(D(0), "Temperature", "Kelvin",
                                       "Degrees Celsius"), D("-273.15"))

    def test_misc(self):
        self.assertEqual(units.convert(D("3.6"), "Velocity", "Kilometer per hour",
                                       "Meter per second"), D(1))
        self.assertEqual(units.convert(D(1), "Weight/Mass", "Pound", "Ounce"), D(16))
        self.assertEqual(units.convert(D(1), "Pressure", "Atmosphere", "Kilo Pascal"),
                         D("101.325"))
        self.assertEqual(units.convert(D(1), "Time", "Week", "Hour"), D(168))
        self.assertEqual(units.format_number(
            units.convert(D(180), "Angle", "Degree", "Radian")), "3.141592653589793")
        self.assertEqual(units.convert(D(1), "Volume", "Gallon (US)", "Quart (US)"), D(4))

    def test_roundtrip(self):
        for c in units.CATEGORIES:
            names = units.units(c)
            for a in names:
                for b in names:
                    r = units.convert(units.convert(D("123.456"), c, a, b), c, b, a)
                    self.assertAlmostEqual(float(r), 123.456, places=9, msg=(c, a, b))

    def test_format(self):
        f = units.format_number
        self.assertEqual(f(D("1.50000")), "1.5")
        self.assertEqual(f(D("100")), "100")
        self.assertEqual(f(D("1E+2")), "100")
        self.assertEqual(f(D("0.1234567890123456789")), "0.1234567890123457")
        self.assertEqual(f(D("1.5E+20")), "1.5e+20")
        self.assertEqual(f(D("-2E-15")), "-2e-15")
        self.assertEqual(f(D(0)), "0")


if __name__ == "__main__":
    unittest.main()
