#include "options.hpp"

#include <cerrno>
#include <cstdlib>

namespace pg {
namespace {

bool parseDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    double v = std::strtod(s.c_str(), &end);
    if (errno != 0 || end != s.c_str() + s.size()) return false;
    out = v;
    return true;
}

// "5" -> {5,5}; "3-8" -> {3,8}. Values are non-negative so '-' is unambiguous.
bool parseRange(const std::string& s, double& lo, double& hi) {
    size_t dash = s.find('-');
    if (dash == std::string::npos) {
        if (!parseDouble(s, lo)) return false;
        hi = lo;
        return true;
    }
    if (!parseDouble(s.substr(0, dash), lo) || !parseDouble(s.substr(dash + 1), hi)) return false;
    return lo <= hi;
}

} // namespace

std::string usageText() {
    return
        "Pasteguise - type the clipboard as if a human were typing it\n"
        "\n"
        "Usage: pasteguise [options]\n"
        "\n"
        "Input (default: clipboard):\n"
        "  --text <string>        type this text instead of the clipboard\n"
        "  --stdin                read the text to type from standard input\n"
        "\n"
        "Speed:\n"
        "  --wpm <n>              average typing speed in words/min (default 60)\n"
        "  --delay <ms>           average delay per keystroke; overrides --wpm\n"
        "  --jitter <pct>         random variation of each keystroke delay (default 30)\n"
        "\n"
        "Hiccups (random slow-downs):\n"
        "  --hiccup-chance <pct>  chance per keystroke to start a slow spell (default 2)\n"
        "  --hiccup-len <a-b>     keystrokes per slow spell (default 3-8)\n"
        "  --hiccup-slow <a-b>    delay multiplier during a spell (default 2-5)\n"
        "  --no-hiccups           disable hiccups\n"
        "\n"
        "Other:\n"
        "  --start-delay <sec>    wait before typing so you can focus the target (default 5)\n"
        "  --seed <n>             reproducible randomness\n"
        "  --dry-run              show the planned timing; type nothing\n"
        "  --unicode              Windows: send Unicode packets instead of layout scancodes\n"
        "  -h, --help             show this help\n"
        "\n"
        "Hold Esc at any time to abort while typing.\n";
}

bool parseOptions(const std::vector<std::string>& args, Options& o, std::string& error) {
    bool delaySet = false;
    double wpm = 60.0;
    double delayMs = 0.0;

    auto need = [&](size_t& i, const std::string& flag, std::string& val) {
        if (i + 1 >= args.size()) {
            error = "missing value for " + flag;
            return false;
        }
        val = args[++i];
        return true;
    };
    auto num = [&](size_t& i, const std::string& flag, double min, double& dst) {
        std::string v;
        if (!need(i, flag, v)) return false;
        if (!parseDouble(v, dst) || dst < min) {
            error = "invalid value for " + flag + ": " + v;
            return false;
        }
        return true;
    };
    auto range = [&](size_t& i, const std::string& flag, double min, double& lo, double& hi) {
        std::string v;
        if (!need(i, flag, v)) return false;
        if (!parseRange(v, lo, hi) || lo < min) {
            error = "invalid range for " + flag + ": " + v + " (expected N or A-B)";
            return false;
        }
        return true;
    };

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        std::string v;
        double d = 0, lo = 0, hi = 0;

        if (a == "-h" || a == "--help") {
            o.showHelp = true;
        } else if (a == "--text") {
            if (!need(i, a, o.text)) return false;
            o.source = Source::Text;
        } else if (a == "--stdin") {
            o.source = Source::Stdin;
        } else if (a == "--wpm") {
            if (!num(i, a, 0.1, wpm)) return false;
        } else if (a == "--delay") {
            if (!num(i, a, 1.0, delayMs)) return false;
            delaySet = true;
        } else if (a == "--jitter") {
            if (!num(i, a, 0.0, o.timing.jitterPct)) return false;
        } else if (a == "--hiccup-chance") {
            if (!num(i, a, 0.0, d)) return false;
            if (d > 100.0) { error = "--hiccup-chance must be 0-100"; return false; }
            o.timing.hiccupChancePct = d;
        } else if (a == "--hiccup-len") {
            if (!range(i, a, 1.0, lo, hi)) return false;
            o.timing.hiccupLenMin = static_cast<int>(lo);
            o.timing.hiccupLenMax = static_cast<int>(hi);
        } else if (a == "--hiccup-slow") {
            if (!range(i, a, 1.0, lo, hi)) return false;
            o.timing.hiccupSlowMin = lo;
            o.timing.hiccupSlowMax = hi;
        } else if (a == "--no-hiccups") {
            o.timing.hiccupChancePct = 0.0;
        } else if (a == "--start-delay") {
            if (!num(i, a, 0.0, o.startDelaySec)) return false;
        } else if (a == "--seed") {
            if (!need(i, a, v)) return false;
            char* end = nullptr;
            o.seed = std::strtoull(v.c_str(), &end, 10);
            if (v.empty() || *end != '\0') { error = "invalid value for --seed: " + v; return false; }
            o.hasSeed = true;
        } else if (a == "--dry-run") {
            o.dryRun = true;
        } else if (a == "--unicode") {
            o.preferUnicode = true;
        } else {
            error = "unknown option: " + a;
            return false;
        }
    }

    o.timing.baseDelayMs = delaySet ? delayMs : wpmToDelayMs(wpm);
    return true;
}

} // namespace pg
