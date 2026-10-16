"""Calculator state machine replicating win32calc (Windows 7 Calculator) behaviour.

Command ids are the original control ids from win32calc's resources, so the
layout tables and keyboard accelerators can use them directly.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from decimal import Decimal

from . import mathfn as M
from .mathfn import CalcError
from .numfmt import format_decimal, format_entry, format_int

# ---------------------------------------------------------------- command ids
SIGN, CLEAR, CE, BACK, POINT = 80, 81, 82, 83, 84
AND, OR, XOR, LSH, RSH, DIV, MUL, ADD, SUB, MOD, ROOT, PWR = 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97
INT, ROL, ROR, NOT = 98, 99, 100, 101
SIN, COS, TAN, SINH, COSH, TANH, LN, LOG = 102, 103, 104, 105, 106, 107, 108, 109
SQRT, SQR, CUBE, FACT, RECIP, DMS, CUBEROOT, POW10, PERCENT, FE, PI, EQU = (
    110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121)
MC, MR, MS, MPLUS, MMINUS, EXP, OPENP, CLOSEP = 122, 123, 124, 125, 126, 127, 128, 129
DIGIT0 = 130          # 130..139 = 0..9, 140..145 = A..F
INV = 146
STAT_ADD, STAT_AVE, STAT_AVE2, STAT_SUM, STAT_SUM2, STAT_SDEV, STAT_SDEV1 = 168, 171, 172, 173, 174, 175, 176
STAT_CAD = 1056
HEX, DEC, OCT, BIN = 313, 314, 315, 316
QWORD, DWORD, WORD, BYTE = 317, 318, 319, 320
DEG, RAD, GRAD = 321, 322, 323

BINOPS = {AND, OR, XOR, LSH, RSH, DIV, MUL, ADD, SUB, MOD, ROOT, PWR}
OP_SYMBOL = {ADD: "+", SUB: "-", MUL: "*", DIV: "/", MOD: "Mod", PWR: "^", ROOT: "yroot",
             AND: "And", OR: "Or", XOR: "Xor", LSH: "Lsh", RSH: "Rsh"}
# Operator precedence from win32calc's engine (higher binds tighter).
PRECEDENCE = {OR: 0, XOR: 0, AND: 1, ADD: 2, SUB: 2, RSH: 3, LSH: 3, MOD: 3, DIV: 3, MUL: 3, PWR: 4, ROOT: 4}

RADIX_IDS = {HEX: 16, DEC: 10, OCT: 8, BIN: 2}
WORD_IDS = {QWORD: 64, DWORD: 32, WORD: 16, BYTE: 8}
ANGLE_IDS = {DEG: "deg", RAD: "rad", GRAD: "grad"}
ANGLE_SUFFIX = {"deg": "d", "rad": "r", "grad": "g"}

MAX_PARENS = 25
STD, SCI, PROG, STAT = "std", "sci", "prog", "stat"


def wrap(v: int, bits: int) -> int:
    """Wrap to a signed two's-complement integer of `bits` width."""
    half = 1 << (bits - 1)
    return ((v + half) & ((1 << bits) - 1)) - half


@dataclass
class Entry:
    """A number being typed."""
    digits: str = "0"
    neg: bool = False
    exp: str | None = None
    exp_neg: bool = False


@dataclass
class Frame:
    """One parenthesis level: pending (lhs, op) pairs and the expression tokens."""
    ops: list = field(default_factory=list)
    tokens: list = field(default_factory=list)


class Engine:
    def __init__(self):
        self.mode = STD
        self.angle = "deg"
        self.radix = 10
        self.bits = 64
        self.grouping = False
        self.fe = False
        self.inv = False
        self.memory = None
        self.history: list[tuple[str, object]] = []
        self.dataset: list[Decimal] = []
        self.clear_all()

    # ------------------------------------------------------------ state helpers
    def clear_all(self):
        self.value = 0 if self.mode == PROG else M.D0
        self.entry: Entry | None = None
        self.frames = [Frame()]
        self.operand_expr: str | None = None
        self.last_binop = None
        self.error: str | None = None
        self.prev: str | None = None
        self._last_applied = None

    @property
    def int_mode(self) -> bool:
        return self.mode == PROG

    @property
    def precision(self) -> int:
        return 16 if self.mode == STD else 32

    @property
    def paren_count(self) -> int:
        return len(self.frames) - 1

    def _zero(self):
        return 0 if self.int_mode else M.D0

    def _entry_value(self, e: Entry):
        if self.int_mode:
            return wrap(int(e.digits, self.radix), self.bits)
        s = ("-" if e.neg else "") + e.digits.rstrip(".")
        if e.exp:
            s += "e" + ("-" if e.exp_neg else "") + e.exp
        return M.check(Decimal(s))

    def current(self):
        """The value currently shown (committing nothing)."""
        if self.entry is not None:
            return self._entry_value(self.entry)
        return self.value

    def _commit(self):
        if self.entry is not None:
            self.value = self._entry_value(self.entry)
            self.entry = None
        return self.value

    def fmt(self, v, grouping: bool | None = None) -> str:
        g = self.grouping if grouping is None else grouping
        if self.int_mode:
            return format_int(v, self.radix, self.bits, g)
        return format_decimal(v, self.precision, g, self.fe)

    def _operand_text(self, x) -> str:
        return self.operand_expr if self.operand_expr is not None else self.fmt(x)

    # ------------------------------------------------------------ public views
    @property
    def display(self) -> str:
        if self.error:
            return self.error
        if self.entry is not None:
            e = self.entry
            if self.int_mode:
                d = e.digits.upper()
                if self.grouping:
                    from .numfmt import group_int, _RADIX_GROUP
                    d = group_int(d, *_RADIX_GROUP[self.radix])
                return d
            return format_entry(e.digits, e.neg, e.exp, e.exp_neg, self.grouping)
        return self.fmt(self.value)

    @property
    def expression(self) -> str:
        parts = []
        for i, f in enumerate(self.frames):
            if i:
                parts.append("(")
            parts.extend(f.tokens)
        if self.operand_expr is not None:
            parts.append(self.operand_expr)
        return " ".join(parts).replace("( ", "(")

    @property
    def has_memory(self) -> bool:
        return self.memory is not None

    def unsigned_value(self) -> int:
        """Programmer-mode value as an unsigned word (for the bit display)."""
        try:
            v = self.current()
        except CalcError:
            v = 0
        return int(v) & ((1 << self.bits) - 1) if self.int_mode else 0

    def is_enabled(self, cid: int) -> bool:
        if self.mode == PROG:
            if cid in (POINT, FE, EXP, SQRT, PERCENT, RECIP):
                return False
            if DIGIT0 <= cid <= DIGIT0 + 15:
                return cid - DIGIT0 < self.radix
        elif DIGIT0 + 10 <= cid <= DIGIT0 + 15:
            return False
        return True

    # ------------------------------------------------------------ dispatcher
    def command(self, cid: int):
        """Execute one command; errors become the display text."""
        if not self.is_enabled(cid):
            return
        try:
            self._dispatch(cid)
        except CalcError as e:
            self._set_error(str(e))
        except (ArithmeticError, ValueError):
            self._set_error(M.ERR_INVALID)

    def _set_error(self, msg: str):
        self.clear_all()
        self.error = msg

    def _dispatch(self, cid: int):
        # Commands that work regardless of an error state.
        if cid == CLEAR:
            return self.clear_all() if self.mode != STAT else self._stat_clear()
        if cid == CE:
            self.error = None
            self.entry = None
            self.operand_expr = None
            self.value = self._zero()
            return
        if cid in ANGLE_IDS:
            self.angle = ANGLE_IDS[cid]
            return
        if cid in RADIX_IDS:
            return self._set_radix(RADIX_IDS[cid])
        if cid in WORD_IDS:
            return self._set_bits(WORD_IDS[cid])
        if cid == INV:
            self.inv = not self.inv
            return
        if cid == FE:
            if not self.error:
                self._commit()
            self.fe = not self.fe
            return
        if cid == MC:
            self.memory = None
            return
        if cid == STAT_CAD:
            self.dataset.clear()
            return

        if self.error:
            # After an error only number entry (which starts over) is accepted.
            if DIGIT0 <= cid <= DIGIT0 + 15 or cid in (POINT, MR):
                self.clear_all()
            else:
                return

        if DIGIT0 <= cid <= DIGIT0 + 15:
            return self._digit(cid - DIGIT0)
        if cid == POINT:
            return self._point()
        if cid == BACK:
            return self._backspace()
        if cid == EXP:
            return self._exp()
        if cid == SIGN:
            return self._sign()
        if cid in BINOPS:
            return self._binop(cid)
        if cid == EQU:
            return self._equals()
        if cid == OPENP:
            return self._open_paren()
        if cid == CLOSEP:
            return self._close_paren()
        if cid == PERCENT:
            return self._percent()
        if cid == PI:
            return self._pi()
        if cid in (MR, MS, MPLUS, MMINUS):
            return self._memory(cid)
        if cid in (STAT_ADD, STAT_AVE, STAT_AVE2, STAT_SUM, STAT_SUM2, STAT_SDEV, STAT_SDEV1):
            return self._stat(cid)
        if cid in UNARY_IDS:
            return self._unary(cid)

    # ------------------------------------------------------------ number entry
    def _start_entry(self):
        if self.entry is None:
            self.operand_expr = None
            self.entry = Entry()
            self.prev = "digit"

    def _digit(self, d: int):
        self._start_entry()
        e = self.entry
        if e.exp is not None:
            if len(e.exp) < 4:
                e.exp = (e.exp + str(d)).lstrip("0") or ""
            return
        ch = "0123456789ABCDEF"[d]
        new = ch if e.digits == "0" else e.digits + ch
        if self.int_mode:
            if int(new, self.radix) >= (1 << self.bits):
                return
        else:
            count = len(new.replace(".", "").lstrip("0")) if new.startswith("0.") else len(new.replace(".", ""))
            if count > self.precision:
                return
        e.digits = new

    def _point(self):
        self._start_entry()
        e = self.entry
        if "." not in e.digits and e.exp is None:
            e.digits += "."

    def _exp(self):
        self._start_entry()
        if self.entry.exp is None:
            self.entry.exp = ""

    def _backspace(self):
        e = self.entry
        if e is None:
            return
        if e.exp is not None:
            if e.exp:
                e.exp = e.exp[:-1]
            else:
                e.exp = None
                e.exp_neg = False
        elif len(e.digits) > 1:
            e.digits = e.digits[:-1]
        else:
            e.digits = "0"
            e.neg = False

    def _sign(self):
        e = self.entry
        if e is not None and not self.int_mode:
            if e.exp is not None:
                e.exp_neg = not e.exp_neg
            elif e.digits != "0":
                e.neg = not e.neg
            return
        x = self._commit()
        self._finish_unary("negate", x, M.neg(x) if not self.int_mode else wrap(-x, self.bits))

    # ------------------------------------------------------------ binary ops
    def _apply(self, lhs, op, rhs):
        self._last_applied = (op, rhs)
        if self.int_mode:
            return self._apply_int(lhs, op, rhs)
        if op == ADD:
            return M.add(lhs, rhs)
        if op == SUB:
            return M.sub(lhs, rhs)
        if op == MUL:
            return M.mul(lhs, rhs)
        if op == DIV:
            return M.div(lhs, rhs)
        if op == MOD:
            return M.mod(lhs, rhs)
        if op == PWR:
            return M.power(lhs, rhs)
        if op == ROOT:
            return M.root(lhs, rhs)
        raise CalcError(M.ERR_INVALID)

    def _apply_int(self, a: int, op: int, b: int) -> int:
        bits, mask = self.bits, (1 << self.bits) - 1
        if op in (DIV, MOD) and b == 0:
            raise CalcError(M.ERR_UNDEFINED if a == 0 else M.ERR_DIV0)
        if op == ADD:
            r = a + b
        elif op == SUB:
            r = a - b
        elif op == MUL:
            r = a * b
        elif op == DIV:
            r = abs(a) // abs(b) * (-1 if (a < 0) != (b < 0) else 1)
        elif op == MOD:
            r = abs(a) % abs(b) * (-1 if a < 0 else 1)
        elif op == AND:
            r = (a & mask) & (b & mask)
        elif op == OR:
            r = (a & mask) | (b & mask)
        elif op == XOR:
            r = (a & mask) ^ (b & mask)
        elif op in (LSH, RSH):
            if b < 0:
                raise CalcError(M.ERR_INVALID)
            if op == LSH:
                r = a << b if b < bits else 0
            else:
                r = a >> min(b, bits)
        else:
            raise CalcError(M.ERR_INVALID)
        return wrap(r, bits)

    def _resolve(self, frame: Frame, x, op=None):
        """Apply pending ops in `frame` that bind at least as tightly as `op` (all if None)."""
        precedence = self.mode != STD
        while frame.ops:
            lhs, top = frame.ops[-1]
            if op is not None and precedence and PRECEDENCE[top] < PRECEDENCE[op]:
                break
            frame.ops.pop()
            x = self._apply(lhs, top, x)
        return x

    def _binop(self, op: int):
        if self.mode == STAT:
            return
        frame = self.frames[-1]
        if self.prev == "binop" and frame.ops:
            # Replace the operator just entered.
            x, _ = frame.ops.pop()
            frame.tokens[-1] = OP_SYMBOL[op]
        else:
            x = self._commit()
            frame.tokens.append(self._operand_text(x))
            frame.tokens.append(OP_SYMBOL[op])
        x = self._resolve(frame, x, op)
        frame.ops.append((x, op))
        self.value = x
        self.operand_expr = None
        self.last_binop = None
        self.prev = "binop"

    def _equals(self):
        if self.mode == STAT:
            return
        x = self._commit()
        pending = len(self.frames) > 1 or self.frames[0].ops
        if pending:
            # The displayed value is the right-hand operand (so "5 + =" gives 10).
            text = self.expression
            if self.operand_expr is None:
                text += " " + self._operand_text(x)
            text += ")" * self.paren_count
            while len(self.frames) > 1:
                x = self._resolve(self.frames.pop(), x)
            x = self._resolve(self.frames[0], x)
            self.last_binop = self._last_applied
            self._add_history(text.strip(), x)
        elif self.last_binop is not None:
            op, rhs = self.last_binop
            text = f"{self._operand_text(x)} {OP_SYMBOL[op]} {self.fmt(rhs)}"
            x = self._apply(x, op, rhs)
            self._add_history(text, x)
        self.frames = [Frame()]
        self.value = x
        self.operand_expr = None
        self.prev = "equ"

    def _add_history(self, text: str, value):
        if self.mode in (STD, SCI):
            self.history.append((text, value))

    # ------------------------------------------------------------ parentheses
    def _open_paren(self):
        if self.mode == STD or self.paren_count >= MAX_PARENS:
            return
        if self.prev not in ("binop", "open", None):
            # A number with no operator before "(" is discarded, like win32calc.
            self.entry = None
            self.operand_expr = None
        self.frames.append(Frame())
        self.prev = "open"

    def _close_paren(self):
        if self.mode == STD or self.paren_count == 0:
            return
        x = self._commit()
        inner = self.frames.pop()
        text = "(" + " ".join(inner.tokens + [self._operand_text(x)]) + ")"
        x = self._resolve(inner, x)
        self.value = x
        self.operand_expr = text
        self.prev = "close"

    # ------------------------------------------------------------ unary ops
    def _finish_unary(self, name: str | None, x, result):
        if name is not None:
            self.operand_expr = f"{name}({self._operand_text(x)})"
        else:
            self.operand_expr = None
        self.value = result
        self.entry = None
        self.prev = "unary"

    def _unary(self, cid: int):
        x = self._commit()
        inv = self.inv
        name, fn = self._unary_fn(cid, inv)
        result = fn(x)
        if cid in INV_AFFECTED:
            self.inv = False
        self._finish_unary(name, x, result)

    def _unary_fn(self, cid: int, inv: bool):
        a = self.angle
        sfx = ANGLE_SUFFIX[a]
        b = self.bits
        if self.int_mode:
            if cid == NOT:
                return "Not", lambda x: wrap(~x, b)
            if cid == ROL:
                return "RoL", lambda x: wrap(((x << 1) | ((x >> (b - 1)) & 1)) & ((1 << b) - 1), b)
            if cid == ROR:
                return "RoR", lambda x: wrap(((x & ((1 << b) - 1)) >> 1) | ((x & 1) << (b - 1)), b)
            if cid == SQR:
                return "sqr", lambda x: wrap(x * x, b)
            if cid == CUBE:
                return "cube", lambda x: wrap(x * x * x, b)
            if cid == FACT:
                return "fact", lambda x: wrap(int(M.factorial(Decimal(x))), b)
            raise CalcError(M.ERR_INVALID)
        table = {
            SQRT: ("sqrt", M.sqrt), SQR: ("sqr", M.square), CUBE: ("cube", M.cube),
            RECIP: ("reciproc", M.reciprocal), FACT: ("fact", M.factorial),
            LOG: ("log", M.log10), POW10: ("powten", M.pow10), CUBEROOT: ("cuberoot", M.cbrt),
            SINH: ("asinh" if inv else "sinh", M.asinh if inv else M.sinh),
            COSH: ("acosh" if inv else "cosh", M.acosh if inv else M.cosh),
            TANH: ("atanh" if inv else "tanh", M.atanh if inv else M.tanh),
            LN: ("powe" if inv else "ln", M.exp if inv else M.ln),
            INT: ("frac" if inv else "Int", M.frac_part if inv else M.int_part),
            DMS: ("degrees" if inv else "dms", M.degrees if inv else M.dms),
            SIN: (("asin" if inv else "sin") + sfx, (lambda x: M.asin(x, a)) if inv else (lambda x: M.sin(x, a))),
            COS: (("acos" if inv else "cos") + sfx, (lambda x: M.acos(x, a)) if inv else (lambda x: M.cos(x, a))),
            TAN: (("atan" if inv else "tan") + sfx, (lambda x: M.atan(x, a)) if inv else (lambda x: M.tan(x, a))),
        }
        if cid not in table:
            raise CalcError(M.ERR_INVALID)
        return table[cid]

    def _percent(self):
        if self.int_mode:
            return
        x = self._commit()
        frame = self.frames[-1]
        r = M.div(M.mul(frame.ops[-1][0], x), Decimal(100)) if frame.ops else M.D0
        self._finish_unary(None, x, r)

    def _pi(self):
        if self.int_mode:
            return
        self._commit()
        with M.ctx():
            r = +(M.PI * 2 if self.inv else M.PI)
        self.inv = False
        self._finish_unary(None, None, r)

    # ------------------------------------------------------------ memory
    def _memory(self, cid: int):
        if cid == MR:
            if self.memory is None:
                return
            self.entry = None
            self.operand_expr = None
            self.value = self.memory
            self.prev = "mem"
            return
        x = self._commit()
        zero = self._zero()
        if cid == MS:
            self.memory = x
        elif self.int_mode:
            m = self.memory or 0
            self.memory = wrap(m + x if cid == MPLUS else m - x, self.bits)
        else:
            m = self.memory if self.memory is not None else zero
            self.memory = M.add(m, x) if cid == MPLUS else M.sub(m, x)
        self.prev = "mem"

    # ------------------------------------------------------------ statistics
    def _stat_clear(self):
        self.entry = None
        self.value = M.D0
        self.error = None
        self.prev = None

    def _stat(self, cid: int):
        if self.mode != STAT:
            return
        if cid == STAT_ADD:
            self.dataset.append(self._commit())
            self.prev = "mem"
            return
        self._commit()
        data = self.dataset
        n = len(data)
        with M.ctx():
            s = sum(data, M.D0)
            s2 = sum((v * v for v in data), M.D0)
            if cid == STAT_SUM:
                r = s
            elif cid == STAT_SUM2:
                r = s2
            elif n == 0:
                raise CalcError("Result not defined")
            elif cid == STAT_AVE:
                r = s / n
            elif cid == STAT_AVE2:
                r = s2 / n
            else:
                d = n - (1 if cid == STAT_SDEV1 else 0)
                if d == 0:
                    raise CalcError("Result not defined")
                var = (s2 - s * s / n) / d
                r = var.sqrt() if var > 0 else M.D0
        self._finish_unary(None, None, M.check(r))

    # ------------------------------------------------------------ modes
    def set_mode(self, mode: str):
        if mode == self.mode:
            return
        try:
            v = self.current()
        except CalcError:
            v = 0
        if self.error:
            v = 0
        old = self.mode
        self.mode = mode
        self.clear_all()
        if mode == PROG:
            self.value = wrap(int(v), self.bits) if old != PROG else v
            if self.memory is not None:
                self.memory = wrap(int(self.memory), self.bits)
        else:
            self.value = Decimal(v) if old == PROG else v
            if old == PROG and self.memory is not None:
                self.memory = Decimal(self.memory)
        self.inv = False

    def _set_radix(self, radix: int):
        if not self.int_mode:
            return
        self._commit()
        self.radix = radix
        self.operand_expr = None if self.prev not in ("unary", "close") else self.operand_expr

    def _set_bits(self, bits: int):
        if not self.int_mode:
            return
        self._commit()
        self.bits = bits
        self.value = wrap(self.value, bits)
        if self.memory is not None:
            self.memory = wrap(self.memory, bits)
        for f in self.frames:
            f.ops = [(wrap(l, bits), o) for l, o in f.ops]

    def toggle_bit(self, i: int):
        if not self.int_mode or i >= self.bits:
            return
        if self.error:
            self.clear_all()
        x = self._commit()
        self.value = wrap(x ^ (1 << i), self.bits)
        self.operand_expr = None
        self.prev = "unary"

    def set_value(self, v):
        """Load a value (paste / history recall) as a finished number."""
        if self.error:
            self.clear_all()
        self.entry = None
        self.operand_expr = None
        self.value = wrap(int(v), self.bits) if self.int_mode else M.check(Decimal(v))
        self.prev = "mem"

    def clear_history(self):
        self.history.clear()


UNARY_IDS = {SQRT, SQR, CUBE, RECIP, FACT, LOG, POW10, CUBEROOT, SINH, COSH, TANH, LN, INT, DMS,
             SIN, COS, TAN, NOT, ROL, ROR}
INV_AFFECTED = {SIN, COS, TAN, SINH, COSH, TANH, LN, INT, DMS, PI}
