#pragma once

#include <QIcon>

// The application icon, assembled from every size baked into the Qt resource.
//
// QIcon(":/ecapplog.png") would work, but it hands Qt a single 64x64 bitmap to scale to whatever
// the platform asks for - and the two sizes that matters most for are the ones a smooth downscale
// serves worst: the 16px window/taskbar icon, and the 32px one on a HiDPI screen. Those entries
// are also different drawings (see scripts/icon/ecapplog-tiny.svg and -small.svg), so Qt has to be
// given them explicitly or they will never be used.
//
// Qt picks the closest available size and only scales when nothing matches - and where it has to
// choose it rounds *up*, to the smallest pixmap larger than the request. That is why 24 is here.
// Qt asks Windows for SM_CXSMICON and SM_CXICON when it builds the window HICONs, which are 20 and
// 40 at 125% scaling - neither is a committed size, so both get satisfied by rounding up. With 24
// present, 20 rounds to it rather than jumping to 32, and 40 rounds to 48; both of those now carry
// the same wordmark drawing, so no request lands on artwork that disagrees with its neighbours.
inline QIcon appIcon()
{
	QIcon icon;
	for (int size : { 16, 24, 32, 48, 64, 128, 256 })
	{
		// ecapplog.png is the 64 - it keeps the bare name because the .qrc, the Linux install
		// rule and external references all point at it.
		icon.addFile(size == 64 ? QStringLiteral(":/ecapplog.png")
		                        : QStringLiteral(":/ecapplog-%1.png").arg(size),
		             QSize(size, size));
	}
	return icon;
}
