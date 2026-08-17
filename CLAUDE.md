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

### Icons

The source of truth is `scripts/icon/*.svg`; everything in `src/resources/` (`.icns`, `.ico`,
and the `ecapplog*.png` set) is generated output that happens to be committed, so a Windows or
Linux build never needs `iconutil` or `rsvg-convert`. After editing an SVG run
`scripts/generate-icons.sh` and commit what it changes — nothing in the build regenerates them.

There are **three optical sizes**, and this is the part that is easy to undo by accident:

| master | used at | drawing |
| --- | --- | --- |
| `ecapplog-tiny.svg` | below 24px | tile, band, one line either side — **no word** |
| `ecapplog-small.svg` | 24–48px | tile, wide band, LOG at cap height 280 |
| `ecapplog.svg` | 64px and up | the full design, LOG at cap height 145 |

Both switches are measurements, not round numbers. Cap height over the 1024 grid, in device
pixels:

| | 16px | 24px | 32px | 48px | 64px |
| --- | --- | --- | --- | --- | --- |
| small (280) | 4.4 | **6.6** | 8.8 | 13.1 | — |
| full (145) | 2.3 | 3.4 | 4.5 | **6.8** | 9.1 |

The full master is only clean from 64px, and 6.8px at 48 is the floor it establishes for "soft but
readable". The small master hits that same floor at 24px, which is where the lower switch goes.
Nothing reaches it at 16px at any cap height, which is why the tiny master exists and is the only
one without a word.

**The upper switch is 64, not 48** — the counter-intuitive half. Handing 48px to the full master
takes the cap from 13.1px down to 6.8px, so the word would get *harder* to read as the icon got
bigger. 64 is also where the `.icns` switches anyway, since `icon_32x32@2x` is its first 64px slot,
so the two files agree at every size they share.

On iconset names: they are *point* sizes, so read the pixel count off the `@2x`. `icon_32x32` is 32
pixels and takes the small master, while `icon_32x32@2x` is the same 32 points at twice the density
— 64 pixels — and takes the full one. Two entries of nominally the same size legitimately show
different artwork.

The tiny master's **gaps are load-bearing**: at 16px one device pixel is 64 grid units, so its
lines sit right out at the edges to leave ~124-unit gaps. Reusing the large master's tighter
spacing renders as a single green blob. Don't close them up to make that file look better at
512 — nothing renders it at 512.

The small master's **band is deliberately not run out to the tile edge.** The identity of the large
master is a dark band on a pale tile; a band much bigger than 920x380 inverts that, the mint stops
being the ground, and the icon reads as a dark tile — a different mark at a glance even with an
identical palette.

The `ecapplog-macos*.svg` files only add Apple's inset (824-in-1024 with a drop shadow for the full
master, a tighter 880 and no shadow for the two small ones) around the three masters, because the
Dock expects that margin and the other platforms expect full-bleed.

**Why the `.ico` carries ten sizes and everything else carries six or ten slots.** Windows asks for
more sizes than any other platform: 16 and 20 (Explorer list and details views), 24 (taskbar at
100%), 32 (Alt-Tab, title bar), 40 (32 at 125% scaling), 48 (Explorer medium, desktop at 100%), 64,
96 (128 at 75%), 128 and 256. A size with no entry is not skipped — Windows invents it by
downscaling the next entry up. 20, 40 and 96 exist *only* inside the `.ico`, so
`generate-icons.sh` renders them to a temp dir the way it renders the macOS iconset; they must not
reach the `.qrc` or the Linux hicolor loop, because 20 and 40 are not sizes `index.theme` declares
and a `hicolor/40x40/apps` directory is never searched. The six committed PNGs are passed through
untouched, so their `.ico` frames stay byte-identical to the files on disk — which makes verifying
the container a hash comparison.

**This is the bug that produced the three-master split.** The icon was reported as "different
artwork" on Windows while being correct on macOS, and nothing was stale — the `.ico` and the `.icns`
agreed at every size they shared. The two platforms simply ask for different sizes. macOS shows you
the Dock at 128px and up, which always carried the wordmark; Windows shows you the taskbar and title
bar at 16–32px, which never did, because the old small master solved illegibility by deleting the
subject. The fix was the standard one for an optical size — simplify by *enlarging*, not by
removing. If you ever find yourself about to drop an element to make a small size legible, enlarge
it instead and drop its neighbours.

`QApplication::setWindowIcon(appIcon())` in `main.cpp` is the only place a window icon is set —
`AppIcon.h` builds a multi-resolution `QIcon` from the resource, and `QWidget::windowIcon()`
falls back to the application icon, so floated log views and dialogs inherit it.

**That call is `#ifndef Q_OS_DARWIN`, and removing the guard visibly breaks the Dock.** Qt maps
`setWindowIcon` onto `NSApplication`'s `applicationIconImage`, so on macOS it replaces the
bundle's `.icns` with the full-bleed resource artwork — which has no 824-in-1024 margin, and
renders ecapplog noticeably larger than every icon beside it. macOS takes its Dock and Finder
icon from the bundle and draws no window icon in title bars, so it wants none of this.

When checking an icon change on screen, remember macOS caches aggressively: the Dock will keep
showing the previous artwork until you `killall Dock`, and sometimes until the
`com.apple.dock.iconcache` under `/private/var/folders` is deleted as well. A stale Dock icon is
not evidence that the build is wrong — verify by extracting the `.icns` back out of the bundle
with `iconutil -c iconset`.

Windows caches harder and per-path: the shell will keep showing a previously *installed* build's
icon indefinitely, so a wrong icon on a machine that has ever had an older ECAppLog on it is not
evidence of anything. Flush with `ie4uinit.exe -show`; if that does not take, kill Explorer, delete
`%LocalAppData%\IconCache.db` and `%LocalAppData%\Microsoft\Windows\Explorer\iconcache_*.db`, then
restart it. Installing to a path that has never held ECAppLog sidesteps the whole problem.

Note also **which of the two Windows pipelines** you are looking at. Explorer, the desktop, pinned
shortcuts and the installer take the `.ico` compiled into the binary via `ecapplog.rc`; the running
app's title bar and Alt-Tab entry take the `QIcon` from `AppIcon.h` and the `.qrc` PNGs. One looking
right while the other looks wrong is the useful diagnostic, and it narrows the fix to one of them.

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
