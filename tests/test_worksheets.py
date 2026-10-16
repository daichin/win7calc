import unittest
from decimal import Decimal as D

from win7calc import worksheets as ws


def solve(w, t, **kw):
    names = {k: v for k, v in kw.items()}
    return ws.solve(w, t, names)


class MortgageTest(unittest.TestCase):
    base = {"Purchase price": D(200000), "Down payment": D(40000),
            "Term (years)": D(30), "Interest rate (%)": D(6)}

    def test_payment(self):
        p = ws.solve("Mortgage", "Monthly payment", self.base)
        self.assertAlmostEqual(float(p), 959.2808, places=3)

    def test_inverse(self):
        pmt = ws.solve("Mortgage", "Monthly payment", self.base)
        full = dict(self.base, **{"Monthly payment": pmt})
        for tgt in ws.WORKSHEETS["Mortgage"]["calculable"]:
            inp = {k: v for k, v in full.items() if k != tgt}
            self.assertAlmostEqual(float(ws.solve("Mortgage", tgt, inp)),
                                   float(full[tgt]), places=6, msg=tgt)

    def test_zero_rate(self):
        inp = dict(self.base, **{"Interest rate (%)": D(0)})
        self.assertAlmostEqual(float(ws.solve("Mortgage", "Monthly payment", inp)),
                               160000 / 360, places=9)

    def test_errors(self):
        inp = dict(self.base)
        inp["Down payment"] = ""
        with self.assertRaises(ValueError) as cm:
            ws.solve("Mortgage", "Monthly payment", inp)
        self.assertEqual(str(cm.exception), "This value cannot be blank.")
        self.assertEqual(cm.exception.field, "Down payment")
        inp["Down payment"] = "abc"
        with self.assertRaisesRegex(ValueError, "This value is not valid."):
            ws.solve("Mortgage", "Monthly payment", inp)
        # payment smaller than interest -> term impossible
        inp = {"Purchase price": D(200000), "Down payment": D(0),
               "Interest rate (%)": D(6), "Monthly payment": D(500)}
        with self.assertRaisesRegex(ValueError, "Unable to calculate"):
            ws.solve("Mortgage", "Term (years)", inp)


class LeaseTest(unittest.TestCase):
    base = {"Lease value": D(30000), "Lease period": D(3), "Payments per year": D(12),
            "Residual value": D(15000), "Interest rate (%)": D(5)}

    def test_roundtrip(self):
        pmt = ws.solve("Vehicle lease", "Periodic payment", self.base)
        # arrears: (30000 - 15000*v^36) * i / (1 - v^36), i = 5%/12
        i = 0.05 / 12
        v = (1 + i) ** -36
        self.assertAlmostEqual(float(pmt), (30000 - 15000 * v) * i / (1 - v), places=6)
        full = dict(self.base, **{"Periodic payment": pmt})
        for tgt in ws.WORKSHEETS["Vehicle lease"]["calculable"]:
            inp = {k: x for k, x in full.items() if k != tgt}
            self.assertAlmostEqual(float(ws.solve("Vehicle lease", tgt, inp)),
                                   float(full[tgt]), places=6, msg=tgt)

    def test_zero_ppy(self):
        with self.assertRaisesRegex(ValueError, "Unable to calculate"):
            ws.solve("Vehicle lease", "Periodic payment",
                     dict(self.base, **{"Payments per year": D(0)}))


class FuelTest(unittest.TestCase):
    def test_mpg(self):
        w = "Fuel economy (mpg)"
        self.assertEqual(ws.solve(w, "Fuel economy (mpg)",
                                  {"Distance (miles)": D(300), "Fuel used (gallons)": D(12)}),
                         D(25))
        self.assertEqual(ws.solve(w, "Distance (miles)",
                                  {"Fuel economy (mpg)": D(25), "Fuel used (gallons)": D(12)}),
                         D(300))
        self.assertEqual(ws.solve(w, "Fuel used (gallons)",
                                  {"Distance (miles)": D(300), "Fuel economy (mpg)": D(25)}),
                         D(12))
        with self.assertRaisesRegex(ValueError, "Unable to calculate"):
            ws.solve(w, "Fuel economy (mpg)",
                     {"Distance (miles)": D(300), "Fuel used (gallons)": D(0)})

    def test_l100(self):
        w = "Fuel economy (L/100 km)"
        self.assertEqual(ws.solve(w, "Fuel economy (L/100 km)",
                                  {"Distance (kilometers)": D(500), "Fuel used (liters)": D(40)}),
                         D(8))
        self.assertEqual(ws.solve(w, "Distance (kilometers)",
                                  {"Fuel economy (L/100 km)": D(8), "Fuel used (liters)": D(40)}),
                         D(500))
        self.assertEqual(ws.solve(w, "Fuel used (liters)",
                                  {"Distance (kilometers)": D(500),
                                   "Fuel economy (L/100 km)": D(8)}), D(40))

    def test_specs(self):
        self.assertEqual(list(ws.WORKSHEETS), ["Mortgage", "Vehicle lease",
                                               "Fuel economy (mpg)", "Fuel economy (L/100 km)"])


if __name__ == "__main__":
    unittest.main()
