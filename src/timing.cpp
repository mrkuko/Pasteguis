#include "timing.hpp"

#include <algorithm>
#include <cmath>

namespace pg {

TypingTimer::TypingTimer(const TimingConfig& cfg, uint64_t seed) : cfg_(cfg), rng_(seed) {}

double TypingTimer::nextDelayMs() {
    double delay = cfg_.baseDelayMs;

    // Log-normal jitter with mean 1: right-skewed like real inter-key intervals
    // (occasional long gaps, never negative).
    const double sigma = cfg_.jitterPct / 100.0;
    if (sigma > 0.0) {
        std::normal_distribution<double> n(0.0, sigma);
        delay *= std::exp(n(rng_) - sigma * sigma / 2.0);
    }

    if (hiccupRemaining_ > 0) {
        delay *= hiccupFactor_;
        --hiccupRemaining_;
    } else if (cfg_.hiccupChancePct > 0.0) {
        std::uniform_real_distribution<double> u(0.0, 100.0);
        if (u(rng_) < cfg_.hiccupChancePct) {
            std::uniform_int_distribution<int> len(cfg_.hiccupLenMin, cfg_.hiccupLenMax);
            std::uniform_real_distribution<double> slow(cfg_.hiccupSlowMin, cfg_.hiccupSlowMax);
            hiccupRemaining_ = len(rng_) - 1;  // this keystroke is the first of the episode
            hiccupFactor_ = slow(rng_);
            delay *= hiccupFactor_;
        }
    }

    return std::max(delay, 1.0);
}

} // namespace pg
