#include "space_scene.h"

#include <QApplication>
#include <QImage>
#include <QImageReader>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QTransform>

#include <cmath>

#define _USE_MATH_DEFINES

namespace
{
float deg2rad(float d)
{
    return d * (float)M_PI / 180.0f;
}
}

SpaceScene::SpaceScene()
    : QOpenGLWindow()
    , QOpenGLFunctions_3_3_Core()
{
    m_time = 0.0f;
    m_speed = 0.01f;
    m_dragging = false;
    m_lastFrame.start();

    // sun: a distant, unreachable light source, slightly above the ecliptic
    m_sunDir = QVector3D(0.72f, 0.28f, 0.63f).normalized();

    // start outside Jupiter, looking at the system
    m_cam.position = QVector3D(0.0f, 80.0f, 620.0f);
    m_cam.lookAt(QVector3D(0, 0, 0));

    connect(&m_timer, &QTimer::timeout, this, [this]() { update(); });
    m_timer.start(15);
}

SpaceScene::~SpaceScene()
{
    if (QOpenGLContext *ctx = context()) {
        const bool wasCurrent = (QOpenGLContext::currentContext() == ctx);
        if (!wasCurrent)
            ctx->makeCurrent(this);

        if (!m_textures.empty())
            glDeleteTextures((GLsizei)m_textures.size(), m_textures.data());
        if (m_sphere)
            m_sphere->release(this);

        if (!wasCurrent)
            ctx->doneCurrent();
    }
}

void SpaceScene::setSpeed(float val)
{
    m_speed = val;
}

void SpaceScene::setCameraForShot(const QVector3D &pos, float yawDeg, float pitchDeg)
{
    m_cam.position = pos;
    m_cam.yaw = deg2rad(yawDeg);
    m_cam.pitch = deg2rad(pitchDeg);
    m_cam.clampPitch();
}

void SpaceScene::requestScreenshot(const QString &path, int frames)
{
    m_shotPath = path;
    m_shotFramesLeft = frames;
    m_shotCount = 0;
}

void SpaceScene::initializeGL()
{
    QOpenGLWindow::initializeGL();
    QOpenGLFunctions_3_3_Core::initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // anisotropic filtering (extension-gated)
    const GLubyte *exts = glGetString(GL_EXTENSIONS);
    if (exts) {
        const char *s = reinterpret_cast<const char *>(exts);
        if (strstr(s, "GL_EXT_texture_filter_anisotropic")) {
            m_hasAniso = true;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &m_maxAniso);
            if (m_maxAniso < 1.0f)
                m_maxAniso = 1.0f;
        }
    }

    buildScene();
}

void SpaceScene::resizeGL(int w, int h)
{
    QOpenGLWindow::resizeGL(w, h);
}

unsigned int SpaceScene::uploadTexture(const QString &resPath)
{
    QImage img;
    if (!img.load(resPath) || img.isNull()) {
        qWarning() << "SpaceScene: cannot load texture" << resPath;
        return 0;
    }
    // JPEG decodes to RGB32 (memory layout BGRA), PNGs vary -> normalize
    img = img.convertToFormat(QImage::Format_RGBA8888);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width(), img.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (m_hasAniso)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_maxAniso);

    m_textures.push_back(tex);
    return tex;
}

void SpaceScene::buildScene()
{
    // --- shaders ---
    // compile() prints the full driver log + source and exits on failure,
    // so a broken program can never leak into the rest of the scene setup.
    m_planetProg = std::make_unique<ShaderProgram>();
    m_planetProg->compile("planet",
                          spaceShaders::kPlanetVertex,
                          spaceShaders::kPlanetFragment);

    m_skyProg = std::make_unique<ShaderProgram>();
    m_skyProg->compile("sky",
                       spaceShaders::kSkyVertex,
                       spaceShaders::kSkyFragment);

    m_planetProg->bind();
    m_pu.model = m_planetProg->uniformLocation("uModel");
    m_pu.view = m_planetProg->uniformLocation("uView");
    m_pu.proj = m_planetProg->uniformLocation("uProj");
    m_pu.camPos = m_planetProg->uniformLocation("uCamPos");
    m_pu.sunDir = m_planetProg->uniformLocation("uSunDir");
    m_pu.sunColor = m_planetProg->uniformLocation("uSunColor");
    m_pu.tex = m_planetProg->uniformLocation("uTex");
    m_pu.atmColor = m_planetProg->uniformLocation("uAtmColor");
    m_pu.atmOn = m_planetProg->uniformLocation("uAtmOn");
    m_pu.spec = m_planetProg->uniformLocation("uSpec");
    m_pu.shin = m_planetProg->uniformLocation("uShin");
    m_planetProg->release();

    m_skyProg->bind();
    m_su.model = m_skyProg->uniformLocation("uModel");
    m_su.view = m_skyProg->uniformLocation("uView");
    m_su.proj = m_skyProg->uniformLocation("uProj");
    m_su.camPos = m_skyProg->uniformLocation("uCamPos");
    m_su.sunDir = m_skyProg->uniformLocation("uSunDir");
    m_su.sunColor = m_skyProg->uniformLocation("uSunColor");
    m_su.skyTex = m_skyProg->uniformLocation("uSkyTex");
    m_skyProg->release();

    // --- geometry: one shared unit sphere for everything ---
    m_sphere = std::make_unique<Mesh>();
    {
        QVector<float> pos, nrm, uv;
        QVector<unsigned int> idx;
        meshes::sphereData(96, 128, pos, nrm, uv, idx);
        if (!m_sphere->build(this, pos, nrm, uv, idx))
            qCritical() << "sphere mesh build failed";
    }

    // --- sky dome ---
    m_skyTex = uploadTexture(":/data/8k_stars_milky_way.jpg");

    // --- bodies (the Jovian system + a distant Uranus) ---
    m_bodies.clear();

    {
        Body jup;
        jup.name = "Jupiter";
        jup.radius = 140.0f;
        jup.texture = ":/data/8k_jupiter.jpg";
        jup.spinPeriodDeg = 6.0f;
        jup.specularStrength = 0.0f;
        jup.atmosphereOn = true;
        jup.atmosphereColor = QVector3D(1.0f, 0.72f, 0.5f);
        m_bodies.push_back(jup);
    }
    {
        Body io;
        io.name = "Io";
        io.radius = 1.0f;
        io.texture = ":/data/io_truecolor_texture_map_8k_by_fargetanik-dbpxndx.jpg";
        io.orbitRadius = 170.0f;
        io.orbitPeriod = 72.0f;    // time units per full revolution
        io.orbitPhaseDeg = 200.0f;
        io.spinPeriodDeg = 10.0f;
        io.specularStrength = 0.1f;
        m_bodies.push_back(io);
    }
    {
        Body eu;
        eu.name = "Europa";
        eu.radius = 1.3f;
        eu.texture = ":/data/ZZBiHOH.jpg";
        eu.orbitRadius = 200.0f;
        eu.orbitPeriod = 108.0f;
        eu.orbitPhaseDeg = 40.0f;
        eu.orbitInclDeg = 4.0f;
        eu.spinPeriodDeg = 8.0f;
        eu.specularStrength = 0.25f;
        eu.shininess = 48.0f;
        m_bodies.push_back(eu);
    }
    {
        Body ce;
        ce.name = "Ceres";
        ce.radius = 0.5f;
        ce.texture = ":/data/8k_ceres_fictional.jpg";
        ce.orbitRadius = 150.0f;
        ce.orbitPeriod = 48.0f;
        ce.orbitPhaseDeg = 320.0f;
        ce.orbitInclDeg = -6.0f;
        ce.spinPeriodDeg = 14.0f;
        m_bodies.push_back(ce);
    }
    {
        Body ur;
        ur.name = "Uranus";
        ur.radius = 56.0f;
        ur.texture = ":/data/uranus.jpg";
        ur.orbitRadius = 450.0f;
        ur.orbitPeriod = 720.0f;   // ~12 min at 1x time
        ur.orbitPhaseDeg = 240.0f;
        ur.orbitInclDeg = -25.0f;
        ur.spinPeriodDeg = 30.0f;
        ur.specularStrength = 0.0f;
        ur.atmosphereOn = true;
        ur.atmosphereColor = QVector3D(0.45f, 0.75f, 0.8f);
        m_bodies.push_back(ur);
    }

    for (Body &b : m_bodies)
        b.texId = uploadTexture(b.texture);

    updateOrbits();
}

void SpaceScene::updateOrbits()
{
    for (size_t i = 0; i < m_bodies.size(); ++i) {
        Body &b = m_bodies[i];
        if (b.orbitRadius <= 0.0f)
            continue; // fixed body keeps its initial position

        // orbitPeriod = time units per full revolution; angle in degrees
        const float ang = deg2rad(b.orbitPhaseDeg + 360.0f * m_time / b.orbitPeriod);
        QVector3D p(cosf(ang) * b.orbitRadius, 0.0f, sinf(ang) * b.orbitRadius);
        if (b.orbitInclDeg != 0.0f) {
            const float c = cosf(deg2rad(b.orbitInclDeg));
            const float s = sinf(deg2rad(b.orbitInclDeg));
            p = QVector3D(p.x(), p.y() * c - p.z() * s, p.y() * s + p.z() * c);
        }
        if (b.parent >= 0 && (size_t)b.parent < m_bodies.size())
            p += m_bodies[b.parent].position;
        b.position = p;
    }
}

void SpaceScene::updateCamera(float dt)
{
    if (m_focus >= 0 && (size_t)m_focus < m_bodies.size()) {
        // follow mode: orbit the focused body
        const Body &b = m_bodies[m_focus];

        // W/S or wheel adjust distance
        if (m_keys[Qt::Key_W])
            m_focusDist = std::max(1.6f, m_focusDist / 1.03f);
        if (m_keys[Qt::Key_S])
            m_focusDist = std::min(60.0f, m_focusDist * 1.03f);

        const QVector3D dir = -m_cam.forward();
        const QVector3D desired = b.position + dir * (b.radius * m_focusDist);
        const float k = 1.0f - std::exp(-4.0f * dt);
        m_cam.position += (desired - m_cam.position) * k;
        m_cam.lookAt(b.position);
    } else {
        // free fly
        float speed = 60.0f * dt;
        if (m_keys[Qt::Key_Shift])
            speed *= 5.0f;

        if (m_keys[Qt::Key_W])
            m_cam.position += m_cam.forward() * speed;
        if (m_keys[Qt::Key_S])
            m_cam.position -= m_cam.forward() * speed;
        if (m_keys[Qt::Key_A])
            m_cam.position -= m_cam.right() * speed;
        if (m_keys[Qt::Key_D])
            m_cam.position += m_cam.right() * speed;
        if (m_keys[Qt::Key_E])
            m_cam.position += QVector3D(0, 1, 0) * speed;
        if (m_keys[Qt::Key_Q])
            m_cam.position -= QVector3D(0, 1, 0) * speed;
    }
}

void SpaceScene::cycleFocus()
{
    const int n = (int)m_bodies.size();
    m_focus = (m_focus + 1) % (n + 1);
    m_focusDist = 4.0f;

    const QString name = (m_focus < 0) ? QString("free fly") : m_bodies[m_focus].name;
    emit focusChanged(name);
    emit infoChanged(QString("%1 — W/S zoom, drag to orbit, M to cycle").arg(name));
}

void SpaceScene::drawSky(const QMatrix4x4 &proj, const QMatrix4x4 &view)
{
    m_skyProg->bind();

    QMatrix4x4 m;
    m.translate(m_cam.position);
    m.scale(kSkyRadius);

    glUniformMatrix4fv(m_su.model, 1, GL_FALSE, (const float *)m.data());
    glUniformMatrix4fv(m_su.view, 1, GL_FALSE, (const float *)view.data());
    glUniformMatrix4fv(m_su.proj, 1, GL_FALSE, (const float *)proj.data());
    glUniform3f(m_su.camPos, m_cam.position.x(), m_cam.position.y(), m_cam.position.z());
    glUniform3f(m_su.sunDir, m_sunDir.x(), m_sunDir.y(), m_sunDir.z());
    glUniform3f(m_su.sunColor, m_sunColor.x(), m_sunColor.y(), m_sunColor.z());
    glUniform1i(m_su.skyTex, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_skyTex);

    glDepthMask(GL_FALSE);
    glCullFace(GL_FRONT); // view the dome from the inside
    m_sphere->draw();
    glCullFace(GL_BACK);
    glDepthMask(GL_TRUE);

    m_skyProg->release();
}

void SpaceScene::drawBody(const Body &b, const QMatrix4x4 &proj, const QMatrix4x4 &view)
{
    if (!b.texId)
        return;

    QMatrix4x4 model;
    model.translate(b.position);
    model.rotate(m_time * b.spinPeriodDeg, 0.0f, 1.0f, 0.0f);
    model.scale(b.radius);

    m_planetProg->bind();
    glUniformMatrix4fv(m_pu.model, 1, GL_FALSE, (const float *)model.data());
    glUniformMatrix4fv(m_pu.view, 1, GL_FALSE, (const float *)view.data());
    glUniformMatrix4fv(m_pu.proj, 1, GL_FALSE, (const float *)proj.data());
    glUniform3f(m_pu.camPos, m_cam.position.x(), m_cam.position.y(), m_cam.position.z());
    glUniform3f(m_pu.sunDir, m_sunDir.x(), m_sunDir.y(), m_sunDir.z());
    glUniform3f(m_pu.sunColor, m_sunColor.x(), m_sunColor.y(), m_sunColor.z());
    glUniform3f(m_pu.atmColor, b.atmosphereColor.x(), b.atmosphereColor.y(), b.atmosphereColor.z());
    glUniform1f(m_pu.atmOn, b.atmosphereOn ? 1.0f : 0.0f);
    glUniform1f(m_pu.spec, b.specularStrength);
    glUniform1f(m_pu.shin, b.shininess);
    glUniform1i(m_pu.tex, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, b.texId);

    m_sphere->draw();

    m_planetProg->release();
}

void SpaceScene::paintGL()
{
    const float dt = std::min(0.1f, m_lastFrame.restart() / 1000.0f);

    m_time += dt * m_speed * 30.0f;
    updateOrbits();
    updateCamera(dt);

    if (width() < 2 || height() < 2)
        return;

    // the default framebuffer is sized in physical pixels: scale by the
    // screen's device pixel ratio (Windows display scaling), otherwise at
    // non-100% scaling the scene renders into a sub-region of the window
    const qreal dpr = devicePixelRatio();
    const int fbW = qRound(width() * dpr);
    const int fbH = qRound(height() * dpr);
    glViewport(0, 0, fbW, fbH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.004f, 0.005f, 0.01f, 1.0f);

    const QMatrix4x4 proj = m_cam.projectionMatrix(float(width()) / float(height()));
    const QMatrix4x4 view = m_cam.viewMatrix();

    drawSky(proj, view);
    for (const Body &b : m_bodies)
        drawBody(b, proj, view);

    if (!m_shotPath.isEmpty()) {
        ++m_shotCount;
        if (m_shotCount >= m_shotFramesLeft)
            saveScreenshot();
    }
}

void SpaceScene::saveScreenshot()
{
    // read the full physical framebuffer (see paintGL)
    const qreal dpr = devicePixelRatio();
    const int w = qRound(width() * dpr);
    const int h = qRound(height() * dpr);
    QImage img(w, h, QImage::Format_RGB888);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, img.bits());
    QTransform flip;
    flip.scale(1.0, -1.0);
    flip.translate(0.0, -h);
    img = img.transformed(flip);
    if (img.save(m_shotPath))
        qInfo().noquote() << "screenshot saved:" << m_shotPath;
    else
        qWarning() << "screenshot save failed:" << m_shotPath;
    m_shotPath.clear();
    // Quit from the event loop, not from inside paintGL: a direct quit() here
    // crashes during window/context shutdown.
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}

void SpaceScene::mousePressEvent(QMouseEvent *e)
{
    m_lastMouse = e->pos();
    m_dragging = true;
}

void SpaceScene::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_dragging)
        return;
    const QPoint d = e->pos() - m_lastMouse;
    m_lastMouse = e->pos();

    // drag = grab the scene: moving the mouse right/down turns the view left/up
    m_cam.yaw -= d.x() * 0.005f;
    m_cam.pitch -= d.y() * 0.005f;
    m_cam.clampPitch();
}

void SpaceScene::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
}

void SpaceScene::wheelEvent(QWheelEvent *e)
{
    const float dy = e->angleDelta().y();
    if (m_focus >= 0) {
        m_focusDist = std::clamp(m_focusDist * (dy > 0 ? 0.9f : 1.111f), 1.6f, 60.0f);
    } else {
        m_cam.fovDeg = std::clamp(m_cam.fovDeg - dy * 0.02f, 20.0f, 100.0f);
    }
    e->accept();
}

void SpaceScene::keyPressEvent(QKeyEvent *e)
{
    m_keys[e->key()] = true;
    if (e->key() == Qt::Key_M)
        cycleFocus();
}

void SpaceScene::keyReleaseEvent(QKeyEvent *e)
{
    m_keys[e->key()] = false;
}
