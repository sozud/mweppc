#!/usr/bin/env python3
"""Build a native mwccps2 of the b69 line: build 69 (Silent Hill 3's compiler,
version string 3.0.3) or 3.0 build 52 (Monster Hunter's), into
build/<orig dir>/native/<arch>/mwccps2. Run through tools/mwccps2/b69/native_b69.py
or tools/mwccps2/b52/native_b52.py:

    native_b69.py [-j N] [--arch x86_64|arm64] [--only UNIT] [-o OUT]

Both are built from the same sources as mwccpsp and mwccps2 b210, selecting
their position with -DMWCCPS2_B69=1 or -DMWCCPS2_B52=1 (include/compiler/mwcc_core.h):
where they differ, the code is under MWCC_CORE comparisons. Each build lists its
units and their defines in tools/mwccps2/<build>/units.txt:

  False  the GameCube-era front end, optimizer core and driver (no MWCCPSP)
  psp    the MIPS back end, as mwccps2 builds it (MWCCPSP, MWCCPS2,
         MWCCPS2_BUILD=69: the b69 line's back end)
  True   the PS2 driver glue, same defines
  b52    b52's IRO optimizer, 3.0.1's (MWCCPSP) with b52's differences

compiled with clang as an LP64 program against the host C library, as
tools/mwccpsp/native.py does. What the objects leave undefined is resolved
from the build's glue/ (the names sorted by kind; the addresses are in its
symbols*.txt):

  parser tables  gen_parser_psp.py on the exe's three ParserTool records;
                 their callbacks are the functions placed at those addresses
                 (glue/text_names.txt), anything else an aborting stub
  alias          the linker's alias to the name we define
  code, dead     aborting stubs (dead data: zeroed storage)
  bss            zeroed storage, at least 2 KB (pointers grow natively)
  data           the copied back end's data, from b145: pointer-free bytes,
                 or the typed tables (the runtime routine table)
  image labels   the few data names nothing of ours defines

Needs the build's exe (orig/PS2_3_0_3_b69 or orig/PS2_3_0_b52) and
orig/PS2_3_0_1_b145/mwccps2.exe (each build's get_orig.py), clang, and lld on
Linux.
"""

import argparse
import collections
import contextlib
import io
import json
import os
import re
import struct
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

TOOLS = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(TOOLS, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "mwccpsp"))
EXE145 = os.path.join(REPO, "orig", "PS2_3_0_1_b145", "mwccps2.exe")
# set by configure(): the build's directory (units.txt, symbols*.txt, glue/), its exe, define and output
BUILDS = {
    "b52": {"orig": "PS2_3_0_b52", "define": "-DMWCCPS2_B52=1", "name": "mwccps2 b52"},
    "b69": {"orig": "PS2_3_0_3_b69", "define": "-DMWCCPS2_B69=1", "name": "mwccps2 b69"},
}
TAG = HERE = GLUE = EXE = BUILD_NAME = None


def configure(build):
    global TAG, HERE, GLUE, EXE, BUILD_NAME
    TAG = build
    HERE = os.path.join(TOOLS, build)
    GLUE = os.path.join(HERE, "glue")
    EXE = os.path.join(REPO, "orig", BUILDS[build]["orig"], "mwccps2.exe")
    BUILD_NAME = BUILDS[build]["name"]
    FLAGS.append(BUILDS[build]["define"])


FLAGS = [
    "-std=gnu17",
    "-O1",
    "-g",
    "-fwrapv",
    "-fno-strict-aliasing",
    "-fsigned-char",
    "-fshort-enums",
    "-fms-extensions",
    "-fgnu89-inline",
    "-Wno-everything",
    "-ferror-limit=50",
    "-DVERSION=0",
    "-DMWCCPSP_NATIVE=1",
]
GROUPS = {
    "False": [],
    "psp": ["-DMWCCPSP=1", "-DMWCCPS2=1", "-DMWCCPS2_BUILD=145"],
    "True": ["-DMWCCPSP=1", "-DMWCCPS2=1", "-DMWCCPS2_BUILD=145"],
    "b52": ["-DMWCCPSP=1"],
}
DARWIN = sys.platform == "darwin"
# macOS: Apple's ld64 (no lld): -alias for --defsym, its own undefined-symbol report
LINK = ["-lc++"] if DARWIN else ["-fuse-ld=lld", "-lm", "-Wl,--error-limit=0", "-lstdc++"]


def arch_flags(arch):
    return ["-arch", arch] if DARWIN else []


def link_flags(arch):
    # packed tables hold pointers at unaligned offsets, which arm64's
    # chained fixups cannot relocate (as tools/mwccpsp/native_build.py)
    return LINK + (["-Wl,-no_fixup_chains"] if DARWIN and arch == "arm64" else [])


def alias(name, target):
    return f"-Wl,-alias,_{target},_{name}" if DARWIN else f"-Wl,--defsym={name}={target}"


def out_dir(arch):
    return os.path.join(REPO, "build", BUILDS[TAG]["orig"], "native", arch)


def units():
    """(source, defines, include dirs) for every unit"""
    for line in open(os.path.join(HERE, "units.txt")):
        if line.startswith("#") or not line.strip():
            continue
        f = line.rstrip("\n").split("\t") + ["", ""]
        path, grp, extra, inc = f[0], f[1], f[2].split(), f[3].split()
        yield path, GROUPS[grp] + extra, inc


def obj_path(src, arch):
    return os.path.join(out_dir(arch), "obj", src.replace("/", "_") + ".o")


def compile_args(u, arch, out=None, mode="-c"):
    path, defs, inc = u
    lang = ["-xc++", "-std=gnu++17"] if path.endswith(".cpp") else ["-xc"]
    fl = [f for f in FLAGS if not (lang[0] == "-xc++" and f in ("-std=gnu17", "-fgnu89-inline"))]
    return [
        "clang",
        mode,
        *arch_flags(arch),
        *fl,
        *lang,
        *defs,
        *[f"-I{d}" for d in inc],
        "-Iinclude",
        path,
        *(["-o", out] if out else []),
    ]


def compile_unit(u, arch):
    p = subprocess.run(compile_args(u, arch, obj_path(u[0], arch)), cwd=REPO, capture_output=True, text=True)
    return u[0], p.returncode == 0, re.findall(r"^(\S+):(\d+):\d+: error: (.*)$", p.stderr, re.M)


def load_symbols(path):
    out = {}
    for line in open(path):
        m = re.match(r"([\w./+$?@-]+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;", line.split("//")[0].strip())
        if m:
            out.setdefault(m.group(1), int(m.group(2), 16))
    return out


def name_to_va():
    out = {}
    for f in ("symbols.manual.txt", "symbols.derived.txt", "symbols.txt"):
        for k, v in load_symbols(os.path.join(HERE, f)).items():
            out.setdefault(k, v)
    return out


def cname(raw):
    return re.sub(r"@\d+$", "", raw[1:] if raw.startswith("_") else raw)


def generate(out, undefined):
    """gen/parser_tables.c, gen/glue.c, gen/strings.c and gen/ldflags.txt"""
    import gen_parser_psp as G
    from pe import PE

    gdir = os.path.join(out, "gen")
    os.makedirs(gdir, exist_ok=True)
    if os.path.exists(os.path.join(gdir, "strings.c")):
        os.remove(os.path.join(gdir, "strings.c"))
    pe = PE(EXE)
    relocs = set(pe.relocations())
    name_va = name_to_va()
    syms = {}
    for k, v in name_va.items():
        if "/" not in k and "$" not in k:
            syms.setdefault(v, k)
    placed = {}
    for line in open(os.path.join(GLUE, "text_names.txt")):
        va, kind, name = line.split()
        if kind == "global":
            placed[int(va, 16)] = cname(name)
    psyms = dict(syms)
    psyms.update(placed)
    va_of = {n: v for v, n in syms.items()}
    roots = [(n, va_of[n]) for n in ("parser_mw", "parser_gcc", "parser_dispatch")]
    g = G.Gen(exe=EXE, syms=psyms, roots=roots, dst=os.path.join(gdir, "parser_tables.c"), prefix="b69_")
    with contextlib.redirect_stdout(io.StringIO()) as log:
        g.run()
    print(log.getvalue().splitlines()[0])
    ptext = open(os.path.join(gdir, "parser_tables.c")).read()
    pdefined = set(re.findall(r'__asm__\(SYM\("(\w+)"\)\) =', ptext))
    prefs = set(re.findall(r'__asm__\(SYM\("(b69_(?:data|func)_[0-9A-F]{8})"\)\);', ptext))

    cats = {}
    for k in ("alias", "code", "bss", "data", "dead"):
        for line in open(os.path.join(GLUE, k + ".txt")):
            f = line.split()
            if f:
                cats.setdefault(f[0], (k, f[1:]))

    lines = [
        "// generated by tools/mwccps2/b52/native_b52.py: do not edit",
        "",
        "extern long write(int fd, const void* buf, unsigned long n);",
        "static unsigned long len(const char* s) { unsigned long n = 0; while (s[n]) n++; return n; }",
        "static void unimplemented(const char* name) {",
        f'    write(2, "{BUILD_NAME} (native): not implemented: ", {len(BUILD_NAME + " (native): not implemented: ")});',
        "    write(2, name, len(name));",
        '    write(2, "\\n", 1);',
        "    __builtin_abort();",
        "}",
        "",
    ]
    ldflags = []
    done = set()

    def stub(name, why):
        lines.append(f'void {name}(void) {{ unimplemented("{name} ({why})"); }}')

    def storage(name, n, init=b""):
        init = init.rstrip(b"\0")  # trailing zeros are the default
        body = f" = {{{', '.join(str(c) for c in init)}}}" if init else ""
        lines.append(f"__attribute__((aligned(16))) unsigned char {name}[{max(n * 2, 2048)}]{body};")

    def image_bytes(va, n):
        """b52's bytes at va (zeros past .data: .bss)"""
        try:
            return pe.read(va, n)
        except Exception:
            return bytes(n)

    def ptr_free(va, n):
        return not any(va <= r < va + n for r in relocs)

    # option variables the parser tables point at: b52's bytes (no pointers
    # in the few bytes up to the next one)
    pvars = sorted(int(n[-8:], 16) for n in prefs if "_data_" in n)
    for i, va in enumerate(pvars):
        n = min((pvars[i + 1] - va) if i + 1 < len(pvars) else 16, 16)
        b = image_bytes(va, n) if ptr_free(va, n) else bytes(n)
        storage(f"b69_data_{va:08X}", n, b)
        done.add(f"b69_data_{va:08X}")
    for n in sorted(prefs):
        if "_func_" in n:
            stub(n, "parser callback with no source placed")
            done.add(n)

    img = {}
    for line in open(os.path.join(GLUE, "image_labels.txt")):
        f = line.split()
        if len(f) >= 2:
            img.setdefault(cname(f[1]), int(f[0], 16))
    img_addrs = sorted(set(img.values()))
    img_at = {}
    for k2, v2 in img.items():
        img_at.setdefault(v2, []).append(k2)
    b145 = None
    b145_relocs = None
    for name in sorted(undefined):
        if name in done or name in pdefined:
            continue
        done.add(name)
        k, f = cats.get(name, (None, None))
        if name == "data_005CBED8" and k != "data":
            # the runtime routine records are written typed from b145's copy
            # (0x5E51B0: its first name pointer reads "__MIPS16___ptr_glue")
            k, f = "data", ["b145", "005E51B0", "256", "1"]
        if k == "alias":
            ldflags.append(alias(name, f[0]))
        elif k == "code":
            stub(name, f"{f[0]} {f[1]}")
        elif k == "dead":
            if f[-1] == "data":
                storage(name, 128)
            else:
                stub(name, "no address")
        elif k == "bss":
            storage(name, int(f[2]))
        elif k == "data":
            if b145 is None:
                b145 = PE(EXE145)
            va, n = int(f[1], 16), int(f[2])
            if name == "data_005CBED8":
                # the runtime routines {name, flags, type, object}[16] (Intrinsics.c)
                inv = {}
                for k2, v2 in json.load(open(os.path.join(GLUE, "b145_map.json")))["map"].items():
                    inv.setdefault(v2, []).append(k2)
                rows = []
                for i in range(16):
                    nm, fl, ty, ob = struct.unpack("<IIII", b145.read(va + 16 * i, 16))
                    text = G.literal(b145.read(nm, 256).split(b"\0")[0]) if nm else "NULL"
                    refs = []
                    for x in (ty, ob):
                        if not x:
                            refs.append("0")
                            continue
                        cand = [c for c in inv.get(x, []) if not c.startswith("data_")] or inv.get(x, [])
                        if not cand:
                            sys.exit(f"data_005CBED8[{i}]: no name for b145 0x{x:08X}")
                        lines.append(f"extern unsigned char {cand[0]}[];")
                        refs.append(cand[0])
                    rows.append(f"    {{{text}, 0x{fl:X}, {refs[0]}, {refs[1]}}},")
                lines.append(
                    "struct { const char* name; unsigned int flags; void* type; void* object; } data_005CBED8[16] = {"
                )
                lines += rows
                lines.append("};")
            elif name == "char_ptr":
                # TypePointer {TypeType type; SInt32 size; Type* target; UInt32 qual} -> stchar
                ty, size, qual = (
                    b145.read(va, 1)[0],
                    struct.unpack("<I", b145.read(va + 4, 4))[0],
                    struct.unpack("<I", b145.read(va + 12, 4))[0],
                )
                lines.append("extern unsigned char stchar[];")
                lines.append(
                    "struct { unsigned char type; int size; void* target; unsigned int qual; } "
                    f"char_ptr = {{{ty}, {size}, stchar, {qual}}};"
                )
            elif f[3] == "0" or name.startswith("data_005CCD7"):
                # pointer-free (the CInt64 constants: the next pointer is past them)
                storage(name, n, b145.read(va, min(n, 64) if name.startswith("data_005CCD7") else n))
            else:
                # b145's bytes up to its first pointer, zeros from there
                if b145_relocs is None:
                    b145_relocs = sorted(b145.relocations())
                first = min((r for r in b145_relocs if va <= r < va + n), default=va + n) - va
                lines.append(f"// {name} (b145 0x{va:08X}) holds pointers: its first {first} bytes")
                storage(name, n, b145.read(va, first) if first else b"")
        elif name == "stint128":
            lines.append(
                "__attribute__((aligned(4))) unsigned char stint128[12] = {"
                + ", ".join(str(c) for c in pe.read(va_of["stint128"], 12))
                + "};"
            )
        elif name == "ps2_targetPragmas":
            # b52 0x543414: {&far_call variable, "far_call", 0}, {&mips16's, "mips16", 0}
            rows, va = [], va_of["ps2_targetPragmas"]
            while True:
                val, nm, fl = struct.unpack("<III", pe.read(va, 12))
                if not nm:
                    break
                if f"b69_data_{val:08X}" not in done:
                    storage(f"b69_data_{val:08X}", 1, pe.read(val, 1))
                    done.add(f"b69_data_{val:08X}")
                lines.append(f"extern unsigned char b69_data_{val:08X}[];")
                rows.append(f"{{b69_data_{val:08X}, {G.literal(pe.read(nm, 64).split(bytes(1))[0])}, {fl}}}")
                va += 12
            lines.append(
                "struct { void* value; const char* pragma; int flags; } ps2_targetPragmas[] = {"
                + ", ".join(rows + ["{0, 0, 0}"])
                + "};"
            )
        elif name in ("InlineAsm_gccmode", "gcc_compat", "useDefaultIncludes", "data_006245C0"):
            va = va_of.get(name)
            storage(name, 4, image_bytes(va, 4) if va else b"")
        elif name in ("native_strings", "native_string_count"):
            if not os.path.exists(os.path.join(gdir, "strings.c")):
                import gen_native_strings

                with contextlib.redirect_stdout(io.StringIO()):
                    gen_native_strings.main(exe=EXE, dst=os.path.join(gdir, "strings.c"))
        elif name == "alloca":
            lines.append(
                '__asm__(".globl alloca\\nalloca: ud2");'
                if "x86" in out or "amd64" in out
                else '__asm__(".globl _alloca\\n_alloca: brk #0");'
                if DARWIN
                else '__asm__(".globl alloca\\nalloca: brk #0");'
            )
        elif name in img:
            # another b52 data name nothing of ours defines: b52's bytes up
            # to the next label when they hold no pointer, else zeros
            va = img[name]
            nxt = next((a for a in img_addrs if a > va), va + 16)
            n = min(nxt - va, 0x100000)
            first = min((r for r in relocs if va <= r < va + n), default=va + n) - va
            if first == n:
                storage(name, n, image_bytes(va, n))
            elif first >= 4:
                lines.append(f"// {name} (b52 0x{va:08X}): the {first} bytes before the next pointer")
                storage(name, n, image_bytes(va, first))
            else:
                lines.append(f"// {name} (b52 0x{va:08X}) holds pointers: zeroed")
                storage(name, n)
        elif name in name_va and any(o != name for o in img_at.get(name_va[name], ())):
            # a second name for a b52 address whose image label is another name
            other = next(o for o in img_at[name_va[name]] if o != name)
            ldflags.append(alias(name, other))
        else:
            stub(name, "no source, no address")
    open(os.path.join(gdir, "glue.c"), "w").write("\n".join(lines) + "\n")
    open(os.path.join(gdir, "ldflags.txt"), "w").write("\n".join(ldflags) + "\n")
    return [
        os.path.join(gdir, f)
        for f in ("parser_tables.c", "glue.c", "strings.c")
        if os.path.exists(os.path.join(gdir, f))
    ]


def undefined_symbols(stderr):
    und = collections.defaultdict(set)
    if DARWIN:
        for blk in re.split(r'\n(?=  "_)', stderr):
            m = re.match(r'\s*"_([^"]+)", referenced from:', blk)
            if m:
                und[m.group(1)] |= {os.path.basename(o) for o in re.findall(r" in (\S+\.o)$", blk, re.M)}
    else:
        for blk in re.split(r"\n(?=ld\.lld: error: )", stderr):
            m = re.match(r"ld\.lld: error: undefined symbol: (.+)", blk)
            if m:
                for o in re.findall(r"^>>>\s+(\S+\.o):", blk, re.M):
                    und[m.group(1)].add(os.path.basename(o))
    return und


def main(build):
    configure(build)
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--arch", default="arm64" if os.uname().machine in ("arm64", "aarch64") else "x86_64")
    ap.add_argument("--only")
    ap.add_argument("-o", "--output")
    a = ap.parse_args()
    for p in (EXE, EXE145):
        if not os.path.exists(p):
            sys.exit(f"{p} is missing: run tools/mwccps2/{TAG}/get_orig.py")
    out = out_dir(a.arch)
    os.makedirs(os.path.join(out, "obj"), exist_ok=True)
    us = list(units())
    todo = [u for u in us if not a.only or a.only in u[0]]
    bad = []
    with ThreadPoolExecutor(a.j) as ex:
        for path, good, errs in ex.map(lambda u: compile_unit(u, a.arch), todo):
            if not good:
                bad.append(path)
                for f, line, msg in errs[:5]:
                    print(f"{f}:{line}: {msg}")
    print(f"{len(todo) - len(bad)}/{len(todo)} units compile")
    if bad:
        sys.exit(1)
    objs = [obj_path(u[0], a.arch) for u in us]
    exe = os.path.join(out, "mwccps2")
    p = subprocess.run(
        ["clang", *arch_flags(a.arch), "-o", exe, *objs, *link_flags(a.arch)], capture_output=True, text=True
    )
    und = undefined_symbols(p.stderr)
    open(os.path.join(out, "undefined.txt"), "w").write(
        "".join(f"{n} {' '.join(sorted(v))}\n" for n, v in sorted(und.items()))
    )
    print(f"{len(und)} undefined -> {os.path.join(out, 'undefined.txt')}")
    gobjs = []
    for src in generate(out, set(und)):
        o = src[:-2] + ".o"
        p = subprocess.run(
            ["clang", "-c", *arch_flags(a.arch), *FLAGS, *GROUPS["psp"], "-Iinclude", src, "-o", o],
            cwd=REPO,
            capture_output=True,
            text=True,
        )
        if p.returncode:
            sys.exit(f"{src}:\n{p.stderr[-3000:]}")
        gobjs.append(o)
    ldflags = open(os.path.join(out, "gen", "ldflags.txt")).read().split()
    p = subprocess.run(
        ["clang", *arch_flags(a.arch), "-o", exe, *objs, *gobjs, *ldflags, *link_flags(a.arch)],
        capture_output=True,
        text=True,
    )
    errs = [l for l in p.stderr.splitlines() if "error" in l]
    if errs or p.returncode:
        print("\n".join(errs[:30]))
        sys.exit(f"link failed (rc={p.returncode})")
    if a.output:
        os.makedirs(os.path.dirname(os.path.abspath(a.output)), exist_ok=True)
        import shutil

        shutil.copy(exe, a.output)
        exe = a.output
    print(exe)


if __name__ == "__main__":
    sys.exit("run tools/mwccps2/b69/native_b69.py or tools/mwccps2/b52/native_b52.py")
