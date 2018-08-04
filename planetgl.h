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
	GLBuffer m_planet;
	GLBuffer m_sattelite;
	GLBuffer m_sattelite2;
	GLBuffer m_sattelite3;
	GLBuffer m_sattelite4;

	GLBuffer m_space;

	float m_dist;
	float m_speed;

	GLBuffer m_sattBlend;
	GLBuffer m_planetBlend;
	GLBuffer m_satt4Blend;

	void setViewport(float w, float h);

	void drawAll();
	void initBuffers();

	void initBlend(GLBuffer& obj, int cnt1, int cnt2, float R,
				   int countBlending, uint tex, float delta = 0.015f,
				   int type = GL_TRIANGLE_STRIP);
};

#endif // PLANETGL_H
