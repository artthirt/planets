#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "planetgl.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private slots:

	void on_hs_speed_valueChanged(int value);

private:
	Ui::MainWindow *ui;
    PlanetGL mPlanetGL;
};

#endif // MAINWINDOW_H
