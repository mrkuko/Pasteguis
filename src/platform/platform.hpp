#pragma once

#include <string>

// Per-OS backend. Implemented in win.cpp / mac.cpp / linux_x11.cpp.
namespace pg::platform {

bool init(std::string& error);
void shutdown();

// Current clipboard text as UTF-8. Returns false (and sets error) if unavailable or not text.
bool readClipboard(std::string& utf8, std::string& error);

// Press and release the key that produces `cp`. '\n' is Enter, '\t' is Tab.
void typeCodepoint(char32_t cp);

// True while the user holds the abort key (Esc).
bool abortRequested();

// Windows only: inject Unicode packets instead of layout-based scancodes. No-op elsewhere.
void setPreferUnicode(bool on);

} // namespace pg::platform
