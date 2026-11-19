#!/usr/bin/env python3
"""How much of each original compiler the decompiled sources reproduce exactly.

    exact.py [--compilers DIR] [--json-out DIR] [-j N] [mwccpsp|mwccps2|b52|b69 ...]

For each compiler (default: all four), every unit is compiled for Windows x86
with the host compiler and flags the original itself was built with (under
wibo), and every function of ours whose name places it in the original's
.text is compared with the original's bytes there, as the research tree's
hybrid build harness does before linking (match.py): equal bytes with
relocation slots masked, and every relocation pointing at the right target.

Exact = the share of the original's function table (functions.csv: each
function's start and size, read from the exe) whose function our source
reproduces this way, in bytes and in functions. Prints one line per compiler,
writes build/exact/<compiler>/match.csv (every compared function), and with
--json-out a shields.io endpoint badge per compiler (mwccpsp.json,
mwccps2.json, mwccps2-b52.json, mwccps2-b69.json).

Inputs, per compiler (paths in COMPILERS):
  units     exact_units.json: each unit, its host compiler and arguments
            (the research harness's command lines, mapped to this tree's
            paths; $LIB is the CodeWarrior discs' MSL/runtime)
  names     where our names are in the original: symbols*.txt; mwccpsp's
            are found from the objects as the research harness does
            (psp_names.py) on top of config/PSP_3_0_1/symbols.manual.txt
  exe       the original (each compiler's get_orig.py, or for mwccpsp the
            download in tools/mwccpsp/Dockerfile)

--compilers DIR (or $MWCC_COMPILERS; default build/mwcc-compilers) holds the
host compilers: cw92/, cwpro6/, cwpro53/, cwpro5/ (mwcc.exe each) and lib/,
the compilers branch of github.com/sozud/mwcc-info. wibo is $WIBO, else
build/tools/wibo (fetched with tools/download_tool.py if missing).
mwccpsp's names need capstone (uv sync).
"""

import argparse
import concurrent.futures as cf
import csv
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools", "mwccpsp"))  # pe.py
sys.path.insert(0, os.path.join(REPO, "tools", "mwccps2"))  # ps2_map.py
from coff import Object, bind_pro6  # noqa: E402
from match import Resolver, check_relocs, check_relocs_pro53, compare, load_symbols  # noqa: E402
from pe import PE  # noqa: E402

B69 = os.path.join("tools", "mwccps2", "b69")
B52 = os.path.join("tools", "mwccps2", "b52")
COMPILERS = {
    "mwccpsp": {
        "label": "mwccpsp b219",
        "badge": "mwccpsp.json",
        "exe": "orig/PSP_3_0_1/mwccpsp.exe",
        "units": "config/PSP_3_0_1/exact_units.json",
        "functions": "config/PSP_3_0_1/functions.csv",
    },
    "mwccps2": {
        "label": "mwccps2 b210",
        "badge": "mwccps2.json",
        "exe": "orig/PS2_3_0_1/mwccps2.exe",
        "units": "config/PS2_3_0_1/exact_units.json",
        "functions": "config/PS2_3_0_1/functions.csv",
        # a unit that needs the PSP compiler's layouts (none today) is
        # compiled without -DMWCCPS2=1, as the research harness does
        "fallback": "-DMWCCPS2=1",
        "map": "config/PS2_3_0_1",
    },
    "b198": {
        "label": "mwccps2 b198",
        "badge": "mwccps2-b198.json",
        "exe": "orig/PS2_3_0_1_b198/mwccps2.exe",
        # b210's units with -DMWCCPS2_BUILD=198; the name map, address map and
        # function table come from the sotn-decomp tree's MWCCPS2_BUILD=198 work
        "units": "config/PS2_3_0_1b198/native/exact_units.json",
        "functions": "config/PS2_3_0_1b198/native/functions.csv",
        "fallback": "-DMWCCPS2=1",
        "map": "config/PS2_3_0_1b198/native",
    },
    "b52": {
        "label": "mwccps2 b52",
        "badge": "mwccps2-b52.json",
        "exe": "orig/PS2_3_0_b52/mwccps2.exe",
        "units": f"{B52}/exact_units.json",
        "functions": f"{B52}/functions.csv",
        "dir": B52,
        "addr_name": r"_b52_([0-9A-Fa-f]{8})$",
    },
    "b69": {
        "label": "mwccps2 b69",
        "badge": "mwccps2-b69.json",
        "exe": "orig/PS2_3_0_3_b69/mwccps2.exe",
        "units": f"{B69}/exact_units.json",
        "functions": f"{B69}/functions.csv",
        "dir": B69,
        "addr_name": r"_b69_([0-9A-Fa-f]{8})$",
    },
}
HOSTS = ("cw92", "cwpro6", "cwpro53", "cwpro5")


def repo(p):
    return os.path.join(REPO, p)


def wibo():
    if os.environ.get("WIBO"):
        return os.environ["WIBO"]
    path = repo("build/tools/wibo")
    if not os.path.exists(path):
        subprocess.run([sys.executable, repo("tools/download_tool.py"), "wibo", path], check=True)
    return path


def compile_unit(u, cfg, out, compilers, wibo_path):
    """(unit, object path or None, error text)"""
    obj = os.path.join(out, "obj", u["path"].replace("/", "_") + ".obj")
    lib = os.path.join(compilers, "lib")
    args = [a.replace("$LIB", lib) for a in u["args"]]
    host = os.path.join(compilers, u["host"], "mwcc.exe")
    env = dict(os.environ, MWCIncludes="include/libc")

    def run(args):
        if os.path.exists(obj):
            os.remove(obj)
        cmd = [wibo_path, host, "-c", *args, u["path"], "-o", obj]
        p = subprocess.run(cmd, cwd=REPO, env=env, capture_output=True, timeout=900)
        return p.returncode == 0 and os.path.isfile(obj), (p.stdout + p.stderr).decode("latin1")

    ok, err = run(args)
    if not ok and cfg.get("fallback") in args:
        ok, err2 = run([a for a in args if a != cfg["fallback"]])
        if ok:
            print(f"{u['path']}: compiled without {cfg['fallback']}", file=sys.stderr)
    return u, obj if ok else None, err


def names(key, cfg, objects, pe, functions, res):
    """{name: address} for the comparison: proven names win over guesses."""
    if key == "mwccpsp":
        import psp_names

        manual = load_symbols(repo("config/PSP_3_0_1/symbols.manual.txt"))
        symbols = {**psp_names.prove(objects, pe), **manual}
        guessed = psp_names.guess(objects, pe, functions, symbols, res)
        return {**guessed, **symbols}
    if "map" in cfg:
        # the research harness's PS2 name map (tools/mwccpsp/sync_sotn.py --ps2)
        return load_symbols(repo(os.path.join(cfg["map"], "symbols.txt")))
    d = repo(cfg["dir"])

    def syms(f):
        p = os.path.join(d, f)
        return load_symbols(p) if os.path.exists(p) else {}

    # tools/mwccps2/b69/build_b69.py's order in the research tree
    symbols = {**syms("symbols.txt"), **syms("symbols.derived.txt"), **syms("symbols.manual.txt")}
    guessed = {k: v for k, v in syms("symbols.guess.txt").items() if k not in symbols}
    own = {k: v for k, v in syms("symbols.guessown.txt").items() if k not in symbols}
    if own:
        placed = set(own.values())
        guessed = {**{k: v for k, v in guessed.items() if v not in placed}, **own}
    taken = set(symbols.values()) | set(guessed.values())
    for k, v in syms("symbols.guess145.txt").items():
        if k not in symbols and k not in guessed and v not in taken:
            guessed[k] = v
    return {**guessed, **symbols}


def measure(key, compilers, wibo_path, jobs):
    cfg = COMPILERS[key]
    out = repo(os.path.join("build", "exact", key))
    os.makedirs(os.path.join(out, "obj"), exist_ok=True)
    units = json.load(open(repo(cfg["units"])))
    for h in {u["host"] for u in units}:
        if not os.path.isfile(os.path.join(compilers, h, "mwcc.exe")):
            raise SystemExit(f"{key}: no {h}/mwcc.exe in {compilers} (--compilers)")
    built, failed = [], []
    with cf.ThreadPoolExecutor(jobs) as ex:
        for u, obj, err in ex.map(lambda u: compile_unit(u, cfg, out, compilers, wibo_path), units):
            if obj:
                built.append((u, obj))
            else:
                failed.append(u["path"])
                print(f"{key}: compile failed: {u['path']}\n{err[-1500:]}", file=sys.stderr)
    built.sort(key=lambda b: os.path.basename(b[1]))
    objects = []
    for u, obj in built:
        o = Object(obj)
        if "dir" in cfg:
            bind_pro6(o)
        objects.append((o, u.get("unit", os.path.basename(u["path"]))))

    pe = PE(repo(cfg["exe"]))
    functions = [(int(r["start"], 16), int(r["size"]), r["unit"]) for r in csv.DictReader(open(repo(cfg["functions"])))]
    amap = None
    if "map" in cfg:
        import ps2_map

        _, amap = ps2_map.ps2_address_map(repo(cfg["map"]))
    res = Resolver(cfg.get("addr_name", r"_([0-9A-Fa-f]{8})$"), amap)
    symbols = names(key, cfg, objects, pe, functions, res)
    rows = compare(objects, symbols, pe, res, check_relocs_pro53 if "dir" in cfg else check_relocs)
    with open(os.path.join(out, "match.csv"), "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["name", "address", "size", "status", "object"])
        for r in sorted(rows, key=lambda r: (r[1], r[0])):
            w.writerow([r[0], f"{r[1]:08x}", r[2], r[3], os.path.basename(r[4])])

    exact = {r[1] for r in rows if r[3] == "match"}
    hit = [(a, n) for a, n, _ in functions if a in exact]
    total_b = sum(n for _, n, _ in functions)
    hit_b = sum(n for _, n in hit)
    return {
        "compiler": key,
        "label": cfg["label"],
        "badge": cfg["badge"],
        "units": len(units),
        "failed": failed,
        "exact_bytes": hit_b,
        "total_bytes": total_b,
        "exact_functions": len(hit),
        "total_functions": len(functions),
        "percent": 100.0 * hit_b / total_b,
    }


def color(pct):
    return (
        "brightgreen"
        if pct >= 90
        else "green"
        if pct >= 75
        else "yellowgreen"
        if pct >= 50
        else "yellow"
        if pct >= 25
        else "orange"
    )


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("compiler", nargs="*", help=f"{', '.join(COMPILERS)} (default: all)")
    ap.add_argument("--compilers", default=os.environ.get("MWCC_COMPILERS", repo("build/mwcc-compilers")))
    ap.add_argument("--json-out", help="write the shields.io endpoint badges here")
    ap.add_argument("-j", type=int, default=min(4, os.cpu_count() or 1))
    a = ap.parse_args()
    for key in a.compiler:
        if key not in COMPILERS:
            ap.error(f"unknown compiler {key}: one of {', '.join(COMPILERS)}")
    compilers = os.path.abspath(a.compilers)
    w = wibo()
    failed = False
    for key in a.compiler or list(COMPILERS):
        r = measure(key, compilers, w, a.j)
        print(
            f"{r['label']:14s} {r['percent']:5.1f}% exact  ({r['exact_bytes'] // 1024}/{r['total_bytes'] // 1024} KB, "
            f"{r['exact_functions']}/{r['total_functions']} functions)"
            + (f"  {len(r['failed'])} of {r['units']} units failed to compile" if r["failed"] else ""),
            flush=True,
        )
        if r["failed"]:
            failed = True
            continue  # no badge from a partial build
        if a.json_out:
            os.makedirs(a.json_out, exist_ok=True)
            badge = {
                "schemaVersion": 1,
                "label": r["label"],
                "message": f"{r['percent']:.1f}% exact",
                "color": color(r["percent"]),
            }
            with open(os.path.join(a.json_out, r["badge"]), "w") as f:
                json.dump(badge, f)
                f.write("\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
