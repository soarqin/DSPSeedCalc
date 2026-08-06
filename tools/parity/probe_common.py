"""Shared helpers for the field-parity probes.

The probes recompute a single generated field in Python from the *game's own*
values of that field's inputs, so a candidate's mismatch count is never
polluted by an upstream field that is still wrong.  Mono float32 semantics are
modelled explicitly: widen to double, evaluate, narrow once per float32 store.
"""
import math
import struct

import numpy as np

F = np.float32


def load(path):
    """Parse a meta dump into {seed: {'s': {id: fields}, 'p': {id: fields}}}."""
    out = {}
    cur = None
    for line in open(path, encoding='utf-8'):
        if line.startswith('galaxy '):
            f = dict(t.split('=', 1) for t in line.split() if '=' in t)
            cur = f['seed']
            out[cur] = {'s': {}, 'p': {}, 'g': f}
        elif line.startswith('star '):
            f = dict(t.split('=', 1) for t in line.split() if '=' in t)
            out[cur]['s'][f['id']] = f
        elif line.startswith('planet '):
            f = dict(t.split('=', 1) for t in line.split() if '=' in t)
            out[cur]['p'][f['id']] = f
    return out


def f32(h):
    return float(struct.unpack('>f', bytes.fromhex(h))[0])


def f64(h):
    return struct.unpack('>d', bytes.fromhex(h))[0]


def h32(v):
    return struct.pack('>f', np.float32(v)).hex().upper()


def h64(v):
    return struct.pack('>d', float(v)).hex().upper()


def w(x):
    """Value of a C# float32 literal after Mono widens it to double."""
    return float(F(x))


# --- UnityEngine.Mathf, with the narrowing points Mono actually applies ---

def mpow(v, p):
    return F(math.pow(float(F(v)), float(F(p))))


def mlog(v):
    return F(math.log(float(F(v))))


def msqrt(v):
    return F(math.sqrt(float(F(v))))


def clamp01(v):
    v = F(v)
    return F(0.0) if v < F(0.0) else (F(1.0) if v > F(1.0) else v)


def mclamp(v, a, b):
    v = F(v)
    return F(a) if v < F(a) else (F(b) if v > F(b) else v)


def mlerp(a, b, t):
    return F(float(F(a)) + (float(F(b)) - float(F(a))) * float(clamp01(t)))
