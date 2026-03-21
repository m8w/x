#!/usr/bin/env python3
"""
x — Prime Cypher: find commonality in life codes.

Combines two complementary modules:
  • prime_engine  — prime generation, testing, factorization
  • life_cipher   — prime-based encoding of life codes & commonality analysis

Usage examples
--------------
  python main.py encode "HELLO WORLD"
  python main.py decode 13 31 37 37 41 3 61 41 53 37 11
  python main.py compare "DNA LIFE" "FIND LINK"
  python main.py fingerprint "ACGT"
  python main.py factor 9699690
  python main.py primes 50
  python main.py check 97
"""

import sys
from prime_engine import sieve, is_prime, factorize, nth_prime
from life_cipher import LifeCipher


CIPHER = LifeCipher()


def cmd_encode(args: list[str]) -> None:
    text = " ".join(args)
    codes = CIPHER.encode(text)
    print(f"Text     : {text}")
    print(f"Codes    : {' '.join(map(str, codes))}")
    print(f"Fingerprint: {CIPHER.fingerprint(text)}")


def cmd_decode(args: list[str]) -> None:
    codes = [int(a) for a in args]
    text = CIPHER.decode(codes)
    print(f"Codes : {' '.join(map(str, codes))}")
    print(f"Text  : {text}")


def cmd_compare(args: list[str]) -> None:
    if len(args) < 2:
        print("Usage: compare <text_a> <text_b>")
        sys.exit(1)
    # Split on '--' separator if provided, otherwise split in half
    if "--" in args:
        idx = args.index("--")
        a, b = " ".join(args[:idx]), " ".join(args[idx + 1 :])
    else:
        mid = len(args) // 2
        a, b = " ".join(args[:mid]), " ".join(args[mid:])

    common = CIPHER.common_tokens(a, b)
    score = CIPHER.commonality_score(a, b)
    gcd = CIPHER.gcd_fingerprint(a, b)

    print(f"Life code A  : {a}")
    print(f"Life code B  : {b}")
    print(f"Common tokens: {', '.join(common) if common else '(none)'}")
    print(f"Commonality  : {score:.1%}")
    print(f"GCD fingerprint: {gcd}")
    if gcd > 1:
        gcd_factors = factorize(gcd)
        shared_primes = list(gcd_factors.keys())
        print(f"  → shared prime codes: {shared_primes}")


def cmd_fingerprint(args: list[str]) -> None:
    text = " ".join(args)
    fp = CIPHER.fingerprint(text)
    pattern = CIPHER.prime_pattern(text)
    print(f"Text       : {text}")
    print(f"Prime map  : {pattern}")
    print(f"Fingerprint: {fp}")
    print(f"Is prime?  : {is_prime(fp)}")


def cmd_factor(args: list[str]) -> None:
    for raw in args:
        n = int(raw)
        factors = factorize(n)
        print(f"factorize({n}) = {factors}")


def cmd_primes(args: list[str]) -> None:
    limit = int(args[0]) if args else 50
    primes = sieve(limit)
    print(f"Primes up to {limit}: {primes}")
    print(f"Count: {len(primes)}")


def cmd_check(args: list[str]) -> None:
    for raw in args:
        n = int(raw)
        print(f"{n} is {'prime' if is_prime(n) else 'composite'}")


COMMANDS = {
    "encode": cmd_encode,
    "decode": cmd_decode,
    "compare": cmd_compare,
    "fingerprint": cmd_fingerprint,
    "factor": cmd_factor,
    "primes": cmd_primes,
    "check": cmd_check,
}


def main() -> None:
    if len(sys.argv) < 2 or sys.argv[1] not in COMMANDS:
        print(__doc__)
        sys.exit(0)
    cmd = sys.argv[1]
    rest = sys.argv[2:]
    COMMANDS[cmd](rest)


if __name__ == "__main__":
    main()
