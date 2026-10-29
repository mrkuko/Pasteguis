#include "platform.hpp"

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>

namespace pg::platform {
namespace {

Display* g_dpy = nullptr;

KeySym keysymFor(char32_t cp) {
    if (cp == U'\n') return XK_Return;
    if (cp == U'\t') return XK_Tab;
    if (cp < 0x20 || cp == 0x7F) return NoSymbol;
    // Latin-1 keysyms equal their code points; everything else uses the 0x01000000 Unicode range.
    if (cp < 0x100) return static_cast<KeySym>(cp);
    return static_cast<KeySym>(0x01000000u | cp);
}

void key(KeyCode kc, bool down) {
    XTestFakeKeyEvent(g_dpy, kc, down ? True : False, CurrentTime);
}

// Finds a keycode with no symbols bound, to borrow for characters the layout lacks.
KeyCode findScratchKeycode() {
    int minKc = 0, maxKc = 0;
    XDisplayKeycodes(g_dpy, &minKc, &maxKc);
    for (int kc = maxKc; kc >= minKc; --kc) {
        bool empty = true;
        for (int level = 0; level < 4 && empty; ++level)
            if (XkbKeycodeToKeysym(g_dpy, static_cast<KeyCode>(kc), 0, level) != NoSymbol) empty = false;
        if (empty) return static_cast<KeyCode>(kc);
    }
    return 0;
}

void typeViaRemap(KeySym ks) {
    const KeyCode kc = findScratchKeycode();
    if (!kc) return;
    KeySym map[1] = {ks};
    XChangeKeyboardMapping(g_dpy, kc, 1, map, 1);
    XSync(g_dpy, False);
    std::this_thread::sleep_for(std::chrono::milliseconds(8));  // let clients see MappingNotify
    key(kc, true);
    key(kc, false);
    XSync(g_dpy, False);
    KeySym none[1] = {NoSymbol};
    XChangeKeyboardMapping(g_dpy, kc, 1, none, 1);
    XSync(g_dpy, False);
}

std::string runCommand(const char* cmd, bool& ok) {
    std::string out;
    FILE* p = popen(cmd, "r");
    if (!p) { ok = false; return out; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
    ok = pclose(p) == 0;
    return out;
}

} // namespace

bool init(std::string& error) {
    g_dpy = XOpenDisplay(nullptr);
    if (!g_dpy) {
        error = "cannot open X display (Wayland-only sessions are not supported; use an X11/XWayland session)";
        return false;
    }
    int ev, er, maj, min;
    if (!XTestQueryExtension(g_dpy, &ev, &er, &maj, &min)) {
        error = "X server lacks the XTest extension";
        return false;
    }
    return true;
}

void shutdown() {
    if (g_dpy) {
        XCloseDisplay(g_dpy);
        g_dpy = nullptr;
    }
}

void setPreferUnicode(bool) {}

bool readClipboard(std::string& utf8, std::string& error) {
    // Try the tools that exist on typical desktops, Wayland first if applicable.
    static const char* cmds[] = {
        "wl-paste --no-newline 2>/dev/null",
        "xclip -selection clipboard -o 2>/dev/null",
        "xsel --clipboard --output 2>/dev/null",
    };
    static const int waylandOrder[] = {0, 1, 2};
    static const int x11Order[] = {1, 2, 0};
    const int* order = std::getenv("WAYLAND_DISPLAY") ? waylandOrder : x11Order;
    for (int k = 0; k < 3; ++k) {
        bool ok = false;
        std::string out = runCommand(cmds[order[k]], ok);
        if (ok) { utf8 = std::move(out); return true; }
    }
    error = "cannot read the clipboard (install xclip, xsel or wl-clipboard)";
    return false;
}

void typeCodepoint(char32_t cp) {
    const KeySym ks = keysymFor(cp);
    if (ks == NoSymbol) return;

    const KeyCode kc = XKeysymToKeycode(g_dpy, ks);
    bool shift = false, direct = false;
    if (kc) {
        if (XkbKeycodeToKeysym(g_dpy, kc, 0, 0) == ks) direct = true;
        else if (XkbKeycodeToKeysym(g_dpy, kc, 0, 1) == ks) { direct = true; shift = true; }
    }

    if (direct) {
        const KeyCode shiftKc = XKeysymToKeycode(g_dpy, XK_Shift_L);
        if (shift) key(shiftKc, true);
        key(kc, true);
        key(kc, false);
        if (shift) key(shiftKc, false);
        XFlush(g_dpy);
    } else {
        typeViaRemap(ks);
    }
}

bool abortRequested() {
    char keys[32];
    XQueryKeymap(g_dpy, keys);
    const KeyCode esc = XKeysymToKeycode(g_dpy, XK_Escape);
    return esc && (keys[esc / 8] & (1 << (esc % 8)));
}

} // namespace pg::platform
