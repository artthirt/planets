#include "planetgl.h"
#include "ui_planetgl.h"

#include <QMouseEvent>
#include <QKeyEvent>

#include <math.h>

const float diffStrength = 0.8f;
const float specStreangth1 = 0.5;
const float specStreangth2 = 0.1;

PlanetGL::PlanetGL(QWidget *parent) :
	QGLWidget(parent),
	QOpenGLFunctions_3_3_Core (),
	ui(new Ui::PlanetGL)
{
	ui->setupUi(this);

	m_time = 0;
	m_dist = 0;
	m_speed = 1;
	m_mouse_down = false;

	m_aX = 0;
	m_aY = 0;

//	QSurfaceFormat format;
//	format.setSamples(4);    // Set the number of samples used for multisampling
//	this->setFormat(format);

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
	m_timer.start(15);

	setMouseTracking(true);}

PlanetGL::~PlanetGL()
{
	delete ui;
}

void PlanetGL::setDistance(float val)
{
	m_dist = val;
}

void PlanetGL::setSpeed(float val)
{
	m_speed = val;
}

void PlanetGL::onTimeout()
{
	updateGL();

	m_time += 0.02;
}


void PlanetGL::initializeGL()
{
	QGLWidget::initializeGL();

	QOpenGLFunctions_3_3_Core::initializeOpenGLFunctions();

	m_model.setToIdentity();

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

//	glEnable(GL_MULTISAMPLE);
//	glEnable(GL_POLYGON_SMOOTH);
//	glEnable(GL_BLEND);
//	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

	glEnable(GL_CULL_FACE);

	glEnable(GL_SMOOTH);

	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	initBuffers();
}

void PlanetGL::resizeGL(int w, int h)
{
	QGLWidget::resizeGL(w, h);

	setViewport(w, h);
}

void PlanetGL::paintGL()
{
	QGLWidget::paintGL();

	drawAll();
}

void PlanetGL::setViewport(float w, float h)
{
	glViewport(0, 0, w, h);
	m_proj.setToIdentity();
	m_proj.perspective(50, w/h, 0.1, 2000);
}

void cnv2arr(const QMatrix4x4 &in, float* out)
{
	for(int i = 0; i < 16; ++i)
		out[i] = in.data()[i];
}

QVector3D mulvec(float *mat, const QVector3D& v)
{
	QVector3D res;
	for(int i = 0; i < 3; ++i){
		for(int j = 0; j < 3; ++j){
			int k = i * 3 + j;
			res[i] += mat[k] * v[j];
		}
	}
	return res;
}

QVector3D rotateX(float a, const QVector3D& v)
{
	float m[9] = {
		-cosf(a), 0, sinf(a),
		0, 1, 0,
		sinf(a), 0, cosf(a),
	};

	return QVector3D(mulvec(m, v));
}

QVector3D rotateY(float a, const QVector3D& v)
{
	float m[9] = {
		1, 0, 0,
		0, -cosf(a), sinf(a),
		0, sinf(a), cosf(a),
	};

	return QVector3D(mulvec(m, v));
}

void PlanetGL::processKey()
{
	if(m_keys[Qt::Key_A]){
		QVector3D v(-1, 0, 0);
		v = rotateY(m_aY, v);
		v = rotateX(-m_aX, v);
		m_positionCamera += v;
	}
	if(m_keys[Qt::Key_W]){
		QVector3D v(0, 0, 1);
		v = rotateY(m_aY, v);
		v = rotateX(-m_aX, v);
		m_positionCamera += v;
	}
	if(m_keys[Qt::Key_S]){
		QVector3D v(0, 0, -1);
		v = rotateY(m_aY, v);
		v = rotateX(-m_aX, v);
		m_positionCamera += v;
	}
	if(m_keys[Qt::Key_D]){
		QVector3D v(1, 0, 0);
		v = rotateY(m_aY, v);
		v = rotateX(-m_aX, v);
		m_positionCamera += v;
	}
}

void PlanetGL::drawAll()
{
	auto func_shadow = [](float t){
		const float m1 = -50;
		const float m2 = m1 + 70;
		t = fmodf(t, 360);
		t = t * M_PI / 180;
		t = atan2(sin(t), cos(t)) * 180 / M_PI;
		if(t >= m1 && t < m2){
			t -= m1;
			t /= (m2 - m1);
			t = cosf(t * M_PI);
			t *= t;
			t *= t;
		}else{
			t = 1;
		}
		return t;
	};

	float time = m_speed * m_time;

	processKey();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0, 0, 0, 1);

	m_model.setToIdentity();

	QVector3D eye(0, 0, 1);
	eye = rotateY(m_aY, eye);
	eye = rotateX(-m_aX, eye);

	QMatrix4x4 mview;
	QMatrix4x4 model = m_model, model2, model3, model4;
	float proj[16], mdl[16], mdlm[16], view[16];

	mview.lookAt(eye, QVector3D(0, 0, 0), QVector3D(0, 1, 0));
	cnv2arr(mview, view);

	cnv2arr(m_proj, proj);

	model4 = m_model;
	cnv2arr(model4, mdl);
	glCullFace(GL_FRONT);
	m_space.drawBuffers(proj, mdl, view);
	glCullFace(GL_BACK);

	m_model.translate(m_positionCamera[0], m_positionCamera[1], m_positionCamera[2]);
	model2 = model;

	float t = time;
	t =  func_shadow(t);

//	m_io.setDiffuseStrength(diffStrength * t);
//	m_io.setSpecularStrength(specStreangth1 * t);
//	m_ioBlend.setDiffuseStrength(diffStrength * t);
//	m_ioBlend.setSpecularStrength(specStreangth1 * t);

//	model.rotate(time, 0, 1, 0);
//	cnv2arr(model, mdlm);
//	m_io.drawBuffers(proj, mdlm);

//	model4 = model2;

	auto func_rotate = [](GLBuffer& obj, const QMatrix4x4& model, float proj[16], float view[16], float time,
			float aZ, float trX, float trZ, float selfRot = 0){
		QMatrix4x4 m = model, mi = model;
		float mf[16];
		m.translate(0, 0, trZ);
		m.rotate(aZ, 1, 0, 0);
		m.rotate(time, 0, 1, 0);
		m.translate(trX, 0, 0);
		m.rotate(selfRot, 0, 1, 0);
		cnv2arr(m, mf);
		obj.drawBuffers(proj, mf, view);
	};

	func_rotate(m_jupiter, m_model, proj, view, time, 0, 0, -200);
	func_rotate(m_io, m_model, proj, view, time - 100, 0, 170, -200, -time);
	for(int i = 0; i < 1; ++i)
		func_rotate(m_europa, m_model, proj, view, 1.2 * time - 90 + i * 20, 10, 200, -200, 0.8 * time);
	for(int i = 0; i < 1; ++i)
		func_rotate(m_ceres, m_model, proj, view, 0.8 * time - 130 + i * 20, -7, 150, -200, 0.7 * time);
	for(int i = 0; i < 1; ++i)
		func_rotate(m_uranus, m_model, proj, view, -1.4 * time + 30 + i * 20, -25, 450, -200, -5 * time);

	glEnable(GL_BLEND);
	func_rotate(m_uranusBlend, m_model, proj, view, -1.4f * time + 30, -25, 450, -200, -5 * time);
	func_rotate(m_jupiterBlend, m_model, proj, view, time, 0, 0, -200);
	func_rotate(m_ioBlend, m_model, proj, view, time - 100, 0, 170, -200, -time);
	glDisable(GL_BLEND);

//	model2.rotate(30, 0, 0, 1);
//	model2.rotate( time, 0, 1, 0);
//	model2.translate(200, 0, 0);

//	model3 = model2;

////	std::vector< QMatrix4x4 > rotmats;
////	rotmats.resize(m_sattBlend.size());
////	float sp = 0;
////	for(QMatrix4x4& it: rotmats){
////		it = model2;
////		it.rotate(-time + sp, 0, 1, 0);
////		sp += 0.01f;
////	}
//	QMatrix4x4 model5;
//	model5 = model2;
//	model5.rotate(-time, 0, 1, 0);

//	model4 = model3;
//	model3.rotate(20, 1, 0, 0);
//	model3.rotate( time - 210, 0, 1, 0);
//	model3.translate(260, 0, 0);
//	cnv2arr(model3, mdl);

//	t = func_shadow(time - 210);
//	m_europa.setDiffuseStrength(0.6 * t);
//	m_europa.drawBuffers(proj, mdl);

//	int cnt = 1;
//	for(int i = 0; i < cnt; ++i){
//		float d = 5 + 1. * i / cnt * 360;
//		QMatrix4x4 m = model4;
//		m.rotate(-20, 1, 0, 0);
//		m.rotate( 0.5 * time - 280 + d, 0, 1, 0);
//		m.translate(300, 0, 0);
//		cnv2arr(m, mdl);
//		t = func_shadow(0.5 * time - 280 + d);
//		m_ceres.setDiffuseStrength(0.6 * t);
//		m_ceres.drawBuffers(proj, mdl);
//	}

//	float mdl4[16];
//	{
//		QMatrix4x4 m = model2;
//		m.rotate(15, 1, 0, 0);
//		m.rotate(-2.5 * time - 220, 0, 1, 0);
//		m.translate(500, 0, 0);
//		m.rotate( 5 * time, 0, 1, 0);
//		cnv2arr(m, mdl4);
////		t = func_shadow(0.5 * time - 280 + d);
////		m_sattelite4.setDiffuseStrength(0.6 * t);
//		m_uranus.drawBuffers(proj, mdl4);
//	}

//	model2.rotate(-time, 0, 1, 0);
//	cnv2arr(model2, mdl);
//	m_jupiter.drawBuffers(proj, mdl);

#if 0
	glEnable(GL_BLEND);
//	uint id = 0;
//	for(GLBuffer& it: m_sattBlend){
//		cnv2arr(rotmats[id], mdl);
//		it.drawBuffers(proj, mdl);
//		id++;
//	}
	m_uranusBlend.drawBuffers(proj, mdl4);

	cnv2arr(model5, mdl);
	m_jupiter.drawBuffers(proj, mdl);


	glCullFace(GL_BACK);
	m_ioBlend.drawBuffers(proj, mdlm);
	glDisable(GL_BLEND);
#endif
}

const 	float lp[] = {10000, 0, 10000};

void PlanetGL::initBuffers()
{
	int cnt1 = 30;
	int cnt2 = 40;
	float R = 1;
	float R2 = 140;
	float R3 = 1.3f;
	float R4 = 0.5f;

	uint type = GL_TRIANGLE_STRIP;

	m_io.initBuffer(this, true);
	m_io.setType(type);
	m_io.setColor(1, 1, 1, 1);
	m_io.setSpecularStrength(specStreangth1);
	m_io.setDiffuseStrength(diffStrength);

	m_io.initSphere(cnt1, cnt2, R);

	m_io.setLightPos(lp[0], lp[1], lp[2]);
	m_io.setEyePos(5000, 10, 10000);

	QImage im;
	im.load("../data/io_rgb_cyl.jpg");
//	im.load("../data/8k_earth_daymap.jpg");
	uint ptex = m_io.initTexture(im);

//	im.load("../data/8k_earth_specular_map.tif");
//	m_planet.initTexture2(im);

	initBlend(m_ioBlend, cnt1, cnt2, R, 40, ptex, 0.05);

	/////////////

	m_space.initBuffer(this, true, false, false);
	m_space.setType(type);
	m_space.setColor(1, 1, 1, 1);

	m_space.initSphere(cnt1, cnt2, 1000);

	m_space.setLightPos(lp[0], lp[1], lp[2]);
	m_space.setEyePos(5000, 10, 10000);

	im.load("../data/8k_stars_milky_way.jpg");
	m_space.initTexture(im);
	/////////////

	m_jupiter.initBuffer(this, true);
	m_jupiter.setType(type);
	m_jupiter.setColor(1, 1, 1, 1);
	m_jupiter.setSpecularStrength(0);
	m_jupiter.setDiffuseStrength(diffStrength);

	m_jupiter.initSphere(cnt1 * 2, cnt2, R2);

	m_jupiter.setLightPos(lp[0], lp[1], lp[2]);
	m_jupiter.setEyePos(5000, 10, 10000);

	im.load("../data/8k_jupiter.jpg");
	uint tex = m_jupiter.initTexture(im);

	///////////

	const uint countBlend = 40;

	initBlend(m_jupiterBlend, cnt1 * 2, cnt2, R2, countBlend, tex, 0.02f);
//	}

	///////////

	m_uranus.initBuffer(this, true);
	m_uranus.setType(type);
	m_uranus.setColor(1, 1, 1, 1);
	m_uranus.setSpecularStrength(0);
	m_uranus.setDiffuseStrength(diffStrength);

	m_uranus.initSphere(cnt1, cnt2, R2 * 0.4f);

	m_uranus.setLightPos(lp[0], lp[1], lp[2]);
	m_uranus.setEyePos(5000, 10, 10000);

	im.load("../data/uranus.jpg");
	tex = m_uranus.initTexture(im);

	initBlend(m_uranusBlend, cnt1, cnt2, R2 * 0.4f, 40, tex, 0.1);
	///////////

	m_europa.initBuffer(this, true);
	m_europa.setType(type);
	m_europa.setColor(1, 1, 1, 1);
	m_europa.setSpecularStrength(specStreangth1);
	m_europa.setDiffuseStrength(diffStrength);

	m_europa.initSphere(cnt1, cnt2, R3);

	m_europa.setLightPos(lp[0], lp[1], lp[2]);
	m_europa.setEyePos(5000, 10, 10000);

	im.load("../data/ZZBiHOH.jpg");
	m_europa.initTexture(im);

	///////////////////

	m_ceres.initBuffer(this, true);
	m_ceres.setType(type);
	m_ceres.setColor(1, 1, 1, 1);
	m_ceres.setSpecularStrength(specStreangth1);
	m_ceres.setDiffuseStrength(diffStrength);

	m_ceres.initSphere(cnt1, cnt2, R4);

	m_ceres.setLightPos(lp[0], lp[1], lp[2]);
	m_ceres.setEyePos(5000, 10, 10000);

	im.load("../data/8k_ceres_fictional.jpg");
	m_ceres.initTexture(im);

//	im.load("../data/8k_earth_specular_map.tif");
	//	m_planet.initTexture2(im);
}

void PlanetGL::initBlend(GLBuffer &obj, int cnt1, int cnt2,
						 float R, int countBlending, uint tex, float delta, int type)
{
	obj.initBuffer(this, true, false, true, countBlending);
	obj.setType(type);
	obj.setColor(1, 1, 1, 1);
	obj.setSpecularStrength(0);
	obj.setDiffuseStrength(diffStrength);
	obj.initSphere(cnt1, cnt2, R);
	obj.setLightPos(lp[0], lp[1], lp[2]);
	obj.setEyePos(5000, 10, 10000);
	obj.initTexture(tex);

	for(uint i = 0; i < countBlending; ++i){
		float di = (float)i / countBlending;
		float d = (di * delta);

		obj.setScale(i, d);
		obj.setBlendStrength(i, (1 - sqrt(di)));
	}
}


void PlanetGL::mousePressEvent(QMouseEvent *event)
{
	setFocus();
	m_mouse_pt = event->pos();
	m_mouse_down = true;
}

void PlanetGL::mouseReleaseEvent(QMouseEvent *event)
{
	m_mouse_down = false;
}

void PlanetGL::mouseMoveEvent(QMouseEvent *event)
{
	if(m_mouse_down){
		QPointF pt = event->pos();
		QPointF ptp = m_mouse_pt;
		QPointF d = pt - ptp;
		m_mouse_pt = pt.toPoint();

		m_aX += d.x() * 0.01;
		m_aY += d.y() * 0.01;

		m_aX = atan2(sin(m_aX), cos(m_aX));
		m_aY = atan2(sin(m_aY), cos(m_aY));
	}
}


void PlanetGL::keyPressEvent(QKeyEvent *event)
{
	m_keys[event->key()] = true;
}

void PlanetGL::keyReleaseEvent(QKeyEvent *event)
{
	m_keys[event->key()] = false;
}
