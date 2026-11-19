"""The byte and relocation comparison of the research harness
(tools/mwccpsp/harness/build.py's compare and check_relocs).

A function of ours matches when its bytes, up to trailing NOP padding, equal
the original's at the address its name has, with every relocation slot
masked, and every relocation also points at the right thing: a named target
must resolve to the address the original's slot holds; a reference to an
anonymous section (string literals, constant tables, jump tables) is accepted
only if that section is read-only and the referenced item equals the
original's bytes there (jump table entries landing at the same offsets in
the function); anonymous writable data is refused, except a C string literal
(-strings noreadonly), compared the same way."""

import re
import struct

from coff import REL_DIR32, SCN_MEM_WRITE


def c_name(sym):
    """COFF name -> C name: strip the cdecl underscore and @N stdcall suffix."""
    n = sym[1:] if sym.startswith("_") else sym
    return re.sub(r"@\d+$", "", n)


def functions_of(o):
    """(symbol, section, start, end) for each function symbol."""
    by_sec = {}
    for s in o.symbols.values():
        if s.defined and s.cls in (2, 3) and o.section(s.section).code and not s.name.startswith((".", "@")):
            if s.cls == 3 and s.value == 0 and s.name == o.section(s.section).name:
                continue
            by_sec.setdefault(s.section, []).append(s)
    for sec, syms in by_sec.items():
        syms = sorted(syms, key=lambda s: s.value)
        size = o.section(sec).size
        for i, s in enumerate(syms):
            end = syms[i + 1].value if i + 1 < len(syms) else size
            yield s, sec, s.value, end


def pattern(data, masked):
    out, i = b"", 0
    while i < len(data):
        if i in masked:
            j = i
            while j < len(data) and j in masked:
                j += 1
            out += b".{%d}" % (j - i)
            i = j
        else:
            out += re.escape(data[i : i + 1])
            i += 1
    return re.compile(out, re.S)


def load_symbols(path):
    """{name: address} from `name = 0xADDRESS;` lines."""
    out = {}
    for line in open(path):
        line = line.split("//")[0].strip()
        m = re.match(r"([\w./+$?@-]+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;", line)
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out


class Resolver:
    """Where a name lives in the original. `addr_name`: the suffix that binds
    a name to an address by itself (func_XXXXXXXX; _b69_XXXXXXXX in the b69
    line). `amap`: for mwccps2 b210, whose sources carry the PSP compiler's
    addresses in such names, the PSP -> b210 address map."""

    def __init__(self, addr_name=r"_([0-9A-Fa-f]{8})$", amap=None):
        self.addr_name = re.compile(addr_name)
        self.amap = amap

    def _plain(self, name, symbols, unit=None):
        if unit and f"{unit}/{name}" in symbols:
            return symbols[f"{unit}/{name}"]
        if name in symbols:
            return symbols[name]
        # a function-local static (name$NNN): the number changes whenever the
        # unit's declarations do, so pins may leave it out
        base = re.sub(r"\$\d+$", "", name)
        if unit and base != name and f"{unit}/{base}" in symbols:
            return symbols[f"{unit}/{base}"]
        m = self.addr_name.search(name)
        return int(m.group(1), 16) if m else None

    def original_address(self, name, symbols, unit=None):
        if self.amap is None or (unit and f"{unit}/{name}" in symbols) or name in symbols:
            return self._plain(name, symbols, unit)
        a = self._plain(name, symbols, unit)
        return self.amap(a) if a is not None else None


def check_relocs(o, unit, s, start, relocs, seg, addr, symbols, pe, res):
    """'' if every relocation in a byte-matching function also points at the
    right thing in the original, else a reason."""
    for off, si, typ in relocs:
        t = o.symbols[si]
        anon = t.name.startswith((".", "$", "@", "??_C")) or (
            t.defined and t.value == 0 and o.section(t.section).name == t.name
        )
        if anon:
            if not t.defined:
                return f"anon undefined {t.name}"
            ts = o.section(t.section)
            if ts is s:
                continue  # same section (jump to a label, local table)
            if ts.code or ts.bss:
                return f"anon writable {ts.name}"
            # a string literal in writable data (-strings noreadonly) is
            # compared like a read-only one; any other writable item is refused
            writable = bool(ts.flags & SCN_MEM_WRITE)
            if typ != REL_DIR32 or pe is None:
                return f"anon {ts.name}"
            addend = struct.unpack_from("<i", s.data, start + off)[0]
            slot = struct.unpack_from("<I", seg, off)[0]
            base = (slot - addend - t.value) & 0xFFFFFFFF
            # compare only the referenced item: up to the next offset this
            # object references in the section (or a string's NUL)
            item = t.value + addend
            starts = sorted(
                {
                    o.symbols[x[1]].value + struct.unpack_from("<i", sec.data, x[0])[0]
                    for sec in o.sections
                    for x in sec.relocs
                    if x[2] == REL_DIR32 and o.symbols[x[1]].defined and o.symbols[x[1]].section == ts.index
                }
                | {item}
            )
            end = next((x for x in starts if x > item), ts.size)
            nul = ts.data.find(b"\0", item)
            cstring = 0 <= nul < end and all(32 <= c < 127 or c in (9, 10, 13) for c in ts.data[item:nul])
            if cstring:
                end = nul + 1  # a C string
            elif writable:
                return f"anon writable {ts.name}"
            try:
                theirs = pe.read(base + item, end - item)
            except Exception:
                return f"anon {ts.name} unreadable"
            masked = {r + k for r, _, _ in ts.relocs for k in range(4)}
            if any(ts.data[i] != theirs[i - item] for i in range(item, end) if i not in masked):
                return f"anon data differs {ts.name}+{item:#x}"
            # jump tables: an entry that points back into this function must
            # land at the same offset in the original
            byoff = {r: (rsi, rtyp) for r, rsi, rtyp in ts.relocs}
            r = item
            while r in byoff and byoff[r][1] == REL_DIR32:
                rt = o.symbols[byoff[r][0]]
                if not rt.defined or rt.section != s.index:
                    break
                ours = rt.value + struct.unpack_from("<i", ts.data, r)[0] - start
                if not (0 <= ours < len(seg)):
                    break
                try:
                    orig = struct.unpack_from("<I", pe.read(base + r, 4))[0] - addr
                except Exception:
                    return f"anon {ts.name} unreadable"
                if ours != orig:
                    return f"jump table differs {ts.name}+{r:#x} entry {(r - item) // 4}"
                r += 4
            continue
        name = c_name(t.name)
        target = symbols.get(f"{unit}/{name}") if t.cls == 3 else None
        target = target if target is not None else res.original_address(name, symbols, unit if t.cls == 3 else None)
        if target is None:
            return f"unknown {name}"
        addend = struct.unpack_from("<i", s.data, start + off)[0]
        slot = struct.unpack_from("<I", seg, off)[0]
        if typ == REL_DIR32:
            ok = slot == (target + addend) & 0xFFFFFFFF
        else:
            ok = (addr + off + 4 + struct.unpack_from("<i", seg, off)[0] - addend) & 0xFFFFFFFF == target
        if not ok:
            return f"{name}+{addend}"
    return ""


def _jump_table_item(o, t, addend):
    """Pro 5.3 (MSL, runtime) puts jump tables (`@N`) among the real statics
    of the unit's one writable .data section: True if the referenced item (up
    to the next offset the object refers to in that section) is nothing but
    DIR32 slots to one code symbol."""
    ts = o.section(t.section)
    item = t.value + addend
    starts = sorted(
        {
            o.symbols[x[1]].value + struct.unpack_from("<i", sec.data, x[0])[0]
            for sec in o.sections
            for x in sec.relocs
            if x[2] == REL_DIR32 and o.symbols[x[1]].defined and o.symbols[x[1]].section == ts.index
        }
        | {item}
    )
    end = next((x for x in starts if x > item), ts.size)
    byoff = {r: (si, typ) for r, si, typ in ts.relocs}
    if end <= item or (end - item) % 4:
        return False
    names = set()
    for r in range(item, end, 4):
        if r not in byoff or byoff[r][1] != REL_DIR32:
            return False
        rt = o.symbols[byoff[r][0]]
        if not (rt.defined and rt.section > 0 and o.section(rt.section).code):
            return False
        names.add(rt.name)
    return len(names) == 1


def check_relocs_pro53(o, unit, s, start, relocs, seg, addr, symbols, pe, res):
    """check_relocs, with a writable section's `@N` item that is a jump table
    (_jump_table_item) treated as read-only for the check (the research
    tree's tools/mwccps2/b69/sections.py)."""
    flip = []
    for off, si, typ in relocs:
        t = o.symbols[si]
        if not (t.defined and t.section > 0 and t.name.startswith("@")):
            continue
        ts = o.section(t.section)
        if ts.code or ts.bss or not ts.flags & SCN_MEM_WRITE:
            continue
        if typ != REL_DIR32 or not _jump_table_item(o, t, struct.unpack_from("<i", s.data, start + off)[0]):
            return check_relocs(o, unit, s, start, relocs, seg, addr, symbols, pe, res)
        flip.append(ts)
    for ts in flip:
        ts.flags &= ~SCN_MEM_WRITE
    try:
        return check_relocs(o, unit, s, start, relocs, seg, addr, symbols, pe, res)
    finally:
        for ts in flip:
            ts.flags |= SCN_MEM_WRITE


def compare(objects, symbols, pe, res, check=check_relocs):
    """objects: [(Object, unit)]. Returns [(name, address, size, status,
    object path)] for every function of ours whose name places it in the
    original's .text; status is `match`, `differs` or `reloc:<why>`."""
    _, tva, tvs, traw, _ = pe.section(".text")
    text = bytes(pe.data[traw : traw + tvs])
    lo = pe.base + tva
    rows = []
    for o, unit in objects:
        for sym, sec, start, end in functions_of(o):
            name = c_name(sym.name)
            key = f"{unit}/{name}" if sym.cls == 3 else name
            addr = symbols.get(key, symbols.get(name))
            if addr is None:
                addr = res.original_address(name, {})  # func_XXXXXXXX
            if addr is None or not lo <= addr < lo + len(text):
                continue
            s = o.section(sec)
            body = s.data[start:end].rstrip(b"\x90")
            relocs = [(off - start, si, typ) for off, si, typ in s.relocs if start <= off < start + len(body)]
            masked = {r + k for r, _, _ in relocs for k in range(4)}
            seg = text[addr - lo : addr - lo + len(body)]
            same = len(seg) == len(body) and pattern(body, masked).fullmatch(seg) is not None
            status = "match" if same else "differs"
            if same:
                why = check(o, unit, s, start, relocs, seg, addr, symbols, pe, res)
                if why:
                    status = "reloc:" + why
            rows.append((key, addr, len(body), status, o.path))
    return rows
