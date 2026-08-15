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
# Two optical sizes are in play; see scripts/icon/ecapplog-small.svg for the reasoning.
#
# The switch is at 48 *device pixels*, and it is set by where the LOG lettering stops resolving,
# not by any round number. Cap height is 145 on a 1024 grid, so it renders about 4.5px at 32,
# 6.8px at 48 and 9.1px at 64 - at 32 the word is an illegible smudge, at 48 it is soft but
# readable, and from 64 it is clean. So 32 and below take the wordless small master.
#
# Device pixels, not points, is the distinction that matters: icon_32x32 (32pt on a non-retina
# screen) is 32 pixels and gets the small design, while icon_32x32@2x is the same 32pt at twice
# the density - 64 pixels - and has the room for the word.

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

echo "==> flat PNGs (Qt resource, Linux hicolor)"
# ecapplog.png keeps its bare name because the .qrc, the Linux install rule and three years of
# external references all point at it.
for size in 16 32; do
	render ecapplog-small.svg "$size" "$out/ecapplog-$size.png"
done
for size in 48 128 256; do
	render ecapplog.svg "$size" "$out/ecapplog-$size.png"
done
render ecapplog.svg 64 "$out/ecapplog.png"

echo "==> ecapplog.ico"
python3 "$icon/make_ico.py" "$out/ecapplog.ico" \
	"$out/ecapplog-16.png" "$out/ecapplog-32.png" "$out/ecapplog-48.png" \
	"$out/ecapplog.png" "$out/ecapplog-128.png" "$out/ecapplog-256.png"

if command -v iconutil >/dev/null 2>&1; then
	echo "==> ecapplog.icns"
	set=$(mktemp -d)/ecapplog.iconset
	mkdir -p "$set"

	# The names are fixed by iconutil, and they are point sizes - so the three entries that come
	# out at 32 pixels or fewer all take the small master, including icon_32x32.png. Its @2x
	# sibling is the same 32 points at 64 pixels and does get the word.
	render ecapplog-macos-small.svg 16 "$set/icon_16x16.png"
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
	rm -rf "$(dirname "$set")"
else
	echo "==> skipping ecapplog.icns (no iconutil - macOS only)"
fi

echo "done"
