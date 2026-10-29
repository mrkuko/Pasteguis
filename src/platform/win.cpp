#include "platform.hpp"

#include <windows.h>

#include <vector>

namespace pg::platform {
namespace {

bool g_preferUnicode = false;

INPUT scanInput(WORD vk, bool up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (up ? KEYEVENTF_KEYUP : 0);
    return in;
}

INPUT unicodeInput(wchar_t ch, bool up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = ch;
    in.ki.dwFlags = KEYEVENTF_UNICODE | (up ? KEYEVENTF_KEYUP : 0);
    return in;
}

void send(std::vector<INPUT>& inputs) {
    if (!inputs.empty()) SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

void typeUnicodeUnit(wchar_t ch) {
    std::vector<INPUT> in{unicodeInput(ch, false), unicodeInput(ch, true)};
    send(in);
}

// Sends the physical key sequence for a virtual key plus modifier state
// (bit 0 shift, bit 1 ctrl, bit 2 alt - as returned by VkKeyScanW).
void typeVk(WORD vk, int mods) {
    std::vector<INPUT> in;
    if (mods & 1) in.push_back(scanInput(VK_SHIFT, false));
    if (mods & 2) in.push_back(scanInput(VK_CONTROL, false));
    if (mods & 4) in.push_back(scanInput(VK_MENU, false));
    in.push_back(scanInput(vk, false));
    in.push_back(scanInput(vk, true));
    if (mods & 4) in.push_back(scanInput(VK_MENU, true));
    if (mods & 2) in.push_back(scanInput(VK_CONTROL, true));
    if (mods & 1) in.push_back(scanInput(VK_SHIFT, true));
    send(in);
}

} // namespace

bool init(std::string&) { return true; }
void shutdown() {}
void setPreferUnicode(bool on) { g_preferUnicode = on; }

bool readClipboard(std::string& utf8, std::string& error) {
    if (!OpenClipboard(nullptr)) {
        error = "cannot open the clipboard";
        return false;
    }
    bool ok = false;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (!h) {
        error = "clipboard does not contain text";
    } else if (const wchar_t* w = static_cast<const wchar_t*>(GlobalLock(h))) {
        int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
        if (len > 1) {
            utf8.assign(static_cast<size_t>(len - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, w, -1, utf8.data(), len, nullptr, nullptr);
        } else {
            utf8.clear();
        }
        GlobalUnlock(h);
        ok = true;
    } else {
        error = "cannot read clipboard data";
    }
    CloseClipboard();
    return ok;
}

void typeCodepoint(char32_t cp) {
    if (cp == U'\n') { typeVk(VK_RETURN, 0); return; }
    if (cp == U'\t') { typeVk(VK_TAB, 0); return; }
    if (cp < 0x20 || cp == 0x7F) return;  // other control characters have no key

    if (cp > 0xFFFF) {  // outside the BMP: surrogate pair as Unicode packets
        char32_t v = cp - 0x10000;
        typeUnicodeUnit(static_cast<wchar_t>(0xD800 + (v >> 10)));
        typeUnicodeUnit(static_cast<wchar_t>(0xDC00 + (v & 0x3FF)));
        return;
    }

    const wchar_t ch = static_cast<wchar_t>(cp);
    if (!g_preferUnicode) {
        const SHORT r = VkKeyScanW(ch);
        if (r != -1) {
            const WORD vk = LOBYTE(r);
            int mods = HIBYTE(r);
            // VkKeyScan ignores Caps Lock; compensate so letters keep their case.
            if (vk >= 'A' && vk <= 'Z' && (GetKeyState(VK_CAPITAL) & 1)) mods ^= 1;
            if ((mods & ~7) == 0) {
                typeVk(vk, mods);
                return;
            }
        }
    }
    typeUnicodeUnit(ch);
}

bool abortRequested() { return (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0; }

} // namespace pg::platform
