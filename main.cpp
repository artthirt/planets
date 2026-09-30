#include "mainwindow.h"
#include <QApplication>
#include <QStringList>
#include <QVector3D>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	// hidden CLI, for verification:
	//   Planets.exe --screenshot out.png [--frames N] [--cam x,y,z,yawDeg,pitchDeg] [--focus name] [--time T]
	QString shotPath;
	int shotFrames = 40;
	bool shotTimeSet = false;
	double shotTime = 0.0;
	QVector3D camPos;
	float camYaw = 0.0f;
	float camPitch = 0.0f;
	bool camSet = false;
	QString focusName;
	bool saveNormals = false;

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
		} else if (arg == "--focus") {
			focusName = next();
		} else if (arg == "--time") {
			shotTime = next().toDouble();
			shotTimeSet = true;
		} else if (arg == "--save-normal-maps") {
			saveNormals = true;
		}
	}

	MainWindow w;
	if (!shotPath.isEmpty()) {
		if (shotTimeSet)
			w.scene()->setTimeForShot(shotTime);
		if (camSet)
			w.scene()->setCameraForShot(camPos, camYaw, camPitch);
		if (!focusName.isEmpty())
			w.scene()->focusByName(focusName);
		if (saveNormals)
			w.scene()->setSaveNormalMaps(true);
		w.scene()->requestScreenshot(shotPath, shotFrames);
	}
	w.show();

	return a.exec();
}
