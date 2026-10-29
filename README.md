# Pasteguise

[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](LICENSE)
![Platforms](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C)

Types your clipboard as a stream of individual keystrokes, with human-like timing. Useful where normal pasting is blocked or unavailable: remote consoles, VMs, some web terminals and login fields.

You control the average speed, how irregular it is, and random slow-downs ("hiccups").

## Features

- Cross-platform: Windows, Linux (X11) and macOS
- Speed by words-per-minute or a fixed per-key delay
- Per-key random jitter (right-skewed, like real typing)
- Random hiccups: short spells of slowed-down typing
- Reproducible runs with `--seed`
- `--dry-run` to preview the total typing time
- Hold **Esc** to abort at any moment
- Full Unicode, including characters outside your keyboard layout

## Build

Requires CMake 3.16+ and a C++17 compiler.

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

| Platform | Extra requirements |
|----------|--------------------|
| Windows  | Visual Studio 2019+ or MinGW |
| Linux    | `libx11-dev libxtst-dev`, plus one of `xclip`, `xsel`, `wl-clipboard` |
| macOS    | Xcode command line tools; grant your terminal **Accessibility** permission |

## Usage

```sh
pasteguise                           # type the clipboard at 60 wpm after a 5 s countdown
pasteguise --wpm 90 --jitter 40      # faster and more irregular
pasteguise --delay 120 --hiccup-chance 5 --hiccup-len 4-10 --hiccup-slow 2-6
pasteguise --text "hello" --dry-run  # estimate the time, type nothing
echo "from a pipe" | pasteguise --stdin
```

Start it, then click into the target window before the countdown ends.

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `--text <string>` | clipboard | Type this text instead of the clipboard |
| `--stdin` | | Read the text from standard input |
| `--wpm <n>` | 60 | Average speed in words per minute (5 chars/word) |
| `--delay <ms>` | | Average delay per key; overrides `--wpm` |
| `--jitter <pct>` | 30 | Random variation of each key's delay |
| `--hiccup-chance <pct>` | 2 | Chance per key to start a slow spell |
| `--hiccup-len <a-b>` | 3-8 | Keystrokes per slow spell |
| `--hiccup-slow <a-b>` | 2-5 | Delay multiplier during a spell |
| `--no-hiccups` | | Disable hiccups |
| `--start-delay <sec>` | 5 | Wait before typing |
| `--seed <n>` | random | Reproducible randomness |
| `--dry-run` | | Show planned timing; type nothing |
| `--unicode` | | Windows: send Unicode packets instead of layout scancodes |

## How the timing works

Each keystroke is followed by a delay of `base × jitter × hiccup`:

- **base** comes from `--wpm` or `--delay`.
- **jitter** is log-normal noise with mean 1, so the average speed stays put while occasional longer gaps appear.
- **hiccup**: with probability `--hiccup-chance` per key, a slow spell begins. The next `--hiccup-len` keys are all slowed by one random factor drawn from `--hiccup-slow`.

## Platform notes

- **Windows:** keys are sent as layout scancodes (works in remote desktop and VMs). Caps Lock is compensated. Dead-key layouts and emoji fall back to Unicode input; use `--unicode` to force it.
- **Linux:** X11/XWayland only (XTest). Pure Wayland sessions are not supported.
- **macOS:** needs Accessibility permission.
- Editors with auto-indent may double indentation when typing code.

## License

Copyright (C) 2026 Emanuel Cisar

Pasteguise is free software, licensed under the [GNU General Public License v2.0](LICENSE). You may use, modify and redistribute it, but you must keep this copyright notice and license, and distribute derivative works under the same license with source available.
