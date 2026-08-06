#!/usr/bin/env python3
"""Compare a --meta dump from the C++ oracle against the in-game reference.

Reports, per field, how many records differ, so a change's effect is visible
field by field rather than as one pass/fail.  Only seeds present in both files
are compared, so a partial C++ run still gives a usable number.

Usage:
    python tools/parity/cmpmeta.py <cpp-dump> [game-dump]
"""
import sys
from collections import Counter

GAME_DEFAULT = 'tests/fixtures/game/meta-parity-game.txt'


def load(path):
    out = {}
    cur = None
    for line in open(path, encoding='utf-8'):
        if line.startswith('galaxy '):
            f = dict(t.split('=', 1) for t in line.split() if '=' in t)
            cur = f['seed']
            out[cur] = {'s': {}, 'p': {}}
        elif line.startswith('star '):
            f = dict(t.split('=', 1) for t in line.split() if '=' in t)
            out[cur]['s'][f['id']] = f
        elif line.startswith('planet '):
            f = dict(t.split('=', 1) for t in line.split() if '=' in t)
            out[cur]['p'][f['id']] = f
    return out


def main():
    game = load(sys.argv[2] if len(sys.argv) > 2 else GAME_DEFAULT)
    cpp = load(sys.argv[1])
    bad = Counter()
    tot = Counter()
    for seed in sorted(set(game) & set(cpp), key=int):
        for kind in ('s', 'p'):
            for ident, want in game[seed][kind].items():
                got = cpp[seed][kind].get(ident)
                if got is None:
                    continue
                for key, value in want.items():
                    tot[key] += 1
                    if got.get(key) != value:
                        bad[key] += 1
    print(f"total differing fields: {sum(bad.values())}")
    for key in sorted(bad, key=lambda k: -bad[k]):
        print(f"   {key:20} {bad[key]:6d} / {tot[key]}")


if __name__ == '__main__':
    main()
