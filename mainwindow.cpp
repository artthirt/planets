#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

    auto w = QWidget::createWindowContainer(&m_scene);
    ui->vlGL->addWidget(w);

	connect(&m_scene, &SpaceScene::focusChanged, this, &MainWindow::onSceneFocusChanged);
	connect(&m_scene, &SpaceScene::infoChanged, this, &MainWindow::onSceneInfo);

	statusBar()->showMessage(
		"WASD fly · Q/E down/up · Shift boost · drag look · wheel zoom · click a body to track · M cycle · Esc release");

	resize(1100, 640);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::on_hs_speed_valueChanged(int value)
{
    m_scene.setSpeed(0.01f + 0.01f * value);
}

void MainWindow::onSceneFocusChanged(const QString &body)
{
	setWindowTitle(QString("Planets — %1").arg(body));
}

void MainWindow::onSceneInfo(const QString &info)
{
	statusBar()->showMessage(info, 8000);
}
