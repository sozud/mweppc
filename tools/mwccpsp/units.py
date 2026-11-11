"""mwccpsp's translation units: config/sources.json and the 3.0.1 units under
src/psp, minus config/PSP_3_0_1/psp-exclude.txt."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))

DRIVER_PLUGIN_UNITS = {
    "IO.c",
    "ParserFace.c",
    "ParserErrors.c",
    "Projects.c",
    "WarningHelpers.c",
    "ToolHelpers.c",
    "ParserHelpers.c",
    "COSToolsCLT.c",
}
DRIVER_MACSPEC_UNITS = {"ELF_Lib_Import.c"}


def units(only=None):
    table = json.load(open(os.path.join(TOP, "config", "sources.json")))
    excl_p = os.path.join(TOP, "config", "PSP_3_0_1", "psp-exclude.txt")
    excluded = {l.strip() for l in open(excl_p) if l.strip() and not l.startswith("#")}
    for root, _, files in os.walk(os.path.join(TOP, "src", "psp")):
        for f in sorted(files):
            if f.endswith(".c"):
                rel = os.path.relpath(os.path.join(root, f), TOP)
                table.setdefault(rel, {"include_dirs": [os.path.dirname(rel)]})
    for src, s in table.items():
        if s.get("system_include_dirs") or not src.endswith((".c", ".cpp")) or src in excluded:
            continue
        if not os.path.isfile(os.path.join(TOP, src)):
            continue
        if only and only not in src:
            continue
        yield src, s
