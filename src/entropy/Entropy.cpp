#include "Entropy.h"
#include <chrono>
#include <cstdio>
#include <cctype>
#include <algorithm>

// ── Construction / destruction ────────────────────────────────────────────────

Entropy::Entropy(bool quantum, std::string api_key)
    : m_api_key(std::move(api_key)), m_quantum(quantum)
{
    // Seed both generators from the OS entropy pool at startup
    std::random_device rd;
    m_rng_hw.seed(((uint64_t)rd() << 32) | rd());
    m_rng_q.seed (((uint64_t)rd() << 32) | rd());

    m_hw_thread = std::thread(&Entropy::hwLoop, this);
    if (quantum)
        m_q_thread = std::thread(&Entropy::qLoop, this);
}

Entropy::~Entropy() {
    m_stop.store(true);
    if (m_hw_thread.joinable()) m_hw_thread.join();
    if (m_q_thread.joinable())  m_q_thread.join();
}

// ── Core draw ─────────────────────────────────────────────────────────────────

uint64_t Entropy::draw64() {
    // Both generators fire on every sample; XOR makes output unpredictable
    // unless BOTH sources are broken simultaneously.
    return m_rng_hw() ^ m_rng_q();
}

float Entropy::randomF() {
    std::lock_guard<std::mutex> lk(m_lock);
    // Top-53-bit trick: exact [0,1) float with full mantissa precision
    return (float)((draw64() >> 11) * (1.0 / (1ULL << 53)));
}

float Entropy::uniformF(float lo, float hi) {
    return lo + (hi - lo) * randomF();
}

int Entropy::uniformI(int lo, int hi) {
    if (lo >= hi) return lo;
    return lo + (int)(randomF() * (float)(hi - lo + 1));
}

uint64_t Entropy::seed64() {
    std::lock_guard<std::mutex> lk(m_lock);
    return draw64();
}

Entropy::Status Entropy::status() const {
    std::lock_guard<std::mutex> lk(m_lock);
    return { true, m_q_live, m_hw_reseeds, m_q_reseeds, m_q_bytes };
}

// ── Hardware reseed loop (every 3 s) ─────────────────────────────────────────

void Entropy::hwLoop() {
    while (!m_stop.load()) {
        // Interruptible 3-second sleep
        for (int i = 0; i < 300 && !m_stop.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (m_stop.load()) break;

        std::random_device rd;
        uint64_t s = ((uint64_t)rd() << 32) | rd();
        std::lock_guard<std::mutex> lk(m_lock);
        m_rng_hw.seed(s);
        ++m_hw_reseeds;
    }
}

// ── Quantum reseed loop (ANU QRNG via curl) ───────────────────────────────────

void Entropy::qLoop() {
    double waitSecs = 2.0;  // first attempt fires quickly after startup
    while (!m_stop.load()) {
        // Interruptible sleep
        for (int i = 0; i < (int)(waitSecs * 10) && !m_stop.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (m_stop.load()) break;

        auto block = fetchQuantum(128);
        if (!block.empty()) {
            // Fold all fetched bytes into a 64-bit seed via XOR accumulation
            uint64_t seed = 0;
            for (int i = 0; i < (int)block.size(); ++i)
                seed = (seed << 8) | block[i];            // rolling shift+OR
            // XOR in 8-byte chunks again for extra mixing
            uint64_t acc = 0;
            for (int i = 0; i + 7 < (int)block.size(); i += 8) {
                uint64_t chunk = 0;
                for (int j = 0; j < 8; ++j) chunk = (chunk << 8) | block[i + j];
                acc ^= chunk;
            }
            seed ^= acc;

            {
                std::lock_guard<std::mutex> lk(m_lock);
                m_rng_q.seed(seed);
                m_q_live = true;
                ++m_q_reseeds;
                m_q_bytes += (int)block.size();
            }
            waitSecs = 60.0;   // nominal interval between reseeds
        } else {
            {
                std::lock_guard<std::mutex> lk(m_lock);
                m_q_live = false;
            }
            waitSecs = std::min(waitSecs * 2.0, 600.0);  // exponential back-off
        }
    }
}

// ── ANU QRNG fetch via curl subprocess ────────────────────────────────────────

std::vector<uint8_t> Entropy::fetchQuantum(int nbytes) {
    // Sanitise the API key — only alphanumeric, hyphen, underscore allowed
    std::string safeKey;
    for (unsigned char c : m_api_key)
        if (std::isalnum(c) || c == '-' || c == '_') safeKey += (char)c;

    std::string cmd;
    if (!safeKey.empty()) {
        // New keyed ANU endpoint: api.quantumnumbers.anu.edu.au
        cmd = "curl -s -m 10 -H \"x-api-key: " + safeKey + "\" "
              "\"https://api.quantumnumbers.anu.edu.au/?length=" +
              std::to_string(std::min(nbytes, 1024)) + "&type=uint8\" 2>/dev/null";
    } else {
        // Legacy keyless endpoint — heavily rate-limited but free
        cmd = "curl -s -m 10 "
              "\"https://qrng.anu.edu.au/API/jsonI.php?length=" +
              std::to_string(std::min(nbytes, 64)) + "&type=uint8\" 2>/dev/null";
    }

    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return {};

    std::string resp;
    char buf[512];
    while (fgets(buf, sizeof(buf), fp)) resp += buf;
    pclose(fp);

    // Minimal JSON parse — find "data":[ ... ] and extract uint8 values
    auto dpos = resp.find("\"data\":[");
    if (dpos == std::string::npos) return {};
    auto bstart = resp.find('[', dpos);
    if (bstart == std::string::npos) return {};
    ++bstart;
    auto bend = resp.find(']', bstart);
    if (bend == std::string::npos) return {};

    std::vector<uint8_t> result;
    std::string num;
    for (size_t i = bstart; i < bend; ++i) {
        char c = resp[i];
        if (std::isdigit((unsigned char)c)) {
            num += c;
        } else if (!num.empty()) {
            result.push_back((uint8_t)(std::stoi(num) & 0xFF));
            num.clear();
        }
    }
    if (!num.empty())
        result.push_back((uint8_t)(std::stoi(num) & 0xFF));

    return result;
}

// ── Singleton ─────────────────────────────────────────────────────────────────

Entropy& globalEntropy() {
    // quantum=true with no API key: uses the rate-limited legacy ANU endpoint.
    // To use the keyed endpoint: change to Entropy(true, "your-key-here").
    static Entropy ent(true, "");
    return ent;
}
