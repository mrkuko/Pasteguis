#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "options.hpp"
#include "timing.hpp"
#include "utf8.hpp"

static int failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static double mean(pg::TypingTimer& t, int n) {
    double s = 0;
    for (int i = 0; i < n; ++i) s += t.nextDelayMs();
    return s / n;
}

static void testTiming() {
    CHECK(std::fabs(pg::wpmToDelayMs(60) - 200.0) < 1e-9);

    // No jitter, no hiccups: exact base delay.
    pg::TimingConfig fixed;
    fixed.baseDelayMs = 100;
    fixed.jitterPct = 0;
    fixed.hiccupChancePct = 0;
    pg::TypingTimer t0(fixed, 1);
    CHECK(t0.nextDelayMs() == 100.0);

    // Jitter keeps the mean near base.
    pg::TimingConfig jit = fixed;
    jit.jitterPct = 30;
    pg::TypingTimer t1(jit, 2);
    const double m = mean(t1, 20000);
    CHECK(m > 95 && m < 105);

    // Hiccups only ever slow typing down and raise the mean.
    pg::TimingConfig hic = fixed;
    hic.hiccupChancePct = 10;
    pg::TypingTimer t2(hic, 3);
    double lo = 1e18;
    for (int i = 0; i < 5000; ++i) lo = std::min(lo, t2.nextDelayMs());
    CHECK(lo >= 100.0 - 1e-9);
    pg::TypingTimer t3(hic, 3);
    CHECK(mean(t3, 20000) > 120);

    // Same seed, same sequence.
    pg::TypingTimer a(hic, 42), b(hic, 42);
    for (int i = 0; i < 100; ++i) CHECK(a.nextDelayMs() == b.nextDelayMs());

    // A hiccup lasts exactly its configured length.
    pg::TimingConfig one = fixed;
    one.hiccupChancePct = 100;
    one.hiccupLenMin = one.hiccupLenMax = 4;
    one.hiccupSlowMin = one.hiccupSlowMax = 3;
    pg::TypingTimer t4(one, 5);
    for (int i = 0; i < 4; ++i) CHECK(t4.nextDelayMs() == 300.0);
}

static void testOptions() {
    pg::Options o;
    std::string err;
    CHECK(pg::parseOptions({}, o, err));
    CHECK(std::fabs(o.timing.baseDelayMs - 200.0) < 1e-9);

    CHECK(pg::parseOptions({"--wpm", "120", "--hiccup-len", "2-5", "--text", "hi", "--seed", "7"}, o, err));
    CHECK(std::fabs(o.timing.baseDelayMs - 100.0) < 1e-9);
    CHECK(o.timing.hiccupLenMin == 2 && o.timing.hiccupLenMax == 5);
    CHECK(o.source == pg::Source::Text && o.text == "hi");
    CHECK(o.hasSeed && o.seed == 7);

    pg::Options d;
    CHECK(pg::parseOptions({"--wpm", "120", "--delay", "50"}, d, err));
    CHECK(d.timing.baseDelayMs == 50.0);  // --delay beats --wpm

    pg::Options bad;
    CHECK(!pg::parseOptions({"--wpm"}, bad, err));
    CHECK(!pg::parseOptions({"--wpm", "abc"}, bad, err));
    CHECK(!pg::parseOptions({"--hiccup-len", "8-3"}, bad, err));
    CHECK(!pg::parseOptions({"--hiccup-chance", "150"}, bad, err));
    CHECK(!pg::parseOptions({"--bogus"}, bad, err));
}

static void testUtf8() {
    const std::string s = "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";  // a é € 😀
    const std::u32string u = pg::decodeUtf8(s);
    CHECK(u.size() == 4);
    CHECK(u[1] == 0xE9 && u[2] == 0x20AC && u[3] == 0x1F600);
    CHECK(pg::encodeUtf8(u[3]) == "\xF0\x9F\x98\x80");
    CHECK(pg::decodeUtf8("\xE2\x82").size() == 1);  // truncated -> U+FFFD
    CHECK(pg::normalizeNewlines(U"a\r\nb\rc\n") == U"a\nb\nc\n");
}

int main() {
    testTiming();
    testOptions();
    testUtf8();
    if (failures == 0) std::puts("all tests passed");
    return failures == 0 ? 0 : 1;
}
