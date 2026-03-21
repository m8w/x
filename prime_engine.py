"""
Prime Engine — core prime number utilities.
Provides generation, primality testing, and factorization.
"""


def sieve(limit: int) -> list[int]:
    """Return all primes up to `limit` using the Sieve of Eratosthenes."""
    if limit < 2:
        return []
    is_prime = bytearray([1]) * (limit + 1)
    is_prime[0] = is_prime[1] = 0
    for i in range(2, int(limit**0.5) + 1):
        if is_prime[i]:
            is_prime[i * i :: i] = bytearray(len(is_prime[i * i :: i]))
    return [i for i, v in enumerate(is_prime) if v]


def is_prime(n: int) -> bool:
    """Miller-Rabin primality test (deterministic for n < 3,317,044,064,679,887,385,961,981)."""
    if n < 2:
        return False
    small = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37]
    if n in small:
        return True
    if any(n % p == 0 for p in small):
        return False
    # write n-1 as 2^r * d
    r, d = 0, n - 1
    while d % 2 == 0:
        r += 1
        d //= 2
    for a in small:
        x = pow(a, d, n)
        if x in (1, n - 1):
            continue
        for _ in range(r - 1):
            x = pow(x, 2, n)
            if x == n - 1:
                break
        else:
            return False
    return True


def factorize(n: int) -> dict[int, int]:
    """Return the prime factorization of `n` as {prime: exponent}."""
    if n < 2:
        return {}
    factors: dict[int, int] = {}
    d = 2
    while d * d <= n:
        while n % d == 0:
            factors[d] = factors.get(d, 0) + 1
            n //= d
        d += 1
    if n > 1:
        factors[n] = factors.get(n, 0) + 1
    return factors


def nth_prime(n: int) -> int:
    """Return the nth prime (1-indexed)."""
    if n < 1:
        raise ValueError("n must be >= 1")
    # rough upper bound via prime number theorem
    import math
    if n < 6:
        limit = 15
    else:
        limit = int(n * (math.log(n) + math.log(math.log(n))) * 1.3) + 10
    primes = sieve(limit)
    while len(primes) < n:
        limit *= 2
        primes = sieve(limit)
    return primes[n - 1]
