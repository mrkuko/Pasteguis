#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "options.hpp"
#include "platform/platform.hpp"
#include "timing.hpp"
#include "utf8.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <fcntl.h>
#include <io.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

std::vector<std::string> collectArgs(int argc, char** argv) {
    std::vector<std::string> args;
#ifdef _WIN32
    // argv is in the ANSI code page; re-read the command line as UTF-16 to keep --text intact.
    (void)argc; (void)argv;
    int n = 0;
    LPWSTR* w = CommandLineToArgvW(GetCommandLineW(), &n);
    for (int i = 1; i < n; ++i) {
        int len = WideCharToMultiByte(CP_UTF8, 0, w[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(len > 0 ? len - 1 : 0, '\0');
        if (len > 1) WideCharToMultiByte(CP_UTF8, 0, w[i], -1, s.data(), len, nullptr, nullptr);
        args.push_back(std::move(s));
    }
    LocalFree(w);
#else
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
#endif
    return args;
}

// Sleeps for `ms`, returning false early if the user holds the abort key.
bool abortableSleep(double ms) {
    const auto end = Clock::now() + std::chrono::duration_cast<Clock::duration>(
                                        std::chrono::duration<double, std::milli>(ms));
    while (true) {
        if (pg::platform::abortRequested()) return false;
        const auto now = Clock::now();
        if (now >= end) return true;
        const auto left = end - now;
        std::this_thread::sleep_for(std::min<Clock::duration>(left, std::chrono::milliseconds(10)));
    }
}

std::string formatDuration(double sec) {
    char buf[64];
    if (sec < 60) std::snprintf(buf, sizeof buf, "%.1fs", sec);
    else std::snprintf(buf, sizeof buf, "%dm %02ds", static_cast<int>(sec) / 60, static_cast<int>(sec) % 60);
    return buf;
}

bool loadText(const pg::Options& opt, std::string& utf8, std::string& error) {
    switch (opt.source) {
    case pg::Source::Text:
        utf8 = opt.text;
        return true;
    case pg::Source::Stdin:
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        utf8.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
        return true;
    case pg::Source::Clipboard:
        return pg::platform::readClipboard(utf8, error);
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    pg::Options opt;
    std::string error;
    if (!pg::parseOptions(collectArgs(argc, argv), opt, error)) {
        std::fprintf(stderr, "error: %s\n\n%s", error.c_str(), pg::usageText().c_str());
        return 2;
    }
    if (opt.showHelp) {
        std::fputs(pg::usageText().c_str(), stdout);
        return 0;
    }

    if (!pg::platform::init(error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    pg::platform::setPreferUnicode(opt.preferUnicode);

    std::string raw;
    if (!loadText(opt, raw, error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        pg::platform::shutdown();
        return 1;
    }
    const std::u32string text = pg::normalizeNewlines(pg::decodeUtf8(raw));
    if (text.empty()) {
        std::fprintf(stderr, "nothing to type (empty input)\n");
        pg::platform::shutdown();
        return 1;
    }

    const uint64_t seed = opt.hasSeed ? opt.seed : std::random_device{}();
    pg::TypingTimer timer(opt.timing, seed);

    if (opt.dryRun) {
        double total = 0, longest = 0;
        for (size_t i = 0; i < text.size(); ++i) {
            const double d = timer.nextDelayMs();
            total += d;
            if (d > longest) longest = d;
        }
        std::printf("characters : %zu\n", text.size());
        std::printf("avg delay  : %.0f ms/key (base %.0f ms)\n", total / text.size(), opt.timing.baseDelayMs);
        std::printf("longest gap: %.0f ms\n", longest);
        std::printf("est. time  : %s\n", formatDuration(total / 1000.0).c_str());
        pg::platform::shutdown();
        return 0;
    }

    std::fprintf(stderr, "Typing %zu characters in %.0fs - focus the target window now. Hold Esc to abort.\n",
                 text.size(), opt.startDelaySec);
    if (!abortableSleep(opt.startDelaySec * 1000.0)) {
        std::fprintf(stderr, "aborted\n");
        pg::platform::shutdown();
        return 130;
    }

    size_t typed = 0;
    bool aborted = false;
    for (char32_t cp : text) {
        pg::platform::typeCodepoint(cp);
        ++typed;
        if (!abortableSleep(timer.nextDelayMs())) { aborted = true; break; }
    }

    pg::platform::shutdown();
    if (aborted) {
        std::fprintf(stderr, "aborted after %zu of %zu characters\n", typed, text.size());
        return 130;
    }
    std::fprintf(stderr, "done\n");
    return 0;
}
