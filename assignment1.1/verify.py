#!/usr/bin/env python3
import sys
from pathlib import Path

OUT = Path('output')

def read_shares(path):
    s = Path(path)
    if not s.exists():
        print(f'Missing file: {path}')
        return None, None
    lines = s.read_text().strip().splitlines()
    if len(lines) < 2:
        print(f'Bad format in {path}: expected at least 2 lines')
        return None, None
    u = list(map(int, lines[0].strip().split()))
    v = list(map(int, lines[1].strip().split()))
    return u, v

def add_vec(a, b):
    n = max(len(a), len(b))
    return [(a[i] if i < len(a) else 0) + (b[i] if i < len(b) else 0) for i in range(n)]

def main():
    # input share files
    f0 = OUT / 'share_p0_0.txt'
    f1 = OUT / 'share_p1_0.txt'
    u0, v0 = read_shares(f0)
    u1, v1 = read_shares(f1)
    if u0 is None or u1 is None:
        print('Cannot read initial shares; aborting')
        sys.exit(2)

    # updated share files
    uf0 = OUT / 'updated_share_p0_0.txt'
    uf1 = OUT / 'updated_share_p1_0.txt'
    uu0, uv0 = read_shares(uf0)
    uu1, uv1 = read_shares(uf1)
    if uu0 is None or uu1 is None:
        print('Cannot read updated shares; aborting')
        sys.exit(2)

    # reconstruct
    u = add_vec(u0, u1)
    v = add_vec(v0, v1)
    updated = add_vec(uu0, uu1)

    if not (len(u) == len(v) == len(updated)):
        print('Dimension mismatch between vectors:')
        print(f'len(u)={len(u)} len(v)={len(v)} len(updated)={len(updated)}')
        sys.exit(2)

    # compute dot and expected updated
    dot = sum(x * y for x, y in zip(u, v))
    expected = [u[i] + v[i] * (1 - dot) for i in range(len(u))]

    print('Reconstructed u:', u)
    print('Reconstructed v:', v)
    print('Dot(u,v) =', dot)
    print('Reconstructed updated u:', updated)
    print('Expected updated u   :', expected)

    ok = True
    diffs = []
    for i, (a, b) in enumerate(zip(updated, expected)):
        if a != b:
            ok = False
            diffs.append((i, a, b))

    if ok:
        print('\nVerification: OK — reconstructed updated vector matches expected updated vector')
        return 0
    else:
        print('\nVerification: FAILED — differences at indices:')
        for idx, got, exp in diffs:
            print(f'  idx {idx}: got={got} expected={exp} diff={got-exp}')
        return 3

if __name__ == '__main__':
    rc = main()
    sys.exit(rc)
