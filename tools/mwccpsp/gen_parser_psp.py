#!/usr/bin/env python3
"""Generate src/native/parser_tables.c from the original exe: the PSP
command-line parser's data (mweppc's .opt-generated ParserGlue tables).

    gen_parser_psp.py

The three ParserTool records (parser_mw, parser_gcc, parser_dispatch) are
walked from the exe: their preference panel names, the preference data
panels with their default contents, and the option lists with every Option
and PARAM_T chain. The hybrid build uses the exe's own copies; the native
build has pointers of 8 bytes, so the tables are written out as typed C.

Pointers become:
  strings          C string literals
  panel contents   addresses inside the panel arrays defined here
  other data       the symbol (config/PSP_3_0_1/symbols*.txt) plus an offset, or
                   data_XXXXXXXX (native_glue.py supplies those)
  functions        the symbol, or func_XXXXXXXX
All references are bound with asm labels, so the C types in our headers do
not matter here.
"""

import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))
REPO = TOP
sys.path.insert(0, HERE)
from pe import PE  # noqa: E402

ROOTS = [("parser_mw", 0x5BDA04), ("parser_gcc", 0x5BDA8C), ("parser_dispatch", 0x5BDB04)]

# PARAM_T layouts (pack(1), 32-bit): (C type, [(field, offset, kind)])
# kinds: b byte, h int16, w int32, s string, v data pointer, f function,
# p PARAM_T pointer
HEAD = [("which", 0, "b"), ("flags", 1, "b"), ("myname", 2, "s"), ("next", 6, "p")]
PARAMS = {
    0: ("PARAM_T", []),
    1: ("FTYPE_T", [("fc", 10, "v"), ("iscreator", 14, "b")]),
    2: ("FILEPATH_T", [("fflags", 10, "b"), ("defaultstr", 11, "s"), ("filename", 15, "v"), ("maxlen", 19, "w")]),
    3: ("NUM_T", [("size", 10, "b"), ("fit", 11, "b"), ("lo", 12, "w"), ("hi", 16, "w"), ("num", 20, "v")]),
    4: ("STRING_T", [("maxlen", 10, "h"), ("pstring", 12, "b"), ("str", 13, "v")]),
    5: ("STRING_T", [("maxlen", 10, "h"), ("pstring", 12, "b"), ("str", 13, "v")]),
    6: ("STRING_T", [("maxlen", 10, "h"), ("pstring", 12, "b"), ("str", 13, "v")]),
    7: ("ONOFF_T", [("var", 10, "v")]),
    8: ("OFFON_T", [("var", 10, "v")]),
    9: ("MASK_T", [("size", 10, "b"), ("ormask", 11, "w"), ("andmask", 15, "w"), ("num", 19, "v")]),
    10: ("TOGGLE_T", [("size", 10, "b"), ("mask", 11, "w"), ("num", 15, "v")]),
    11: ("SET_T", [("size", 10, "b"), ("value", 11, "w"), ("num", 15, "v")]),
    12: ("SETSTRING_T", [("value", 10, "s"), ("pstring", 14, "b"), ("var", 15, "v")]),
    13: ("GENERIC_T", [("parse", 10, "f"), ("var", 14, "v"), ("help", 18, "s")]),
    14: ("IFARG_T", [("parg", 10, "p"), ("helpa", 14, "s"), ("pnone", 18, "p"), ("helpn", 22, "s")]),
    15: ("SETTING_T", [("parse", 10, "f"), ("valuename", 14, "s")]),
}


def load_symbols():
    syms = {}
    for f in ("symbols.guess.txt", "symbols.txt", "symbols.manual.txt"):
        for line in open(os.path.join(HERE, f)):
            m = re.match(r"\s*([^=\s]+)\s*=\s*0x([0-9A-Fa-f]+)", line)
            if m:
                syms[int(m.group(2), 16)] = m.group(1).split("/")[-1]
    return syms


def literal(b):
    s = []
    for c in b:
        ch = chr(c)
        if ch == "\\":
            s.append("\\\\")
        elif ch == '"':
            s.append('\\"')
        elif ch == "\n":
            s.append("\\n")
        elif ch == "\t":
            s.append("\\t")
        elif 32 <= c < 127:
            s.append(ch)
        else:
            s.append(f"\\{c:03o}")
    return '"' + "".join(s) + '"'


class Gen:
    def __init__(self):
        self.pe = PE(os.path.join(REPO, "orig", "PSP_3_0_1", "mwccpsp.exe"))
        self.syms = load_symbols()
        self.panels = {}  # data address -> (name, size)
        self.refs = {}  # C identifier -> asm symbol
        self.decls = []  # forward declarations
        self.defs = []  # definitions
        self.seen = set()
        self.statics = set()

    def read(self, va, n):
        # all-zero initialised structs (some PARAM_Ts) were put in .bss
        if self.section(va) == ".bss":
            return bytes(n)
        return self.pe.read(va, n)

    def u8(self, va):
        return self.read(va, 1)[0]

    def u16(self, va):
        return struct.unpack("<h", self.read(va, 2))[0]

    def u32(self, va):
        return struct.unpack("<I", self.read(va, 4))[0]

    def cstr(self, va):
        if not va:
            return "NULL"
        out = bytearray()
        while True:
            c = self.read(va + len(out), 1)[0]
            if c == 0:
                return literal(bytes(out))
            out.append(c)

    def ref(self, name, kind):
        ident = f"r_{name}".replace("$", "_")
        self.refs[ident] = (name, kind)
        return ident

    # structs our source defines whose fields options point at
    STRUCTS = {"parseopts": 0x400}

    def symbol_at(self, va):
        """name and offset of the data at va"""
        if va in self.syms:
            return self.syms[va], 0
        for a, name in self.syms.items():
            if name in self.STRUCTS and a <= va < a + self.STRUCTS[name]:
                return name, va - a
        return f"data_{va:08X}", 0

    def string_at(self, va):
        """a C literal when va holds an unnamed NUL-terminated string"""
        if va in self.syms or self.section(va) not in (".data", ".rdata"):
            return None
        b = bytearray()
        while len(b) < 512:
            c = self.read(va + len(b), 1)[0]
            if c == 0:
                break
            if not (32 <= c < 127 or c in (9, 10)):
                return None
            b.append(c)
        return literal(bytes(b))

    def var(self, va, string_ok=False):
        if va == 0:
            return "NULL"
        if va < 0x10000:
            return f"(void*){va}"
        if string_ok:
            lit = self.string_at(va)
            if lit is not None:
                return f"(void*){lit}"
        for base, (name, size) in self.panels.items():
            if base <= va < base + size:
                return f"(void*)({name}_panel + {va - base})"
        sec = self.section(va)
        if sec == ".text":
            return f"(void*){self.func(va)}"
        name, off = self.symbol_at(va)
        r = self.ref(name, "data")
        return f"(void*)({r} + {off})" if off else f"(void*){r}"

    def func(self, va):
        if va == 0:
            return "NULL"
        name = self.syms.get(va, f"func_{va:08X}")
        return self.ref(name, "func")

    def section(self, va):
        for n, s, vs, raw, rs in self.pe.sections:
            if self.pe.base + s <= va < self.pe.base + s + vs:
                return n
        return "?"

    # the option graph

    def param(self, va):
        if not va:
            return "NULL"
        name = f"p_{va:06X}"
        if va not in self.seen:
            self.seen.add(va)
            which = self.u8(va)
            ctype, fields = PARAMS[which]
            self.decls.append(f"static {ctype} {name};")
            vals = []
            for fname, off, kind in HEAD + fields:
                if which == 13 and fname == "var":
                    # GENERIC_T's var is often the text its handler adds
                    vals.append(f".var = {self.var(self.u32(va + off), string_ok=True)}")
                    continue
                vals.append(f".{fname} = {self.field(va + off, kind)}")
            self.defs.append(f"static {ctype} {name} = {{{', '.join(vals)}}};")
        return f"(PARAM_T*)&{name}"

    def field(self, va, kind):
        if kind == "b":
            return str(self.u8(va))
        if kind == "h":
            return str(self.u16(va))
        if kind == "w":
            return f"0x{self.u32(va):X}"
        p = self.u32(va)
        if kind == "s":
            return self.cstr(p)
        if kind == "v":
            return self.var(p)
        if kind == "f":
            return f"(void*){self.func(p)}" if p else "NULL"
        if kind == "p":
            return self.param(p)
        raise ValueError(kind)

    def optlist(self, va):
        if not va:
            return "NULL"
        name = f"ol_{va:06X}"
        if va not in self.seen:
            self.seen.add(va)
            self.decls.append(f"static OptionList {name};")
            help_, flags, lst = self.cstr(self.u32(va)), self.u32(va + 4), self.u32(va + 8)
            self.defs.append(f"static OptionList {name} = {{{help_}, 0x{flags:X}, {self.optarray(lst)}}};")
        return f"&{name}"

    def optarray(self, va):
        if not va:
            return "NULL"
        name = f"oa_{va:06X}"
        if va not in self.seen:
            self.seen.add(va)
            items = []
            i = 0
            while True:
                o = self.u32(va + 4 * i)
                if not o:
                    break
                items.append(self.option(o))
                i += 1
            self.decls.append(f"static Option* {name}[{len(items) + 1}];")
            self.defs.append(f"static Option* {name}[{len(items) + 1}] = {{{', '.join(items + ['NULL'])}}};")
        return name

    def option(self, va):
        name = f"o_{va:06X}"
        if va not in self.seen:
            self.seen.add(va)
            self.decls.append(f"static Option {name};")
            names = self.cstr(self.u32(va))
            avail = self.u32(va + 4)
            param = self.param(self.u32(va + 8))
            sub = self.optlist(self.u32(va + 12))
            conf = self.optlist(self.u32(va + 16))
            help_ = self.cstr(self.u32(va + 20))
            self.defs.append(f"static Option {name} = {{{names}, 0x{avail:X}, {param}, {sub}, {conf}, {help_}}};")
        return f"&{name}"

    # the tools

    def tool(self, cname, va):
        f = [self.u32(va + 4 * i) for i in range(15)]
        typ, lang, cpu, os_, npanels, panels, info, copyright_, nlists, lists, ndata, data, pre, mid, post = f
        pname = self.strarray(panels, npanels)
        lname = self.listarray(lists, nlists)
        dname = self.dataarray(data, ndata)
        self.defs.append(
            f'ParserTool {cname}_tables __asm__(SYM("{cname}")) = {{0x{typ:08X}, 0x{lang:08X}, 0x{cpu:08X}, '
            f"0x{os_:08X}, {npanels}, {pname}, {self.cstr(info)}, {self.cstr(copyright_)}, {nlists}, {lname}, "
            f"{ndata}, {dname}, (void*){self.func(pre)}, (void*){self.func(mid)}, (void*){self.func(post)}}};"
        )

    def strarray(self, va, n):
        name = f"sa_{va:06X}"
        if va not in self.seen:
            self.seen.add(va)
            items = [self.cstr(self.u32(va + 4 * i)) for i in range(n)]
            self.defs.append(f"static char* {name}[] = {{{', '.join(items)}}};")
        return name

    def listarray(self, va, n):
        if not va:
            return "NULL"
        name = f"la_{va:06X}"
        if va not in self.seen:
            self.seen.add(va)
            items = [self.optlist(self.u32(va + 4 * i)) for i in range(n)]
            self.defs.append(f"static OptionList* {name}[] = {{{', '.join(items)}}};")
        return name

    def dataarray(self, va, n):
        name = f"pd_{va:06X}"
        if va not in self.seen:
            self.seen.add(va)
            items = []
            for i in range(n):
                e = va + 12 * i
                pname, data, size = self.u32(e), self.u32(e + 4), self.u32(e + 8)
                items.append(f"{{{self.cstr(pname)}, (void*){self.panels[data][0]}_panel, {size}}}")
            self.defs.append(f"static PrefDataPanel {name}[] = {{{', '.join(items)}}};")
        return name

    def find_panels(self):
        """every tool's preference data panels, defined here with their defaults"""
        for _, va in ROOTS:
            n, data = self.u32(va + 0x28), self.u32(va + 0x2C)
            for i in range(n):
                e = data + 12 * i
                d, size = self.u32(e + 4), self.u32(e + 8)
                name = self.syms.get(d, f"data_{d:08X}")
                self.panels[d] = (name, size)

    def run(self):
        self.find_panels()
        for cname, va in ROOTS:
            self.tool(cname, va)
        out = [
            "// generated by tools/mwccpsp/gen_parser_psp.py from the 3.0.1 exe: do not edit",
            "// The PSP command-line parser's tables for the native build.",
            '#include "driver/option_system.h"',
            '#include "driver/ParserFace.h"',
            "#include <stddef.h>",
            "",
            "#ifdef __APPLE__",
            '#define SYM(name) "_" name',
            "#else",
            "#define SYM(name) name",
            "#endif",
            "",
        ]
        out.append("// preference panels with their 3.0.1 defaults (0x5BD918's data)")
        for d, (name, size) in sorted(self.panels.items()):
            b = self.pe.read(d, size)
            body = ", ".join(str(c) for c in b)
            out.append(
                f'__attribute__((aligned(16))) unsigned char {name}_panel[{size}] __asm__(SYM("{name}")) = {{{body}}};'
            )
        out.append("")
        for ident, (name, kind) in sorted(self.refs.items()):
            if kind == "data":
                out.append(f'extern unsigned char {ident}[] __asm__(SYM("{name}"));')
            else:
                out.append(f'extern void {ident}(void) __asm__(SYM("{name}"));')
        out.append("")
        out += self.decls
        out.append("")
        out += self.defs
        dst = os.path.join(TOP, "src", "native", "parser_tables.c")
        open(dst, "w").write("\n".join(out) + "\n")
        print(f"wrote {dst}: {len(self.panels)} panels, {len(self.decls)} objects, {len(self.refs)} references")
        for ident, (name, kind) in sorted(self.refs.items()):
            print(f"  {kind} {name}")


if __name__ == "__main__":
    Gen().run()
