#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	resize(1100, 600);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::on_hs_dist_valueChanged(int value)
{
	float val = value;
	ui->wgl->setDistance(val);
}

void MainWindow::on_hs_speed_valueChanged(int value)
{
	float val = 0.1 * value;
	ui->wgl->setSpeed(1 + val);
}
