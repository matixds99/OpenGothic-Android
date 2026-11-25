#!/usr/bin/env python3
"""Compare the calls of script functions: PC (nblog.txt of tools/pc-logger) vs OpenGothic (nbcalls.txt).

  python3 tools/nbcompare.py nblog.txt nbcalls.txt [--anchor AI_ONGETDAMAGE_IMARAH] [--npc Im'Arah] [--min 3]

Both files: lines "[ms] NAME self=A other=B". The time of each is counted from its first ANCHOR call (the start of the
fight), so the files may come from different runs. Output: per function the number of calls and the rate (per second) in
both versions, the interval between calls (median), the first call after the anchor, and a mark for the differences:
  <<  the function is only on PC (the port does not call it)
  >>  only in the port
  !   the rate differs more than 1.7x
"""
import re, sys, statistics, argparse

LINE = re.compile(r"^\[(\d+)\]\s+(\S+)(?:\s+self=(.*?))?(?:\s+other=(.*))?\s*$")

def ascii_name(s):
    return re.sub(r"[^A-Za-z0-9' ]", "", s or "").strip().lower()

def load(path, anchor, npc):
    ev = []
    for raw in open(path, "rb").read().decode("latin-1").splitlines():
        m = LINE.match(raw.strip())
        if not m or m.group(2) == "PLUGIN":
            continue
        t, fn, a, b = int(m.group(1)), m.group(2), ascii_name(m.group(3)), ascii_name(m.group(4))
        ev.append((t, fn, a, b))
    t0 = next((t for t, fn, a, b in ev if anchor is None or fn == anchor), None)
    if t0 is None:
        sys.exit(f"{path}: no call of {anchor}")
    out = []
    for t, fn, a, b in ev:
        if t < t0:
            continue
        if npc and npc not in (a, b):
            continue
        out.append(((t - t0) / 1000.0, fn))
    return out

def stats(ev):
    d = {}
    for t, fn in ev:
        d.setdefault(fn, []).append(t)
    return d

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pc"); ap.add_argument("port")
    ap.add_argument("--anchor", default=None, help="the function that starts the fight (default: the first call)")
    ap.add_argument("--npc", default=None, help="only calls with this self/other (a name, e.g. Im'Arah)")
    ap.add_argument("--min", type=int, default=2, help="skip the functions with fewer calls on both sides")
    ap.add_argument("--window", type=float, default=None, help="compare only the first N seconds")
    a = ap.parse_args()
    npc = ascii_name(a.npc) if a.npc else None
    pc, port = load(a.pc, a.anchor, npc), load(a.port, a.anchor, npc)
    if a.window:
        pc = [e for e in pc if e[0] <= a.window]; port = [e for e in port if e[0] <= a.window]
    dpc, dpo = stats(pc), stats(port)
    spc = max((t for t, _ in pc), default=1) or 1
    spo = max((t for t, _ in port), default=1) or 1
    print(f"PC: {len(pc)} calls in {spc:.0f} s    OpenGothic: {len(port)} calls in {spo:.0f} s\n")
    print(f"{'function':38} {'PC n':>5} {'/s':>6} {'int':>6} {'1st':>6} | {'OG n':>5} {'/s':>6} {'int':>6} {'1st':>6}")
    rows = []
    for fn in sorted(set(dpc) | set(dpo)):
        x, y = dpc.get(fn, []), dpo.get(fn, [])
        if len(x) < a.min and len(y) < a.min:
            continue
        def iv(v):
            return statistics.median([b - a_ for a_, b in zip(v, v[1:])]) if len(v) > 1 else 0.0
        rx, ry = len(x) / spc, len(y) / spo
        flag = ""
        if x and not y: flag = "<<"
        elif y and not x: flag = ">>"
        elif rx and ry and (rx / ry > 1.7 or ry / rx > 1.7): flag = "!"
        rows.append((flag, fn, x, y, rx, ry, iv(x), iv(y)))
    for flag, fn, x, y, rx, ry, ix, iy in sorted(rows, key=lambda r: (r[0] == "", r[1])):
        f = lambda v: f"{min(v):6.1f}" if v else "     -"
        print(f"{fn[:38]:38} {len(x):5d} {rx:6.2f} {ix:6.2f} {f(x)} | {len(y):5d} {ry:6.2f} {iy:6.2f} {f(y)}  {flag}")
    print("\n<< only on PC, >> only in OpenGothic, ! rate differs >1.7x; int = the median interval [s], 1st = the first call [s]")

main()
