"""
Life Cipher — encode/decode life codes using prime mapping,
and find commonality between two encoded sequences.

A "life code" is any sequence of tokens (characters, codons, words).
Each unique token is mapped to a prime number; the sequence becomes
a list of primes whose product (or GCD between sequences) reveals
shared structure.
"""

import math
from prime_engine import nth_prime, factorize, is_prime


class LifeCipher:
    """Encodes sequences of tokens to prime-based numeric codes."""

    # Built-in alphabet: DNA codons → primes (first 64 primes)
    DNA_BASES = list("ACGT")

    def __init__(self, alphabet: list[str] | None = None):
        if alphabet is None:
            alphabet = list("ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789")
        self._token_to_prime: dict[str, int] = {}
        self._prime_to_token: dict[int, str] = {}
        for i, token in enumerate(alphabet, start=1):
            p = nth_prime(i)
            self._token_to_prime[token] = p
            self._prime_to_token[p] = token

    # ------------------------------------------------------------------
    # Encoding / Decoding
    # ------------------------------------------------------------------

    def encode(self, text: str) -> list[int]:
        """Convert each character in `text` to its prime code."""
        result = []
        for ch in text.upper():
            if ch not in self._token_to_prime:
                raise ValueError(f"Character {ch!r} not in cipher alphabet")
            result.append(self._token_to_prime[ch])
        return result

    def decode(self, codes: list[int]) -> str:
        """Convert a list of prime codes back to text."""
        out = []
        for code in codes:
            if code not in self._prime_to_token:
                raise ValueError(f"Code {code} has no mapping in this cipher")
            out.append(self._prime_to_token[code])
        return "".join(out)

    def fingerprint(self, text: str) -> int:
        """
        Compute the numeric fingerprint of a text as the product of its
        prime codes.  Two texts share a common factor iff they share a
        character.
        """
        product = 1
        for p in self.encode(text):
            product *= p
        return product

    # ------------------------------------------------------------------
    # Commonality analysis
    # ------------------------------------------------------------------

    def common_tokens(self, a: str, b: str) -> list[str]:
        """Return characters that appear in both `a` and `b`."""
        set_a = set(a.upper())
        set_b = set(b.upper())
        shared = sorted(set_a & set_b)
        return [ch for ch in shared if ch in self._token_to_prime]

    def commonality_score(self, a: str, b: str) -> float:
        """
        Jaccard-like score: |shared tokens| / |union of tokens|.
        Returns a value in [0, 1].
        """
        set_a = set(a.upper()) & self._token_to_prime.keys()
        set_b = set(b.upper()) & self._token_to_prime.keys()
        if not set_a and not set_b:
            return 0.0
        return len(set_a & set_b) / len(set_a | set_b)

    def gcd_fingerprint(self, a: str, b: str) -> int:
        """
        GCD of the two fingerprints: the product of primes shared by
        both sequences (with multiplicity of the lesser occurrence).
        """
        return math.gcd(self.fingerprint(a), self.fingerprint(b))

    def prime_pattern(self, text: str) -> dict[str, int]:
        """
        Return {character: prime_code} for all unique characters in text.
        Useful for visualising the prime mapping of a life code.
        """
        seen: dict[str, int] = {}
        for ch in text.upper():
            if ch in self._token_to_prime and ch not in seen:
                seen[ch] = self._token_to_prime[ch]
        return dict(sorted(seen.items()))
