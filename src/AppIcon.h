#pragma once

#include <QIcon>

// The application icon, assembled from every size baked into the Qt resource.
//
// QIcon(":/ecapplog.png") would work, but it hands Qt a single 64x64 bitmap to scale to whatever
// the platform asks for - and the two sizes that matters most for are the ones a smooth downscale
// serves worst: the 16px window/taskbar icon, and the 32px one on a HiDPI screen. The 16px entry
// is also a different drawing (see scripts/icon/ecapplog-small.svg), so Qt has to be given it
// explicitly or it will never be used.
//
// Qt picks the closest available size and only scales when nothing matches.
inline QIcon appIcon()
{
	QIcon icon;
	for (int size : { 16, 32, 48, 64, 128, 256 })
	{
		// ecapplog.png is the 64 - it keeps the bare name because the .qrc, the Linux install
		// rule and external references all point at it.
		icon.addFile(size == 64 ? QStringLiteral(":/ecapplog.png")
		                        : QStringLiteral(":/ecapplog-%1.png").arg(size),
		             QSize(size, size));
	}
	return icon;
}
