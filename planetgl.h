#ifndef PLANETGL_H
#define PLANETGL_H

#include <QWidget>
#include <QGLWidget>
#include <QGLShaderProgram>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>

#include <memory>

#include "glutils.h"

#define _USE_MATH_DEFINES

namespace Ui {
class PlanetGL;
}

class PlanetGL : public QGLWidget, private QOpenGLFunctions_3_3_Core
{
	Q_OBJECT

public:
	explicit PlanetGL(QWidget *parent = nullptr);
	~PlanetGL();

	void setDistance(float val);
	void setSpeed(float val);

public slots:
	void onTimeout();

private:
	Ui::PlanetGL *ui;

	// QGLWidget interface
protected:
	void initializeGL();
	void resizeGL(int w, int h);
	void paintGL();

private:
	QTimer m_timer;
	QGLShaderProgram m_shpr;

	float m_time;

	QMatrix4x4 m_proj;
	QMatrix4x4 m_model;
	GLBuffer m_io;
	GLBuffer m_jupiter;
	GLBuffer m_europa;
	GLBuffer m_ceres;
	GLBuffer m_uranus;

	GLBuffer m_space;

	float m_dist;
	float m_speed;

	GLBuffer m_jupiterBlend;
	GLBuffer m_ioBlend;
	GLBuffer m_uranusBlend;

	QPoint m_mouse_pt;
	float m_aX;
	float m_aY;
	bool m_mouse_down;
	QVector3D m_positionCamera;

	QMap<int, bool> m_keys;

    int m_mode;

	void setViewport(float w, float h);

	void drawAll();
	void initBuffers();
	void processKey();

	void initBlend(GLBuffer& obj, int cnt1, int cnt2, float R,
				   int countBlending, uint tex, float delta = 0.015f,
				   int type = GL_TRIANGLE_STRIP);

	// QWidget interface
protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);

	// QWidget interface
protected:
	void keyPressEvent(QKeyEvent *event);
	void keyReleaseEvent(QKeyEvent *event);
};

#endif // PLANETGL_H
