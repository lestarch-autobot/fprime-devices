#!/usr/bin/env python3
"""Flatten a GenICam device description into a static register table.

Design-time tool (Python 3 standard library only). It reads the camera's
GenApi XML, follows each requested feature down to the registers behind it and
emits:

  <out>/blackfly_regtable.json   everything, machine readable
  <out>/BlackflyRegTable.h       C header with the same table
  <out>/regmap.tsv               name/address map consumed by probe/u3vprobe -m

Nothing here runs in flight: the flight code uses the generated table only.

How the XML is flattened
  * A feature (Integer/Float/Enumeration/Boolean/Command/StringReg) is followed
    through pValue until a register node (IntReg/MaskedIntReg/StringReg/
    Register) is reached.
  * Converter/SwissKnife nodes on the way are classified:
      identity     value = raw
      linear       value = raw * scale                  (scale constant)
      fixed_point  value = raw * scale / 2**N           (N read from a second register)
      reciprocal   value = scale * M / raw              (M read from a second register)
      formula      anything else: kept as text, flagged "unflattened"
    The classification is numeric (the formula is evaluated at sample points and
    compared with each closed form), so it does not depend on how the vendor
    spelled the expression.
  * Register addresses are Address constants plus pAddress nodes. A pAddress
    formula that depends on a selector is evaluated for every selector entry,
    giving one address per selector value ("variants").
  * pMin/pMax/pInc, pIsImplemented/pIsAvailable/pIsLocked are resolved the same
    way and reported as registers (or constants).
  * With --snapshot (register values read from a camera by regsnap.sh) the
    implemented/available bits and limits are folded in as observed values.
"""
import argparse
import itertools
import json
import re
import sys
import xml.etree.ElementTree as ET

NS = "{http://www.genicam.org/GenApi/Version_1_1}"

FEATURES = """
DeviceVendorName DeviceModelName DeviceVersion DeviceFirmwareVersion DeviceSerialNumber DeviceID
DeviceUserID DeviceManufacturerInfo SensorDescription DeviceTemperatureSelector DeviceTemperature
DeviceUptime LinkUptime EnumerationCount DevicePowerSupplySelector PowerSupplyVoltage PowerSupplyCurrent
SensorWidth SensorHeight WidthMax HeightMax Width Height OffsetX OffsetY PixelFormat PixelSize
PixelColorFilter AdcBitDepth BinningSelector BinningHorizontalMode BinningVerticalMode BinningHorizontal
BinningVertical DecimationSelector DecimationHorizontalMode DecimationVerticalMode DecimationHorizontal
DecimationVertical ReverseX ReverseY IspEnable TestPatternGeneratorSelector TestPattern
AcquisitionMode AcquisitionStart AcquisitionStop AcquisitionFrameCount AcquisitionBurstFrameCount
ExposureMode ExposureTime ExposureAuto AcquisitionFrameRate AcquisitionResultingFrameRate
AcquisitionFrameRateEnable TriggerSelector TriggerMode TriggerSoftware TriggerSource TriggerActivation
TriggerOverlap TriggerDelay SensorShutterMode
GainSelector Gain GainAuto BlackLevelSelector BlackLevel BlackLevelRaw BlackLevelClampingEnable Gamma
GammaEnable BalanceRatioSelector BalanceRatio BalanceWhiteAuto
PayloadSize DeviceLinkSpeed DeviceLinkThroughputLimit DeviceMaxThroughput DeviceLinkCurrentThroughput
DeviceLinkBandwidthReserve LinkErrorCount LinkRecoveryCount TimestampLatch TimestampLatchValue
TimestampIncrement DeviceReset MaxDeviceResetTime DeviceIndicatorMode
ChunkModeActive ChunkSelector ChunkEnable
TransferControlMode TransferOperationMode TransferBlockCount TransferQueueMaxBlockCount
TransferQueueCurrentBlockCount TransferQueueOverflowCount TransferQueueMode TransferStart TransferStop
EventSelector EventNotification UserSetSelector UserSetLoad UserSetSave UserSetDefault
TestPendingAck TestEventGenerate
""".split()

REGISTER_TAGS = ("IntReg", "MaskedIntReg", "StringReg", "Register", "FloatReg")
COMPUTED_TAGS = ("Converter", "IntConverter", "SwissKnife", "IntSwissKnife")


# --------------------------------------------------------------------------
# GenApi formula evaluation (integer/float arithmetic, comparisons, ?:)
# --------------------------------------------------------------------------
TOKEN = re.compile(r"\s*(0x[0-9A-Fa-f]+|\d+\.\d*|\d+|[A-Za-z_][A-Za-z0-9_.]*|\*\*|<>|<=|>=|<<|>>|&&|\|\||[-+*/%()?:=<>&|^~])")

BINARY = [  # lowest precedence first
    ("||",), ("&&",), ("|",), ("^",), ("&",), ("=", "<>"), ("<", ">", "<=", ">="), ("<<", ">>"), ("+", "-"), ("*", "/", "%"), ("**",),
]


def tokenize(text):
    pos, out = 0, []
    text = text.strip()
    while pos < len(text):
        m = TOKEN.match(text, pos)
        if m is None:
            raise ValueError("cannot tokenize formula: %r" % text[pos:])
        out.append(m.group(1))
        pos = m.end()
    return out


class Formula:
    def __init__(self, text, env):
        self.tok = tokenize(text)
        self.i = 0
        self.env = env

    def peek(self):
        return self.tok[self.i] if self.i < len(self.tok) else None

    def take(self):
        self.i += 1
        return self.tok[self.i - 1]

    def value(self):
        v = self.ternary()
        if self.peek() is not None:
            raise ValueError("trailing tokens in formula")
        return v

    def ternary(self):
        cond = self.binary(0)
        if self.peek() == "?":
            self.take()
            a = self.ternary()
            if self.take() != ":":
                raise ValueError("expected ':'")
            b = self.ternary()
            return a if cond else b
        return cond

    def binary(self, level):
        if level == len(BINARY):
            return self.unary()
        left = self.binary(level + 1)
        while self.peek() in BINARY[level]:
            op = self.take()
            right = self.binary(level + 1)
            left = self.apply(op, left, right)
        return left

    @staticmethod
    def apply(op, a, b):
        if op == "||": return int(bool(a) or bool(b))
        if op == "&&": return int(bool(a) and bool(b))
        if op == "|": return int(a) | int(b)
        if op == "^": return int(a) ^ int(b)
        if op == "&": return int(a) & int(b)
        if op == "=": return int(a == b)
        if op == "<>": return int(a != b)
        if op == "<": return int(a < b)
        if op == ">": return int(a > b)
        if op == "<=": return int(a <= b)
        if op == ">=": return int(a >= b)
        if op == "<<": return int(a) << int(b)
        if op == ">>": return int(a) >> int(b)
        if op == "+": return a + b
        if op == "-": return a - b
        if op == "*": return a * b
        if op == "/": return a // b if isinstance(a, int) and isinstance(b, int) and a % b == 0 else a / b
        if op == "%": return a % b
        return a ** b

    def unary(self):
        t = self.take()
        if t == "-": return -self.unary()
        if t == "+": return self.unary()
        if t == "~": return ~int(self.unary())
        if t == "(":
            v = self.ternary()
            if self.take() != ")":
                raise ValueError("expected ')'")
            return v
        if t[0].isdigit():
            return int(t, 16) if t.lower().startswith("0x") else (float(t) if "." in t else int(t))
        if t in self.env:
            return self.env[t]
        raise KeyError(t)


# --------------------------------------------------------------------------
# XML model
# --------------------------------------------------------------------------
class Model:
    def __init__(self, path):
        root = ET.parse(path).getroot()
        self.attrs = dict(root.attrib)
        self.nodes = {e.get("Name"): e for e in root if e.get("Name")}
        self.snapshot = {}

    def tag(self, name):
        return self.nodes[name].tag.replace(NS, "")

    def text(self, name, child):
        e = self.nodes[name].find(NS + child)
        return None if e is None or e.text is None else e.text.strip()

    def all(self, name, child):
        return [e for e in self.nodes[name].findall(NS + child)]

    def variables(self, name):
        return {e.get("Name"): e.text.strip() for e in self.all(name, "pVariable")}

    # ---- selectors ------------------------------------------------------
    def selector_values(self, name):
        """Return {entry_name: value} for an enumeration selector, or None when the
        node is a plain constant. On this camera every selector the flight list
        needs is host-side: it has no register, the XML only uses it to compute
        register addresses, so each selector entry becomes an address variant."""
        tag = self.tag(name)
        if tag == "Enumeration":
            entries = {e.get("Name"): int(e.find(NS + "Value").text, 0) for e in self.all(name, "EnumEntry")}
            return entries if len(entries) > 1 or self.text(name, "pValue") is not None else None
        if tag == "Integer" and self.text(name, "pValue") is not None:
            raise LookupError("integer selector %s is register backed (index range not static)" % name)
        return None

    def const_value(self, name):
        v = self.text(name, "Value")
        if v is None:
            raise LookupError("%s has no constant value" % name)
        return float(v) if "." in v else int(v, 0)

    def free_selectors(self, name, seen=None):
        """Selector feature nodes a (pAddress) formula tree depends on."""
        seen = set() if seen is None else seen
        tag = self.tag(name)
        if tag in ("Enumeration", "Integer"):
            if tag == "Integer" and self.text(name, "pValue") is not None:
                raise LookupError("integer selector %s is register backed (index range not static)" % name)
            return [name] if self.selector_values(name) is not None else []
        out = []
        for var in self.variables(name).values():
            for s in self.free_selectors(var, seen):
                if s not in out:
                    out.append(s)
        return out

    def eval_node(self, name, choice):
        """Evaluate a constant / selector / swissknife node for a selector choice."""
        tag = self.tag(name)
        if tag in ("Enumeration", "Integer", "Float"):
            if name in choice:
                return choice[name]
            return self.const_value(name)
        if tag in ("IntSwissKnife", "SwissKnife"):
            env = {k: self.eval_node(v, choice) for k, v in self.variables(name).items()}
            return Formula(self.text(name, "Formula"), env).value()
        raise LookupError("cannot evaluate %s <%s> statically" % (name, tag))

    # ---- registers ------------------------------------------------------
    def register(self, name):
        """Describe a register node, with one address per selector combination."""
        node = self.nodes[name]
        tag = self.tag(name)
        base = sum(int(e.text, 0) for e in node.findall(NS + "Address"))
        paddr = [e.text.strip() for e in node.findall(NS + "pAddress")]
        selectors = []
        for p in paddr:
            for s in self.free_selectors(p):
                if s not in selectors:
                    selectors.append(s)
        variants = []
        tables = [self.selector_values(s) for s in selectors]
        defaults = []
        for sel, table in zip(selectors, tables):
            # variant 0 is the XML default of a host-side selector
            default = self.text(sel, "Value")
            defaults.append(None if default is None else int(default, 0))
        ordered = [sorted(t.items(), key=lambda kv: (kv[1] != d, kv[1])) for t, d in zip(tables, defaults)]
        for combo in itertools.product(*ordered):
            choice = {s: kv[1] for s, kv in zip(selectors, combo)}
            addr = base + sum(int(self.eval_node(p, choice)) for p in paddr)
            variants.append({"selector": {s: kv[0] for s, kv in zip(selectors, combo)}, "address": addr})
        if self.text(name, "Length") is None:
            raise LookupError("register %s has a computed length (pLength)" % name)
        length = int(self.text(name, "Length"), 0)
        endian = "big" if (self.text(name, "Endianess") or "LittleEndian") == "BigEndian" else "little"
        reg = {
            "node": name,
            "address": variants[0]["address"],
            "length": length,
            "access": self.text(name, "AccessMode") or "RW",
            "endianness": endian,
            "sign": (self.text(name, "Sign") or "Unsigned").lower(),
            "port": self.text(name, "pPort"),
        }
        if selectors:
            reg["selectors"] = selectors
            reg["variants"] = variants
        if tag == "MaskedIntReg":
            bit = self.text(name, "Bit")
            lsb = int(bit if bit is not None else self.text(name, "LSB"))
            msb = int(bit if bit is not None else self.text(name, "MSB"))
            reg["lsb"], reg["msb"] = min(lsb, msb), max(lsb, msb)
        if tag == "StringReg":
            reg["kind"] = "string"
        elif tag == "Register":
            reg["kind"] = "bytes"
        elif tag == "FloatReg":
            reg["kind"] = "float"
        else:
            reg["kind"] = "int"
        return reg

    # ---- value chains ---------------------------------------------------
    def resolve(self, name, depth=0):
        """Follow a node to its register. Returns {"const": v} or
        {"reg": {...}, "conv": {...}} or {"formula": text, "inputs": {...}}."""
        if depth > 12:
            raise LookupError("chain too deep at %s" % name)
        tag = self.tag(name)
        if tag in REGISTER_TAGS:
            return {"reg": self.register(name), "conv": {"kind": "identity"}}
        if tag in ("Integer", "Float", "Boolean", "Enumeration", "Command"):
            pv = self.text(name, "pValue")
            if pv is None:
                out = {"const": self.const_value(name)}
                if tag == "Enumeration" and self.selector_values(name) is not None:
                    out["host_side_selector"] = True
                return out
            return self.resolve(pv, depth + 1)
        if tag in ("Converter", "IntConverter"):
            inner = self.resolve(self.text(name, "pValue"), depth + 1)
            inner["conv"] = self.classify(self.text(name, "FormulaFrom"), "TO", self.variables(name), depth, self.text(name, "FormulaTo"))
            return inner
        if tag in ("SwissKnife", "IntSwissKnife"):
            variables = self.variables(name)
            formula = self.text(name, "Formula")
            # try each variable as the raw value, the other (if any) as scaling register
            for raw_name in variables:
                conv = self.classify(formula, raw_name, {k: v for k, v in variables.items() if k != raw_name}, depth)
                if conv["kind"] != "formula":
                    inner = self.resolve(variables[raw_name], depth + 1)
                    if "reg" in inner and inner["conv"]["kind"] == "identity":
                        inner["conv"] = conv
                        return inner
            if len(variables) == 1 and formula.strip() == next(iter(variables)):
                return self.resolve(next(iter(variables.values())), depth + 1)
            inputs = {}
            for k, v in variables.items():
                try:
                    inputs[k] = self.resolve(v, depth + 1)
                except (LookupError, KeyError, ValueError) as err:
                    inputs[k] = {"unresolved": v, "why": str(err)}
            return {"formula": formula, "inputs": inputs}
        raise LookupError("unsupported node %s <%s>" % (name, tag))

    def classify(self, formula, raw, variables, depth, formula_to=None):
        """Classify a conversion formula numerically: evaluate it at sample points
        and compare with the candidate closed forms."""
        def f(x, aux=None):
            env = {raw: x}
            if aux is not None:
                env[aux[0]] = aux[1]
            return float(Formula(formula, env).value())

        def close(a, b):
            return abs(a - b) <= 1e-9 * max(1.0, abs(a), abs(b))

        samples = (1.0, 3.0, 1000.0, 65536.0)
        try:
            if not variables:
                k = f(1.0)
                if all(close(f(x), k * x) for x in samples):
                    return {"kind": "identity"} if close(k, 1.0) else {"kind": "linear", "scale": k}
            elif len(variables) == 1:
                var, node = next(iter(variables.items()))
                k = f(1.0, (var, 0))
                if all(close(f(x, (var, n)), k * x / 2 ** n) for x in samples for n in (0, 4, 16)):
                    aux = self.resolve(node, depth + 1)
                    return {"kind": "fixed_point", "scale": k, "fraction_bits": aux.get("reg", aux)}
                k = f(1.0, (var, 1))
                if all(close(f(x, (var, n)), k * n / x) for x in samples for n in (1, 1000, 1000000)):
                    aux = self.resolve(node, depth + 1)
                    return {"kind": "reciprocal", "scale": k, "numerator": aux.get("reg", aux)}
        except (KeyError, ValueError, ZeroDivisionError, LookupError):
            pass
        conv = {"kind": "formula", "from_register": formula, "unflattened": True}
        if formula_to is not None:
            conv["to_register"] = formula_to
        if variables:
            conv["variables"] = variables
        return conv

    # ---- feature --------------------------------------------------------
    def feature(self, name):
        tag = self.tag(name)
        out = {"name": name, "type": {"StringReg": "String"}.get(tag, tag)}
        for key in ("Unit", "Representation", "ImposedAccessMode"):
            v = self.text(name, key)
            if v is not None:
                out[key.lower()] = v
        try:
            out["value"] = self.resolve(name)
        except (LookupError, KeyError, ValueError) as err:
            out["value"] = {"unresolved": True, "why": str(err)}
        for key, child in (("min", "pMin"), ("max", "pMax"), ("inc", "pInc")):
            ref = self.text(name, child)
            const = self.text(name, child[1:])
            try:
                if ref is not None:
                    out[key] = self.resolve(ref)
                elif const is not None:
                    out[key] = {"const": float(const) if "." in const else int(const, 0)}
            except (LookupError, KeyError, ValueError) as err:
                out[key] = {"unresolved": True, "why": str(err)}
        for key, child in (("implemented", "pIsImplemented"), ("available", "pIsAvailable"), ("locked", "pIsLocked")):
            ref = self.text(name, child)
            if ref is not None:
                out[key] = self.flag(ref)
        if tag == "Command":
            out["command_value"] = int(self.text(name, "CommandValue") or "1", 0)
        if tag == "Enumeration":
            out["entries"] = []
            for e in self.all(name, "EnumEntry"):
                entry = {"name": e.get("Name"), "value": int(e.find(NS + "Value").text, 0)}
                for key, child in (("implemented", "pIsImplemented"), ("available", "pIsAvailable")):
                    ref = e.find(NS + child)
                    if ref is not None:
                        entry[key] = self.flag(ref.text.strip())
                out["entries"].append(entry)
        selected = [e.text.strip() for e in self.all(name, "pSelected")]
        if selected:
            out["selects"] = selected
        out["selected_by"] = sorted(n for n, e in self.nodes.items() if any(s.text.strip() == name for s in e.findall(NS + "pSelected")))
        reg = out["value"].get("reg")
        access = out.get("imposedaccessmode") or (reg["access"] if reg else "RO")
        if reg and reg["access"] == "RO":
            access = "RO"
        out["access"] = access
        return out

    def flag(self, ref):
        """A boolean condition node: a register bit, or a formula over bits."""
        try:
            r = self.resolve(ref)
        except (LookupError, KeyError, ValueError) as err:
            return {"unresolved": True, "why": str(err)}
        if "reg" in r and r["conv"]["kind"] == "identity":
            return r["reg"]
        if "formula" in r:
            # TLParamsLocked is a host-side variable (set by the host while
            # streaming): keep the formula, expose the register inputs.
            return {"formula": r["formula"], "inputs": {k: (v.get("reg") or v) for k, v in r["inputs"].items()}}
        return r


# --------------------------------------------------------------------------
# Emitters
# --------------------------------------------------------------------------
def reg_type(reg, conv=None):
    if reg["kind"] == "string":
        return "str"
    if reg["kind"] == "bytes":
        return "raw"
    if reg["kind"] == "float":
        return "f%d%s" % (reg["length"] * 8, "be" if reg["endianness"] == "big" else "le")
    prefix = "i" if reg["sign"] == "signed" else "u"
    if reg["length"] == 1:
        return prefix + "8"
    return "%s%d%s" % (prefix, reg["length"] * 8, "be" if reg["endianness"] == "big" else "le")


def tsv_rows(features):
    rows, seen = [], set()

    def add(label, reg):
        variants = reg.get("variants") or [{"selector": {}, "address": reg["address"]}]
        for i, v in enumerate(variants):
            suffix = "".join("[%s]" % e for e in v["selector"].values())
            # the default variant keeps the plain name, the others get [Entry]
            key = label if i == 0 else label + suffix
            if key in seen:
                continue
            seen.add(key)
            rows.append("%s\t0x%08X\t%d\t%s\t%s\t%s\t%s" % (key, v["address"], reg["length"], reg["access"], reg_type(reg),
                                                         reg.get("lsb", "-"), reg.get("msb", "-")))

    for f in features:
        val = f["value"]
        if "reg" in val:
            add(f["name"], val["reg"])
            for key, label in (("fraction_bits", ".fracbits"), ("numerator", ".numerator")):
                aux = val["conv"].get(key)
                if isinstance(aux, dict) and "address" in aux:
                    add(f["name"] + label, aux)
        for key in ("min", "max", "inc"):
            if key in f and "reg" in f[key]:
                add("%s.%s" % (f["name"], key), f[key]["reg"])
        for key in ("implemented", "available", "locked"):
            flag = f.get(key)
            if flag is None:
                continue
            if "address" in flag:
                add("%s.%s" % (f["name"], key), flag)
            for var, reg in (flag.get("inputs") or {}).items():
                if isinstance(reg, dict) and "address" in reg:
                    add("%s.%s.%s" % (f["name"], key, var), reg)
        for e in f.get("entries", []):
            for key in ("implemented", "available"):
                flag = e.get(key)
                if flag is not None and "address" in flag:
                    add("%s.%s.%s" % (f["name"], e["name"], key), flag)
    return rows


def c_ident(text):
    return re.sub(r"[^A-Za-z0-9]", "_", text).upper()


def emit_header(features, model, source_sha256):
    kinds = {"identity": "BFLY_CONV_IDENTITY", "fixed_point": "BFLY_CONV_FIXED_POINT", "linear": "BFLY_CONV_LINEAR", "reciprocal": "BFLY_CONV_RECIPROCAL",
             "formula": "BFLY_CONV_FORMULA"}
    lines = [
        "// GENERATED by genicam/gen_regtable.py - do not edit.",
        "// Source: GenICam XML of %s %s (file version %s.%s.%s), sha256 %s" % (
            model.attrs.get("VendorName"), model.attrs.get("ModelName"), model.attrs.get("MajorVersion"), model.attrs.get("MinorVersion"),
            model.attrs.get("SubMinorVersion"), source_sha256),
        "// All registers are accessed with GenCP READMEM/WRITEMEM at the given address.",
        "#ifndef BLACKFLY_REG_TABLE_H", "#define BLACKFLY_REG_TABLE_H", "", "#include <stdint.h>", "",
        "typedef enum { BFLY_T_INTEGER, BFLY_T_FLOAT, BFLY_T_ENUMERATION, BFLY_T_BOOLEAN, BFLY_T_COMMAND, BFLY_T_STRING } BflyFeatureType;",
        "typedef enum { BFLY_ACC_RO, BFLY_ACC_RW, BFLY_ACC_WO } BflyAccess;",
        "typedef enum {\n    BFLY_CONV_IDENTITY,    // value = raw\n    BFLY_CONV_LINEAR,      // value = raw * scale\n    BFLY_CONV_FIXED_POINT, // value = raw * scale / 2^reg(auxAddress)\n    BFLY_CONV_RECIPROCAL,  // value = scale * reg(auxAddress) / raw\n    BFLY_CONV_FORMULA,     // not flattened: see blackfly_regtable.json\n    BFLY_CONV_CONSTANT     // no register: constant in the XML\n} BflyConversion;",
        "#define BFLY_NO_ADDRESS 0xFFFFFFFFu",
        "#define BFLY_NO_BIT 0xFFu",
        "",
        "// Every feature block on this camera has the same shape: a flags register",
        "// (implemented / available / locked bits) followed by value, min, max, inc.",
        "typedef struct {",
        "    const char* name;",
        "    BflyFeatureType type;",
        "    BflyAccess access;",
        "    uint32_t address;        // value register (selector variant 0), BFLY_NO_ADDRESS for constants",
        "    uint8_t length;          // bytes",
        "    uint8_t lsb, msb;        // bit field inside the register, BFLY_NO_BIT when whole register",
        "    uint8_t isSigned;",
        "    uint8_t bigEndian;",
        "    BflyConversion conversion;",
        "    uint32_t auxAddress;     // fraction-bits register (fixed point) or numerator register (reciprocal)",
        "    double scale;            // see BflyConversion",
        "    uint32_t minAddress, maxAddress, incAddress; // limit registers (same conversion as the value)",
        "    uint32_t flagsAddress;   // register holding implemented/available/locked bits",
        "    uint8_t implementedBit, availableBit, lockedBit;",
        "    uint32_t commandValue;   // value written to execute a command",
        "    const char* selector;    // feature that selects the address variant, or 0",
        "    uint8_t variantCount;    // number of selector variants (0 when no selector)",
        "    uint16_t variantOffset;  // index into BFLY_VARIANTS",
        "    uint8_t entryCount;      // enumeration entries",
        "    uint16_t entryOffset;    // index into BFLY_ENUM_ENTRIES",
        "} BflyFeature;",
        "",
        "typedef struct { const char* name; uint32_t value; uint32_t implementedAddress; uint8_t implementedBit; } BflyEnumEntry;",
        "typedef struct { const char* selectorEntry; uint32_t selectorValue; uint32_t address; } BflyVariant;",
        "",
    ]
    entries, variants, rows, ids = [], [], [], []
    for f in features:
        val = f["value"]
        reg = val.get("reg")
        conv = val.get("conv", {})

        def addr_of(key):
            r = f.get(key, {})
            r = r.get("reg") if "reg" in r else None
            return "0x%08Xu" % r["address"] if r else "BFLY_NO_ADDRESS"

        def flag_bit(key):
            fl = f.get(key)
            if fl is None:
                return None, "BFLY_NO_BIT"
            if "address" in fl:
                return fl["address"], str(fl.get("lsb", "BFLY_NO_BIT"))
            for r in (fl.get("inputs") or {}).values():
                if isinstance(r, dict) and "address" in r:
                    return r["address"], str(r.get("lsb", "BFLY_NO_BIT"))
            return None, "BFLY_NO_BIT"

        flag_addrs = [a for a in (flag_bit(k)[0] for k in ("implemented", "available", "locked")) if a is not None]
        entry_offset = len(entries)
        for e in f.get("entries", []):
            imp = e.get("implemented") or {}
            entries.append('    {"%s", %du, %s, %s},' % (e["name"], e["value"], "0x%08Xu" % imp["address"] if "address" in imp else "BFLY_NO_ADDRESS",
                                                       imp.get("lsb", "BFLY_NO_BIT")))
        variant_offset = len(variants)
        selector = "0"
        nvar = 0
        if reg and reg.get("variants") and len(reg["selectors"]) == 1:
            selector = '"%s"' % reg["selectors"][0]
            sel_values = model.selector_values(reg["selectors"][0])
            for v in reg["variants"]:
                entry = v["selector"][reg["selectors"][0]]
                variants.append('    {"%s", %du, 0x%08Xu},' % (entry, sel_values[entry], v["address"]))
            nvar = len(reg["variants"])
        frac = conv.get("fraction_bits") or conv.get("numerator") or {}
        ids.append("    BFLY_F_%s," % c_ident(f["name"]))
        rows.append(
            '    {"%s", BFLY_T_%s, BFLY_ACC_%s, %s, %d, %s, %s, %d, %d, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %du, %s, %d, %d, %d, %d},' % (
                f["name"], f["type"].upper(), f["access"], "0x%08Xu" % reg["address"] if reg else "BFLY_NO_ADDRESS", reg["length"] if reg else 0,
                reg.get("lsb", "BFLY_NO_BIT") if reg else "BFLY_NO_BIT", reg.get("msb", "BFLY_NO_BIT") if reg else "BFLY_NO_BIT",
                1 if reg and reg["sign"] == "signed" else 0, 1 if reg and reg["endianness"] == "big" else 0,
                kinds.get(conv.get("kind"), "BFLY_CONV_CONSTANT" if "const" in val else "BFLY_CONV_FORMULA"),
                "0x%08Xu" % frac["address"] if "address" in frac else "BFLY_NO_ADDRESS", repr(conv.get("scale", 1.0)),
                addr_of("min"), addr_of("max"), addr_of("inc"), "0x%08Xu" % flag_addrs[0] if flag_addrs else "BFLY_NO_ADDRESS",
                flag_bit("implemented")[1], flag_bit("available")[1], flag_bit("locked")[1], f.get("command_value", 0), selector, nvar,
                variant_offset, len(f.get("entries", [])), entry_offset))
    lines += ["typedef enum {"] + ids + ["    BFLY_F_COUNT", "} BflyFeatureId;", ""]
    lines += ["static const BflyEnumEntry BFLY_ENUM_ENTRIES[] = {"] + entries + ["};", ""]
    lines += ["static const BflyVariant BFLY_VARIANTS[] = {"] + (variants or ['    {0, 0u, BFLY_NO_ADDRESS},']) + ["};", ""]
    lines += ["static const BflyFeature BFLY_FEATURES[BFLY_F_COUNT] = {"] + rows + ["};", "", "#endif", ""]
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("xml")
    ap.add_argument("-o", "--out", default=".")
    ap.add_argument("--features", help="file with feature names (default: built-in flight list)")
    ap.add_argument("--all", action="store_true", help="every feature reachable from the Root category")
    args = ap.parse_args()
    import hashlib
    sha = hashlib.sha256(open(args.xml, "rb").read()).hexdigest()
    model = Model(args.xml)
    names = FEATURES
    if args.features:
        names = open(args.features).read().split()
    if args.all:
        names, stack = [], ["Root"]
        while stack:
            for e in model.all(stack.pop(), "pFeature"):
                n = e.text.strip()
                (stack if model.tag(n) == "Category" else names).append(n)
    missing = [n for n in names if n not in model.nodes]
    for n in missing:
        print("warning: feature %s is not in the XML" % n, file=sys.stderr)
    features = [model.feature(n) for n in names if n in model.nodes]
    doc = {"source": {"file": args.xml, "sha256": sha, **model.attrs}, "features": features}
    json.dump(doc, open(args.out + "/blackfly_regtable.json", "w"), indent=1)
    open(args.out + "/regmap.tsv", "w").write("# name\taddress\tlength\taccess\ttype\tlsb\tmsb  (generated by gen_regtable.py)\n" + "\n".join(tsv_rows(features)) + "\n")
    open(args.out + "/BlackflyRegTable.h", "w").write(emit_header(features, model, sha))
    computed = [f["name"] for f in features if "reg" in f["value"] and f["value"]["conv"]["kind"] != "identity"]
    unresolved = [f["name"] for f in features if "reg" not in f["value"] and "const" not in f["value"]]
    print("%d features, %d computed (flattened), %d not a single register: %s" % (len(features), len(computed), len(unresolved), unresolved))


if __name__ == "__main__":
    main()
