#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTime>

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

    auto w = QWidget::createWindowContainer(&m_scene);
    ui->vlGL->addWidget(w);

	connect(&m_scene, &SpaceScene::focusChanged, this, &MainWindow::onSceneFocusChanged);
	connect(&m_scene, &SpaceScene::infoChanged, this, &MainWindow::onSceneInfo);
	connect(&m_scene, &SpaceScene::screenshotRequested, this, &MainWindow::onScreenshotRequested);
	connect(&m_scene, &SpaceScene::screenshotSaved, this, &MainWindow::onScreenshotSaved);

	statusBar()->showMessage(
		"WASD fly · Q/E down/up · Shift boost · drag look · wheel zoom · click a body to track · M cycle · Esc release · F12 screenshot");

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

void MainWindow::onScreenshotRequested()
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
	if (dir.isEmpty() || !QDir(dir).exists())
		dir = QDir::homePath();

	QFileDialog dlg(this, tr("Save screenshot"), dir, tr("PNG image (*.png)"));
	dlg.setAcceptMode(QFileDialog::AcceptSave);
	dlg.setFileMode(QFileDialog::AnyFile);
	dlg.setDefaultSuffix(QStringLiteral("png"));
	dlg.setOption(QFileDialog::DontConfirmOverwrite, true);
	dlg.selectFile(QStringLiteral("planets_%1_%2.png")
			.arg(QDate::currentDate().toString("yyyyMMdd"))
			.arg(QTime::currentTime().toString("HHmmss")));
	if (dlg.exec() != QDialog::Accepted)
		return;
	const QString path = dlg.selectedFiles().value(0);
	if (!path.isEmpty())
		m_scene.takeScreenshot(path);
}

void MainWindow::onScreenshotSaved(const QString &path)
{
	statusBar()->showMessage(tr("screenshot saved: %1").arg(path), 8000);
}
