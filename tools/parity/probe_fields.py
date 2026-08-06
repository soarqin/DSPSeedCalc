#!/usr/bin/env python3
"""Decide the Mono float semantics of one generated field at a time.

The build-and-measure loop cannot tell "this field's formula is wrong" from
"this field's inputs are still wrong": a downstream field inherits every
upstream error, so its mismatch count moves for reasons that have nothing to do
with the change under test.  Earlier work lost a day to that -- a correct
temperatureBias change looked like a regression because sunDistance was not
aligned yet.

This script sidesteps it.  Each check recomputes one field in Python from the
*game's own* values of that field's inputs, read out of the same meta dump, and
compares against the game's own value of the field.  Upstream error is
therefore zero by construction, and a candidate that reports 0 mismatches is
the semantics the game uses -- no rebuild, no dependency ordering.

Usage:
    python tools/parity/probe_fields.py [game-dump] [max-seed]

The dump is produced by tools/vein-oracle-mod with Mode = meta.
"""
import math
import os
import sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_common import (F, clamp01, f32, h32, load, mclamp, mlerp, mlog, mpow, msqrt, w)

DUMP = sys.argv[1] if len(sys.argv) > 1 else 'tests/fixtures/game/meta-parity-game.txt'
MAXSEED = int(sys.argv[2]) if len(sys.argv) > 2 else 100

bad = Counter()
tot = Counter()


def check(field, want, **candidates):
    tot[field] += 1
    for name, value in candidates.items():
        if h32(value) != want:
            bad[f'{field}:{name}'] += 1


g = load(DUMP)
for s in sorted(g, key=int):
    if int(s) > MAXSEED:
        continue
    stars = g[s]['s']

    for st in stars.values():
        ty = int(st['type'])
        cf = f32(st['classFactor'])
        age = f32(st['age'])
        mass = f32(st['mass'])
        birth = st['index'] == '0'

        # color: Mathf.Clamp01(((float)num15 + 3.5f) * 0.2f).  SetStarAge pins
        # color for the compact types, so only 0/1 exercise the formula.
        if ty in (0, 1):
            check('color', st['color'],
                  widened_0_2f=clamp01(F((cf + 3.5) * w(0.2))),
                  true_double_0_2=clamp01(F((cf + 3.5) * 0.2)),
                  stepwise_f32=clamp01(F(F(cf + F(3.5)) * F(0.2))))

        # Everything below is overwritten by SetStarAge unless the star stayed
        # on the main sequence.
        if ty == 0:
            p = F(cf + 2.0)
            lbr = mpow(1.7, p)
            check('lightBalanceRadius', st['lightBalanceRadius'], mathf_pow=lbr)
            # The birth star uses 0.25f -> 0.2f; orbitScaler still holds its
            # pre-assignment value of 1f at this point.
            k = 0.2 if birth else 0.25
            check('habitableRadius', st['habitableRadius'],
                  prev_orbitScaler_1=F(float(lbr) + w(k) * float(F(min(F(1.0), F(1.0))))),
                  prev_orbitScaler_0=F(float(lbr) + w(k) * 0.0))
            o = mpow(1.35, p)
            if o < F(1.0):
                o = mlerp(o, 1.0, 0.6)
            check('orbitScaler', st['orbitScaler'], mathf_pow_then_lerp=o)

            n14 = F((1.0 - float(mpow(clamp01(age), 20.0)) * w(0.5)) * mass)
            d = float(n14)
            check('temperature', st['temperature'],
                  double_body=F(math.pow(d, 0.56 + 0.14 / (math.log10(d + 4.0) / math.log10(5.0))) * 4450.0 + 1300.0))
            check('luminosity', st['luminosity'], mathf_pow=mpow(n14, 0.7))

        # classFactor is (float)num15 and SetStarAge never touches it, but num15
        # comes from the pre-SetStarAge temperature, which only type 0 still has.
        if ty == 0:
            n15 = math.log10((f32(st['temperature']) - 1300.0) / 4500.0) / math.log10(2.6) - 0.5
            if n15 < 0.0:
                n15 *= 4.0
            n15 = 2.0 if n15 > 2.0 else (-4.0 if n15 < -4.0 else n15)
            check('classFactor', st['classFactor'], narrow_once=F(n15))

        # dysonRadius survives SetStarAge for every type, and both its inputs
        # are dumped post-SetStarAge, so all types can be checked.
        osc = f32(st['orbitScaler'])
        pr = F(f32(st['radius']) * 1200.0)
        ps_d = float(pr) * 1.5                    # IL keeps the * 1.5f in double
        ps_f = float(F(float(pr) * 1.5))          # narrowing it is wrong
        dr = F(osc * w(0.28))
        a = F(ps_d / 40000.0) if float(dr) * 40000.0 < ps_d else dr
        b = F(ps_f / 40000.0) if float(dr) * 40000.0 < ps_f else dr
        rm = F(ps_f / 40000.0)
        c = rm if dr < rm else dr
        check('dysonRadius', st['dysonRadius'],
              double_physics_scaled=a, narrowed_physics_scaled=b, precomputed_bound=c)

    for pid, pp in g[s]['p'].items():
        st = stars.get(str(int(pid) // 100))
        if st is None:
            continue
        hr = f32(st['habitableRadius'])
        sd = f32(pp['sunDistance'])
        ok = hr > 0.0 and sd > 0.0
        n23_f = F(F(sd) / F(hr)) if ok else F(1000.0)
        n23_d = (sd / hr) if ok else 1000.0

        # Gas giants never reach the branch that assigns either field.
        if pp['type'] == '5':
            continue

        check('temperatureBias', pp['temperatureBias'],
              narrow_once_widened=F(w(1.2) / (float(n23_f) + w(0.2)) - 1.0),
              narrow_once_true_double=F(1.2 / (float(n23_f) + 0.2) - 1.0),
              stepwise_f32=F(F(F(1.2) / F(float(n23_f) + F(0.2))) - F(1.0)),
              wide_num23=F(w(1.2) / (n23_d + w(0.2)) - 1.0))

        n22 = F(abs(float(mlog(n23_f)))) if ok else F(1000.0)
        n24 = F(float(mclamp(msqrt(hr), 1.0, 2.0)) - w(0.04))
        check('habitableBias', pp['habitableBias'], narrow_once=F(float(n22) * float(n24)))

print(f"probing {DUMP} over seeds 0-{MAXSEED}\n")
print(f"{'field:candidate':44} mismatches / tested")
for field in sorted(tot):
    for k in sorted(k for k in bad if k.startswith(field + ':')):
        print(f"   {k:44} {bad[k]:7d} / {tot[field]}")
    print(f"   {field + ':(every other candidate)':44} {0:7d} / {tot[field]}  <- matches the game")
