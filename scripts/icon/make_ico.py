#!/usr/bin/env python3
"""Pack PNG files into a Windows .ico.

Usage: make_ico.py out.ico in16.png in32.png ...

Every entry is stored PNG-compressed rather than as a classic BMP/DIB. Windows has accepted
PNG-compressed ICO entries at any size since Vista, and the alternative - emitting DIBs - would
mean decoding the PNGs back to raw RGBA first, which needs a dependency this project does not
otherwise have (there is no Pillow on the build machine, and pulling in ImageMagick just for the
Windows icon is not worth it for a project that only ships macOS releases).

If pre-Vista support ever matters, replace the call in generate-icons.sh with
`magick in16.png ... out.ico`, which writes DIBs below 256 the old way.
"""

import struct
import sys


def main(argv):
	if len(argv) < 3:
		sys.exit("usage: make_ico.py out.ico in.png [in.png ...]")

	out, pngs = argv[1], argv[2:]
	images = []
	for path in pngs:
		with open(path, "rb") as f:
			data = f.read()
		# The IHDR width/height sit at a fixed offset, so there is no need to parse the whole
		# stream: 8 bytes of signature, 4 length, 4 "IHDR", then the two dimensions.
		if data[:8] != b"\x89PNG\r\n\x1a\n":
			sys.exit("%s is not a PNG" % path)
		width, height = struct.unpack(">II", data[16:24])
		images.append((width, height, data))

	images.sort(key=lambda i: i[0])

	# ICONDIR, then one ICONDIRENTRY per image, then the payloads back to back.
	offset = 6 + 16 * len(images)
	header = struct.pack("<HHH", 0, 1, len(images))
	entries, payloads = b"", b""
	for width, height, data in images:
		# 256 is encoded as 0 in the single-byte dimension fields.
		entries += struct.pack(
			"<BBBBHHII",
			width if width < 256 else 0,
			height if height < 256 else 0,
			0,   # palette size: 0 for truecolor
			0,   # reserved
			1,   # colour planes
			32,  # bits per pixel
			len(data),
			offset,
		)
		payloads += data
		offset += len(data)

	with open(out, "wb") as f:
		f.write(header + entries + payloads)


if __name__ == "__main__":
	main(sys.argv)
