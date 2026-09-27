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

#include <algorithm>
#include <cmath>

#define _USE_MATH_DEFINES

namespace
{
float deg2rad(float d)
{
    return d * (float)M_PI / 180.0f;
}

// apparent angular radius of the painted sun disc (~0.8 deg); it sets the
// penumbra width of the shadows every body casts
constexpr float kSunAngleRad = 0.01396f;
}

SpaceScene::SpaceScene()
    : QOpenGLWindow()
    , QOpenGLFunctions_3_3_Core()
{
    m_time = 0.0f;
    m_speed = 0.01f;
    m_dragging = false;
    m_lastFrame.start();

    // sun: a distant, unreachable light source, in the ecliptic plane
    // (space has no "above" — keep the sun on the orbital horizon)
    m_sunDir = QVector3D(0.72f, 0.0f, 0.63f).normalized();

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
        if (m_ring)
            m_ring->release(this);
        if (m_fsTri)
            m_fsTri->release(this);

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
    m_pu.spec = m_planetProg->uniformLocation("uSpec");
    m_pu.shin = m_planetProg->uniformLocation("uShin");
    m_pu.occluders = m_planetProg->uniformLocation("uOccluders[0]");
    m_pu.occluderCount = m_planetProg->uniformLocation("uOccluderCount");
    m_pu.sunAngle = m_planetProg->uniformLocation("uSunAngle");
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

    m_atmProg = std::make_unique<ShaderProgram>();
    m_atmProg->compile("atmosphere-shell",
                       spaceShaders::kAtmShellVertex,
                       spaceShaders::kAtmFragment);

    m_atmFsProg = std::make_unique<ShaderProgram>();
    m_atmFsProg->compile("atmosphere-fullscreen",
                         spaceShaders::kAtmFsVertex,
                         spaceShaders::kAtmFragment);

    m_atmProg->bind();
    m_au.model = m_atmProg->uniformLocation("uModel");
    m_au.view = m_atmProg->uniformLocation("uView");
    m_au.proj = m_atmProg->uniformLocation("uProj");
    m_au.camPos = m_atmProg->uniformLocation("uCamPos");
    m_au.center = m_atmProg->uniformLocation("uCenter");
    m_au.planetR = m_atmProg->uniformLocation("uPlanetR");
    m_au.shellR = m_atmProg->uniformLocation("uShellR");
    m_au.sunDir = m_atmProg->uniformLocation("uSunDir");
    m_au.sunColor = m_atmProg->uniformLocation("uSunColor");
    m_au.atmColor = m_atmProg->uniformLocation("uAtmColor");
    m_au.density = m_atmProg->uniformLocation("uDensity");
    m_au.scaleH = m_atmProg->uniformLocation("uScaleH");
    m_au.tex = m_atmProg->uniformLocation("uTex");
    m_au.spin = m_atmProg->uniformLocation("uSpin");
    m_au.time = m_atmProg->uniformLocation("uTime");
    m_au.noiseAmt = m_atmProg->uniformLocation("uNoiseAmt");
    m_au.occluders = m_atmProg->uniformLocation("uOccluders[0]");
    m_au.occluderCount = m_atmProg->uniformLocation("uOccluderCount");
    m_atmProg->release();

    m_atmFsProg->bind();
    m_afu.invViewProj = m_atmFsProg->uniformLocation("uInvViewProj");
    m_afu.camPos = m_atmFsProg->uniformLocation("uCamPos");
    m_afu.center = m_atmFsProg->uniformLocation("uCenter");
    m_afu.planetR = m_atmFsProg->uniformLocation("uPlanetR");
    m_afu.shellR = m_atmFsProg->uniformLocation("uShellR");
    m_afu.sunDir = m_atmFsProg->uniformLocation("uSunDir");
    m_afu.sunColor = m_atmFsProg->uniformLocation("uSunColor");
    m_afu.atmColor = m_atmFsProg->uniformLocation("uAtmColor");
    m_afu.density = m_atmFsProg->uniformLocation("uDensity");
    m_afu.scaleH = m_atmFsProg->uniformLocation("uScaleH");
    m_afu.tex = m_atmFsProg->uniformLocation("uTex");
    m_afu.spin = m_atmFsProg->uniformLocation("uSpin");
    m_afu.time = m_atmFsProg->uniformLocation("uTime");
    m_afu.noiseAmt = m_atmFsProg->uniformLocation("uNoiseAmt");
    m_afu.occluders = m_atmFsProg->uniformLocation("uOccluders[0]");
    m_afu.occluderCount = m_atmFsProg->uniformLocation("uOccluderCount");
    m_atmFsProg->release();

    m_ringProg = std::make_unique<ShaderProgram>();
    // the ring vertex stage is the planet's (same layout: model/view/proj +
    // pos/normal/uv); only the fragment differs
    m_ringProg->compile("rings",
                        spaceShaders::kPlanetVertex,
                        spaceShaders::kRingFragment);

    m_ringProg->bind();
    m_ru.model = m_ringProg->uniformLocation("uModel");
    m_ru.view = m_ringProg->uniformLocation("uView");
    m_ru.proj = m_ringProg->uniformLocation("uProj");
    m_ru.sunDir = m_ringProg->uniformLocation("uSunDir");
    m_ru.sunColor = m_ringProg->uniformLocation("uSunColor");
    m_ru.center = m_ringProg->uniformLocation("uCenter");
    m_ru.planetR = m_ringProg->uniformLocation("uPlanetR");
    m_ru.tex = m_ringProg->uniformLocation("uRingTex");
    m_ringProg->release();

    // --- geometry: one shared unit sphere for everything ---
    m_sphere = std::make_unique<Mesh>();
    {
        QVector<float> pos, nrm, uv;
        QVector<unsigned int> idx;
        meshes::sphereData(96, 128, pos, nrm, uv, idx);
        if (!m_sphere->build(this, pos, nrm, uv, idx))
            qCritical() << "sphere mesh build failed";
    }

    // fullscreen triangle (NDC) for the "camera inside the gas" pass
    m_fsTri = std::make_unique<Mesh>();
    {
        QVector<float> pos, nrm, uv;
        QVector<unsigned int> idx{0, 1, 2};
        const float tri[3][2] = {{-1.0f, -1.0f}, {3.0f, -1.0f}, {-1.0f, 3.0f}};
        for (const auto &t : tri) {
            pos << t[0] << t[1] << 0.0f;
            nrm << 0.0f << 0.0f << 0.0f;
            uv << 0.0f << 0.0f;
        }
        if (!m_fsTri->build(this, pos, nrm, uv, idx))
            qCritical() << "fullscreen triangle build failed";
    }

    // --- ring annulus: one shared mesh, sized from the first ringOn body ---
    m_ring = std::make_unique<Mesh>();
    {
        float rIn = 1.2f;
        float rOut = 2.35f;
        for (const Body &b : m_bodies) {
            if (b.ringOn) {
                rIn = b.ringInnerScale;
                rOut = b.ringOuterScale;
                break;
            }
        }
        QVector<float> pos, nrm, uv;
        QVector<unsigned int> idx;
        meshes::ringData(rIn, rOut, 256, pos, nrm, uv, idx);
        if (!m_ring->build(this, pos, nrm, uv, idx))
            qCritical() << "ring mesh build failed";
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
        jup.atmosphereDensity = 0.02f;   // grazing limb optical depth ~1.7
        jup.atmosphereNoise = 0.55f;     // banded, structured gas
        m_bodies.push_back(jup);
    }
    {
        Body io;
        io.name = "Io";
        io.radius = 1.0f;
        io.texture = ":/data/io_truecolor_texture_map_8k_by_fargetanik-dbpxndx.jpg";
        io.orbitRadius = 170.0f;
        io.orbitPeriod = 288.0f;   // time units per full revolution
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
        eu.orbitPeriod = 432.0f;
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
        ce.orbitPeriod = 192.0f;
        ce.orbitPhaseDeg = 320.0f;
        ce.orbitInclDeg = -6.0f;
        ce.spinPeriodDeg = 14.0f;
        m_bodies.push_back(ce);
    }
    {
        Body ga;
        ga.name = "Ganymede";
        ga.radius = 1.5f;
        ga.texture = ":/data/8k_eris_fictional.jpg";
        ga.orbitRadius = 240.0f;
        ga.orbitPeriod = 720.0f;
        ga.orbitPhaseDeg = 300.0f;
        ga.orbitInclDeg = 2.0f;
        ga.spinPeriodDeg = 6.0f;
        ga.specularStrength = 0.05f;
        m_bodies.push_back(ga);
    }
    {
        Body ca;
        ca.name = "Callisto";
        ca.radius = 1.35f;
        ca.texture = ":/data/8k_haumea_fictional.jpg";
        ca.orbitRadius = 290.0f;
        ca.orbitPeriod = 1080.0f;
        ca.orbitPhaseDeg = 130.0f;
        ca.orbitInclDeg = -3.0f;
        ca.spinPeriodDeg = 5.0f;
        m_bodies.push_back(ca);
    }
    {
        Body ur;
        ur.name = "Uranus";
        ur.radius = 56.0f;
        ur.texture = ":/data/uranus.jpg";
        ur.orbitRadius = 900.0f;
        ur.orbitPeriod = 2880.0f;  // ~48 min at 1x time
        ur.orbitPhaseDeg = 240.0f;
        ur.orbitInclDeg = -25.0f;
        ur.spinPeriodDeg = 30.0f;
        ur.specularStrength = 0.0f;
        ur.atmosphereOn = true;
        ur.atmosphereColor = QVector3D(0.45f, 0.75f, 0.8f);
        ur.atmosphereDensity = 0.05f;    // thinner, hazier gas
        ur.atmosphereNoise = 0.25f;      // mostly smooth haze
        m_bodies.push_back(ur);
    }
    {
        Body sa;
        sa.name = "Saturn";
        sa.radius = 115.0f;
        sa.texture = ":/data/8k_saturn.jpg";
        sa.orbitRadius = 1300.0f;
        sa.orbitPeriod = 4320.0f;
        sa.orbitPhaseDeg = 60.0f;
        sa.orbitInclDeg = -8.0f;
        sa.spinPeriodDeg = 8.0f; // fastest spinner in the scene
        sa.specularStrength = 0.0f;
        // the ring system: flat annulus, radial band texture (stage D2b)
        sa.ringOn = true;
        sa.ringTexture = ":/data/8k_saturn_ring_alpha.png";
        sa.ringTiltDeg = 27.0f; // real axial tilt
        m_bodies.push_back(sa);
    }
    {
        Body ti;
        ti.name = "Titan";
        ti.radius = 1.2f;
        ti.texture = ":/data/8k_makemake_fictional.jpg";
        ti.orbitRadius = 420.0f; // outside the rings (added in D2b)
        ti.orbitPeriod = 1440.0f;
        ti.orbitPhaseDeg = 80.0f;
        ti.orbitInclDeg = 1.5f;
        ti.parent = (int)m_bodies.size() - 1; // Saturn
        ti.spinPeriodDeg = 1.5f;
        ti.specularStrength = 0.0f;
        // Titan's famous thick orange haze: small shell, so the base
        // density must be high to reach a limb optical depth ~1
        ti.atmosphereOn = true;
        ti.atmosphereColor = QVector3D(1.0f, 0.55f, 0.25f);
        ti.atmosphereRadiusScale = 1.15f;
        ti.atmosphereDensity = 1.2f;
        ti.atmosphereScaleHeightRatio = 0.05f;
        ti.atmosphereNoise = 0.3f;
        m_bodies.push_back(ti);
    }

    for (Body &b : m_bodies) {
        b.texId = uploadTexture(b.texture);
        if (b.ringOn)
            b.ringTexId = uploadTexture(b.ringTexture);
    }

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

        // W/S or wheel adjust distance (frame-rate independent, ~1.5x per second)
        const float zoom = std::exp(1.5f * dt);
        if (m_keys[Qt::Key_W])
            m_focusDist = std::max(1.6f, m_focusDist / zoom);
        if (m_keys[Qt::Key_S])
            m_focusDist = std::min(60.0f, m_focusDist * zoom);

        const QVector3D dir = -m_cam.forward();
        const QVector3D desired = b.position + dir * (b.radius * m_focusDist);
        const float k = 1.0f - std::exp(-4.0f * dt);
        m_cam.position += (desired - m_cam.position) * k;
        m_cam.lookAt(b.position);
    } else {
        // free fly: slow, deliberate flight — Jupiter is huge, crossing
        // its diameter should take ~20 seconds
        float speed = 15.0f * dt;
        if (m_keys[Qt::Key_Shift])
            speed *= 5.0f;

        if (m_keys[Qt::Key_W])
            m_cam.position += m_cam.forward() * speed;
        if (m_keys[Qt::Key_S])
            m_cam.position -= m_cam.forward() * speed;
        // A/D inverted (user preference, matches inverted mouse drag)
        if (m_keys[Qt::Key_A])
            m_cam.position += m_cam.right() * speed;
        if (m_keys[Qt::Key_D])
            m_cam.position -= m_cam.right() * speed;
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

void SpaceScene::drawBody(size_t i, const QMatrix4x4 &proj, const QMatrix4x4 &view)
{
    const Body &b = m_bodies[i];
    if (!b.texId)
        return;

    QMatrix4x4 model;
    model.translate(b.position);
    model.rotate(m_time * b.spinPeriodDeg, 0.0f, 1.0f, 0.0f);
    model.scale(b.radius);

    // every other body can cast a shadow on this one
    float occ[16 * 4] = {0};
    int nOcc = 0;
    for (size_t j = 0; j < m_bodies.size() && nOcc < 16; ++j) {
        if (j == i)
            continue;
        const Body &o = m_bodies[j];
        occ[nOcc * 4 + 0] = o.position.x();
        occ[nOcc * 4 + 1] = o.position.y();
        occ[nOcc * 4 + 2] = o.position.z();
        occ[nOcc * 4 + 3] = o.radius;
        ++nOcc;
    }

    m_planetProg->bind();
    glUniformMatrix4fv(m_pu.model, 1, GL_FALSE, (const float *)model.data());
    glUniformMatrix4fv(m_pu.view, 1, GL_FALSE, (const float *)view.data());
    glUniformMatrix4fv(m_pu.proj, 1, GL_FALSE, (const float *)proj.data());
    glUniform3f(m_pu.camPos, m_cam.position.x(), m_cam.position.y(), m_cam.position.z());
    glUniform3f(m_pu.sunDir, m_sunDir.x(), m_sunDir.y(), m_sunDir.z());
    glUniform3f(m_pu.sunColor, m_sunColor.x(), m_sunColor.y(), m_sunColor.z());
    glUniform1f(m_pu.spec, b.specularStrength);
    glUniform1f(m_pu.shin, b.shininess);
    glUniform4fv(m_pu.occluders, 16, occ);
    glUniform1i(m_pu.occluderCount, nOcc);
    glUniform1f(m_pu.sunAngle, kSunAngleRad);
    glUniform1i(m_pu.tex, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, b.texId);

    m_sphere->draw();

    m_planetProg->release();
}

void SpaceScene::drawAtmosphere(size_t i, const QMatrix4x4 &proj, const QMatrix4x4 &view)
{
    const Body &b = m_bodies[i];
    const float shellR = b.radius * b.atmosphereRadiusScale;
    const float scaleH = b.radius * b.atmosphereScaleHeightRatio;
    const bool inside = (m_cam.position - b.position).length() < shellR;

    // every solid body shadows the gas — including the planet itself, whose
    // night-side gas sits in its own umbra (proper terminator on the gas)
    float occ[16 * 4] = {0};
    int nOcc = 0;
    for (size_t j = 0; j < m_bodies.size() && nOcc < 16; ++j) {
        const Body &o = m_bodies[j];
        occ[nOcc * 4 + 0] = o.position.x();
        occ[nOcc * 4 + 1] = o.position.y();
        occ[nOcc * 4 + 2] = o.position.z();
        occ[nOcc * 4 + 3] = o.radius;
        ++nOcc;
    }

    ShaderProgram *prog;
    const Mesh *mesh;
    if (inside) {
        // camera inside the gas: it surrounds everything on screen, so a
        // fullscreen pass composites over the already-drawn frame
        glDisable(GL_DEPTH_TEST);
        prog = m_atmFsProg.get();
        mesh = m_fsTri.get();
        prog->bind();
        QMatrix4x4 invVP = (proj * view).inverted();
        glUniformMatrix4fv(m_afu.invViewProj, 1, GL_FALSE, (const float *)invVP.data());
        glUniform3f(m_afu.camPos, m_cam.position.x(), m_cam.position.y(), m_cam.position.z());
        glUniform3f(m_afu.center, b.position.x(), b.position.y(), b.position.z());
        glUniform1f(m_afu.planetR, b.radius);
        glUniform1f(m_afu.shellR, shellR);
        glUniform3f(m_afu.sunDir, m_sunDir.x(), m_sunDir.y(), m_sunDir.z());
        glUniform3f(m_afu.sunColor, m_sunColor.x(), m_sunColor.y(), m_sunColor.z());
        glUniform3f(m_afu.atmColor, b.atmosphereColor.x(), b.atmosphereColor.y(), b.atmosphereColor.z());
        glUniform1f(m_afu.density, b.atmosphereDensity);
        glUniform1f(m_afu.scaleH, scaleH);
        glUniform1f(m_afu.spin, m_time * b.spinPeriodDeg * 0.017453293f);
        glUniform1f(m_afu.time, m_time);
        glUniform1f(m_afu.noiseAmt, b.atmosphereNoise);
        glUniform4fv(m_afu.occluders, 16, occ);
        glUniform1i(m_afu.occluderCount, nOcc);
        glUniform1i(m_afu.tex, 0);
    } else {
        // camera outside: draw the shell's near hemisphere; the far one is
        // backface-culled (it is only ever hidden by the planet or by the
        // near hemisphere), and the depth test keeps it in front of the scene
        prog = m_atmProg.get();
        mesh = m_sphere.get();
        prog->bind();
        QMatrix4x4 model;
        model.translate(b.position);
        model.scale(shellR);
        glUniformMatrix4fv(m_au.model, 1, GL_FALSE, (const float *)model.data());
        glUniformMatrix4fv(m_au.view, 1, GL_FALSE, (const float *)view.data());
        glUniformMatrix4fv(m_au.proj, 1, GL_FALSE, (const float *)proj.data());
        glUniform3f(m_au.camPos, m_cam.position.x(), m_cam.position.y(), m_cam.position.z());
        glUniform3f(m_au.center, b.position.x(), b.position.y(), b.position.z());
        glUniform1f(m_au.planetR, b.radius);
        glUniform1f(m_au.shellR, shellR);
        glUniform3f(m_au.sunDir, m_sunDir.x(), m_sunDir.y(), m_sunDir.z());
        glUniform3f(m_au.sunColor, m_sunColor.x(), m_sunColor.y(), m_sunColor.z());
        glUniform3f(m_au.atmColor, b.atmosphereColor.x(), b.atmosphereColor.y(), b.atmosphereColor.z());
        glUniform1f(m_au.density, b.atmosphereDensity);
        glUniform1f(m_au.scaleH, scaleH);
        glUniform1f(m_au.spin, m_time * b.spinPeriodDeg * 0.017453293f);
        glUniform1f(m_au.time, m_time);
        glUniform1f(m_au.noiseAmt, b.atmosphereNoise);
        glUniform4fv(m_au.occluders, 16, occ);
        glUniform1i(m_au.occluderCount, nOcc);
        glUniform1i(m_au.tex, 0);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, b.texId);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied gas
    glDepthMask(GL_FALSE);
    mesh->draw();
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // scene default
    prog->release();
    if (inside)
        glEnable(GL_DEPTH_TEST);
}

void SpaceScene::drawRing(size_t i, const QMatrix4x4 &proj, const QMatrix4x4 &view)
{
    const Body &b = m_bodies[i];
    if (!b.ringOn || !b.ringTexId)
        return;

    QMatrix4x4 model;
    model.translate(b.position);
    model.rotate(b.ringTiltDeg, 1.0f, 0.0f, 0.0f);
    model.scale(b.radius);

    m_ringProg->bind();
    glUniformMatrix4fv(m_ru.model, 1, GL_FALSE, (const float *)model.data());
    glUniformMatrix4fv(m_ru.view, 1, GL_FALSE, (const float *)view.data());
    glUniformMatrix4fv(m_ru.proj, 1, GL_FALSE, (const float *)proj.data());
    glUniform3f(m_ru.sunDir, m_sunDir.x(), m_sunDir.y(), m_sunDir.z());
    glUniform3f(m_ru.sunColor, m_sunColor.x(), m_sunColor.y(), m_sunColor.z());
    glUniform3f(m_ru.center, b.position.x(), b.position.y(), b.position.z());
    glUniform1f(m_ru.planetR, b.radius);
    glUniform1i(m_ru.tex, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, b.ringTexId);

    // flat disc: visible from both sides; keep the depth test on (the planet
    // hides the far ring) but write no depth, and alpha-blend with the scene
    // default over everything already drawn
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    m_ring->draw();
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);

    m_ringProg->release();
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
    for (size_t i = 0; i < m_bodies.size(); i++)
        drawBody(i, proj, view);

    // atmospheres after all opaque bodies, far to near, so nearer gas
    // composites correctly over farther gas
    std::vector<size_t> order;
    order.reserve(m_bodies.size());
    for (size_t i = 0; i < m_bodies.size(); ++i)
        if (m_bodies[i].atmosphereOn)
            order.push_back(i);
    auto sqDist = [](const QVector3D &p, const QVector3D &o) {
        const float dx = p.x() - o.x(), dy = p.y() - o.y(), dz = p.z() - o.z();
        return dx * dx + dy * dy + dz * dz;
    };
    std::sort(order.begin(), order.end(), [this, &sqDist](size_t a, size_t b) {
        return sqDist(m_bodies[a].position, m_cam.position) >
               sqDist(m_bodies[b].position, m_cam.position);
    });
    for (size_t i : order)
        drawAtmosphere(i, proj, view);

    // rings last: thin translucent discs blended over the already-drawn
    // planet/gas (same accepted ordering approximation as the atmosphere pass)
    for (size_t i = 0; i < m_bodies.size(); ++i)
        if (m_bodies[i].ringOn)
            drawRing(i, proj, view);

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
