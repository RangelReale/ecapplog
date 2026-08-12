# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A networked logging GUI: a Qt 5.15 Widgets / C++17 desktop application that listens on TCP
`localhost:13991` and displays log entries pushed to it by other programs. Cross-platform
(Windows, macOS, Linux); macOS is the platform it is developed and released on here.

The wire protocol is length-prefixed binary frames with JSON payloads, documented in
`README.md` under Protocol. Docking is provided by Qt Advanced Docking System, fetched at
configure time via `FetchContent` (tag 3.8.2) — the first configure of a build tree needs
network access.

## Build and run

```sh
cmake --build build-release            # builds build-release/bin/ecapplog.app
```

Use the existing `build-release` tree. It is the one configured for packaging:
`ECAPPLOG_BUILD_INSTALLER=ON`, `ECAPPLOG_NOTARIZE=ON`, and the Developer ID signing
identity. The other `build*` directories are scratch and gitignored.

CMake options (`CMakeLists.txt:35-55`): `ECAPPLOG_BUILD_INSTALLER`,
`ECAPPLOG_WITH_DEBUG_MENUS`, `ECAPPLOG_BUILD_CONSOLE_EXE`,
`ECAPPLOG_DISABLE_XCODE_CODESIGNING`, `ECAPPLOG_NOTARIZE`, `ECAPPLOG_MAC_SIGN_IDENTITY`,
`ECAPPLOG_NOTARY_PROFILE`.

The version number lives in exactly one place, `project(ECAppLog ... VERSION x.y.z)` at
`CMakeLists.txt:12`. The macOS bundle's `CFBundleShortVersionString` and `CFBundleVersion`
derive from `PROJECT_VERSION`.

**There are no tests, no CI, and no lint or format configuration.** Verification means
building and running the application — do not go looking for a test command.

To get log data in, either build with `-DECAPPLOG_WITH_DEBUG_MENUS=ON`, which adds
*Debug → Publish logs* (248 synthetic lines across three categories), or write a throwaway
TCP client: frames are `<quint8 cmd><quint32 size><payload>` big-endian, starting with a
banner (`cmd 99`, payload `ECAPPLOG <appname>`) and then one `cmd 0` frame of log JSON per
entry (`Server.cpp:105-174`).

### Two build-output gotchas

**After changing the version, rebuild before launching.** Reconfiguring regenerates
`Contents/Info.plist` *after* the bundle was signed, so `codesign --verify` reports
`invalid Info.plist` and macOS refuses to launch it with `Launchd job spawn failed`
(POSIX 163). `touch src/main.cpp && cmake --build build-release` forces the relink that
re-runs macdeployqt's signing step.

`ERROR: Cannot resolve rpath "@rpath/libwebp.7.dylib"` lines during the build are
pre-existing macdeployqt noise about image-format plugins that are pruned immediately
afterwards. Not a failure.

## Packaging and release (macOS)

```sh
cd build-release && cpack              # signed, notarized, stapled DMG
```

Notarization needs network access and the `ecapplog-notary` notarytool keychain profile, and
takes several minutes. For a local-only test image, `cpack -D CPACK_MAC_NOTARIZE=OFF` — that
DMG is deliberately rejected by the release gate below, because an un-notarized DMG installs
fine on the machine that built it and is blocked by Gatekeeper everywhere else.

```sh
scripts/release-mac.sh --build-dir build-release --version X.Y.Z --verify-only
```

Equivalent to `make release-check`: gates on `codesign --verify`, `xcrun stapler validate`,
and `spctl` reporting `source=Notarized Developer ID`. Drop `--verify-only` and it also
creates the annotated `vX.Y.Z` tag, pushes it, and opens a **draft** GitHub release with the
DMG attached (`--publish` skips the draft; `--notes-file` supplies release notes, which past
releases have used). Its confirmation prompt reads stdin, so from a non-interactive shell
pass `-y` or it aborts.

## Architecture

Three layers joined by Qt signals. Understanding how one log entry reaches one tab requires
all three.

**`Server`** (`Server.h/.cpp`) — a `QTcpServer`. One `ApplicationInfo` per connection, named
`<banner name>:<seq>` with the sequence appended per connection, so a reconnecting client
gets a distinct application tab. Frames are read inside a `QDataStream` transaction, so a
partial frame waits for more data instead of being misparsed. JSON is parsed off the GUI
thread (`ServerWorker` on `_workerthread`) and arrives back as `onJsonReceived`.

**`Data`** (`Data.h/.cpp`) — the model layer, no widgets. Owns
`Data_Application` → `Data_Category` → `LogModel`, and emits `newApplication`,
`newCategory`, `logAmount`, `logItemsPerSecond`, `newFilter` for `MainWindow` to turn into
widgets.

`Data::log` is the routing hub and the first thing to read. A single entry can land in
several categories: the one it was sent to, plus `ALL` when categories are being grouped,
plus `ERROR` when the priority is an error or warning, plus each of `extra_categories`, plus
one copy per matching filter. Things that are easy to get wrong there:

- A `logged` list guarantees each category takes a given entry at most once, however many
  rules name it.
- `Category::CAT_ALL` / `Category::CAT_ERROR` are in `Config.h` and must match the tab
  ordering rule in `MainWindow`. `"FILTER"`, `"ECAPPLOG"` and `"<unknown>"` are still bare
  literals. A blank category becomes `<unknown>` in `internalLog`.
- Grouping is the global *View → Group categories* setting **OR** a per-application flag.
- **Filters are a second routing layer, not a view filter.** A `Data_Filter` is a pseudo
  application named `FILTER<n>` that receives copies of matching entries; membership keys
  are `<appname-without-:seq>$$<category>`.
- **Entries are batched.** `Data_Category::addLog` holds items back unless more than 10 are
  queued or the rate is below 15/s, with a 1s timer (`checkExpiredLog`) flushing the rest.
  A newly created tab can therefore take up to a second to appear — that is not a bug.

**`MainWindow`** (`MainWindow.h/.cpp`, ~1000 lines and most of the UI) — mirrors `Data`'s
tree with `Main_Application` / `Main_Category` and one `QListView` per category. Menus are
built inline in the constructor; there are no `.ui` files anywhere.

- Docking is `ads::CDockManager`, and log views can be floated out into genuine top-level
  windows. Two consequences: keyboard shortcuts must use `Qt::ApplicationShortcut` (helpers
  `setStandardShortcuts` and `setShortcut`), and anything that must touch *every* log view
  has to walk `_applicationlist` rather than the main window's children.
- Category tab order comes from `categoryRank()`: `ALL`, then `ERROR`, then other
  all-uppercase categories, then the rest. A new tab is inserted ahead of the first
  higher-ranked tab, with the rank read back from the tab text so tabs moved by hand survive.
- `LogDelegate` paints every row and does the `Highlight` word matching. Its `sizeHint` must
  stay in step with what `paint` draws, or the horizontal scrollbar stops short of the text —
  which is why highlight-word changes go through `doItemsLayout` rather than a viewport
  repaint.
- `LogModel` inserts the newest entry at row 0. Fields are exposed through the
  `MODELROLE_*` roles in `LogModel.h`.

## Conventions

Tabs for indentation, brace on its own line, `_camelCase` members. Comments in this
codebase carry a lot of hard-won rationale — they explain *why*, often at length. Match that
density rather than narrating what a line does.

Menu text uses `&` mnemonics, so a literal `&` in dynamic text must be doubled (see
`refreshHighlightMenu`). Settings go through `QSettings` under `com.rangelreale.ECAppLog`,
and only `group_categories` and `font_size` are persisted. macOS-only deviations, both in
`main.cpp` / `MainWindow`: the Fusion style is forced (to get scrollable tabs) and the
default font size is overridden to 16pt.

One name collision to keep straight: **two unrelated things are called "highlight"** — the
`Highlight` word list (`View → Highlight`) and the transient find-result row state
(`_findHighlightView`, `clearFindHighlight`).

## Driving the UI to verify a change

Menu bar items are scriptable, e.g.
`osascript -e 'tell application "System Events" to tell process "ecapplog" to click menu item "Group categories" of menu 1 of menu bar item "View" of menu bar 1'`,
and reading a `screencapture -R x,y,w,h` PNG is the practical way to check what the window
actually shows.

**Qt popup and context menus are absent from the accessibility tree**, though: `AXShowMenu`
followed by keystrokes never activates an item. Anything reachable only from a context menu —
toggling a category onto a filter, clearing a category — cannot be automated and has to be
checked by hand.
