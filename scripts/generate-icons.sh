#!/bin/sh
#
# Regenerate every platform icon asset in src/resources/ from the SVG masters in scripts/icon/.
#
# Nothing in the build calls this - the generated files are committed, because a build machine
# should not need rsvg-convert and a Windows or Linux build has no iconutil at all. Run it by
# hand after editing the SVGs and commit whatever changes.
#
# Needs: rsvg-convert (brew install librsvg), plus iconutil, which is macOS-only. On other
# platforms the .icns step is skipped and the committed one is left alone.
#
# Three optical sizes are in play, switching at 24 and 64 *device pixels*:
#
#   below 24    ecapplog-tiny.svg    tile, band, one line either side - no word
#   24 to 48    ecapplog-small.svg   tile, full-width band, LOG at cap height 280
#   64 and up   ecapplog.svg         the full design, LOG at cap height 145
#
# Both switches are set by where the lettering resolves, not by round numbers. Cap height over the
# 1024 grid gives, in device pixels:
#
#            tiny    small (280)   full (145)
#     16px     -         4.4          2.3
#     24px     -         6.6          3.4
#     32px     -         8.8          4.5
#     48px     -        13.1          6.8
#     64px     -        17.5          9.1
#
# The full master is only clean from 64px. The small master reaches the same legibility at 24px
# that the full one manages at 48px, and nothing reaches it at 16px, which is why the tiny master
# is the only one without a word. Note the upper switch is 64 rather than 48: handing 48px to the
# full master takes the cap from 13.1 down to 6.8, so the word would get *harder* to read as the
# icon got bigger. See scripts/icon/ecapplog-small.svg for the full reasoning.
#
# Device pixels, not points, is the distinction that matters: icon_32x32 (32pt on a non-retina
# screen) is 32 pixels and gets the small design, while icon_32x32@2x is the same 32pt at twice
# the density - 64 pixels - and takes the full one.

set -e

here=$(cd "$(dirname "$0")" && pwd)
icon="$here/icon"
out="$here/../src/resources"

command -v rsvg-convert >/dev/null 2>&1 || {
	echo "rsvg-convert not found - brew install librsvg" >&2
	exit 1
}

# render <source.svg> <size> <destination.png>
render() {
	rsvg-convert -w "$2" -h "$2" "$icon/$1" -o "$3"
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

echo "==> flat PNGs (Qt resource, Linux hicolor)"
# ecapplog.png keeps its bare name because the .qrc, the Linux install rule and three years of
# external references all point at it.
render ecapplog-tiny.svg 16 "$out/ecapplog-16.png"
for size in 24 32 48; do
	render ecapplog-small.svg "$size" "$out/ecapplog-$size.png"
done
for size in 128 256; do
	render ecapplog.svg "$size" "$out/ecapplog-$size.png"
done
render ecapplog.svg 64 "$out/ecapplog.png"

echo "==> ecapplog.ico"
# Windows asks for sizes nobody else does - 20 in Explorer's details view, 24 on the taskbar at
# 100% scaling, 40 for that same taskbar at 125%, 96 for the large-icons view. Leaving them out
# does not mean Windows picks a sensible neighbour: it invents its own downscale, and the worst
# case used to be scaling the 48px wordmark down to 40, well under the legibility floor. These
# four are only ever repacked into the .ico, so they render to a temp dir rather than joining the
# committed set - the same treatment the iconset below gets.
render ecapplog-tiny.svg  20 "$tmp/ecapplog-20.png"
render ecapplog-small.svg 40 "$tmp/ecapplog-40.png"
render ecapplog.svg       96 "$tmp/ecapplog-96.png"

python3 "$icon/make_ico.py" "$out/ecapplog.ico" \
	"$out/ecapplog-16.png" "$tmp/ecapplog-20.png" "$out/ecapplog-24.png" \
	"$out/ecapplog-32.png" "$tmp/ecapplog-40.png" "$out/ecapplog-48.png" \
	"$out/ecapplog.png" "$tmp/ecapplog-96.png" \
	"$out/ecapplog-128.png" "$out/ecapplog-256.png"

if command -v iconutil >/dev/null 2>&1; then
	echo "==> ecapplog.icns"
	set=$tmp/ecapplog.iconset
	mkdir -p "$set"

	# The names are fixed by iconutil, and they are point sizes, so read the pixel count off the
	# @2x rather than the number: icon_32x32.png is 32 pixels and takes the small master, while
	# icon_32x32@2x.png is the same 32 points at 64 pixels and takes the full one. Only the true
	# 16px slot falls below the small master's floor and needs the wordless tiny one.
	#
	# macOS has no 48px slot at all, which is exactly how the Windows bug this addresses stayed
	# invisible here for so long: the .icns simply never rendered the size where the two files
	# disagreed.
	render ecapplog-macos-tiny.svg  16 "$set/icon_16x16.png"
	render ecapplog-macos-small.svg 32 "$set/icon_16x16@2x.png"
	render ecapplog-macos-small.svg 32 "$set/icon_32x32.png"
	render ecapplog-macos.svg 64 "$set/icon_32x32@2x.png"
	render ecapplog-macos.svg 128 "$set/icon_128x128.png"
	render ecapplog-macos.svg 256 "$set/icon_128x128@2x.png"
	render ecapplog-macos.svg 256 "$set/icon_256x256.png"
	render ecapplog-macos.svg 512 "$set/icon_256x256@2x.png"
	render ecapplog-macos.svg 512 "$set/icon_512x512.png"
	render ecapplog-macos.svg 1024 "$set/icon_512x512@2x.png"

	iconutil -c icns "$set" -o "$out/ecapplog.icns"
else
	echo "==> skipping ecapplog.icns (no iconutil - macOS only)"
fi

echo "done"
