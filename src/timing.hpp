#pragma once

#include <cstdint>
#include <random>

namespace pg {

struct TimingConfig {
    double baseDelayMs = 200.0;   // average delay between keystrokes
    double jitterPct = 30.0;      // per-key random variation (log-normal sigma, in %)

    // Hiccups: random slow episodes where several consecutive keys are delayed.
    double hiccupChancePct = 2.0; // chance per keystroke of starting an episode
    int hiccupLenMin = 3;         // episode length in keystrokes
    int hiccupLenMax = 8;
    double hiccupSlowMin = 2.0;   // delay multiplier during an episode
    double hiccupSlowMax = 5.0;
};

// Converts words-per-minute (5 chars per word) to a per-keystroke delay.
inline double wpmToDelayMs(double wpm) { return 60000.0 / (wpm * 5.0); }

// Produces the delay to wait after each keystroke. Deterministic for a given seed.
class TypingTimer {
public:
    TypingTimer(const TimingConfig& cfg, uint64_t seed);

    double nextDelayMs();

private:
    TimingConfig cfg_;
    std::mt19937_64 rng_;
    int hiccupRemaining_ = 0;
    double hiccupFactor_ = 1.0;
};

} // namespace pg
