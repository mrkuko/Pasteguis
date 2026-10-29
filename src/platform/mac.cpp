#include "platform.hpp"

#include <ApplicationServices/ApplicationServices.h>

#include <cstdio>

namespace pg::platform {
namespace {

constexpr CGKeyCode kReturn = 36;
constexpr CGKeyCode kTab = 48;
constexpr CGKeyCode kEscape = 53;

void post(CGKeyCode code, const UniChar* chars, UniCharCount n) {
    for (bool down : {true, false}) {
        CGEventRef ev = CGEventCreateKeyboardEvent(nullptr, code, down);
        if (!ev) return;
        if (chars) CGEventKeyboardSetUnicodeString(ev, n, chars);
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }
}

} // namespace

bool init(std::string&) {
    if (!AXIsProcessTrusted()) {
        std::fprintf(stderr,
                     "warning: this terminal is not allowed to control the computer. Grant it access in\n"
                     "System Settings > Privacy & Security > Accessibility, otherwise no keys will be typed.\n");
    }
    return true;
}

void shutdown() {}
void setPreferUnicode(bool) {}

bool readClipboard(std::string& utf8, std::string& error) {
    FILE* p = popen("pbpaste", "r");
    if (!p) {
        error = "cannot run pbpaste";
        return false;
    }
    utf8.clear();
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) utf8.append(buf, n);
    pclose(p);
    return true;
}

void typeCodepoint(char32_t cp) {
    if (cp == U'\n') { post(kReturn, nullptr, 0); return; }
    if (cp == U'\t') { post(kTab, nullptr, 0); return; }
    if (cp < 0x20 || cp == 0x7F) return;

    UniChar units[2];
    UniCharCount n = 1;
    if (cp > 0xFFFF) {
        const char32_t v = cp - 0x10000;
        units[0] = static_cast<UniChar>(0xD800 + (v >> 10));
        units[1] = static_cast<UniChar>(0xDC00 + (v & 0x3FF));
        n = 2;
    } else {
        units[0] = static_cast<UniChar>(cp);
    }
    post(0, units, n);
}

bool abortRequested() {
    return CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kEscape);
}

} // namespace pg::platform
