#pragma once

#include <string>

namespace pg {

// Lenient UTF-8 -> UTF-32 decoder. Invalid bytes become U+FFFD instead of failing,
// since clipboard contents are arbitrary.
inline std::u32string decodeUtf8(const std::string& s) {
    std::u32string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        int extra = 0;
        char32_t cp = 0;
        if (c < 0x80) { cp = c; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { out.push_back(0xFFFD); ++i; continue; }

        if (extra > 0 && i + extra >= s.size()) {  // truncated sequence at end of input
            out.push_back(0xFFFD);
            break;
        }
        bool ok = true;
        for (int k = 1; k <= extra; ++k) {
            unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { out.push_back(0xFFFD); ++i; continue; }
        out.push_back(cp);
        i += 1 + extra;
    }
    return out;
}

inline std::string encodeUtf8(char32_t cp) {
    std::string o;
    if (cp < 0x80) o.push_back(static_cast<char>(cp));
    else if (cp < 0x800) {
        o.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        o.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        o.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        o.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        o.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        o.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        o.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        o.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        o.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return o;
}

// Normalizes CRLF / lone CR to LF so each line break is exactly one Enter press.
inline std::u32string normalizeNewlines(const std::u32string& in) {
    std::u32string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == U'\r') {
            out.push_back(U'\n');
            if (i + 1 < in.size() && in[i + 1] == U'\n') ++i;
        } else {
            out.push_back(in[i]);
        }
    }
    return out;
}

} // namespace pg
