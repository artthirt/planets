#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "space_scene.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

	SpaceScene *scene() { return &m_scene; }

private slots:

	void on_hs_speed_valueChanged(int value);
	void onSceneFocusChanged(const QString &body);
	void onSceneInfo(const QString &info);

private:
	Ui::MainWindow *ui;
    SpaceScene m_scene;
};

#endif // MAINWINDOW_H
