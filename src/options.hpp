#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "timing.hpp"

namespace pg {

enum class Source { Clipboard, Text, Stdin };

struct Options {
    TimingConfig timing;
    Source source = Source::Clipboard;
    std::string text;            // for Source::Text (UTF-8)
    double startDelaySec = 5.0;  // time to focus the target window
    bool hasSeed = false;
    uint64_t seed = 0;
    bool dryRun = false;
    bool preferUnicode = false;  // Windows: inject Unicode packets instead of layout scancodes
    bool showHelp = false;
};

// Returns false and fills `error` on invalid input.
bool parseOptions(const std::vector<std::string>& args, Options& out, std::string& error);

std::string usageText();

} // namespace pg
