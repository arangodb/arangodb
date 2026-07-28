#!/usr/bin/env python3
"""Tally status-code cell differences between the classic baseline and the RBAC run.

Usage: python3 diffstat.py [classic_baseline.txt] [rbac.txt] [--by-endpoint]
"""
import re, sys
from collections import Counter, defaultdict

A = open(sys.argv[1] if len(sys.argv) > 1 else "classic_baseline.txt").read().splitlines()
B = open(sys.argv[2] if len(sys.argv) > 2 else "rbac.txt").read().splitlines()
by_ep = "--by-endpoint" in sys.argv

def cells(l): return re.findall(r'\| *([0-9]{3}|SKIP) *(?=\|)', l)
methre = re.compile(r'^(GET|POST|PUT|PATCH|DELETE|HEAD) /')

trans = Counter(); diff = 0; total = 0
per_ep = defaultdict(Counter); cur = '?'
for a, b in zip(A, B):
    if methre.match(a): cur = a.strip()
    ca, cb = cells(a), cells(b)
    if not ca or len(ca) != len(cb): continue
    for x, y in zip(ca, cb):
        total += 1
        if x != y:
            diff += 1; trans[(x, y)] += 1; per_ep[cur][(x, y)] += 1

print(f"differing cells: {diff} / {total}  ({100*diff/total:.1f}%)")
print("\nclassic -> rbac   count")
for (x, y), n in trans.most_common():
    print(f"  {x:>4} -> {y:<4}   {n}")

if by_ep:
    print("\n=== by endpoint (most differing first) ===")
    for ep, c in sorted(per_ep.items(), key=lambda kv: -sum(kv[1].values())):
        top = ' '.join(f'{x}->{y}:{n}' for (x, y), n in c.most_common(3))
        print(f"{sum(c.values()):3d}  {ep[:60]:60s} {top}")
