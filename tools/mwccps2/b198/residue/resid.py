#!/usr/bin/env python3
# resid.py OUT -- CMD: at b119 0x4cf18f (func_004C3C60 kind-4 path) log
# residue rclass byte [esp+1], flags word [esp+2], and return chain
# (OUT ending in .seq: only "kind rclass flags" per hit, for MWCCPS2_B119_RESIDUE)
import ctypes
import os
import sys

libc = ctypes.CDLL(None, use_errno=True)
libc.ptrace.restype = ctypes.c_long
libc.ptrace.argtypes = [ctypes.c_long, ctypes.c_long, ctypes.c_void_p, ctypes.c_void_p]
TRACEME, PEEK, POKE, CONT, STEP, GETREGS, SETREGS, SYSCALL = 0, 2, 5, 7, 9, 12, 13, 24
# RESID_BP / RESID_BP3 for another build (b198: 0x4d006f, 0x4d00a4; func_004C3C60 is exact there)
BP = int(os.environ.get("RESID_BP", "0x4cf18f"), 16)
BP3 = int(os.environ.get("RESID_BP3", "0x4cf1c4"), 16)  # the kind-3 (immediate) path: [esp+0x19..0x1b] unwritten
out = open(sys.argv[1], "w")
cmd = sys.argv[sys.argv.index("--") + 1 :]
pid = os.fork()
if pid == 0:
    libc.ptrace(TRACEME, 0, None, None)
    d = os.open("/dev/null", os.O_WRONLY)
    os.dup2(d, 1)
    os.dup2(d, 2)
    os.execvp(cmd[0], cmd)
os.waitpid(pid, 0)
peek = lambda x: libc.ptrace(PEEK, pid, ctypes.c_void_p(x), None) & 0xFFFFFFFFFFFFFFFF
poke = lambda x, v: libc.ptrace(POKE, pid, ctypes.c_void_p(x), ctypes.c_void_p(v))
while True:
    libc.ptrace(SYSCALL, pid, None, None)
    _, st = os.waitpid(pid, 0)
    if os.WIFEXITED(st):
        sys.exit("exited")
    ctypes.set_errno(0)
    v = peek(BP)
    if ctypes.get_errno() == 0 and v:
        break
orig = peek(BP)
poke(BP, (orig & ~0xFF) | 0xCC)
orig3 = peek(BP3)
poke(BP3, (orig3 & ~0xFF) | 0xCC)
ORIG = {BP: orig, BP3: orig3}
libc.ptrace(CONT, pid, None, None)
n = 0
while True:
    _, st = os.waitpid(pid, 0)
    if os.WIFEXITED(st) or os.WIFSIGNALED(st):
        break
    if os.WSTOPSIG(st) != 5:
        libc.ptrace(CONT, pid, None, ctypes.c_void_p(os.WSTOPSIG(st)))
        continue
    r = (ctypes.c_ulonglong * 27)()
    libc.ptrace(GETREGS, pid, None, ctypes.byref(r))
    esp = r[19] & 0xFFFFFFFF
    at = (r[16] - 1) & 0xFFFFFFFF
    kind = 4 if at == BP else 3
    w = peek(esp + (0 if kind == 4 else 0x18)) & 0xFFFFFFFF
    ret1 = peek(esp + 0x34) & 0xFFFFFFFF
    # walk ebp chain for a couple more frames
    chain = [ret1]
    ebp = r[4] & 0xFFFFFFFF
    for _ in range(3):
        if not (0x10000 < ebp < 0xFFFFFFF0):
            break
        chain.append(peek(ebp + 4) & 0xFFFFFFFF)
        ebp = peek(ebp) & 0xFFFFFFFF
    if sys.argv[1].endswith(".seq"):
        out.write(f"{kind} {(w >> 8) & 0xFF:x} {(w >> 16) & 0xFFFF:x}\n")
    else:
        out.write(
            f"{n} k{kind} esp={esp:08x} rc={(w >> 8) & 0xFF:02x} fl={(w >> 16) & 0xFFFF:04x} "
            + " ".join(f"{c:08x}" for c in chain)
            + "\n"
        )
    n += 1
    r[16] -= 1
    libc.ptrace(SETREGS, pid, None, ctypes.byref(r))
    poke(at, ORIG[at])
    libc.ptrace(STEP, pid, None, None)
    os.waitpid(pid, 0)
    poke(at, (ORIG[at] & ~0xFF) | 0xCC)
    libc.ptrace(CONT, pid, None, None)
print(n, "hits")
