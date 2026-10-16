import unittest
from datetime import date

from win7calc import datecalc as dc


class DateCalcTest(unittest.TestCase):
    def test_difference(self):
        r = dc.date_difference(date(2024, 1, 1), date(2025, 3, 15))
        self.assertEqual(r, (1, 2, 2, 0, 439))
        self.assertEqual(dc.date_difference(date(2025, 3, 15), date(2024, 1, 1)), r)
        self.assertEqual(dc.format_difference(*r[:4]), "1 year, 2 months, 2 weeks")
        self.assertEqual(dc.format_total_days(r[4]), "439 days")

    def test_difference_month_end(self):
        # consistent with add_to_date clamping: Jan 31 + 1 month == Feb 29
        self.assertEqual(dc.date_difference(date(2024, 1, 31), date(2024, 2, 29)),
                         (0, 1, 0, 0, 29))
        self.assertEqual(dc.date_difference(date(2024, 1, 30), date(2024, 2, 28)),
                         (0, 0, 4, 1, 29))
        self.assertEqual(dc.date_difference(date(2024, 1, 31), date(2024, 3, 31)),
                         (0, 2, 0, 0, 60))

    def test_format(self):
        self.assertEqual(dc.format_difference(1, 2, 3, 4), "1 year, 2 months, 3 weeks, 4 days")
        self.assertEqual(dc.format_difference(2, 0, 1, 0), "2 years, 1 week")
        self.assertEqual(dc.format_difference(0, 0, 0, 1), "1 day")
        self.assertEqual(dc.format_difference(0, 0, 0, 0), "Same dates")
        self.assertEqual(dc.format_total_days(1), "1 day")
        same = dc.date_difference(date(2026, 10, 16), date(2026, 10, 16))
        self.assertEqual(same, (0, 0, 0, 0, 0))

    def test_add(self):
        self.assertEqual(dc.add_to_date(date(2023, 1, 31), months=1), date(2023, 2, 28))
        self.assertEqual(dc.add_to_date(date(2024, 1, 31), months=1), date(2024, 2, 29))
        self.assertEqual(dc.add_to_date(date(2024, 2, 29), years=1), date(2025, 2, 28))
        self.assertEqual(dc.add_to_date(date(2024, 3, 31), months=1, subtract=True),
                         date(2024, 2, 29))
        self.assertEqual(dc.add_to_date(date(2026, 10, 16), 1, 2, 3), date(2027, 12, 19))
        self.assertEqual(dc.add_to_date(date(2026, 10, 16), days=16, subtract=True),
                         date(2026, 9, 30))

    def test_bounds(self):
        for args in [dict(years=9999), dict(years=500, subtract=True),
                     dict(days=99999, years=9000)]:
            with self.assertRaisesRegex(ValueError, "Date out of bound"):
                dc.add_to_date(date(2026, 1, 1), **args)
        with self.assertRaisesRegex(ValueError, "Date out of bound"):
            dc.add_to_date(date(1601, 1, 5), days=10, subtract=True)


if __name__ == "__main__":
    unittest.main()
