#include "mainwindow.h"
#include <QApplication>
#include <QStringList>
#include <QVector3D>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	// hidden CLI, for verification:
	//   Planets.exe --screenshot out.png [--frames N] [--cam x,y,z,yawDeg,pitchDeg]
	QString shotPath;
	int shotFrames = 40;
	QVector3D camPos;
	float camYaw = 0.0f;
	float camPitch = 0.0f;
	bool camSet = false;

	for (int i = 1; i < argc; ++i) {
		const QString arg = QString::fromLocal8Bit(argv[i]);
		auto next = [&]() {
			return (i + 1 < argc) ? QString::fromLocal8Bit(argv[++i]) : QString();
		};
		if (arg == "--screenshot") {
			shotPath = next();
		} else if (arg == "--frames") {
			shotFrames = next().toInt();
		} else if (arg == "--cam") {
			const QStringList p = next().split(',');
			if (p.size() == 5) {
				camPos = QVector3D(p[0].toFloat(), p[1].toFloat(), p[2].toFloat());
				camYaw = p[3].toFloat();
				camPitch = p[4].toFloat();
				camSet = true;
			}
		}
	}

	MainWindow w;
	if (!shotPath.isEmpty()) {
		if (camSet)
			w.scene()->setCameraForShot(camPos, camYaw, camPitch);
		w.scene()->requestScreenshot(shotPath, shotFrames);
	}
	w.show();

	return a.exec();
}
