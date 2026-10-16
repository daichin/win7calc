import unittest

from win7calc import engine as E
from win7calc.engine import Engine

KEYS = {
    "+": E.ADD, "-": E.SUB, "*": E.MUL, "/": E.DIV, "=": E.EQU, ".": E.POINT, "(": E.OPENP, ")": E.CLOSEP,
    "%": E.PERCENT, "~": E.SIGN, "<": E.BACK, "C": E.CLEAR, "E": E.CE, "^": E.PWR, "r": E.SQRT,
}


def run(keys, mode=E.STD, **attrs):
    c = Engine()
    c.set_mode(mode)
    for k, v in attrs.items():
        setattr(c, k, v)
    for k in keys.split():
        if k.isdigit():
            for d in k:
                c.command(E.DIGIT0 + int(d))
        elif k in KEYS:
            c.command(KEYS[k])
        else:
            c.command(getattr(E, k))
    return c


class StandardMode(unittest.TestCase):
    def test_no_precedence(self):
        self.assertEqual(run("2 + 3 * 4 =").display, "20")

    def test_intermediate_result_shown(self):
        c = run("2 + 3 +")
        self.assertEqual(c.display, "5")
        self.assertEqual(c.expression, "2 + 3 +")

    def test_repeat_equals(self):
        self.assertEqual(run("5 + 3 = = =").display, "14")

    def test_new_number_repeats_last_op(self):
        self.assertEqual(run("2 + 3 = 4 =").display, "7")

    def test_operand_defaults_to_display(self):
        self.assertEqual(run("5 + =").display, "10")

    def test_operator_replacement(self):
        self.assertEqual(run("8 - * 2 =").display, "16")

    def test_division_precision(self):
        self.assertEqual(run("1 / 3 =").display, "0.3333333333333333")
        self.assertEqual(run("2 / 3 =").display, "0.6666666666666667")

    def test_divide_by_zero(self):
        self.assertEqual(run("5 / 0 =").display, "Cannot divide by zero")
        self.assertEqual(run("0 / 0 =").display, "Result is undefined")
        self.assertEqual(run("5 / 0 = 7").display, "7")

    def test_sqrt_expression(self):
        c = run("9 + 16 r")
        self.assertEqual(c.display, "4")
        self.assertEqual(c.expression, "9 + sqrt(16)")
        c.command(E.EQU)
        self.assertEqual(c.display, "13")
        self.assertEqual(c.history[-1], ("9 + sqrt(16)", c.value))

    def test_memory_keeps_expression(self):
        c = run("12 + 7 r MS")
        self.assertEqual(c.expression, "12 + sqrt(7)")

    def test_invalid_sqrt(self):
        self.assertEqual(run("4 ~ r").display, "Invalid input")

    def test_percent(self):
        self.assertEqual(run("200 + 10 % =").display, "220")
        self.assertEqual(run("50 %").display, "0")

    def test_entry_editing(self):
        self.assertEqual(run("123 <").display, "12")
        self.assertEqual(run("1 . 50").display, "1.50")
        self.assertEqual(run("12 ~").display, "-12")
        self.assertEqual(run("5 + 3 E 4 =").display, "9")

    def test_digit_limit(self):
        self.assertEqual(run("12345678901234567").display, "1234567890123456")

    def test_grouping(self):
        self.assertEqual(run("1234567 . 5", grouping=True).display, "1,234,567.5")

    def test_memory(self):
        c = run("5 MS C 3 MPLUS MR")
        self.assertEqual(c.display, "8")
        self.assertTrue(c.has_memory)
        c.command(E.MC)
        self.assertFalse(c.has_memory)

    def test_exponent_entry(self):
        c = run("1 EXP 5")
        self.assertEqual(c.display, "1.e+5")
        c.command(E.ADD)
        self.assertEqual(c.display, "100,000" if c.grouping else "100000")


class ScientificMode(unittest.TestCase):
    def test_precedence(self):
        self.assertEqual(run("2 + 3 * 4 =", E.SCI).display, "14")
        self.assertEqual(run("2 * 3 ^ 2 =", E.SCI).display, "18")
        self.assertEqual(run("8 - 3 * + 1 =", E.SCI).display, "6")

    def test_parens(self):
        c = run("( 2 + 3 ) * 4", E.SCI)
        self.assertEqual(c.expression, "(2 + 3) *")
        self.assertEqual(c.paren_count, 0)
        c.command(E.EQU)
        self.assertEqual(c.display, "20")
        self.assertEqual(run("2 * ( 3 + 4 =", E.SCI).display, "14")

    def test_precision_32(self):
        self.assertEqual(run("1 / 3 =", E.SCI).display, "0.33333333333333333333333333333333")

    def test_trig_degrees(self):
        self.assertEqual(run("30 SIN", E.SCI).display, "0.5")
        self.assertEqual(run("180 SIN", E.SCI).display, "0")
        self.assertEqual(run("90 TAN", E.SCI).display, "Invalid input")
        self.assertEqual(run("45 TAN", E.SCI).display, "1")
        self.assertEqual(run("0 . 5 INV SIN", E.SCI).display, "30")

    def test_inv_resets(self):
        c = run("1 INV LN", E.SCI)
        self.assertTrue(c.display.startswith("2.7182818284590452353602874713527"))
        self.assertFalse(c.inv)
        self.assertEqual(c.expression, "powe(1)")

    def test_trig_radians_and_expr(self):
        c = run("RAD 0 COS", E.SCI)
        self.assertEqual(c.display, "1")
        self.assertEqual(c.expression, "cosr(0)")

    def test_misc_functions(self):
        self.assertEqual(run("5 FACT", E.SCI).display, "120")
        self.assertEqual(run("27 CUBEROOT", E.SCI).display, "3")
        self.assertEqual(run("27 ROOT 3 =", E.SCI).display, "3")
        self.assertEqual(run("2 POW10", E.SCI).display, "100")
        self.assertEqual(run("1000 LOG", E.SCI).display, "3")
        self.assertEqual(run("7 MOD 3 =", E.SCI).display, "1")
        self.assertEqual(run("2 . 75 INT", E.SCI).display, "2")
        self.assertEqual(run("2 . 75 INV INT", E.SCI).display, "0.75")
        self.assertEqual(run("1 . 5 DMS", E.SCI).display, "1.3")
        self.assertEqual(run("1 . 3 INV DMS", E.SCI).display, "1.5")

    def test_fe(self):
        self.assertEqual(run("1234 FE", E.SCI).display, "1.234e+3")

    def test_pi(self):
        self.assertTrue(run("PI", E.SCI).display.startswith("3.1415926535897932384626433832795"))

    def test_overflow(self):
        self.assertEqual(run("10 ^ 10000 =", E.SCI).display, "Overflow")


class ProgrammerMode(unittest.TestCase):
    def test_hex_entry_and_ops(self):
        c = run("HEX", E.PROG)
        for d in (15, 15):
            c.command(E.DIGIT0 + d)
        c.command(E.ADD)
        c.command(E.DIGIT0 + 1)
        c.command(E.EQU)
        self.assertEqual(c.display, "100")

    def test_word_wrap(self):
        c = run("BYTE 127 + 1 =", E.PROG)
        self.assertEqual(c.display, "-128")
        c.command(E.HEX)
        self.assertEqual(c.display, "80")

    def test_bitwise(self):
        self.assertEqual(run("12 AND 10 =", E.PROG).display, "8")
        self.assertEqual(run("12 OR 3 =", E.PROG).display, "15")
        self.assertEqual(run("1 LSH 4 =", E.PROG).display, "16")
        self.assertEqual(run("0 NOT", E.PROG).display, "-1")
        self.assertEqual(run("BYTE 1 ROR HEX", E.PROG).display, "80")

    def test_integer_division(self):
        self.assertEqual(run("7 / 2 =", E.PROG).display, "3")

    def test_disabled(self):
        c = run("BIN", E.PROG)
        self.assertFalse(c.is_enabled(E.DIGIT0 + 2))
        self.assertFalse(c.is_enabled(E.POINT))

    def test_toggle_bit(self):
        c = run("", E.PROG)
        c.toggle_bit(3)
        self.assertEqual(c.display, "8")


class StatisticsMode(unittest.TestCase):
    def test_stats(self):
        c = run("2 STAT_ADD 4 STAT_ADD 4 STAT_ADD 4 STAT_ADD 5 STAT_ADD 5 STAT_ADD 7 STAT_ADD 9 STAT_ADD", E.STAT)
        self.assertEqual(len(c.dataset), 8)
        c.command(E.STAT_AVE)
        self.assertEqual(c.display, "5")
        c.command(E.STAT_SUM)
        self.assertEqual(c.display, "40")
        c.command(E.STAT_SDEV)
        self.assertEqual(c.display, "2")
        c.command(E.STAT_CAD)
        self.assertEqual(c.dataset, [])

    def test_empty(self):
        self.assertEqual(run("STAT_AVE", E.STAT).display, "Result not defined")


class ModeSwitch(unittest.TestCase):
    def test_keeps_value(self):
        c = run("12 . 7", E.SCI)
        c.set_mode(E.PROG)
        self.assertEqual(c.display, "12")
        c.set_mode(E.STD)
        self.assertEqual(c.display, "12")


class FullPrecision(unittest.TestCase):
    """Scientific mode keeps 32 significant digits, never Decimal's default 28."""

    def calc(self, value, *cmds):
        c = Engine()
        c.set_mode(E.SCI)
        c.set_value(value)
        for cmd in cmds:
            c.set_value(cmd) if isinstance(cmd, str) else c.command(cmd)
        return c.display

    def test_negate(self):
        self.assertEqual(self.calc("0.33333333333333333333333333333333", E.SIGN),
                         "-0.33333333333333333333333333333333")

    def test_int_of_long_number(self):
        self.assertEqual(self.calc("1234567890123456789012345678901.5", E.INT),
                         "1234567890123456789012345678901")

    def test_atanh_near_one(self):
        self.assertEqual(self.calc("0.99999999999999999999999999999999", E.INV, E.TANH),
                         "37.187935078184703598996479335679")

    def test_odd_root_of_negative(self):
        self.assertEqual(self.calc("-2.0000000000000000000000000000001", E.ROOT, "3", E.EQU),
                         "-1.2599210498948731647672106072782")

    def test_cosh_uses_full_input(self):
        self.assertEqual(self.calc("-1.0000000000000000000000000000001", E.COSH),
                         "1.5430806348152437784779056207572")


if __name__ == "__main__":
    unittest.main()
