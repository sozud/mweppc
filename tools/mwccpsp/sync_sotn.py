#!/usr/bin/env python3
"""Bring the PSP and PS2 compilers over from the sotn-decomp tree.

    sync_sotn.py SOTN [--rev REV] [--ps2] [--no-tools] [--tmp DIR]

The compilers are developed in Castlevania: Symphony of the Night's
decompilation (https://github.com/Xeeynamo/sotn-decomp), branch
mwccpsp-decomp (PSP) and mwccps2-decomp (PS2), under tools/mwccpsp/ and
tools/mwccps2/. That tree started from mweppc 919828d and edits mweppc's
sources in place; here the GameCube builds must keep reading exactly what they
did, so every change to a file mweppc already has goes behind #if MWCCPSP with
the 919828d lines in the #else. Files mweppc doesn't have are copied as they
are.

Path mapping (sotn tools/mwccpsp/X -> mweppc Y):

    src/**, include/**                -> src/**, include/** (guarded as above;
                                         src/PROVENANCE.md and the trace
                                         instrumentation are left out)
    re/symbols.txt, symbols.manual.txt -> config/PSP_3_0_1/
    config/psp-exclude.txt            -> config/PSP_3_0_1/psp-exclude.txt
    harness/{native,native_build,native_glue,pe}.py,
    re/gen_{cc_psp,native_data,native_strings,opcode_table,parser_psp}.py
                                      -> tools/mwccpsp/ (paths rewritten:
                                         harness/build.py -> units.py,
                                         bin/mwccpsp.exe -> orig/PSP_3_0_1/,
                                         build/mwccpsp-native -> build/PSP_3_0_1/native)

and with --ps2 (sotn tools/mwccps2/X):

    harness/native_ps2.py             -> tools/mwccps2/native_ps2.py
    re/addrmap.csv                    -> config/PS2_3_0_1/addrmap.csv
    (the hybrid build's name map)     -> config/PS2_3_0_1/symbols.txt
    tools/mwccpsp/harness/lint64.py   -> tools/mwccpsp/lint64.py

SOTN is a sotn-decomp git checkout; REV (default HEAD) is read with git
archive, so the checkout itself is never touched. Needs unifdef, and the
venv's clang-format and ruff (uv sync) to format what it writes.
"""

import argparse
import difflib
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))
BASE = "919828d97d33d236da675824d43c5d57332afe74"

# ---------------------------------------------------------------------------
# guarding: OUT reads as OLD with MWCCPSP undefined and as NEW with it set

PSP = {
    "#if MWCCPSP": True,
    "#ifdef MWCCPSP": True,
    "#if defined(MWCCPSP)": True,
    "#if !MWCCPSP": False,
    "#ifndef MWCCPSP": False,
    "#if !defined(MWCCPSP)": False,
}
TAG = re.compile(r"^@@(\d+)@@")
DTAG = re.compile(r" /\*@@(\d+)@@\*/$")
DIRECTIVE = re.compile(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b")
GC = ["-UMWCCPSP", "-UMWCCPSP_NATIVE", "-UMWCCPSP_TRACE", "-UMWCCPS2", "-UMWCCPS2_BUILD"]


def directive(line):
    m = re.match(r"\s*#\s*(\w+)(.*)$", line)
    if not m:
        return None, None
    key = "#" + m.group(1) + (" " + m.group(2).strip() if m.group(2).strip() else "")
    key = re.sub(r"\s*//.*$|\s*/\*.*\*/\s*$", "", key)
    return m.group(1), key


def resolve(lines):
    """Drop conditionals on MWCCPSP whose value an enclosing one decides."""
    out = []
    stack = []
    known = None
    live = True
    for line in lines:
        kind, key = directive(line)
        if kind in ("if", "ifdef", "ifndef"):
            if key in PSP and known is not None:
                taken = PSP[key] == known
                stack.append(["psp-resolved", known, live, taken])
                live = live and taken
                continue
            if key in PSP:
                stack.append(["psp", known, live, None])
                if live:
                    out.append(line)
                known = PSP[key]
                continue
            stack.append(["other", known, live, None])
            if live:
                out.append(line)
            continue
        if kind == "elif":
            f = stack[-1]
            if f[0] == "psp-resolved":
                if f[3]:
                    live = False
                else:
                    f[0] = "other"
                    live = f[2]
                    if live:
                        out.append(line.replace("#elif", "#if", 1))
                continue
            if f[0] == "psp":
                f[0] = "other"
                known = f[1]
            if f[2]:
                out.append(line)
            continue
        if kind == "else":
            f = stack[-1]
            if f[0] == "psp-resolved":
                live = f[2] and not f[3]
                continue
            if f[0] == "psp":
                known = not known
            if f[2]:
                out.append(line)
            continue
        if kind == "endif":
            f = stack.pop()
            known = f[1]
            live = f[2]
            if f[0] == "psp-resolved":
                continue
            if live:
                out.append(line)
            continue
        if live:
            out.append(line)
    assert not stack, "unbalanced conditionals"
    return out


def gc_view(new_lines):
    """The line numbers of NEW that the GameCube build reads."""
    tagged = []
    for n, line in enumerate(new_lines):
        body = line.rstrip("\n")
        if re.match(r"\s*#", body):
            tagged.append(f"{body} /*@@{n}@@*/\n")
        else:
            tagged.append(f"@@{n}@@{body}\n")
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False, encoding="latin-1") as f:
        f.writelines(tagged)
        path = f.name
    p = subprocess.run(["unifdef", *GC, path], capture_output=True, text=True, encoding="latin-1")
    os.unlink(path)
    view = []
    for line in p.stdout.splitlines():
        m = TAG.match(line) or DTAG.search(line)
        if not m:
            sys.exit(f"untagged line from unifdef: {line!r}")
        view.append(int(m.group(1)))
    return view


def kind(line):
    m = DIRECTIVE.match(line)
    return m.group(1) if m else None


def balance(new, a, b):
    """Grow [a, b] to whole logical lines and until its conditionals balance."""
    while True:
        while a > 0 and new[a - 1].rstrip("\n").endswith("\\"):
            a -= 1
        while b < len(new) - 1 and new[b].rstrip("\n").endswith("\\"):
            b += 1
        depth = 0
        grow_back = False
        for i in range(a, b + 1):
            k = kind(new[i])
            if k in ("if", "ifdef", "ifndef"):
                depth += 1
            elif k == "endif":
                depth -= 1
                if depth < 0:
                    grow_back = True
                    break
            elif k in ("else", "elif") and depth == 0:
                grow_back = True
                break
        if grow_back:
            d = 0
            i = a - 1
            while True:
                k = kind(new[i])
                if k == "endif":
                    d += 1
                elif k in ("if", "ifdef", "ifndef"):
                    if d == 0:
                        break
                    d -= 1
                i -= 1
            a = i
            continue
        if depth > 0:
            d = depth
            i = b + 1
            while True:
                k = kind(new[i])
                if k in ("if", "ifdef", "ifndef"):
                    d += 1
                elif k == "endif":
                    d -= 1
                    if d == 0:
                        break
                i += 1
            b = i
            continue
        return a, b


def guard(old, new):
    view = gc_view(new)
    vtext = [new[n] for n in view]
    sm = difflib.SequenceMatcher(None, old, vtext, autojunk=False)
    replace = {}
    before = {}
    ranges = []
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == "equal":
            continue
        if j1 == j2:
            before.setdefault(j1, []).extend(old[i1:i2])
            continue
        replace[j1] = old[i1:i2]
        for j in range(j1 + 1, j2):
            replace[j] = []
        ranges.append(balance(new, view[j1], view[j2 - 1]))
    ranges.sort()
    merged = []
    for a, b in ranges:
        if merged and a <= merged[-1][1] + 1:
            merged[-1] = balance(new, merged[-1][0], max(b, merged[-1][1]))
        else:
            merged.append((a, b))
    done = False
    while not done:
        done = True
        for i in range(len(merged) - 1):
            if merged[i][1] >= merged[i + 1][0] - 1:
                merged[i] = balance(new, merged[i][0], max(merged[i][1], merged[i + 1][1]))
                del merged[i + 1]
                done = False
                break
    slot_of = {n: j for j, n in enumerate(view)}
    out = []
    used_before = set()
    n = 0
    ri = 0
    while n < len(new):
        if ri < len(merged) and n == merged[ri][0]:
            a, b = merged[ri]
            ri += 1
            alt = []
            for i in range(a, b + 1):
                j = slot_of.get(i)
                if j is None:
                    continue
                if j in before and j not in used_before:
                    alt += before[j]
                    used_before.add(j)
                alt += replace.get(j, [new[i]])
            last = max((slot_of[i] for i in range(a, b + 1) if i in slot_of), default=None)
            if last is not None and last + 1 in before and (last + 1 >= len(view) or view[last + 1] > b):
                alt += before[last + 1]
                used_before.add(last + 1)
            out += ["#if MWCCPSP\n", *new[a : b + 1]]
            out += ["#else\n", *alt, "#endif\n"] if alt else ["#endif\n"]
            n = b + 1
            continue
        j = slot_of.get(n)
        if j is not None and j in before and j not in used_before:
            out += ["#if !MWCCPSP\n", *before[j], "#endif\n"]
            used_before.add(j)
        out.append(new[n])
        n += 1
    tail = len(view)
    if tail in before and tail not in used_before:
        out += ["#if !MWCCPSP\n", *before[tail], "#endif\n"]
    return resolve(out)


# ---------------------------------------------------------------------------
# cleaning the decomp tree's sources: its debugging traces and notes about its
# own harness don't belong here


def edit_text(s, pairs):
    for a, b in pairs:
        s = s.replace(a, b)
    return s


COMMENTS = {
    "src/psp/backend/BE_macro.c": [
        (
            "// assembler macros that remain after register allocation. The expander\n"
            "// itself (0x48B5D0) is in progress (notes/BE_macro.c.draft); these are its\n"
            "// helpers.",
            "// assembler macros that remain after register allocation.",
        )
    ],
    "src/psp/backend/AliasPropagation.c": [
        (
            "// mwccpsp 3.0.1 AliasPropagation.c (0x53B390..): only the functions the\n"
            "// golden corpus reaches at -Op so far.",
            "// mwccpsp 3.0.1 AliasPropagation.c (0x53B390..)",
        )
    ],
    "src/psp/backend/ObjGen.c": [
        (
            "// on top of ELFgen. Globals and callees that are not identified yet are named\n"
            "// by address (data_XXXXXXXX / func_XXXXXXXX); the hybrid linker resolves those\n"
            "// names to the original's addresses.",
            "// on top of ELFgen. Unidentified globals and callees are named by address\n"
            "// (data_XXXXXXXX / func_XXXXXXXX).",
        )
    ],
    "src/psp/backend/SchedMachine.c": [
        ("// dependence hook. The hybrid uses the originals in the exe's data.", "// dependence hook.")
    ],
    "src/psp/backend/BE_binary.c": [
        (
            "// Only the encoder (0x4B7080) is written so far. The instruction and operand\n"
            "// records are modelled as far as the encoder reads them.\n",
            "",
        )
    ],
    "src/psp/backend/CMachine.c": [
        (
            "// mwccpsp 3.0.1 CMachine.c (MIPS). 2.4.7's is PowerPC-specific and stays\n"
            "// out of the build; functions are written here as the corpus reaches them.",
            "// mwccpsp 3.0.1 CMachine.c (MIPS)",
        )
    ],
    "src/psp/backend/InlineAsmMIPS.c": [("// assembler. Written so far: the setup (0x483F30).", "// assembler.")],
    "src/compiler/frontend/CExpr.c": [
        (
            "    // 3.0.1 accepts only integral operands (not yet byte-identical: the\n"
            "    // exe's inlined type test reloads the operand)",
            "    // 3.0.1 accepts only integral operands",
        )
    ],
    "src/compiler/frontend/CClass.c": [
        (
            "// is the current class or function a friend of tclass (not yet matched to\n"
            "// 3.0.1's access code, which is laid out differently)",
            "// is the current class or function a friend of tclass",
        )
    ],
    "src/compiler/frontend/CFunc.c": [
        (" // computed-goto label handling (no corpus coverage)", " // computed-goto label handling")
    ],
    "src/native/win32_posix.c": [
        ("// only, Phase 4). Only", "// only). Only"),
        (
            "// Shift-JIS file name wrappers (src/host/ShiftJIS.c): sotn's paths are ASCII",
            "// Shift-JIS file name wrappers (src/host/ShiftJIS.c): paths are passed through as is",
        ),
    ],
    "src/compiler/frontend/CPrep.c": [
        (
            "// The 3.0.1 table, extracted from mwccpsp.exe in order (see re/gen_copts.py).",
            "// The 3.0.1 table, extracted from mwccpsp.exe in order.",
        )
    ],
    "src/psp/backend/GlobalOptimizer.c": [
        (
            "// (0x4AF700..0x4AF9FF). The original unit name is not known; the passes it\n"
            "// runs (common subexpressions, copy propagation, the peephole pass, the\n"
            "// store/load optimization) are still the original code.",
            "// (0x4AF700..0x4AF9FF). The original unit name is not known.",
        )
    ],
    "src/psp/backend/ELFgen.c": [
        (
            "// the original links them. The three unreferenced functions in that range\n"
            "// (0x531420, 0x531460, 0x5314F0..0x531580) never run for sotn and are not\n"
            "// written yet.",
            "// the original links them, except the three unreferenced functions in that\n"
            "// range (0x531420, 0x531460, 0x5314F0..0x531580).",
        )
    ],
    "include/compiler/CParser.h": [
        (
            "    // mwccpsp 3.0.1 layout, 0x144 bytes, generated by re/gen_copts.py from\n"
            "    // the compileroptions table in mwccpsp.exe (#pragma name -> offset),\n"
            "    // initialize_compiler_options (0x423500) and re/field_map.py. Regenerate\n"
            "    // rather than edit by hand.",
            "    // mwccpsp 3.0.1 layout, 0x144 bytes, from the compileroptions table in\n"
            "    // mwccpsp.exe (#pragma name -> offset) and initialize_compiler_options\n"
            "    // (0x423500).",
        )
    ],
}

TRACE_FILES = ["src/psp/backend/BETrace.c", "src/psp/optimizer/IroTrace301.c", "src/PROVENANCE.md"]
GETENV_TRACE = re.compile(r'#if MWCCPSP_NATIVE\n[ \t]+if \(getenv\("MWCCPSP_TRACE"\)[^\n]*\{\n.*?\n#endif\n', re.S)


def clean(work):
    def path(p):
        return os.path.join(work, p)

    def rw(p, fn):
        if not os.path.exists(path(p)):
            return
        s = open(path(p), encoding="latin-1").read()
        t = fn(s)
        if t != s:
            open(path(p), "w", encoding="latin-1").write(t)

    def be_h(s):
        s = re.sub(r"// TRACE_PCODE\(label\).*?#endif\n\n", "", s, count=1, flags=re.S)
        a = s.find("// Debug traces comparable")
        m = s.find("#define BE_TRACE(args)\n")
        if a >= 0 and m >= 0:
            b = s.index("#endif\n", m) + len("#endif\n\n")
            s = s[:a] + s[b:]
        return s

    rw("include/psp/BE.h", be_h)
    rw("src/psp/backend/CodeGen.c", lambda s: s.replace("        TRACE_PCODE(label); \\\n", ""))
    rw("src/psp/backend/GlobalOptimizer.c", lambda s: s.replace("        TRACE_PCODE(title); \\\n", ""))

    def iro(s):
        s = re.sub(
            r"#if MWCCPSP && \(MWCCPSP_NATIVE \|\| defined\(MWCCPSP_TRACE\)\)\n// debug: the linear IR.*?#endif\n\n",
            "",
            s,
            count=1,
            flags=re.S,
        )
        return s.replace('    IRO_TRACE("IRO_Linearize");\n', "").replace(
            '    IRO_TRACE("IRO_DoTransformations");\n', ""
        )

    rw("src/compiler/optimizer/IrOptimizer.c", iro)
    for root, _, files in os.walk(path("src")):
        for f in files:
            p = os.path.join(root, f)
            t = open(p, encoding="latin-1").read()
            if "MWCCPSP_TRACE" in t:
                open(p, "w", encoding="latin-1").write(GETENV_TRACE.sub("", t))

    def coloring(s):
        a = s.find("#if MWCCPSP_NATIVE || defined(MWCCPSP_TRACE)\n")
        if a >= 0:
            b = s.index("#endif\n", a) + len("#endif\n")
            s = s[:a] + s[b:]
        return re.sub(r"\n[ \t]*BE_TRACE\(\(stderr,[^;]*;\n", "\n", s)

    rw("src/psp/backend/Coloring.c", coloring)
    for p, pairs in COMMENTS.items():
        rw(p, lambda s, pairs=pairs: edit_text(s, pairs))
    for root, _, files in os.walk(work):
        for f in files:
            p = os.path.join(root, f)
            t = open(p, encoding="latin-1").read()
            n = t.replace("tools/mwccpsp/re/gen_", "tools/mwccpsp/gen_").replace(
                "tools/mwccpsp/harness/", "tools/mwccpsp/"
            )
            n = n.replace("re/gen_", "tools/mwccpsp/gen_").replace("re/symbols", "config/PSP_3_0_1/symbols")
            n = n.replace("tools/mwccps2/harness/native_ps2.py", "tools/mwccps2/native_ps2.py")
            if n != t:
                open(p, "w", encoding="latin-1").write(n)


# ---------------------------------------------------------------------------
# the Python tools: the same programs with mweppc's paths

TOOLS = {
    "harness/native.py": "tools/mwccpsp/native.py",
    "harness/native_build.py": "tools/mwccpsp/native_build.py",
    "harness/native_glue.py": "tools/mwccpsp/native_glue.py",
    "harness/pe.py": "tools/mwccpsp/pe.py",
    "re/gen_cc_psp.py": "tools/mwccpsp/gen_cc_psp.py",
    "re/gen_native_data.py": "tools/mwccpsp/gen_native_data.py",
    "re/gen_native_strings.py": "tools/mwccpsp/gen_native_strings.py",
    "re/gen_opcode_table.py": "tools/mwccpsp/gen_opcode_table.py",
    "re/gen_parser_psp.py": "tools/mwccpsp/gen_parser_psp.py",
}
# with --ps2: the --ps2 modes above, plus (relative to sotn's tools/)
PS2_TOOLS = {
    "mwccpsp/harness/lint64.py": "tools/mwccpsp/lint64.py",
    "mwccps2/harness/native_ps2.py": "tools/mwccps2/native_ps2.py",
}
PS2_DATA = {
    "mwccps2/re/addrmap.csv": "config/PS2_3_0_1/addrmap.csv",
}
DATA = {
    "re/symbols.txt": "config/PSP_3_0_1/symbols.txt",
    "re/symbols.manual.txt": "config/PSP_3_0_1/symbols.manual.txt",
    "config/psp-exclude.txt": "config/PSP_3_0_1/psp-exclude.txt",
}


def port_tool(s):
    """sotn's tools/mwccpsp/{harness,re}/*.py read for mweppc's layout."""
    pairs = [
        ("TOP = os.path.normpath(os.path.join(HERE, \"..\"))\n"
         "REPO = os.path.normpath(os.path.join(TOP, \"..\", \"..\"))\n",
         "TOP = os.path.normpath(os.path.join(HERE, \"..\", \"..\"))\nREPO = TOP\n"),
        ('sys.path.insert(0, os.path.join(TOP, "harness"))\n', "sys.path.insert(0, HERE)\n"),
        ('os.path.join(TOP, "re", f)', 'os.path.join(TOP, "config", "PSP_3_0_1", f)'),
        ("from build import ", "from units import "),
        ("build/mwccpsp-native/", "build/PSP_3_0_1/native/"),
        (" (Phase 4, milestones M2/M3).", "."),
        (" (Phase 4, M2).", "."),
        ("Compiles every unit that build.py compiles, with the host clang, as a\n",
         "Compiles every unit with the host clang, as a\n"),
        ("errors. The development Mac is x86_64, so x86_64 is the default (it can run\n"
         "what it builds); arm64 is cross-compiled to keep that target building.\n", "errors.\n"),
        ("Flags follow the plan: -fwrapv", "Flags: -fwrapv"),
        ('    bss_start = next(s for n, s, vs in secs if n == ".bss")\n', ""),
        # mweppc's own: packed tables hold pointers at 2-byte alignment, which
        # arm64 chained fixups cannot relocate
        ('*ldflags, *native_glue.LINK], check=True)',
         '*ldflags, *native_glue.LINK,\n'
         '                    # packed tables hold pointers at 2-byte alignment, which arm64\n'
         '                    # chained fixups cannot relocate\n'
         '                    *(["-Wl,-no_fixup_chains"] if sys.platform == "darwin" and a.arch == "arm64" else [])],\n'
         '                   check=True)'),
        ('os.path.join(REPO, "build", "mwccpsp-native", ', 'os.path.join(REPO, "build", "PSP_3_0_1", "native", '),
        # the PS2 build: orig/PS2_3_0_1, build/PS2_3_0_1/native, and the name
        # map from config/PS2_3_0_1 (tools/mwccps2/ps2_map.py) in place of
        # the hybrid build's (tools/mwccps2/harness/build_ps2.py)
        ('os.path.join(REPO, "build", name() + "-native", ',
         'os.path.join(REPO, "build", "PS2_3_0_1" if PS2 else "PSP_3_0_1", "native", '),
        ('os.path.join(REPO, "bin", "ps2", "mwccps2.exe")', 'os.path.join(REPO, "orig", "PS2_3_0_1", "mwccps2.exe")'),
        ("bin/ps2/mwccps2.exe", "orig/PS2_3_0_1/mwccps2.exe"),
        ("build/mwccps2-native/", "build/PS2_3_0_1/native/"),
        ("tools/mwccps2/harness/native_ps2.py", "tools/mwccps2/native_ps2.py"),
        ('    sys.path.insert(0, os.path.join(REPO, "tools", "mwccps2", "harness"))\n'
         "    import build_ps2  # noqa: E402\n"
         "    addr, AMAP = build_ps2.ps2_address_map()\n",
         '    sys.path.insert(0, os.path.join(REPO, "tools", "mwccps2"))\n'
         "    import ps2_map  # noqa: E402\n"
         "    addr, AMAP = ps2_map.ps2_address_map()\n"),
        ("the hybrid build gives them (tools/mwccps2/harness/build_ps2.py's\n",
         "config/PS2_3_0_1 gives them (tools/mwccps2/ps2_map.py's\n"),
        ('table = json.load(open(os.path.join(TOP, "config", "mweppc-sources.json")))',
         'table = json.load(open(os.path.join(TOP, "config", "sources.json")))'),
        ('TOP = os.path.normpath(os.path.join(HERE, ".."))\n\nWARN', 'TOP = os.path.normpath(os.path.join(HERE, "..", ".."))\n\nWARN'),
        ('os.path.join(REPO, "bin", "mwccpsp.exe")', 'os.path.join(REPO, "orig", "PSP_3_0_1", "mwccpsp.exe")'),
        ("bin/mwccpsp.exe", "orig/PSP_3_0_1/mwccpsp.exe"),
        ("tools/mwccpsp/re/gen_", "tools/mwccpsp/gen_"),
        ("tools/mwccpsp/harness/", "tools/mwccpsp/"),
        ("harness/native_glue.py", "tools/mwccpsp/native_glue.py"),
        ("tools/mwccpsp/harness/native_build.py", "tools/mwccpsp/native_build.py"),
        ("re/gen_", "tools/mwccpsp/gen_"),
        ("re/symbols", "config/PSP_3_0_1/symbols"),
    ]  # fmt: skip
    return edit_text(s, pairs)


def port_ps2(s):
    """sotn's tools/mwccps2/harness/native_ps2.py for mweppc's layout."""
    pairs = [
        ('TOP = os.path.normpath(os.path.join(HERE, ".."))\n'
         'REPO = os.path.normpath(os.path.join(TOP, "..", ".."))\n',
         'REPO = os.path.normpath(os.path.join(HERE, "..", ".."))\n'),
        ('sys.path.insert(0, os.path.join(PSP_TOP, "re"))\nsys.path.insert(0, os.path.join(PSP_TOP, "harness"))\n',
         "sys.path.insert(0, PSP_TOP)\n"),
        ("come from the hybrid\n   build's name map (build_ps2.ps2_address_map)",
         "come from config/PS2_3_0_1\n   (ps2_map.ps2_address_map)"),
        ("import build_ps2  # noqa: E402", "import ps2_map  # noqa: E402"),
        ("build_ps2.", "ps2_map."),
        ('EXE = os.path.join(REPO, "bin", "ps2", "mwccps2.exe")', 'EXE = os.path.join(REPO, "orig", "PS2_3_0_1", "mwccps2.exe")'),
        ('DST = os.path.join(PSP_TOP, "src", "native", "ps2")', 'DST = os.path.join(REPO, "src", "native", "ps2")'),
        ('os.path.join(PSP_TOP, "harness", "native_build.py")', 'os.path.join(PSP_TOP, "native_build.py")'),

        ("tools/mwccpsp/re/gen_", "tools/mwccpsp/gen_"),
    ]  # fmt: skip
    return port_tool(edit_text(s, pairs))


# config/PS2_3_0_1/symbols.txt: every name the PS2 build places, as the
# hybrid build places it (sotn's tools/mwccps2/harness/build_ps2.py, which
# moves the PSP names through the address map and adds the derived and
# hand-derived PS2 addresses). Run in the archived sotn tree.
DUMP_PS2_NAMES = """
import sys
sys.path.insert(0, sys.argv[1])
import build_ps2
addr, _ = build_ps2.ps2_address_map()
with open(sys.argv[2], "w") as f:
    f.write("// PS2 3.0.1 b210 addresses of the names the sources use: generated by\\n")
    f.write("// tools/mwccpsp/sync_sotn.py --ps2 from the sotn-decomp tree's PS2 name map\\n")
    for k in sorted(addr):
        f.write(f"{k} = 0x{addr[k]:08X};\\n")
"""


def tool(name):
    """The venv's pinned formatter (uv sync), else the one on PATH."""
    p = os.path.join(TOP, ".venv", "bin", name)
    return p if os.path.exists(p) else name


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sotn", help="a sotn-decomp git checkout")
    ap.add_argument("--rev", default="HEAD")
    ap.add_argument("--dst", default=TOP, help="the mweppc checkout to update (default: this one)")
    ap.add_argument("--no-tools", action="store_true", help="only the C sources and their data")
    ap.add_argument("--ps2", action="store_true", help="also the PS2 compiler (mwccps2-decomp)")
    ap.add_argument("--tmp", help="where to put the temporary files (default: the system's)")
    a = ap.parse_args()

    tmp = tempfile.mkdtemp(prefix="sync_sotn.", dir=a.tmp)
    tree = os.path.join(tmp, "sotn")
    base = os.path.join(tmp, "base")
    os.makedirs(tree)
    os.makedirs(base)
    subprocess.run(
        f"git -C {a.sotn} archive {a.rev} tools/mwccpsp{' tools/mwccps2' if a.ps2 else ''} | tar -x -C {tree}",
        shell=True,
        check=True,
    )
    subprocess.run(f"git -C {a.dst} archive {BASE} src include | tar -x -C {base}", shell=True, check=True)
    psp = os.path.join(tree, "tools", "mwccpsp")
    work = os.path.join(tmp, "work")
    for d in ("src", "include"):
        shutil.copytree(os.path.join(psp, d), os.path.join(work, d))
    clean(work)

    guarded = added = 0
    written = []
    for root, _, files in os.walk(work):
        for f in files:
            p = os.path.join(root, f)
            rel = os.path.relpath(p, work)
            if rel in TRACE_FILES:
                continue
            dst = os.path.join(a.dst, rel)
            old = os.path.join(base, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            if os.path.exists(old) and rel.endswith((".c", ".cpp", ".h", ".inc", ".opt", ".r")):
                o = open(old, encoding="latin-1").readlines()
                n = open(p, encoding="latin-1").readlines()
                open(dst, "w", encoding="latin-1").writelines(guard(o, n))
                guarded += 1
            else:
                shutil.copyfile(p, dst)
                added += 1
            written.append(dst)
    for d in ("src/psp", "src/native", "include/psp"):
        for root, _, files in os.walk(os.path.join(a.dst, d)):
            for f in files:
                rel = os.path.relpath(os.path.join(root, f), a.dst)
                if not os.path.exists(os.path.join(work, rel)):
                    print("no longer in sotn (remove it):", rel)
    for s, d in DATA.items():
        shutil.copyfile(os.path.join(psp, s), os.path.join(a.dst, d))
    if a.ps2:
        os.makedirs(os.path.join(a.dst, "config", "PS2_3_0_1"), exist_ok=True)
        for s, d in PS2_DATA.items():
            shutil.copyfile(os.path.join(tree, "tools", s), os.path.join(a.dst, d))
        harness = os.path.join(tree, "tools", "mwccps2", "harness")
        names = os.path.join(a.dst, "config", "PS2_3_0_1", "symbols.txt")
        subprocess.run([sys.executable, "-c", DUMP_PS2_NAMES, harness, names], cwd=tmp, check=True)
    # the sotn tree isn't formatted with this repo's settings
    cf = tool("clang-format")
    sources = [p for p in written if p.endswith((".c", ".cpp", ".h"))]
    for i in range(0, len(sources), 200):
        subprocess.run([cf, "-i", *sources[i : i + 200]], cwd=a.dst, check=True)
    if not a.no_tools:
        ported = []
        for s, d in TOOLS.items():
            t = open(os.path.join(psp, s)).read()
            open(os.path.join(a.dst, d), "w").write(port_tool(t))
            ported.append(d)
        if a.ps2:
            os.makedirs(os.path.join(a.dst, "tools", "mwccps2"), exist_ok=True)
            for s, d in PS2_TOOLS.items():
                t = open(os.path.join(tree, "tools", s)).read()
                open(os.path.join(a.dst, d), "w").write(port_ps2(t) if "mwccps2/" in s else port_tool(t))
                ported.append(d)
        subprocess.run([tool("ruff"), "format", "-q", *ported], cwd=a.dst, check=True)
        subprocess.run([tool("ruff"), "check", "-q", "--fix", "--select", "F401,F541,I", *ported], cwd=a.dst)
    print(f"guarded {guarded}, copied {added}; temporary files in {tmp}")


if __name__ == "__main__":
    main()
