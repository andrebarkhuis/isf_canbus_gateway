#!/usr/bin/env python3
"""ECUmaster ADU project (.adu) editor. See SKILL.md next to this file for the file format and the codes.

  python adu_project.py info        <in.adu>
  python adu_project.py extract     <in.adu> <dir>
  python adu_project.py pack        <dir> <out.adu>
  python adu_project.py import-canx <in.adu> <stream.canx> --bus CAN1|CAN2 [--group NAME] [--channels A,B,..]
                                    [--channels-file F] [--only-overrides] [--names dot|keep] --out <out.adu>
  python adu_project.py set-channel <in.adu> --page PAGE --label "TEXT" --channel NAME [--decimals N] --out <out.adu>
  python adu_project.py replace-channel <in.adu> --from OLD --to NEW --out <out.adu>
  python adu_project.py remove-input <in.adu> --name NAME --out <out.adu>
  python adu_project.py remove-mob  <in.adu> --name NAME --out <out.adu>      (renumbers mobIndex of the inputs)

Edits are plain text edits that keep the client's formatting; the result is checked to be well-formed XML.
"""
import argparse
import os
import re
import sys
import zipfile
import xml.etree.ElementTree as ET

# --- codes: ADU client Xml/ADU/Project/project0_120.xml paramlists --------------------------------------------
DATA_FORMAT = {"u8": 0, "s8": 1, "u16": 2, "s16": 3, "u64": 4, "s64": 5}   # CanbusDataFormat
ENDIAN = {"little": 0, "big": 1}                                            # CanbusDataEndian
WIDTH = {1: 0, 2: 1, 4: 2, 8: 3}                                            # CanbusMobWidth
CANIF = {"CAN1": 0, "CAN2": 1}                                              # Canif
EXT_FLAG = 0x20000000   # a 29-bit canbusID carries this flag (seen on one 29-bit object in a project; inferred)
# (quantity, unit) client codes per unit text, harvested from the override inputs in ECUmaster's own templates
UNIT_CODES = {"": (0, 0), "rpm": (3, 1), "°": (9, 0), "°BTDC": (9, 0), "lambda": (11, 0), "kPa": (16, 1),
              "bar": (16, 3), "%": (17, 0), "°C": (18, 0), "ms": (19, 3), "km/h": (20, 0), "V": (21, 0), "L": (22, 0)}

MOB_VARS = ["name", "groupId", "canbusID", "canifChannel", "type", "width", "timeout", "isExpanded", "muxStartValue",
            "muxEndian", "muxBitCount", "muxBitPosition"]
MOB_TYPES = {"name": "name", "groupId": "u8", "canbusID": "u32", "canifChannel": "Canif", "type": "CanbusMobType",
             "width": "CanbusMobWidth", "timeout": "u16", "isExpanded": "bool", "muxStartValue": "u16",
             "muxEndian": "CanbusDataEndian", "muxBitCount": "u8", "muxBitPosition": "u8"}
INPUT_VARS = ["name", "groupId", "mobIndex", "mobOffset", "byteOffset", "dataFormat", "dataEndian", "bitCount",
              "bitPosition", "multiplier", "divider", "offset", "holdPrevious", "defaultValue", "timeoutValue",
              "overrideChannel", "quantity", "unit", "decimalPlaces"]
INPUT_TYPES = {"name": "name", "groupId": "u8", "mobIndex": "u8", "mobOffset": "u8", "byteOffset": "u8",
               "dataFormat": "CanbusDataFormat", "dataEndian": "CanbusDataEndian", "bitCount": "u8", "bitPosition": "u8",
               "multiplier": "s16", "divider": "s16", "offset": "s16", "holdPrevious": "bool", "defaultValue": "s16",
               "timeoutValue": "s16", "overrideChannel": "channel", "quantity": "u8", "unit": "u8", "decimalPlaces": "u8"}
CAN_INPUT_LIMIT = 100   # ADU manual: up to 100 CAN channels over both buses


def fail(msg):
    print("adu_project.py: " + msg, file=sys.stderr)
    sys.exit(1)


class Project:
    """The project.adu XML inside the .adu zip, edited as text."""

    def __init__(self, path):
        self.path = path
        with zipfile.ZipFile(path) as z:
            self.names = z.namelist()
            self.files = {n: z.read(n) for n in self.names}
        if "project.adu" not in self.files:
            fail("%s: no project.adu inside" % path)
        self.text = self.files["project.adu"].decode("utf-8")

    def save(self, path):
        ET.fromstring(self.text.encode("utf-8"))  # well-formed, or raise
        self.files["project.adu"] = self.text.encode("utf-8")
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
            for n in self.names:
                z.writestr(n, self.files[n])

    # -- arrays and items --------------------------------------------------------------------------------------
    def array_span(self, array_id):
        """(content_start, content_end) of <array id="..."> ... </array>; a self-closing array is opened first."""
        m = re.search(r'([ \t]*)<array id="%s"( type="[^"]*")?\s*(/?)>' % re.escape(array_id), self.text)
        if not m:
            fail("no array %s in the project" % array_id)
        if m.group(3) == "/":
            opened = '%s<array id="%s"%s>\n%s</array>' % (m.group(1), array_id, m.group(2) or "", m.group(1))
            self.text = self.text[:m.start()] + opened + self.text[m.end():]
            m = re.search(r'([ \t]*)<array id="%s"( type="[^"]*")?\s*>' % re.escape(array_id), self.text)
        start = m.end()
        end = self.text.index("</array>", start)
        return start, end

    def items(self, array_id):
        """Top-level <item> blocks of an array: list of (start, end, text), nesting respected."""
        start, end = self.array_span(array_id)
        seg = self.text[start:end]
        out, depth, open_at = [], 0, None
        for m in re.finditer(r"<item\b[^>]*>|</item>", seg):
            if m.group(0).startswith("</"):
                depth -= 1
                if depth == 0:
                    out.append((start + open_at, start + m.end(), seg[open_at:m.end()]))
            else:
                if depth == 0:
                    open_at = m.start()
                depth += 1
        return out

    def item_names(self, array_id):
        return [item_var(t, "name") for _, _, t in self.items(array_id)]

    def append_item(self, array_id, item_text):
        start, end = self.array_span(array_id)
        indent = re.search(r"([ \t]*)</array>", self.text[end - 200:end + 8]).group(1)
        block = "\n".join(indent + "  " + l for l in item_text.strip().splitlines()) + "\n" + indent
        # the closing tag is preceded by its indent; insert before that indent
        k = self.text.rfind("\n", 0, end) + 1
        self.text = self.text[:k] + block + self.text[end:]

    def remove_item(self, array_id, name):
        for s, e, t in self.items(array_id):
            if item_var(t, "name") == name:
                k = self.text.rfind("\n", 0, s)
                self.text = self.text[:k] + self.text[e:]
                return True
        return False

    # -- channels -----------------------------------------------------------------------------------------------
    def logged_has(self, name):
        return ('<channel id="%s" ' % name) in self.text

    def logged_add(self, name):
        if self.logged_has(name):
            return
        k = self.text.index("</loggedChannels>")
        k = self.text.rfind("\n", 0, k) + 1
        self.text = self.text[:k] + '    <channel id="%s" base="" cond2="" cond3="" cond4=""/>\n' % name + self.text[k:]

    def logged_remove(self, name):
        self.text = re.sub(r'\n[ \t]*<channel id="%s" [^>]*/>' % re.escape(name), "", self.text)

    def group_index(self, name, create=True):
        """groupId of a project-tree group: 1-based position in the groups array, 0 = none."""
        if not name:
            return 0
        names = self.item_names("groups")
        if name in names:
            return names.index(name) + 1
        if not create:
            fail("no group %s" % name)
        self.append_item("groups", '<item>\n  <variable id="name" value="%s" type="name"/>\n'
                                   '  <variable id="groupId" value="0" type="u8"/>\n'
                                   '  <variable id="isExpanded" value="1" type="bool"/>\n</item>' % name)
        return len(names) + 1

    def replace_channel(self, old, new):
        pat = 'value="%s" type="channel"' % old
        n = self.text.count(pat)
        self.text = self.text.replace(pat, 'value="%s" type="channel"' % new)
        return n

    # -- pages --------------------------------------------------------------------------------------------------
    def page_span(self, page):
        names = self.item_names("dashboardPages")
        if page not in names:
            fail("no page %s; pages: %s" % (page, names))
        s, e, _ = self.items("dashboardPages")[names.index(page)]
        return s, e

    def set_channel(self, page, label, channel, decimals=None):
        """Points the value item that follows the label (or carries the label as its own text) at `channel`."""
        s, e = self.page_span(page)
        seg = self.text[s:e]
        pending, hits, out, pos = False, 0, [], 0
        for m in re.finditer(r'<item type="eUIText">.*?</item>', seg, re.S):
            block = m.group(0)
            text, chan = item_var(block, "text"), item_var(block, "variableChannel")
            new = block
            if text == label and chan:
                new = set_item_var(new, "variableChannel", channel)
                if decimals is not None:
                    new = set_item_var(new, "decimalPlaces", str(decimals))
                hits += 1
            elif text == label and not chan:
                pending = True
            elif pending and chan and not text:
                new = set_item_var(new, "variableChannel", channel)
                if decimals is not None:
                    new = set_item_var(new, "decimalPlaces", str(decimals))
                pending, hits = False, hits + 1
            elif text:
                pending = False
            out.append(seg[pos:m.start()])
            out.append(new)
            pos = m.end()
        out.append(seg[pos:])
        self.text = self.text[:s] + "".join(out) + self.text[e:]
        return hits

    def page_report(self):
        lines = []
        for s, e, t in self.items("dashboardPages"):
            lines.append("PAGE %s" % item_var(t, "name"))
            pending = None
            for m in re.finditer(r'<item type="(eUI\w+)">(.*?)</item>', t, re.S):
                kind, block = m.group(1), m.group(2)
                text, chan = item_var(block, "text"), item_var(block, "variableChannel")
                vis, col = item_var(block, "visibleChannel"), item_var(block, "colorChannel")
                if kind == "eUIText" and text and not chan:
                    pending = text
                elif kind == "eUIText" and chan:
                    lines.append("   %-26s -> %s (%s dp)" % (text or pending or "?", chan, item_var(block, "decimalPlaces")))
                    pending = None
                extra = [x for x in ((vis if vis not in (None, "", "one") else None), col) if x]
                if extra and not chan:
                    lines.append("   %-26s %s %s" % (text or kind, kind, " ".join(extra)))
        return "\n".join(lines)


def item_var(text, vid):
    m = re.search(r'<variable id="%s" value="([^"]*)"' % vid, text)
    return m.group(1) if m else None


def set_item_var(text, vid, value):
    pat = re.compile(r'(<variable id="%s" value=")([^"]*)(")' % vid)
    if not pat.search(text):
        fail("no variable %s in item" % vid)
    return pat.sub(lambda m: m.group(1) + value + m.group(3), text, count=1)


def make_item(order, types, values):
    lines = ["<item>"]
    for vid in order:
        lines.append('  <variable id="%s" value="%s" type="%s"/>' % (vid, values.get(vid, ""), types[vid]))
    lines.append("</item>")
    return "\n".join(lines)


# --- CANX import ----------------------------------------------------------------------------------------------
def import_canx(p, canx_path, bus, group, select=None, only_overrides=False, names="dot"):
    root = ET.parse(canx_path).getroot()
    group_id = p.group_index(group) if group else 0
    bus_code = CANIF[bus]
    existing_mobs = p.item_names("canbusMobs")
    existing_inputs = p.item_names("canbusInputs")
    unknown_units, added_mobs, added_inputs, skipped = set(), [], [], []
    for mob in root.findall("mob"):
        can_id = int(mob.get("canbusID"), 16)
        ext = mob.get("ext") == "1"
        width = int(mob.get("width", "1"))
        frames = []
        for fr in mob.findall("frame"):
            chans = []
            for ch in fr.findall("channel"):
                cid, override = ch.get("id"), ch.get("override")
                if select is not None and cid not in select:
                    continue
                if only_overrides and not override:
                    continue
                if cid in existing_inputs:
                    skipped.append(cid)
                    continue
                chans.append(ch)
            if chans:
                frames.append((int(fr.get("offset", "0")), chans))
        if not frames:
            continue
        mob_name = "m_%s%X" % ("x" if ext else "", can_id)
        if mob_name in existing_mobs:
            fail("message object %s already exists; remove it first (remove-mob)" % mob_name)
        mob_index = len(p.item_names("canbusMobs"))
        p.append_item("canbusMobs", make_item(MOB_VARS, MOB_TYPES, {
            "name": mob_name, "groupId": group_id, "canbusID": can_id | (EXT_FLAG if ext else 0),
            "canifChannel": bus_code, "type": 0, "width": WIDTH[width], "timeout": 100, "isExpanded": 0,
            "muxStartValue": 0, "muxEndian": 1, "muxBitCount": 8, "muxBitPosition": 56}))
        added_mobs.append(mob_name)
        for offset, chans in frames:
            for ch in chans:
                kind = ch.get("type")
                base, _, order = kind.partition("-")
                if base not in DATA_FORMAT:
                    fail("channel %s: type %s is not one the ADU input knows" % (ch.get("id"), kind))
                unit = ch.get("unit", "")
                if unit not in UNIT_CODES:
                    unknown_units.add(unit)
                q, u = UNIT_CODES.get(unit, (0, 0))
                override = ch.get("override") or ""
                name = "." if (override and names == "dot") else ch.get("id")
                p.append_item("canbusInputs", make_item(INPUT_VARS, INPUT_TYPES, {
                    "name": name, "groupId": group_id, "mobIndex": mob_index, "mobOffset": offset,
                    "byteOffset": ch.get("byteOffset", "0"), "dataFormat": DATA_FORMAT[base],
                    "dataEndian": ENDIAN["big" if order == "be" else "little"],
                    "bitCount": ch.get("bitCount") or {"u8": 8, "s8": 8, "u16": 16, "s16": 16, "u64": 64, "s64": 64}[base],
                    "bitPosition": ch.get("bitPosition", "0"), "multiplier": ch.get("multiplier", "1"),
                    "divider": ch.get("divider", "1"), "offset": ch.get("offset", "0"), "holdPrevious": 1,
                    "defaultValue": 0, "timeoutValue": 0, "overrideChannel": override, "quantity": q, "unit": u,
                    "decimalPlaces": ch.get("decimalPlaces", "0")}))
                if name != ".":
                    p.logged_add(name)
                added_inputs.append("%s%s" % (ch.get("id"), " -> " + override if override else ""))
    total = len(p.item_names("canbusInputs"))
    print("added %d message objects: %s" % (len(added_mobs), " ".join(added_mobs)))
    print("added %d CAN inputs:\n  " % len(added_inputs) + "\n  ".join(added_inputs))
    if skipped:
        print("already in the project, skipped: " + " ".join(skipped))
    if unknown_units:
        print("WARNING: no client code for unit(s) %s; those inputs got no unit, set it in the client" % sorted(unknown_units))
    print("CAN inputs in the project now: %d (ADU limit %d)%s" % (total, CAN_INPUT_LIMIT, " OVER THE LIMIT" if total > CAN_INPUT_LIMIT else ""))


def remove_mob(p, name):
    names = p.item_names("canbusMobs")
    if name not in names:
        fail("no message object %s" % name)
    k = names.index(name)
    # inputs on it go, inputs on later objects move down one
    for s, e, t in reversed(p.items("canbusInputs")):
        idx = int(item_var(t, "mobIndex") or 0)
        if idx == k:
            if item_var(t, "name") != ".":
                p.logged_remove(item_var(t, "name"))
            start = p.text.rfind("\n", 0, s)
            p.text = p.text[:start] + p.text[e:]
        elif idx > k:
            p.text = p.text[:s] + set_item_var(t, "mobIndex", str(idx - 1)) + p.text[e:]
    p.remove_item("canbusMobs", name)


def info(p):
    print("project %s: %s" % (p.path, re.search(r"<project [^>]*>", p.text).group(0)))
    print("groups:", p.item_names("groups"))
    mobs = p.items("canbusMobs")
    print("message objects (%d):" % len(mobs))
    for i, (_, _, t) in enumerate(mobs):
        cid = int(item_var(t, "canbusID"))
        print("  [%d] %-26s id 0x%X%s %s width %s" % (i, item_var(t, "name"), cid & ~EXT_FLAG, " ext" if cid & EXT_FLAG else "",
              "CAN2" if item_var(t, "canifChannel") == "1" else "CAN1", [1, 2, 4, 8][int(item_var(t, "width") or 0)]))
    inputs = p.items("canbusInputs")
    print("CAN inputs (%d of %d):" % (len(inputs), CAN_INPUT_LIMIT))
    for _, _, t in inputs:
        ov = item_var(t, "overrideChannel")
        print("  %-28s mob %s+%s byte %s bits %s%s" % (item_var(t, "name"), item_var(t, "mobIndex"), item_var(t, "mobOffset"),
              item_var(t, "byteOffset"), item_var(t, "bitCount"), " -> " + ov if ov else ""))
    for arr in ("analogInputs", "digitalInputs", "numbers", "functions", "enumerations"):
        print("%s: %s" % (arr, p.item_names(arr)))
    print(p.page_report())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("info"); s.add_argument("adu")
    s = sub.add_parser("extract"); s.add_argument("adu"); s.add_argument("dir")
    s = sub.add_parser("pack"); s.add_argument("dir"); s.add_argument("out")
    s = sub.add_parser("import-canx"); s.add_argument("adu"); s.add_argument("canx")
    s.add_argument("--bus", required=True, choices=["CAN1", "CAN2"]); s.add_argument("--group", default="")
    s.add_argument("--channels", default=""); s.add_argument("--channels-file", default="")
    s.add_argument("--only-overrides", action="store_true"); s.add_argument("--names", default="dot", choices=["dot", "keep"])
    s.add_argument("--out", required=True)
    s = sub.add_parser("set-channel"); s.add_argument("adu"); s.add_argument("--page", required=True)
    s.add_argument("--label", required=True); s.add_argument("--channel", required=True); s.add_argument("--decimals", type=int)
    s.add_argument("--out", required=True)
    s = sub.add_parser("replace-channel"); s.add_argument("adu"); s.add_argument("--from", dest="old", required=True)
    s.add_argument("--to", dest="new", required=True); s.add_argument("--out", required=True)
    s = sub.add_parser("remove-input"); s.add_argument("adu"); s.add_argument("--name", required=True); s.add_argument("--out", required=True)
    s = sub.add_parser("remove-mob"); s.add_argument("adu"); s.add_argument("--name", required=True); s.add_argument("--out", required=True)
    a = ap.parse_args()

    if a.cmd == "extract":
        with zipfile.ZipFile(a.adu) as z:
            z.extractall(a.dir)
        print("extracted to", a.dir)
        return
    if a.cmd == "pack":
        names = sorted(os.listdir(a.dir))
        if "project.adu" not in names:
            fail("no project.adu in " + a.dir)
        ET.parse(os.path.join(a.dir, "project.adu"))
        with zipfile.ZipFile(a.out, "w", zipfile.ZIP_DEFLATED) as z:
            for n in ["project.adu"] + [n for n in names if n != "project.adu"]:
                z.write(os.path.join(a.dir, n), n)
        print("written", a.out)
        return

    p = Project(a.adu)
    if a.cmd == "info":
        info(p)
        return
    if a.cmd == "import-canx":
        select = None
        if a.channels or a.channels_file:
            select = set(x.strip() for x in a.channels.split(",") if x.strip())
            if a.channels_file:
                select |= set(l.strip() for l in open(a.channels_file, encoding="utf-8") if l.strip() and not l.startswith("#"))
        import_canx(p, a.canx, a.bus, a.group, select, a.only_overrides, a.names)
    elif a.cmd == "set-channel":
        n = p.set_channel(a.page, a.label, a.channel, a.decimals)
        print("%d item(s) on %s now show %s" % (n, a.page, a.channel))
        if not n:
            fail("label %r not found on page %s" % (a.label, a.page))
    elif a.cmd == "replace-channel":
        print("%d reference(s) %s -> %s" % (p.replace_channel(a.old, a.new), a.old, a.new))
    elif a.cmd == "remove-input":
        if not p.remove_item("canbusInputs", a.name):
            fail("no CAN input " + a.name)
        p.logged_remove(a.name)
        print("removed input", a.name)
    elif a.cmd == "remove-mob":
        remove_mob(p, a.name)
        print("removed message object", a.name, "and its inputs")
    p.save(a.out)
    print("written", a.out)


if __name__ == "__main__":
    main()
