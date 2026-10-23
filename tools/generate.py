#!/usr/bin/env python3
"""Generates the gateway's parameter table, translation table and DBC from one frame specification.

Inputs:
  tools/gateway_frames.json          the output frames and the signals in them (this repository's own spec)
  <ToyotaLens>/data/{engine,ect,abs}.json   the parameter definitions, taken verbatim (never edited here)

Outputs (checked in; regenerate after editing the inputs):
  src/uds/uds_definitions.cpp        UDS_DEFINITIONS[]: every parameter a signal references
  src/translator_table.cpp           OUTPUT_FRAMES[], TRANSLATION_SIGNALS[] and BROADCAST_SIGNALS[]
  gateway.dbc                        the same frames and signals for SavvyCAN / the ECUmaster ADU
  docs/adu-frames.md                 the ADU frames as a readable table

Usage:
  python tools/generate.py [--toyotalens C:/projects/ToyotaLens/data]
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

MODULES = {  # ToyotaLens Definitions/Modules.cs
    "engine": {"requestId": 0x7E0, "name": "Engine"},
    "ect": {"requestId": 0x7E1, "name": "ECT"},
    "abs": {"requestId": 0x7B0, "name": "ABS/VSC/TRAC"},
}

# ToyotaLens Definitions/UnitMap.cs (§6.7): stored unit code -> (unit, scale, offset)
PI = 3.14159265358979323846
UNIT_MAP = {
    0: ("", 1, 0), 1: ("m/s2", 1, 0), 2: ("m/s2", 0.001, 0), 3: ("m/s2", 5.0 / 18.0, 0), 4: ("m/s2", 9.81, 0),
    5: ("deg", 1, 0), 6: ("deg", 180.0 / PI, 0), 7: ("deg/s", 1, 0), 8: ("deg/s", 180.0 / PI, 0),
    9: ("A", 1, 0), 10: ("m3/h", 1, 0), 11: ("uA", 1, 0), 12: ("mA", 1, 0),
    13: ("km", 1, 0), 14: ("cm", 1, 0), 15: ("mm", 1, 0), 16: ("um", 1, 0), 17: ("km", 1, 0),
    18: ("Ah", 1, 0), 19: ("W", 1, 0), 20: ("W", 1000, 0), 21: ("kJ", 1, 0), 22: ("kWh", 1, 0),
    23: ("l", 1, 0), 24: ("ml", 1, 0), 25: ("N", 1000, 0), 26: ("N", 1, 0), 27: ("m3/s", 1, 0),
    28: ("Hz", 1, 0), 29: ("kHz", 1, 0), 30: ("lx", 1, 0), 31: ("us", 1, 0), 32: ("ppm", 1, 0),
    33: ("%", 1, 0), 34: ("kPa", 1, 0), 35: ("kPa", 0.001, 0), 36: ("MPa", 1, 0), 37: ("MPa/s", 1, 0),
    38: ("", 1, 0), 39: ("rpm", 1, 0), 40: ("ohm", 1, 0), 41: ("mohm", 1, 0),
    42: ("km/h", 1, 0), 43: ("cm/s", 1, 0), 44: ("km/h", 3.6, 0),
    45: ("g/s", 1.0 / 3600.0, 0), 46: ("g/s", 1, 0), 47: ("kg", 1, 0), 48: ("V", 1, 0), 49: ("mV", 1, 0),
    50: ("ml", 1, 0), 51: ("kW", 1, 0), 52: ("s", 1, 0), 53: ("min", 1, 0), 54: ("h", 1, 0), 55: ("d", 1, 0),
    56: ("ms", 1, 0), 57: ("degC", 1, 0), 58: ("Nm", 1, 0), 59: ("", 1, 0), 60: ("deg", 1, 0),
    61: ("g/rev", 1, 0), 62: ("l/stroke", 1, 0), 63: ("kPa", 100, 0), 64: ("l/hr", 3.6, 0), 65: ("W/m2", 1, 0),
    66: ("degC", 5.0 / 9.0, -17.77777777777778), 67: ("As", 1, 0), 68: ("kg", 0.001, 0), 69: ("g/s", 5.0 / 18.0, 0),
    70: ("J", 1, 0), 71: ("l/hr", 1, 0), 72: ("kJ", 0.001, 0), 73: ("m/s2", 0.00981, 0), 74: ("kohm", 1, 0),
    75: ("m3/s", 1.0 / 60.0, 0),
}

# Units a broadcast signal may declare -> the stored unit code with scale 1 (the reverse of UNIT_MAP).
UNIT_CODES = {"": 0, "m/s2": 1, "deg": 5, "deg/s": 7, "%": 33, "kPa": 34, "rpm": 39, "km/h": 42, "V": 48, "degC": 57, "Nm": 58}

DATA_TYPES = {"Numeric": "DataType::Numeric", "Flag": "DataType::Flag", "Hex": "DataType::Hex",
              "Ascii": "DataType::Ascii", "Special": "DataType::Special"}


def fail(msg):
    print("generate.py: " + msg, file=sys.stderr)
    sys.exit(1)


def load_toyotalens(data_dir):
    """uniqueId -> parameter entry (with its module key), from the three ToyotaLens files."""
    params = {}
    for module in MODULES:
        path = os.path.join(data_dir, module + ".json")
        if not os.path.exists(path):
            fail("ToyotaLens data file not found: " + path)
        with open(path, encoding="utf-8") as f:
            file = json.load(f)
        if int(file["requestId"], 16) != MODULES[module]["requestId"]:
            fail("%s: requestId %s does not match module %s" % (path, file["requestId"], module))
        for p in file["parameters"]:
            p = dict(p)
            p["module"] = module
            params[p["uniqueId"]] = p
    return params


def load_broadcast(spec, params):
    """The "broadcast" section: frames the IS F modules send on their own, whose signals become parameters of
    module "isf" so an output frame can reference them like a Mode 21 parameter."""
    frames = []
    for fr in spec.get("broadcast", []):
        fr = dict(fr)
        fr["idInt"] = int(fr["id"], 16)
        if fr["idInt"] > 0x7FF:
            fail("broadcast %s: the IS F modules use 11-bit ids" % fr["id"])
        fr["resolved"] = []
        for s in fr["signals"]:
            uid = s["uniqueId"]
            if uid in params:
                fail("broadcast %s: uniqueId %s already defined" % (fr["id"], uid))
            if s.get("order", "big") not in ("big", "little"):
                fail("broadcast %s signal %s: order is 'big' or 'little'" % (fr["id"], uid))
            if s.get("unit", "") not in UNIT_CODES:
                fail("broadcast %s signal %s: unit %r has no unit code; add it to UNIT_CODES" % (fr["id"], uid, s.get("unit")))
            p = {"uniqueId": uid, "name": s["name"], "module": "isf", "isfId": fr["idInt"], "frameName": fr["name"],
                 "startBit": s["startBit"], "bits": s["bits"], "bigEndian": s.get("order", "big") == "big",
                 "signed": bool(s.get("signed", False)), "scaleFactor": s.get("factor", 1.0), "offset": s.get("offset", 0.0),
                 "unitType": UNIT_CODES[s.get("unit", "")], "unit": s.get("unit", "")}
            params[uid] = p
            fr["resolved"].append(p)
        frames.append(fr)
    return frames


def signal_name(text):
    """A DBC identifier: letters, digits, underscore, starting with a letter."""
    s = re.sub(r"[^A-Za-z0-9]+", "_", text).strip("_")
    if not s or not s[0].isalpha():
        s = "S_" + s
    return s


def raw_bits(p):
    """Field width in bits after the mask and bit offset are applied (what the decoder leaves in `raw`)."""
    mask = int(p["mask"], 16)
    value = mask >> p["bitOffset"]
    return max(1, value.bit_length())


def expand_signals(frame, params):
    """Resolves each signal entry of a frame to a concrete (parameter, placement) list."""
    out = []
    for s in frame.get("signals", []):
        if "blockByte" in s:
            b = s["blockByte"]
            module, address, byte = b["module"], b["parameterAddress"].upper(), b["bytePosition"]
            frame_byte = s["frameByte"]
            seen = set()
            for p in params.values():
                if p["module"] != module or p["parameterAddress"].upper() != address or p["bytePosition"] != byte:
                    continue
                if len(p["mask"]) != 2:
                    continue  # multi-byte fields are not what a byte copy is for
                key = (p["mask"], p["bitOffset"], p["name"])
                if key in seen:
                    continue  # ToyotaLens carries duplicate rows for some fields (two uniqueIds, one byte)
                seen.add(key)
                out.append({"param": p, "name": signal_name(p["name"]), "startBit": frame_byte * 8 + p["bitOffset"],
                            "bits": raw_bits(p), "passRaw": True})
            continue
        uid = s["uniqueId"]
        if uid not in params:
            fail("frame %s: uniqueId %s not in ToyotaLens data" % (frame["id"], uid))
        p = params[uid]
        if p["module"] != s["module"]:
            fail("frame %s: %s is a %s parameter, not %s" % (frame["id"], uid, p["module"], s["module"]))
        name = s.get("name") or signal_name(p["name"])
        if "factor" in s:
            if "bits" not in s:
                fail("frame %s signal %s: a converted signal needs 'bits'" % (frame["id"], name))
            out.append({"param": p, "name": name, "startBit": s["startBit"], "bits": s["bits"], "passRaw": False,
                        "factor": s["factor"], "offset": s.get("offset", 0.0), "signed": s.get("signed", False),
                        "unit": s.get("unit", "")})
        else:
            out.append({"param": p, "name": name, "startBit": s["startBit"], "bits": s.get("bits", raw_bits(p)),
                        "passRaw": True})
    return out


def check_layout(frame, signals):
    used = [None] * 64
    for s in signals:
        if s["startBit"] + s["bits"] > 64:
            fail("frame %s: signal %s does not fit (start %d, %d bits)" % (frame["id"], s["name"], s["startBit"], s["bits"]))
        for bit in range(s["startBit"], s["startBit"] + s["bits"]):
            if used[bit] is not None:
                fail("frame %s: signals %s and %s overlap at bit %d" % (frame["id"], used[bit], s["name"], bit))
            used[bit] = s["name"]
    names = [s["name"] for s in signals]
    if len(names) != len(set(names)):
        fail("frame %s: duplicate signal name" % frame["id"])


def dbc_factor_offset(sig):
    """DBC factor and offset so that physical = raw * factor + offset."""
    p = sig["param"]
    if sig["passRaw"]:
        unit, uscale, uoffset = UNIT_MAP.get(p["unitType"], ("", 1, 0))
        return p["scaleFactor"] * uscale, p["offset"] * uscale + uoffset, unit
    return sig["factor"], sig["offset"], sig.get("unit", "")


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def fmt(x):
    return repr(float(x))


def write_definitions(path, used_params):
    lines = ["// GENERATED by tools/generate.py from ToyotaLens data/{engine,ect,abs}.json. Do not edit.",
             "// One row per parameter referenced by tools/gateway_frames.json (ToyotaLens ParameterDefinition, §6.6).",
             "", '#include "uds/uds_mapper.h"', "", "const UdsDefinition UDS_DEFINITIONS[] = {"]
    for p in used_params:
        if p["module"] == "isf":
            continue  # broadcast sources live in BROADCAST_SIGNALS
        mask = bytes.fromhex(p["mask"])
        mask_c = ", ".join("0x%02X" % b for b in mask) + (", 0x00" * (4 - len(mask)))
        lines.append("    {%s, %s, 0x%03X, 0x%02X, %d, %d, {%s}, %d, %s, %s, %s, %d, %s}," % (
            c_str(p["uniqueId"]), c_str(p["name"]), MODULES[p["module"]]["requestId"],
            int(p["parameterAddress"], 16), p["bytePosition"], p["bitOffset"], mask_c, len(mask),
            "true" if p["signed"] else "false", fmt(p["scaleFactor"]), fmt(p["offset"]), p["unitType"],
            DATA_TYPES.get(p["dataType"], "DataType::Numeric")))
    lines += ["};", "", "const size_t UDS_DEFINITIONS_COUNT = sizeof(UDS_DEFINITIONS) / sizeof(UDS_DEFINITIONS[0]);", ""]
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def c_id(fr):
    """The frame id as a C literal: 3 hex digits for an 11-bit id, 8 for a 29-bit one."""
    return ("0x%08X" if fr["extended"] else "0x%03X") % fr["idInt"]


def dbc_id(fr):
    """The DBC message id: a 29-bit id carries the extended flag in bit 31."""
    return fr["idInt"] | (0x80000000 if fr["extended"] else 0)


def write_translator_table(path, frames, broadcast):
    lines = ["// GENERATED by tools/generate.py from tools/gateway_frames.json. Do not edit; edit the JSON and regenerate.",
             "", '#include "isf_broadcast.h"', '#include "message_translator.h"', "", "const OutputFrame OUTPUT_FRAMES[] = {"]
    for fr in frames:
        d = ", ".join("0x%02X" % b for b in fr["defaults"])
        lines.append("    {%s, %s, {%s}, %d, %s, %s}," % (c_id(fr), c_str(fr["name"]), d, fr["interval"],
                                                       "true" if fr["extended"] else "false",
                                                       "true" if fr["isf"] else "false"))
    lines += ["};", "", "const size_t OUTPUT_FRAMES_COUNT = sizeof(OUTPUT_FRAMES) / sizeof(OUTPUT_FRAMES[0]);", "",
              "const TranslationSignal TRANSLATION_SIGNALS[] = {"]
    for fr in frames:
        for s in fr["resolved"]:
            factor, offset, _ = dbc_factor_offset(s)
            signed = s["param"]["signed"] if s["passRaw"] else s.get("signed", False)
            lines.append("    {%s, %s, %s, %d, %d, %s, %s, %s, %s}," % (
                c_str(s["param"]["uniqueId"]), c_id(fr), c_str(s["name"]), s["startBit"], s["bits"],
                "true" if signed else "false", "true" if s["passRaw"] else "false", fmt(factor), fmt(offset)))
    lines += ["};", "", "const size_t TRANSLATION_SIGNALS_COUNT = sizeof(TRANSLATION_SIGNALS) / sizeof(TRANSLATION_SIGNALS[0]);", "",
              "const BroadcastSignal BROADCAST_SIGNALS[] = {"]
    for fr in broadcast:
        for p in fr["resolved"]:
            lines.append("    {0x%03X, {%s, %s, 0x%03X, 0x00, 0, 0, {0x00, 0x00, 0x00, 0x00}, 0, %s, %s, %s, %d, DataType::Numeric}, %d, %d, %s}," % (
                fr["idInt"], c_str(p["uniqueId"]), c_str(p["name"]), fr["idInt"], "true" if p["signed"] else "false",
                fmt(p["scaleFactor"]), fmt(p["offset"]), p["unitType"], p["startBit"], p["bits"],
                "true" if p["bigEndian"] else "false"))
    lines += ["};", "", "const size_t BROADCAST_SIGNALS_COUNT = sizeof(BROADCAST_SIGNALS) / sizeof(BROADCAST_SIGNALS[0]);", ""]
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def write_dbc(path, frames):
    out = ['VERSION "GT86-ISF gateway output frames (generated by tools/generate.py)"', "", "NS_ :",
           "    NS_DESC_", "    CM_", "    BA_DEF_", "    BA_", "    VAL_", "    CAT_DEF_", "    CAT_", "    FILTER",
           "    BA_DEF_DEF_", "    EV_DATA_", "    ENVVAR_DATA_", "    SGTYPE_", "    SGTYPE_VAL_", "    BA_DEF_SGTYPE_",
           "    BA_SGTYPE_", "    SIG_TYPE_REF_", "    VAL_TABLE_", "    SIG_GROUP_", "    SIG_VALTYPE_",
           "    SIGTYPE_VALTYPE_", "    BO_TX_BU_", "    BA_DEF_REL_", "    BA_REL_", "    BA_DEF_DEF_REL_",
           "    BU_SG_REL_", "    BU_EV_REL_", "    BU_BO_REL_", "    SG_MUL_VAL_", "", "BS_:", "",
           "BU_: GATEWAY ADU GT86", ""]
    comments, vals = [], []
    for fr in frames:
        receiver = "GT86" if fr.get("gt86") else "ADU"
        out.append("BO_ %d %s: 8 GATEWAY" % (dbc_id(fr), fr["name"]))
        for s in fr["resolved"]:
            p = s["param"]
            factor, offset, unit = dbc_factor_offset(s)
            signed = p["signed"] if s["passRaw"] else s.get("signed", False)
            if signed:
                lo, hi = -(2 ** (s["bits"] - 1)), 2 ** (s["bits"] - 1) - 1
            else:
                lo, hi = 0, 2 ** s["bits"] - 1
            vmin, vmax = sorted((lo * factor + offset, hi * factor + offset))
            out.append(' SG_ %s : %d|%d@1%s (%s,%s) [%s|%s] "%s" %s' % (
                s["name"], s["startBit"], s["bits"], "-" if signed else "+", fmt(factor).rstrip("0").rstrip("."),
                fmt(offset).rstrip("0").rstrip("."), ("%g" % vmin), ("%g" % vmax), unit, receiver))
            # No per-signal comment: the ECUmaster importer limits a message's DBC text to about 1000 bytes, and
            # docs/adu-frames.md carries the source of every signal instead.
            if s["passRaw"] and p.get("states"):
                pairs = " ".join('%s "%s"' % (k, v.replace('"', "'")) for k, v in sorted(p["states"].items(), key=lambda kv: int(kv[0])))
                vals.append("VAL_ %d %s %s ;" % (dbc_id(fr), s["name"], pairs))
        if fr.get("comment"):
            comments.append('CM_ BO_ %d "%s";' % (dbc_id(fr), fr["comment"].replace('"', "'")))
        out.append("")
    out += comments + [""] + vals + [""]
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))


def write_markdown(path, frames):
    """docs/adu-frames.md: the ADU frames and their signals, for people (the DBC is for machines)."""
    out = ["# ADU frames (generated by tools/generate.py from tools/gateway_frames.json; do not edit)", "",
           "29-bit ids, sent on the GT86 bus and on the IS F bus. Every signal is little-endian and pass-through:",
           "physical = raw x factor + offset, with ToyotaLens's scaling. Source = ToyotaLens module, Mode 21 block and",
           "byte position. At most 8 signals per frame (ECUmaster per-frame limit).", ""]
    for fr in frames:
        if fr.get("gt86"):
            continue
        out.append("## %s  (0x%08X, every %d ms)" % (fr["name"], fr["idInt"], fr["interval"]))
        out.append("")
        out.append("| Signal | Bits | Factor | Offset | Unit | Source |")
        out.append("|---|---|---|---|---|---|")
        for sg in fr["resolved"]:
            p = sg["param"]
            factor, offset, unit = dbc_factor_offset(sg)
            start, bits = sg["startBit"], sg["bits"]
            if bits % 8 == 0 and start % 8 == 0:
                where = "byte %d" % (start // 8) if bits == 8 else "bytes %d-%d" % (start // 8, start // 8 + bits // 8 - 1)
            else:
                where = "byte %d bit %d" % (start // 8, start % 8) + ("-%d" % (start % 8 + bits - 1) if bits > 1 else "")
            states = ""
            if p.get("states"):
                states = " (" + ", ".join("%s=%s" % (k, v) for k, v in sorted(p["states"].items(), key=lambda kv: int(kv[0]))[:4]) + (", ..." if len(p["states"]) > 4 else "") + ")"
            out.append("| %s | %s | %s | %s | %s | %s %s: %s, 21 %s byte %d%s |" % (
                sg["name"], where, ("%g" % factor), ("%g" % offset), unit, p["module"], p["uniqueId"], p["name"],
                p["parameterAddress"].upper(), p["bytePosition"], states))
        out.append("")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--toyotalens", default=None, help="ToyotaLens data directory (default: from gateway_frames.json)")
    ap.add_argument("--spec", default=os.path.join(HERE, "gateway_frames.json"))
    args = ap.parse_args()
    with open(args.spec, encoding="utf-8") as f:
        spec = json.load(f)
    data_dir = args.toyotalens or spec["toyotaLensData"]
    if not os.path.isabs(data_dir):
        data_dir = os.path.normpath(os.path.join(HERE, data_dir))
    params = load_toyotalens(data_dir)
    broadcast = load_broadcast(spec, params)

    frames, used, ids = [], [], set()
    for fr in spec["frames"]:
        fr = dict(fr)
        fr["idInt"] = int(fr["id"], 16)
        fr["extended"] = bool(fr.get("extended", False))
        fr["isf"] = bool(fr.get("isf", False))
        if fr["idInt"] in ids:
            fail("frame id %s defined twice" % fr["id"])
        if fr["extended"] and fr["idInt"] > 0x1FFFFFFF:
            fail("frame %s: a 29-bit id is at most 0x1FFFFFFF" % fr["id"])
        if not fr["extended"] and fr["idInt"] > 0x7FF:
            fail("frame %s: an 11-bit id is at most 0x7FF; set \"extended\": true for a 29-bit id" % fr["id"])
        if fr["isf"] and fr.get("gt86"):
            fail("frame %s: GT86 packets never go onto the ISF bus" % fr["id"])
        ids.add(fr["idInt"])
        defaults = fr.get("defaults", "00 00 00 00 00 00 00 00").split()
        if len(defaults) != 8:
            fail("frame %s: defaults must be 8 bytes" % fr["id"])
        fr["defaults"] = [int(b, 16) for b in defaults]
        fr["resolved"] = expand_signals(fr, params)
        check_layout(fr, fr["resolved"])
        for s in fr["resolved"]:
            if s["param"] not in used:
                used.append(s["param"])
        frames.append(fr)

    write_definitions(os.path.join(ROOT, "src", "uds", "uds_definitions.cpp"), used)
    write_translator_table(os.path.join(ROOT, "src", "translator_table.cpp"), frames, broadcast)
    write_dbc(os.path.join(ROOT, "gateway.dbc"), frames)
    write_markdown(os.path.join(ROOT, "docs", "adu-frames.md"), frames)
    print("generated: %d frames, %d signals, %d parameter definitions, %d broadcast signals" % (
        len(frames), sum(len(f["resolved"]) for f in frames), len([p for p in used if p["module"] != "isf"]),
        sum(len(f["resolved"]) for f in broadcast)))


if __name__ == "__main__":
    main()
