#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

    auto w = QWidget::createWindowContainer(&mPlanetGL);
    ui->vlGL->addWidget(w);

	resize(1100, 600);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::on_hs_speed_valueChanged(int value)
{
    float val = 0.01 * value;
    mPlanetGL.setSpeed(0.01 + val);
}
