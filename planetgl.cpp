#include "planetgl.h"
#include "ui_planetgl.h"

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

//	QSurfaceFormat format;
//	format.setSamples(4);    // Set the number of samples used for multisampling
//	this->setFormat(format);

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
	m_timer.start(15);
}

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

void PlanetGL::drawAll()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0, 0, 0, 1);

	m_model.setToIdentity();

	QMatrix4x4 model = m_model, model2, model3, model4;
	float proj[16], mdl[16], mdlm[16];
	cnv2arr(m_proj, proj);

	model.translate(0, 0, -5 - m_dist);
	model2 = model;

	float time = m_speed * m_time;

	model.rotate(time, 0, 1, 0);
	cnv2arr(model, mdlm);

	float t = time;

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

	t =  func_shadow(t);

	//qDebug("t %f", t);

	m_planet.setDiffuseStrength(diffStrength * t);
	m_planet.setSpecularStrength(specStreangth1 * t);
	m_planetBlend.setDiffuseStrength(diffStrength * t);
	m_planetBlend.setSpecularStrength(specStreangth1 * t);

	m_planet.drawBuffers(proj, mdlm);

	model4.rotate(-0.5f * time, 0, 1, 0);
	cnv2arr(model4, mdl);
	glCullFace(GL_FRONT);
	m_space.drawBuffers(proj, mdl);
	glCullFace(GL_BACK);

	model4 = model2;

	model2.rotate(30, 0, 0, 1);
	model2.rotate( time, 0, 1, 0);
	model2.translate(200, 0, 0);

	model3 = model2;

//	std::vector< QMatrix4x4 > rotmats;
//	rotmats.resize(m_sattBlend.size());
//	float sp = 0;
//	for(QMatrix4x4& it: rotmats){
//		it = model2;
//		it.rotate(-time + sp, 0, 1, 0);
//		sp += 0.01f;
//	}
	QMatrix4x4 model5;
	model5 = model2;
	model5.rotate(-time, 0, 1, 0);

	model4 = model3;
	model3.rotate(20, 1, 0, 0);
	model3.rotate( time - 210, 0, 1, 0);
	model3.translate(260, 0, 0);
	cnv2arr(model3, mdl);

	t = func_shadow(time - 210);
	m_sattelite2.setDiffuseStrength(0.6 * t);
	m_sattelite2.drawBuffers(proj, mdl);

	int cnt = 1;
	for(int i = 0; i < cnt; ++i){
		float d = 5 + 1. * i / cnt * 360;
		QMatrix4x4 m = model4;
		m.rotate(-20, 1, 0, 0);
		m.rotate( 0.5 * time - 280 + d, 0, 1, 0);
		m.translate(300, 0, 0);
		cnv2arr(m, mdl);
		t = func_shadow(0.5 * time - 280 + d);
		m_sattelite3.setDiffuseStrength(0.6 * t);
		m_sattelite3.drawBuffers(proj, mdl);
	}

	float mdl4[16];
	{
		QMatrix4x4 m = model2;
		m.rotate(15, 1, 0, 0);
		m.rotate(-2.5 * time - 220, 0, 1, 0);
		m.translate(500, 0, 0);
		m.rotate( 5 * time, 0, 1, 0);
		cnv2arr(m, mdl4);
//		t = func_shadow(0.5 * time - 280 + d);
//		m_sattelite4.setDiffuseStrength(0.6 * t);
		m_sattelite4.drawBuffers(proj, mdl4);
	}

	model2.rotate(-time, 0, 1, 0);
	cnv2arr(model2, mdl);
	m_sattelite.drawBuffers(proj, mdl);

	glEnable(GL_BLEND);
//	uint id = 0;
//	for(GLBuffer& it: m_sattBlend){
//		cnv2arr(rotmats[id], mdl);
//		it.drawBuffers(proj, mdl);
//		id++;
//	}
	m_satt4Blend.drawBuffers(proj, mdl4);

	cnv2arr(model5, mdl);
	m_sattBlend.drawBuffers(proj, mdl);


	glCullFace(GL_BACK);
	m_planetBlend.drawBuffers(proj, mdlm);
	glDisable(GL_BLEND);
}

void PlanetGL::initBuffers()
{
	int cnt1 = 30;
	int cnt2 = 40;
	float R = 1;
	float R2 = 140;
	float R3 = 1.3f;
	float R4 = 0.5f;

	m_planet.initBuffer(this, true);
	m_planet.setType(GL_TRIANGLE_STRIP);
	m_planet.setColor(1, 1, 1, 1);
	m_planet.setSpecularStrength(specStreangth1);
	m_planet.setDiffuseStrength(diffStrength);

	m_planet.initSphere(cnt1, cnt2, R);

	m_planet.setLightPos(5000, 10, 10000);
	m_planet.setEyePos(5000, 10, 10000);

	QImage im;
	im.load("../data/io_rgb_cyl.jpg");
//	im.load("../data/8k_earth_daymap.jpg");
	uint ptex = m_planet.initTexture(im);

//	im.load("../data/8k_earth_specular_map.tif");
//	m_planet.initTexture2(im);

	initBlend(m_planetBlend, cnt1, cnt2, R, 40, ptex, 0.05);

	/////////////

	m_space.initBuffer(this, true, false, false);
	m_space.setType(GL_TRIANGLE_STRIP);
	m_space.setColor(1, 1, 1, 1);

	m_space.initSphere(cnt1, cnt2, 1000);

	m_space.setLightPos(5000, 10, 10000);
	m_space.setEyePos(5000, 10, 10000);

	im.load("../data/8k_stars_milky_way.jpg");
	m_space.initTexture(im);
	/////////////

	m_sattelite.initBuffer(this, true);
	m_sattelite.setType(GL_TRIANGLE_STRIP);
	m_sattelite.setColor(1, 1, 1, 1);
	m_sattelite.setSpecularStrength(0);
	m_sattelite.setDiffuseStrength(diffStrength);

	m_sattelite.initSphere(cnt1 * 2, cnt2, R2);

	m_sattelite.setLightPos(5000, 10, 10000);
	m_sattelite.setEyePos(5000, 10, 10000);

	im.load("../data/8k_jupiter.jpg");
	uint tex = m_sattelite.initTexture(im);

	///////////

	const uint countBlend = 40;

	initBlend(m_sattBlend, cnt1 * 2, cnt2, R2, countBlend, tex, 0.01f);
//	}

	///////////

	m_sattelite4.initBuffer(this, true);
	m_sattelite4.setType(GL_TRIANGLE_STRIP);
	m_sattelite4.setColor(1, 1, 1, 1);
	m_sattelite4.setSpecularStrength(0);
	m_sattelite4.setDiffuseStrength(diffStrength);

	m_sattelite4.initSphere(cnt1, cnt2, R2 * 0.4f);

	m_sattelite4.setLightPos(5000, 10, 10000);
	m_sattelite4.setEyePos(5000, 10, 10000);

	im.load("../data/uranus.jpg");
	tex = m_sattelite4.initTexture(im);

	initBlend(m_satt4Blend, cnt1, cnt2, R2 * 0.4f, 40, tex, 0.1);
	///////////

	m_sattelite2.initBuffer(this, true);
	m_sattelite2.setType(GL_TRIANGLE_STRIP);
	m_sattelite2.setColor(1, 1, 1, 1);
	m_sattelite2.setSpecularStrength(specStreangth1);
	m_sattelite2.setDiffuseStrength(diffStrength);

	m_sattelite2.initSphere(cnt1, cnt2, R3);

	m_sattelite2.setLightPos(5000, 10, 10000);
	m_sattelite2.setEyePos(5000, 10, 10000);

	im.load("../data/ZZBiHOH.jpg");
	m_sattelite2.initTexture(im);

	///////////////////

	m_sattelite3.initBuffer(this, true);
	m_sattelite3.setType(GL_TRIANGLE_STRIP);
	m_sattelite3.setColor(1, 1, 1, 1);
	m_sattelite3.setSpecularStrength(specStreangth1);
	m_sattelite3.setDiffuseStrength(diffStrength);

	m_sattelite3.initSphere(cnt1, cnt2, R4);

	m_sattelite3.setLightPos(5000, 10, 10000);
	m_sattelite3.setEyePos(5000, 10, 10000);

	im.load("../data/8k_ceres_fictional.jpg");
	m_sattelite3.initTexture(im);

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
	obj.setLightPos(5000, 10, 10000);
	obj.setEyePos(5000, 10, 10000);
	obj.initTexture(tex);

	for(uint i = 0; i < countBlending; ++i){
		float di = (float)i / countBlending;
		float d = (di * delta);

		obj.setScale(i, d);
		obj.setBlendStrength(i, (1 - sqrt(di)));
	}
}
