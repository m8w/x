#pragma once
/*
 * Entropy.h — dual-source true-entropy RNG for fractal stream.
 *
 * Two physically-independent sources XOR-combined at every sample:
 *
 *   1. Hardware  (std::random_device → OS entropy pool / RDSEED).
 *      Always live. Reseeds m_rng_hw every 3 s from the kernel pool.
 *
 *   2. Quantum   (ANU vacuum-fluctuation API, fetched via curl in background).
 *      Reseeds m_rng_q every 60 s when network is reachable; degrades
 *      gracefully to hardware-only if the endpoint is down or curl is absent.
 *
 * XOR property: output is at least as unpredictable as the *better* of the
 * two sources, and uncorrelated with either.
 *
 * Thread safety: all public methods are safe to call from any thread.
 * The render thread never blocks on network I/O.
 */

#include <cstdint>
#include <random>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

class Entropy {
public:
    explicit Entropy(bool quantum = true, std::string api_key = "");
    ~Entropy();

    // ── Core draw — hardware XOR quantum at every sample ─────────────────────
    float    randomF();                     // uniform float in [0, 1)
    float    uniformF(float lo, float hi);  // uniform float in [lo, hi)
    int      uniformI(int lo, int hi);      // uniform int  in [lo, hi] inclusive
    uint64_t seed64();                      // raw 64-bit XOR (for re-seeding other RNGs)

    // ── Telemetry ─────────────────────────────────────────────────────────────
    struct Status {
        bool hw_live;
        bool q_live;
        int  hw_reseeds;
        int  q_reseeds;
        int  q_bytes;
    };
    Status status() const;

private:
    mutable std::mutex m_lock;
    std::mt19937_64    m_rng_hw;       // reseeded from OS entropy pool
    std::mt19937_64    m_rng_q;        // reseeded from ANU quantum API
    bool               m_q_live     = false;
    int                m_hw_reseeds = 0;
    int                m_q_reseeds  = 0;
    int                m_q_bytes    = 0;

    std::atomic<bool>  m_stop{false};
    std::thread        m_hw_thread;
    std::thread        m_q_thread;
    std::string        m_api_key;
    bool               m_quantum;

    uint64_t             draw64();      // must be called under m_lock
    void                 hwLoop();
    void                 qLoop();
    std::vector<uint8_t> fetchQuantum(int nbytes);
};

// Process-wide singleton — initialised on first call (C++11 thread-safe).
Entropy& globalEntropy();
